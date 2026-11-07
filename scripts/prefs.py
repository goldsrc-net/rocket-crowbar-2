#!/usr/bin/env python3
"""prefs.py - every rc2.ini key rc2.dll knows: its slot, type, heading, key and default.

RC2 describes rc2.ini (cvar rccfgfile) with a static table in .data, one 168-byte record
per key: { int type; char heading[64]; char key[64]; char default[32]; int flags; }. The
values live in a parallel array of 8-byte slots, record i at VALUES + 8*i, which is what
the game code reads (fld dword ptr [slot]). The table's address and bounds are where the
rc2.ini writer walks them (FUN_100aec40: records from the "Attributes"/"TotalPoints" one
to 0x1017bfd4, values from 0x10191bc8 to 0x10193e90).

Types, from the value comparer (FUN_100b1180): 0 and 1 float, 2 double, 3 int, 4 and 5
short, 6 char, 7 bool.

Writes reference/prefs-rc2.tsv: index, value rva, type, heading, key, default, flags.
Usage: scripts/prefs.py
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RECORDS = 0x1014e568   # first record (its type word; the heading follows)
RECORDS_END = 0x1017bfd4 - 4
VALUES = 0x10191bc8
VALUES_END = 0x10193e90
SIZE = 0xa8
TYPES = {0: "float", 1: "float", 2: "double", 3: "int", 4: "short", 5: "short", 6: "char", 7: "bool"}


def table(pe):
    n = (VALUES_END - VALUES) // 8
    out = []
    for i in range(n):
        b = pe.read(RECORDS - pe.base + i * SIZE, SIZE)
        t = struct.unpack_from("<i", b, 0)[0]
        cs = lambda x: x.split(b"\0")[0].decode("latin-1")
        heading, key, default = cs(b[4:68]), cs(b[68:132]), cs(b[132:164])
        flags = struct.unpack_from("<I", b, 164)[0]
        out.append((i, VALUES - pe.base + 8 * i, TYPES.get(t, str(t)), heading, key, default, flags))
    return out


def main():
    pe = PE(os.path.join(ROOT, "reference", "rc2.dll"))
    rows = table(pe)
    assert (RECORDS_END - RECORDS) // SIZE + 1 >= len(rows), "record and value arrays disagree"
    out = os.path.join(ROOT, "reference", "prefs-rc2.tsv")
    with open(out, "w") as f:
        f.write("index\tvalue\ttype\theading\tkey\tdefault\tflags\n")
        for i, rva, t, h, k, d, fl in rows:
            f.write("%d\t0x%x\t%s\t%s\t%s\t%s\t0x%x\n" % (i, rva, t, h, k, d, fl))
    print("%d rc2.ini keys in %d headings -> %s" % (len(rows), len({r[3] for r in rows}),
                                                   os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
