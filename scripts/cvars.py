#!/usr/bin/env python3
"""cvars.py - every cvar_t an RC2 library defines: name, default string, flags.

A cvar_t in .data is { char *name; char *string; int flags; float value; cvar_t *next; },
20 bytes, with the two pointers pointing at strings (VC6 keeps literals in .data) and
value/next zero until the engine registers it. Records are listed in .data order, which is the order they are defined
in the source, and marked "stock" when SDK 2.3's dlls/game.cpp defines the same name.

Writes reference/cvars-<lib>.tsv: rva, name, default, flags, stock (SDK 2.3 default or empty).
Usage: scripts/cvars.py reference/rc2.dll
"""
import os, re, struct, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAME = re.compile(r"[A-Za-z_][A-Za-z0-9_]{1,63}")


def sdk_cvars():
    """{name: default} from SDK 2.3's game.cpp (goldsrc-net/hlsdk c026655)."""
    sdk = os.path.join(ROOT, "..", "hlds64-buildchain", "hlsdk")
    try:
        src = subprocess.run(["git", "-C", sdk, "show", "c026655:singleplayer/dlls/game.cpp"],
                             capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return {}
    return dict(re.findall(r'cvar_t\s+\w+\s*=\s*\{\s*"([^"]+)"\s*,\s*"([^"]*)"', src))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    pe = PE(sys.argv[1])
    lib = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    _, va, size, ro, rs = pe.section(".data")
    stock = sdk_cvars()
    rows = []
    for rva in range(va, va + min(size, rs) - 20, 4):
        name_p, str_p, flags, value, nxt = struct.unpack("<IIIII", pe.read(rva, 20))
        if value or nxt or flags > 0x3ffff:
            continue
        # VC6 puts string literals in .data, not .rdata
        if not all(pe.in_section(p - pe.base, ".rdata") or pe.in_section(p - pe.base, ".data")
                   for p in (name_p, str_p)):
            continue
        name = pe.cstr(name_p - pe.base)
        default = pe.cstr(str_p - pe.base)
        if not NAME.fullmatch(name) or len(default) > 64:
            continue
        rows.append((rva, name, default, flags, stock.get(name)))
    out = os.path.join(ROOT, "reference", "cvars-%s.tsv" % lib)
    with open(out, "w") as f:
        f.write("rva\tname\tdefault\tflags\tstock\n")
        for rva, name, default, flags, sd in rows:
            f.write("0x%x\t%s\t%s\t%d\t%s\n" % (rva, name, default, flags, "" if sd is None else sd))
    new = [r for r in rows if r[4] is None]
    changed = [r for r in rows if r[4] is not None and r[4] != r[2]]
    print("%s: %d cvars, %d not in SDK 2.3, %d with a changed default, %d SDK 2.3 cvars absent -> %s" % (
        lib, len(rows), len(new), len(changed), len(set(stock) - {r[1] for r in rows}),
        os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
