#!/usr/bin/env python3
"""gaps.py - the parts of rc2.dll's .text that no function in Ghidra's list covers.

Ghidra's analysis misses functions (some only reached through a vtable or a pointer, some
large ones such as ClientCommand), so reference/functions-rc2.tsv doesn't list all the code.
This walks .text up to the C runtime, takes every stretch between listed functions that is more
than padding, and says what each one is:

  stock      it is byte for byte a function of the RC2-layout VC6 reference build (named)
  code       it starts on a 16-byte boundary and decodes as code: a function to reconstruct
  data       anything else: a switch jump table or the tail of the function before it, which
             Ghidra sized too short; check it, but it is usually nobody's work

Writes reference/gaps-rc2.tsv: rva, size, kind, reference name (stock), the function before it.
scripts/check_coverage.py counts the "code" ones as functions to reconstruct.
Usage: scripts/gaps.py
"""
import os, re, sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "vc6"))
import match
from pe import PE

ROOT = os.path.dirname(HERE)
REF = os.path.join(ROOT, "build-vc6", "hl-rc2")


def load(p):
    return [l.split("\t") for l in open(p).read().split("\n")[1:] if l]


def main():
    ref = os.path.join(ROOT, "reference")
    pe = PE(os.path.join(ref, "rc2.dll"))
    _, va, _, _, _ = pe.section(".text")
    fns = sorted((int(r[0], 16), int(r[1]), r[2]) for r in load(os.path.join(ref, "functions-rc2.tsv")))
    files = {int(r[0], 16): r[3] for r in load(os.path.join(ref, "files-rc2.tsv"))}
    crt = min(a for a, f in files.items() if f.startswith("libcmt"))
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    pats = []
    if os.path.exists(os.path.join(REF, "hl.map")):
        rb, _, rt = match.text_section(os.path.join(REF, "hl.dll"))
        funcs = match.map_functions(os.path.join(REF, "hl.map"))
        for i, (off, name, obj) in enumerate(funcs):
            end = funcs[i + 1][0] if i + 1 < len(funcs) else len(rt)
            p = match.pattern(rt[off:end], rb)
            if p:
                pats.append((re.compile(p, re.S), name))
    _, tva, tt = match.text_section(os.path.join(ref, "rc2.dll"))

    rows, cur, prev = [], va, ""
    for a, size, name in fns:
        if a > cur and cur < crt:
            g = pe.read(cur, a - cur)
            k = 0
            while k < len(g) and g[k] in (0xcc, 0x90):
                k += 1
            st, body = cur + k, g[k:]
            if len(body.rstrip(b"\xcc\x90")) >= 16:
                hit = next((n for p, n in pats if p.match(tt, st - tva)), None)
                if hit:
                    kind = "stock"
                else:
                    ins = list(md.disasm(body[:48], pe.base + st))
                    kind = "code" if st % 16 == 0 and len(ins) >= 3 and sum(i.size for i in ins[:5]) >= 8 else "data"
                rows.append((st, a - st, kind, hit or "", prev))
        if a + size > cur:
            cur, prev = a + size, name

    out = os.path.join(ref, "gaps-rc2.tsv")
    with open(out, "w") as f:
        f.write("rva\tsize\tkind\treference\tafter\n")
        for r in rows:
            f.write("0x%x\t%d\t%s\t%s\t%s\n" % r)
    for kind in ("stock", "code", "data"):
        sel = [r for r in rows if r[2] == kind]
        print("%-5s %4d stretches, %6d bytes" % (kind, len(sel), sum(r[1] for r in sel)))
    print("-> %s" % os.path.relpath(out, ROOT))


if __name__ == "__main__":
    main()
