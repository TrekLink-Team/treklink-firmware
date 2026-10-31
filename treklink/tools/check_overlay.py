#!/usr/bin/env python3
"""TrekLink overlay check (specs/architecture design §4.3).

Run from the repository root:

    python3 treklink/tools/check_overlay.py            check HEAD against the recorded upstream base
    python3 treklink/tools/check_overlay.py --against <rev>
                                                        before a sync: list the overrides whose stock code
                                                        differs between the base and <rev>
    python3 treklink/tools/check_overlay.py --update   record the current hashes of the watched stock code
    python3 treklink/tools/check_overlay.py --elf .pio/build/treklink-v2/firmware.elf
                                                        after a build: no call site bypasses a --wrap (LTO)

Checks:
  1. Every stock path is identical to the upstream base, except the TrekLink-owned files.
  2. No TREKLINK_V2 / V3 / V4 test outside variants/.
  3. The stock code each override depends on still hashes as recorded in treklink/overrides.json.
  4. Every -Wl,--wrap in a TrekLink env has a __wrap_ definition in treklink/src/ and an overrides.json
     entry naming it.
  With --elf: the linked image calls each wrapped symbol only from its __wrap_ function. The envs build
  with -flto, and a call the compiler resolved inside one LTO unit would bypass --wrap silently.

Standard library only; needs git on PATH. Exit code 0 when every check passes.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
UPSTREAM_FILE = os.path.join(ROOT, "treklink", "UPSTREAM")
OVERRIDES_FILE = os.path.join(ROOT, "treklink", "overrides.json")

# Paths TrekLink owns (design §2.1 and §2.3). Anything else must match the upstream base.
OWNED = [
    re.compile(r"^treklink/"),
    re.compile(r"^variants/[^/]+/treklink_[^/]+/"),
    re.compile(r"^test/treklink_[^/]+/"),
    re.compile(r"^docs/"),
    re.compile(r"^specs/"),
    re.compile(r"^\.github/workflows/(checksum|overlay)\.yml$"),
    re.compile(r"^(AGENTS|CLAUDE|README)\.md$"),
]
VERSION_FLAG = re.compile(r"\bTREKLINK_V[0-9]\b")


def git(*args):
    return subprocess.run(["git", "-C", ROOT, *args], check=True, capture_output=True, text=True).stdout


def upstream_base():
    with open(UPSTREAM_FILE) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                return line.split()[0]
    sys.exit("treklink/UPSTREAM names no commit")


def owned(path):
    return any(p.search(path) for p in OWNED)


def check_stock_paths(base):
    # --no-renames: a stock file moved into treklink/ must show its stock path as deleted.
    changed = [p for p in git("diff", "--no-renames", "--name-only", base, "HEAD").splitlines() if p and not owned(p)]
    for p in changed:
        print(f"FAIL stock path differs from upstream {base[:9]}: {p}")
    return not changed


def check_version_flags():
    ok = True
    for path in git("ls-files", "treklink", "test").splitlines():
        if path.startswith("test/") and not path.startswith("test/treklink_"):
            continue
        if path.startswith("treklink/tools/"):
            continue
        with open(os.path.join(ROOT, path), errors="replace") as f:
            for n, line in enumerate(f, 1):
                if VERSION_FLAG.search(line):
                    print(f"FAIL version flag outside variants/: {path}:{n}: {line.strip()}")
                    ok = False
    return ok


def function_body(text, signature, occurrence=1):
    """The text from the `occurrence`-th line holding `signature` to its matching closing brace."""
    start = -1
    for _ in range(occurrence):
        start = text.find(signature, start + 1)
        if start < 0:
            return None
    brace = text.find("{", start)
    if brace < 0:
        return None
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start : i + 1]
    return None


def watched_hash(rev, entry):
    try:
        text = git("show", f"{rev}:{entry['file']}")
    except subprocess.CalledProcessError:
        return "missing-file"
    body = function_body(text, entry["signature"], entry.get("occurrence", 1))
    if body is None:
        return "missing-signature"
    return hashlib.sha256(body.encode()).hexdigest()


def load_overrides():
    with open(OVERRIDES_FILE) as f:
        return json.load(f)


def check_overrides(rev):
    ok = True
    for entry in load_overrides():
        if "signature" not in entry:
            continue
        h = watched_hash(rev, entry)
        if h != entry.get("sha256"):
            print(f"FAIL {entry['id']}: {entry['file']} `{entry['signature']}` changed at {rev[:9]} ({h[:12]}); "
                  f"review {entry.get('mirror', 'the override')} against it, then run --update")
            ok = False
    return ok


WRAP = re.compile(r"-Wl,--wrap=(\S+)")


def wrapped_symbols():
    """{symbol: [ini paths]} for every --wrap in a TrekLink env."""
    found = {}
    for path in git("ls-files", "variants").splitlines():
        if re.search(r"/treklink_[^/]+/platformio\.ini$", path):
            with open(os.path.join(ROOT, path)) as f:
                for m in WRAP.finditer(f.read()):
                    found.setdefault(m.group(1), []).append(path)
    return found


def check_wraps():
    ok = True
    listed = {e["wrap"] for e in load_overrides() if "wrap" in e}
    sources = "".join(
        open(os.path.join(ROOT, p), errors="replace").read()
        for p in git("ls-files", "treklink/src").splitlines()
        if p.endswith((".cpp", ".c", ".h"))
    )
    for sym, inis in sorted(wrapped_symbols().items()):
        if "__wrap_" + sym not in sources:
            print(f"FAIL --wrap={sym} ({', '.join(inis)}) has no __wrap_{sym} in treklink/src/")
            ok = False
        if sym not in listed:
            print(f"FAIL --wrap={sym} ({', '.join(inis)}) is not named by an overrides.json entry")
            ok = False
    return ok


def find_objdump(elf):
    with open(elf, "rb") as f:
        head = f.read(20)
    pio = os.path.expanduser(os.environ.get("PLATFORMIO_CORE_DIR", "~/.platformio"))
    # e_machine 94 is Xtensa; both ESP32 toolchains disassemble either core.
    for tool in ("toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-objdump", "toolchain-xtensa-esp32/bin/xtensa-esp32-elf-objdump"):
        path = os.path.join(pio, "packages", tool)
        if head[18] == 94 and os.path.exists(path):
            return path
    sys.exit("no Xtensa objdump found under " + os.path.join(pio, "packages"))


def check_elf(elf):
    syms = set(wrapped_symbols())
    label = re.compile(r"^[0-9a-f]+ <(\S+)>:$")
    call = re.compile(r"\bcall(?:x)?\d*\s+[0-9a-f]+ <(\S+?)(?:\+0x[0-9a-f]+)?>")
    current, wrapped_calls, bad = "", {s: 0 for s in syms}, []
    proc = subprocess.Popen([find_objdump(elf), "-d", "--no-show-raw-insn", elf], stdout=subprocess.PIPE, text=True)
    for line in proc.stdout:
        m = label.match(line)
        if m:
            current = m.group(1)
            continue
        m = call.search(line)
        if not m:
            continue
        target = m.group(1)
        if target.startswith("__wrap_") and target[len("__wrap_") :] in syms:
            wrapped_calls[target[len("__wrap_") :]] += 1
        elif target in syms and current != "__wrap_" + target:
            bad.append(f"{current} calls {target} directly")
    proc.wait()
    for b in bad:
        print("FAIL " + b)
    for sym, n in sorted(wrapped_calls.items()):
        print(f"{'ok  ' if n else 'note'} {n} call(s) to __wrap_{sym}")
    print("elf check: " + ("pass" if not bad else "FAIL"))
    return not bad


def update(base):
    data = load_overrides()
    for entry in data:
        if "signature" in entry:
            entry["sha256"] = watched_hash(base, entry)
    with open(OVERRIDES_FILE, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    print(f"recorded {sum('signature' in e for e in data)} hashes at {base[:9]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--against", metavar="REV", help="compare the watched stock code at REV instead of the base")
    ap.add_argument("--update", action="store_true", help="record hashes at the base")
    ap.add_argument("--elf", metavar="PATH", help="check a linked image for calls that bypass a --wrap")
    args = ap.parse_args()
    if args.elf:
        return 0 if check_elf(args.elf) else 1
    base = upstream_base()
    if args.update:
        update(base)
        return 0
    if args.against:
        return 0 if check_overrides(args.against) else 1
    results = [check_stock_paths(base), check_version_flags(), check_overrides(base), check_wraps()]
    print("overlay check: " + ("pass" if all(results) else "FAIL"))
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
