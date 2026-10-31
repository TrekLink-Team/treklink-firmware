#!/usr/bin/env python3
"""tk: read, build and flash TrekLink units from a menu, without an assistant (specs/architecture REQ-ARC-09).

Run it from the repository root with the Python that PlatformIO installed, which already has pyserial:

    ~/.platformio/penv/bin/python treklink/tools/tk.py            the menu
    ~/.platformio/penv/bin/python treklink/tools/tk.py ports      list serial ports
    ~/.platformio/penv/bin/python treklink/tools/tk.py read  [--port P]
    ~/.platformio/penv/bin/python treklink/tools/tk.py build --env treklink-v2
    ~/.platformio/penv/bin/python treklink/tools/tk.py flash --env treklink-v2 [--port P]      application only, keeps settings
    ~/.platformio/penv/bin/python treklink/tools/tk.py install --env treklink-v2 [--port P]    erase, then a fresh image
    ~/.platformio/penv/bin/python treklink/tools/tk.py log [--port P] [--seconds 30] [--out FILE]

Every step prints the command it runs, so each one can be repeated by hand. Opening a port never
resets the unit: the DTR and RTS lines stay asserted (an ESP32-S3 resets on RTS high with DTR low).
"""
import argparse
import curses
import glob
import json
import re
import os
import shutil
import struct
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PIO_HOME = os.path.expanduser("~/.platformio")
ESPTOOL = os.path.join(PIO_HOME, "packages", "tool-esptoolpy", "esptool.py")

# TrekLink envs. chip is the esptool chip name; latch marks the v2 power latch (chapter 17 §7 rule 1).
ENVS = {
    "treklink-v2": {"name": "v2 custom board", "chip": "esp32s3", "latch": True},
    "treklink-v3": {"name": "v3 LilyGo T-Beam v1.2", "chip": "esp32", "latch": False},
    "treklink-v4": {"name": "v4 LilyGo T-Beam Supreme", "chip": "esp32s3", "latch": False},
}
USB_NAMES = {
    (0x303A, 0x1001): "ESP32-S3 USB (v2 or v4)",
    (0x10C4, 0xEA60): "CP210x USB-UART (T-Beam)",
    (0x1A86, 0x55D4): "CH9102 USB-UART (T-Beam)",
    (0x1A86, 0x7523): "CH340 USB-UART",
}

LATCH_WARNING = """
  v2 POWER LATCH: any reset drops the power latch and the unit turns off.
  Hold the power button down (or clamp it) now, and keep it held until the
  unit has booted after the flash.
"""


def serial_module():
    try:
        import serial  # noqa: F401
        import serial.tools.list_ports  # noqa: F401

        return serial
    except ImportError:
        sys.exit("pyserial is missing: run this tool with ~/.platformio/penv/bin/python")


def run(cmd, cwd=ROOT):
    print("\n$ " + " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=cwd)


# ---------------------------------------------------------------- ports


def list_ports():
    serial = serial_module()
    ports = []
    for p in serial.tools.list_ports.comports():
        if p.vid is None:
            continue
        ports.append({"port": p.device, "vid": p.vid, "pid": p.pid, "kind": USB_NAMES.get((p.vid, p.pid), p.description)})
    return ports


def pick_port(port):
    if port:
        return port
    ports = list_ports()
    if not ports:
        sys.exit("no USB serial port found: is the unit connected and powered?")
    if len(ports) > 1:
        sys.exit("several ports found, choose one with --port: " + ", ".join(p["port"] for p in ports))
    return ports[0]["port"]


def open_port(port):
    serial = serial_module()
    # pyserial asserts DTR and RTS on open by default, the state that never resets the chip.
    return serial.Serial(port, 115200, timeout=0.2)


# ---------------------------------------------------------------- client API (protobuf, read only)


def varint(buf, i):
    shift = value = 0
    while True:
        b = buf[i]
        i += 1
        value |= (b & 0x7F) << shift
        if not b & 0x80:
            return value, i
        shift += 7


def enc_varint(value):
    out = bytearray()
    while True:
        b = value & 0x7F
        value >>= 7
        out.append(b | (0x80 if value else 0))
        if not value:
            return bytes(out)


def fields(buf):
    """Yield (field number, wire type, value) of one protobuf message; nested messages stay bytes."""
    i = 0
    while i < len(buf):
        key, i = varint(buf, i)
        num, wt = key >> 3, key & 7
        if wt == 0:
            v, i = varint(buf, i)
        elif wt == 2:
            n, i = varint(buf, i)
            v, i = buf[i : i + n], i + n
        elif wt == 5:
            v, i = struct.unpack("<I", buf[i : i + 4])[0], i + 4
        elif wt == 1:
            v, i = struct.unpack("<Q", buf[i : i + 8])[0], i + 8
        else:
            return
        yield num, wt, v


def frames(raw):
    """FromRadio payloads in a stream that mixes text log lines and 0x94 0xC3 framed packets."""
    i = 0
    while True:
        i = raw.find(b"\x94\xc3", i)
        if i < 0 or i + 4 > len(raw):
            return
        n = (raw[i + 2] << 8) | raw[i + 3]
        if n > 512 or i + 4 + n > len(raw):
            i += 2
            continue
        yield raw[i + 4 : i + 4 + n]
        i += 4 + n


def read_unit(port, seconds=6.0):
    """Ask the unit for its configuration (ToRadio.want_config_id) and decode the identity fields."""
    s = open_port(port)
    payload = bytes([0x18]) + enc_varint(0x7EC1)  # ToRadio field 3, want_config_id
    s.write(b"\x94\xc3" + struct.pack(">H", len(payload)) + payload)
    raw = bytearray()
    end = time.time() + seconds
    while time.time() < end:
        raw += s.read(4096)
    s.close()
    info = {"port": port}
    my_num = None
    try:
        for fr in frames(bytes(raw)):
            for num, wt, v in fields(fr):
                if num == 3 and wt == 2:  # my_info
                    for n2, _, v2 in fields(v):
                        if n2 == 1:
                            my_num = v2
                            info["node"] = "!%08x" % v2
                        elif n2 == 8:
                            info["reboots"] = v2
                        elif n2 == 13:
                            info["env"] = v2.decode(errors="replace")
                elif num == 13 and wt == 2:  # metadata
                    for n2, _, v2 in fields(v):
                        if n2 == 1:
                            info["firmware"] = v2.decode(errors="replace")
                        elif n2 == 9:
                            info["hw_model"] = v2
                elif num == 4 and wt == 2:  # node_info
                    node = dict((n2, v2) for n2, _, v2 in fields(v))
                    if my_num is not None and node.get(1) == my_num:
                        for n3, _, v3 in fields(node.get(2, b"")):
                            if n3 == 2:
                                info["long_name"] = v3.decode(errors="replace")
                            elif n3 == 3:
                                info["short_name"] = v3.decode(errors="replace")
                        for n3, _, v3 in fields(node.get(6, b"")):
                            if n3 == 1:
                                info["battery_percent"] = v3
    except (IndexError, struct.error):
        pass  # a frame cut by the read window; keep what decoded
    if "node" not in info:
        info["error"] = ("no client API reply. A v2 release image has no USB console (finding F1); "
                         "flash a diagnostic image to talk to it over USB.")
    return info


def ld(field, data):
    return bytes([(field << 3) | 2]) + enc_varint(len(data)) + data


def send_test_command(port, command, listen=8.0, out_file=None):
    """Diagnostic images only: send "tk-test:<command>" to the unit itself and print its TrekLink log lines."""
    s = open_port(port)
    want = bytes([0x18]) + enc_varint(0x7EC2)  # ToRadio.want_config_id
    s.write(b"\x94\xc3" + struct.pack(">H", len(want)) + want)
    raw = bytearray()
    node = None
    end = time.time() + 6
    while time.time() < end and node is None:
        raw += s.read(4096)
        for fr in frames(bytes(raw)):
            for num, wt, v in fields(fr):
                if num == 3 and wt == 2:
                    node = dict((n2, v2) for n2, _, v2 in fields(v)).get(1)
    if node is None:
        s.close()
        sys.exit("no reply from the unit: is it running a diagnostic image?")
    data = ld(2, ("tk-test:" + command).encode())  # Data.payload
    data = bytes([0x08, 0x01]) + data  # Data.portnum = TEXT_MESSAGE_APP
    pkt = bytes([0x15]) + struct.pack("<I", node) + ld(4, data) + bytes([0x35]) + struct.pack("<I", int(time.time()) & 0x7FFFFFFF)
    msg = ld(1, pkt)  # ToRadio.packet
    print(f"  sending tk-test:{command} to !{node:08x}")
    s.write(b"\x94\xc3" + struct.pack(">H", len(msg)) + msg)
    out = bytearray()
    end = time.time() + listen
    while time.time() < end:
        out += s.read(4096)
    s.close()
    if out_file:
        with open(out_file, "ab") as f:
            f.write(out)
    text = re.sub(r"\x1b\[[0-9;]*m", "", out.decode("utf-8", "replace"))
    for line in text.splitlines():
        line = "".join(ch for ch in line if ch.isprintable())
        if " | " in line and any(k in line for k in ("TrekLink", "SOS", "Fall", "Input", "Button", "Buzz", "alarm")):
            print("  " + line[line.find(" | ") + 3 :])
    return text


# ---------------------------------------------------------------- build and flash


# A diagnostic image logs over USB (v2 has no USB console otherwise, finding F1) and accepts the
# tk-test button commands (treklink/src/test/TrekLinkTestHooks.h). Never flash one to a rented unit.
DIAGNOSTIC_FLAGS = "-DARDUINO_USB_CDC_ON_BOOT=1 -DTREKLINK_TEST_HOOKS=1"


def build(env, diagnostic=False):
    if diagnostic:
        print(f"\n  DIAGNOSTIC build: PLATFORMIO_BUILD_FLAGS=\"{DIAGNOSTIC_FLAGS}\"")
        os.environ["PLATFORMIO_BUILD_FLAGS"] = DIAGNOSTIC_FLAGS
    else:
        os.environ.pop("PLATFORMIO_BUILD_FLAGS", None)
    return run(["pio", "run", "-e", env])


def build_dir(env):
    return os.path.join(ROOT, ".pio", "build", env)


# Application-only offsets (chapter 17 §7 rule 2): bootloader, partitions, boot_app0, application. NVS and
# LittleFS are left alone, so the unit keeps its settings. The classic ESP32 bootloader sits at 0x1000.
# Offsets are fixed here on purpose: asking PlatformIO for its metadata re-runs its configuration and
# can clean the build directory when the build flags differ (seen 2026-10-31).
BOOTLOADER_OFFSET = {"esp32s3": 0x0, "esp32": 0x1000}
BOOT_APP0 = os.path.join(PIO_HOME, "packages", "framework-arduinoespressif32", "tools", "partitions", "boot_app0.bin")


def flash_images(env):
    d = build_dir(env)
    apps = [p for p in glob.glob(os.path.join(d, "firmware*.bin")) if not p.endswith(".factory.bin")]
    app = max(apps, key=os.path.getmtime) if apps else os.path.join(d, "firmware.bin")
    images = [
        (BOOTLOADER_OFFSET[ENVS[env]["chip"]], os.path.join(d, "bootloader.bin")),
        (0x8000, os.path.join(d, "partitions.bin")),
        (0xE000, BOOT_APP0),
        (0x10000, app),
    ]
    missing = [p for _, p in images if not os.path.exists(p)]
    if missing:
        sys.exit("not built yet, missing: " + ", ".join(missing) + f"\nrun: tk.py build --env {env}")
    print("  image: " + os.path.basename(app))
    return images


def factory_image(env):
    found = sorted(glob.glob(os.path.join(build_dir(env), "*.factory.bin")))
    if not found:
        sys.exit(f"no factory image in {build_dir(env)}; run: tk.py build --env {env}")
    return found[-1]


def confirm_latch(env, ask=input):
    if ENVS.get(env, {}).get("latch"):
        print(LATCH_WARNING)
        ask("  Press Enter when the power button is held... ")


def esptool(chip, port, *args):
    return run([sys.executable, ESPTOOL, "--chip", chip, "--port", port, "--baud", "921600", *args])


def flash(env, port, ask=input):
    images = flash_images(env)
    confirm_latch(env, ask)
    args = ["write_flash"]
    for off, path in images:
        args += [hex(off), path]
    return esptool(ENVS[env]["chip"], port, *args)


def install(env, port, ask=input):
    image = factory_image(env)
    print("\n  FULL INSTALL erases the unit: node settings, channel key, fall calibration and the")
    print("  onboard queue are lost.")
    if ask("  Type ERASE to continue: ").strip() != "ERASE":
        print("  cancelled")
        return 1
    confirm_latch(env, ask)
    rc = esptool(ENVS[env]["chip"], port, "erase_flash")
    if rc == 0:
        rc = esptool(ENVS[env]["chip"], port, "write_flash", "0x0", image)
    return rc


def capture_log(port, seconds, out):
    s = open_port(port)
    end = time.time() + seconds
    with open(out, "ab") as f:
        while time.time() < end:
            chunk = s.read(4096)
            if chunk:
                f.write(chunk)
                sys.stdout.write(chunk.decode("utf-8", "replace"))
                sys.stdout.flush()
    s.close()
    print(f"\n  log appended to {out}")


# ---------------------------------------------------------------- menu


MENU = [
    ("ports", "List connected units"),
    ("read", "Read a unit (identity, firmware, battery)"),
    ("build", "Build an image"),
    ("flash", "Flash the application (keeps settings)"),
    ("install", "Full install (erase, fresh image)"),
    ("log", "Capture the serial log"),
    ("quit", "Quit"),
]


def choose(stdscr, title, items):
    curses.curs_set(0)
    sel = 0
    while True:
        stdscr.erase()
        stdscr.addstr(0, 2, title, curses.A_BOLD)
        for i, label in enumerate(items):
            stdscr.addstr(2 + i, 4, label, curses.A_REVERSE if i == sel else curses.A_NORMAL)
        stdscr.addstr(3 + len(items), 2, "Arrows to move, Enter to choose, q to go back")
        key = stdscr.getch()
        if key in (curses.KEY_UP, ord("k")):
            sel = (sel - 1) % len(items)
        elif key in (curses.KEY_DOWN, ord("j")):
            sel = (sel + 1) % len(items)
        elif key in (curses.KEY_ENTER, 10, 13):
            return sel
        elif key in (ord("q"), 27):
            return None


def menu_pick(title, items):
    return curses.wrapper(choose, title, items)


def menu_port():
    ports = list_ports()
    if not ports:
        print("no USB serial port found")
        return None
    i = menu_pick("Which port?", [f"{p['port']}  {p['kind']}" for p in ports])
    return None if i is None else ports[i]["port"]


def menu_env():
    keys = list(ENVS)
    i = menu_pick("Which variant?", [f"{k}  ({ENVS[k]['name']})" for k in keys])
    return None if i is None else keys[i]


def print_unit(info):
    for k in ("port", "node", "long_name", "short_name", "env", "firmware", "battery_percent", "reboots", "error"):
        if k in info:
            print(f"  {k:16} {info[k]}")


def menu_loop():
    while True:
        i = menu_pick("TrekLink unit tool", [label for _, label in MENU])
        if i is None or MENU[i][0] == "quit":
            return 0
        action = MENU[i][0]
        if action == "ports":
            for p in list_ports():
                print(f"  {p['port']:14} {p['vid']:04x}:{p['pid']:04x}  {p['kind']}")
        elif action == "read":
            port = menu_port()
            if port:
                print_unit(read_unit(port))
        elif action == "build":
            env = menu_env()
            if env:
                build(env)
        elif action in ("flash", "install"):
            env = menu_env()
            port = menu_port() if env else None
            if env and port:
                (flash if action == "flash" else install)(env, port)
        elif action == "log":
            port = menu_port()
            if port:
                out = os.path.join(ROOT, "tk-log-%s.log" % time.strftime("%Y%m%d-%H%M%S"))
                capture_log(port, 30, out)
        input("\n  Press Enter for the menu... ")


def main():
    ap = argparse.ArgumentParser(description="TrekLink unit tool")
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("ports")
    p = sub.add_parser("read")
    p.add_argument("--port")
    p.add_argument("--json", action="store_true")
    for name in ("build", "flash", "install"):
        p = sub.add_parser(name)
        p.add_argument("--env", required=True, choices=list(ENVS))
        if name != "build":
            p.add_argument("--port")
        else:
            p.add_argument("--diagnostic", action="store_true", help="USB console and test hooks, bench only")
    p = sub.add_parser("test", help="diagnostic images only: press a button or print the SOS state")
    p.add_argument("command", help='for example "press 4 3500" or "state"')
    p.add_argument("--port")
    p.add_argument("--listen", type=float, default=8.0)
    p.add_argument("--out", help="append the raw serial output to this file")
    p = sub.add_parser("log")
    p.add_argument("--port")
    p.add_argument("--seconds", type=float, default=30)
    p.add_argument("--out", default="tk-serial.log")
    a = ap.parse_args()

    if a.cmd is None:
        if not sys.stdin.isatty():
            ap.print_help()
            return 2
        return menu_loop()
    if a.cmd == "ports":
        for p in list_ports():
            print(f"{p['port']:14} {p['vid']:04x}:{p['pid']:04x}  {p['kind']}")
        return 0
    if a.cmd == "read":
        info = read_unit(pick_port(a.port))
        print(json.dumps(info, indent=2) if a.json else "", end="")
        if not a.json:
            print_unit(info)
        return 0 if "node" in info else 1
    if a.cmd == "build":
        return build(a.env, a.diagnostic)
    if a.cmd == "flash":
        return flash(a.env, pick_port(a.port))
    if a.cmd == "install":
        return install(a.env, pick_port(a.port))
    if a.cmd == "test":
        send_test_command(pick_port(a.port), a.command, a.listen, a.out)
        return 0
    if a.cmd == "log":
        capture_log(pick_port(a.port), a.seconds, a.out)
        return 0
    return 2


if __name__ == "__main__":
    if shutil.which("pio") is None and not os.path.exists(os.path.join(PIO_HOME, "penv", "bin", "pio")):
        print("warning: PlatformIO (pio) is not on PATH; build and flash need it", file=sys.stderr)
    sys.exit(main())
