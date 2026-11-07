#!/usr/bin/env python3
"""stub_addrs.py <repo> [dir ...]: for each TEMP STUB block, the original addresses its comment
names and every other line in the tree that names the same address (outside TEMP STUB blocks).

Packages reconstructed in parallel name the same original function or global differently; the
stub one package wrote and the real one another package wrote then both link, and the stub's
callers never reach the real code. The address in the stub's comment is what ties them. An
address with no other mention is still nobody's (or Ghidra missed it and the package that owns
it didn't tag it).

Addresses: "0x" plus 4 to 8 hex digits; a VA (0x10xxxxxx) and its RVA count as the same.
Default dirs: cl_dll dlls pm_shared game_shared common."""
import os, re, sys

root = sys.argv[1]
dirs = sys.argv[2:] or ["cl_dll", "dlls", "pm_shared", "game_shared", "common"]
ADDR = re.compile(r"0x([0-9a-fA-F]{4,8})\b")


def norm(h):
    a = int(h, 16)
    return a - 0x10000000 if a >= 0x10000000 else a


files = []
for d in dirs:
    for dp, _, fs in os.walk(os.path.join(root, d)):
        files += [os.path.join(dp, f) for f in fs if f.endswith((".cpp", ".h", ".c"))]

blocks, mentions = [], {}
for p in files:
    lines = open(p, encoding="latin-1").read().split("\n")
    in_stub = set()
    i = 0
    while i < len(lines):
        if "TEMP STUB" in lines[i] and lines[i].lstrip().startswith(("//", "*")):
            j = i
            comment = []
            while j < len(lines) and lines[j].lstrip().startswith(("//", "*")):
                comment.append(lines[j])
                in_stub.add(j)
                j += 1
            defs = []
            while j < len(lines) and lines[j].strip() and not ("TEMP STUB" in lines[j]):
                in_stub.add(j)
                if not lines[j].startswith(("\t", " ", "{", "}")):
                    defs.append(lines[j].strip()[:110])
                j += 1
            addrs = sorted({norm(m.group(1)) for c in comment for m in ADDR.finditer(c)})
            blocks.append((os.path.relpath(p, root), i + 1, addrs, defs))
            i = j
            continue
        i += 1
    for n, l in enumerate(lines):
        if n in in_stub:
            continue
        for m in ADDR.finditer(l):
            mentions.setdefault(norm(m.group(1)), []).append("%s:%d: %s" % (os.path.relpath(p, root), n + 1, l.strip()[:120]))

for f, n, addrs, defs in blocks:
    print("== %s:%d" % (f, n))
    for d in defs:
        print("   def  " + d)
    for a in addrs:
        ms = mentions.get(a, [])
        print("   0x%x: %s" % (a, "nobody else" if not ms else ""))
        for m in ms[:4]:
            print("        " + m)
