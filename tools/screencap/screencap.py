#!/usr/bin/env python3
"""Capture the TrekLink device screens from the native capture build, with no device attached.

The build `treklink-screencap-<variant>` (variants/native/treklink_screencap/platformio.ini) runs the real firmware
UI on the host and draws into a 128x64 in-memory display. This script starts it in simulated-radio mode on a fresh
virtual filesystem, presses the buttons a scenario lists and saves each requested screen as a PNG.

Usage (tools/screencap/capture-all.sh runs every scenario of every variant):
    screencap.py --variant v3 --scenario scenarios/first-boot-v3.txt --out shots/v3 --save-state state/v3
    screencap.py --variant v3 --scenario scenarios/frames-v3.txt --out shots/v3 --load-state state/v3

Scenario lines (blank lines and lines starting with # are skipped):
    press <button> <ms>   hold a button for <ms> ms (v2: up, down, menu, sos; v3 and v4: user)
    wait <ms>             let the firmware run
    shot <name>           save <out>/<name>.png
    heading <degrees>     point the simulated compass (v2 only)

Images are the panel's own pixels scaled up with nearest neighbour: lit pixels white on black, like the OLED.
Needs Pillow.
"""

import argparse
import datetime
import os
import pty
import queue
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]


def free_tcp_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def read_lines(stream, out_queue, log):
    for raw in iter(stream.readline, b""):
        text = raw.decode("utf-8", "replace").rstrip("\n")
        log.write(text + "\n")
        if text.startswith("SCREENCAP "):
            out_queue.put(text)


def nmea(body):
    checksum = 0
    for ch in body:
        checksum ^= ord(ch)
    return f"${body}*{checksum:02X}\r\n"


def feed_gps(master_fd, lat, lon, alt, stop):
    """Write a fixed GGA and RMC fix once a second, the way a GPS module does."""

    def dm(value, is_lat):
        hemi = ("N" if value >= 0 else "S") if is_lat else ("E" if value >= 0 else "W")
        value = abs(value)
        deg = int(value)
        minutes = (value - deg) * 60
        return (
            f"{deg:02d}{minutes:07.4f}" if is_lat else f"{deg:03d}{minutes:07.4f}"
        ), hemi

    la, lah = dm(lat, True)
    lo, loh = dm(lon, False)
    heard = b""
    next_fix = 0.0
    while not stop.is_set():
        # Answer the firmware's module probe the way an L76K does, so it accepts the simulated module.
        while select.select([master_fd], [], [], 0)[0]:
            try:
                heard = (heard + os.read(master_fd, 1024))[-4096:]
            except OSError:
                return
        if b"$PCAS06,0*1B" in heard:
            heard = b""
            os.write(master_fd, nmea("GPTXT,01,01,02,SW=URANUS5,V5.3.0.0").encode())
        if time.time() >= next_fix:
            next_fix = time.time() + 1
            now = datetime.datetime.now(datetime.timezone.utc)
            hms = now.strftime("%H%M%S.00")
            dmy = now.strftime("%d%m%y")
            data = nmea(
                f"GPGGA,{hms},{la},{lah},{lo},{loh},1,08,0.9,{alt:.1f},M,0.0,M,,"
            )
            data += nmea(f"GPRMC,{hms},A,{la},{lah},{lo},{loh},0.0,0.0,{dmy},,,A")
            try:
                os.write(master_fd, data.encode())
            except OSError:
                return
        stop.wait(0.05)


def pbm_to_png(pbm, png, scale):
    # PBM stores a set bit as black; the capture build sets the bit for a lit pixel, so invert to white on black.
    img = Image.open(pbm).convert("L").point(lambda v: 255 - v)
    img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(png)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--variant", required=True, choices=["v2", "v3", "v4"])
    ap.add_argument("--scenario", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument(
        "--binary",
        type=Path,
        help="default: .pio/build/treklink-screencap-<variant>/meshtasticd",
    )
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument(
        "--boot-wait",
        type=int,
        default=4000,
        help="ms to wait after start before the first step",
    )
    ap.add_argument(
        "--hwid",
        default="02:00:00:00:A1:23",
        help="virtual MAC; sets the node ID shown on screen",
    )
    ap.add_argument(
        "--gps", help="simulate a GPS fix: LAT,LON[,ALT_M] (a made-up position)"
    )
    ap.add_argument(
        "--load-state", type=Path, help="start from a node state saved by --save-state"
    )
    ap.add_argument(
        "--save-state",
        type=Path,
        help="keep the node state (prefs) here when the scenario ends",
    )
    args = ap.parse_args()

    binary = (
        args.binary
        or REPO
        / ".pio"
        / "build"
        / f"treklink-screencap-{args.variant}"
        / "meshtasticd"
    )
    if not binary.exists():
        sys.exit(
            f"{binary} not found; build it with: pio run -e treklink-screencap-{args.variant}"
        )

    args.out.mkdir(parents=True, exist_ok=True)
    fsdir = Path(tempfile.mkdtemp(prefix="treklink-screencap-"))
    if args.load_state:
        shutil.copytree(args.load_state, fsdir, dirs_exist_ok=True)
    log = open(args.out / "firmware.log", "w")
    env = dict(os.environ)
    gps_stop = threading.Event()
    if args.gps:
        parts = [float(v) for v in args.gps.split(",")]
        master_fd, slave_fd = pty.openpty()
        env["TREKLINK_SCREENCAP_GPS"] = os.ttyname(slave_fd)
        threading.Thread(
            target=feed_gps,
            args=(
                master_fd,
                parts[0],
                parts[1],
                parts[2] if len(parts) > 2 else 0.0,
                gps_stop,
            ),
            daemon=True,
        ).start()
    proc = subprocess.Popen(
        [
            str(binary),
            "-s",
            "-d",
            str(fsdir),
            "-p",
            str(free_tcp_port()),
            "-h",
            args.hwid,
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=fsdir,
        env=env,
    )
    replies = queue.Queue()
    threading.Thread(
        target=read_lines, args=(proc.stdout, replies, log), daemon=True
    ).start()

    def send(cmd, expect=None, timeout=10):
        proc.stdin.write((cmd + "\n").encode())
        proc.stdin.flush()
        if expect is not None:
            return wait_for(expect, timeout, cmd)

    def wait_for(expect, timeout, cmd=""):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                reply = replies.get(timeout=max(0.1, deadline - time.time()))
            except queue.Empty:
                break
            if reply.startswith("SCREENCAP error"):
                raise RuntimeError(f"{cmd}: {reply}")
            if reply.startswith(expect):
                return reply
        raise RuntimeError(
            f"{cmd}: no '{expect}' within {timeout} s (see {args.out / 'firmware.log'})"
        )

    shots = 0
    try:
        time.sleep(args.boot_wait / 1000)
        for number, line in enumerate(args.scenario.read_text().splitlines(), 1):
            parts = line.split()
            if not parts or parts[0].startswith("#"):
                continue
            if proc.poll() is not None:
                raise RuntimeError(
                    f"firmware exited with {proc.returncode} before line {number}"
                )
            if parts[0] == "press" and len(parts) == 3:
                send(f"press {parts[1]} {parts[2]}", "SCREENCAP pressed")
                wait_for("SCREENCAP released", int(parts[2]) / 1000 + 5)
            elif parts[0] == "heading" and len(parts) == 2:
                send(f"heading {parts[1]}", "SCREENCAP heading")
            elif parts[0] == "wait" and len(parts) == 2:
                time.sleep(int(parts[1]) / 1000)
            elif parts[0] == "shot" and len(parts) == 2:
                pbm = fsdir / f"{parts[1]}.pbm"
                send(f"shot {pbm}", "SCREENCAP shot")
                pbm_to_png(pbm, args.out / f"{parts[1]}.png", args.scale)
                shots += 1
            else:
                raise RuntimeError(f"{args.scenario}:{number}: cannot read '{line}'")
    finally:
        gps_stop.set()
        if proc.poll() is None:
            try:
                send("quit")
                proc.wait(timeout=5)
            except Exception:
                proc.kill()
        log.close()
        if args.save_state:
            shutil.rmtree(args.save_state, ignore_errors=True)
            shutil.copytree(
                fsdir, args.save_state, ignore=shutil.ignore_patterns("*.pbm")
            )
        shutil.rmtree(fsdir, ignore_errors=True)
    print(f"{shots} screens saved in {args.out}")


if __name__ == "__main__":
    main()
