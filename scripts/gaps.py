#!/usr/bin/env python3
"""gaps.py - the parts of rc2.dll's .text that no function in Ghidra's list covers.

Ghidra's analysis misses functions (some only reached through a vtable or a pointer, some
large ones such as ClientCommand), so reference/functions-rc2.tsv doesn't list all the code.
This walks .text up to the C runtime, takes every stretch between listed functions that is more
than padding, and splits it into pieces. A piece ends where padding (int3, nop) follows a ret
or jmp, or runs up to a 16-byte boundary; a switch's jump table (aligned addresses into the
code before it, after VC6's 2- or 3-byte filler, with its byte index table) is a piece of its
own. Each piece is one of:

  stock      byte for byte a function of the RC2-layout VC6 reference build (named)
  code       starts on a 16-byte boundary: a function to reconstruct
  jumptable  a switch's jump table, belonging to the function it points into
  thunk      the linker's import jumps (jmp [import address]): nobody's source
  tail       starts right where the function before it ends: Ghidra sized that one too short
  data       anything else; check it

Writes reference/gaps-rc2.tsv: rva, size, kind, reference name (stock), the listed function
before it. scripts/check_coverage.py counts the "code" ones as functions to reconstruct.
Usage: scripts/gaps.py
"""
import os, re, struct, sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "vc6"))
import match
from pe import PE

ROOT = os.path.dirname(HERE)
REF = os.path.join(ROOT, "build-vc6", "hl-rc2")
PAD = (0xcc, 0x90)
FILL = (b"\x8d\x49\x00", b"\x8b\xff")  # VC6's alignment filler before a jump table


def load(p):
    return [l.split("\t") for l in open(p).read().split("\n")[1:] if l]


def jumptable(b, j, base, lo, hi):
    """Length of the jump table at b[j:] (with its filler and byte index table), or 0."""
    k = j
    for f in FILL:
        if b.startswith(f, k):
            k += len(f)
            break
    if (base + k) % 4:
        return 0
    n = 0
    while k + 4 <= len(b) and lo <= struct.unpack_from("<I", b, k)[0] < hi:
        n, k = n + 1, k + 4
    if not n:
        return 0
    while k < len(b) and b[k] < n:
        k += 1
    return k - j


def is_thunk(code, va, md):
    ins = list(md.disasm(code, va))
    return bool(ins) and sum(i.size for i in ins) == len(code) and all(
        i.mnemonic == "jmp" and i.op_str.startswith("dword ptr [0x") for i in ins)


def pieces(b, base, lo, hi, md):
    """Split the stretch b (at rva base) into (offset, size, is_jumptable)."""
    out, j = [], 0
    while j < len(b):
        while j < len(b) and b[j] in PAD:
            j += 1
        if j >= len(b):
            break
        n = jumptable(b, j, base, lo, hi)
        if n:
            out.append((j, n, True))
            j += n
            continue
        k = j
        while k < len(b):
            ins = next(md.disasm(b[k:k + 16], base + k, 1), None)
            if ins is None:  # not code: up to the next padding
                while k < len(b) and b[k] not in PAD:
                    k += 1
                break
            k += ins.size
            if k >= len(b) or jumptable(b, k, base, lo, hi):
                break
            if b[k] in PAD:
                m = k
                while m < len(b) and b[m] in PAD:
                    m += 1
                if ins.mnemonic in ("ret", "jmp", "int3") or m >= len(b) or (base + m) % 16 == 0:
                    break
                k = m
        out.append((j, k - j, False))
        j = k
    return out


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
    else:
        sys.exit("no %s: build the RC2-layout reference first (scripts/vc6/README.md)" % REF)
    _, tva, tt = match.text_section(os.path.join(ref, "rc2.dll"))

    rows, cur, prev, prevstart = [], va, "", va
    for a, size, name in fns:
        if a > cur and cur < crt:
            g = pe.read(cur, a - cur)
            # jump tables point back into the function before the stretch or into the stretch
            for off, n, jt in pieces(g, cur, pe.base + prevstart, pe.base + a, md):
                st = cur + off
                if jt:
                    kind, hit = "jumptable", ""
                elif is_thunk(g[off:off + n], pe.base + st, md):
                    kind, hit = "thunk", ""
                else:
                    hit = next((nm for p, nm in pats if p.match(tt, st - tva)), None)
                    kind = "stock" if hit else "code" if st % 16 == 0 else "tail" if off == 0 else "data"
                rows.append((st, n, kind, hit or "", prev))
        if a + size > cur:
            cur, prev, prevstart = a + size, name, a

    out = os.path.join(ref, "gaps-rc2.tsv")
    with open(out, "w") as f:
        f.write("rva\tsize\tkind\treference\tafter\n")
        for r in rows:
            f.write("0x%x\t%d\t%s\t%s\t%s\n" % r)
    for kind in ("stock", "code", "jumptable", "thunk", "tail", "data"):
        sel = [r for r in rows if r[2] == kind]
        print("%-9s %4d pieces, %6d bytes" % (kind, len(sel), sum(r[1] for r in sel)))
    print("-> %s" % os.path.relpath(out, ROOT))


if __name__ == "__main__":
    main()
