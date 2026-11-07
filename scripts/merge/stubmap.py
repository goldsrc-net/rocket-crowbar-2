#!/usr/bin/env python3
"""stubmap.py <repo>: TEMP STUBs whose address now has a real (orig-tagged) function.
Prints: address  stub-symbol (file)  real-symbol (file)."""
import os, re, sys
root = sys.argv[1]
src = []
for d in ("dlls", "pm_shared", "cl_dll", "game_shared"):
    for dp, _, fs in os.walk(os.path.join(root, d)):
        for f in fs:
            if f.endswith((".cpp", ".h")):
                src.append(os.path.join(dp, f))
real, stubs = {}, []
name_re = re.compile(r"([A-Za-z_][\w:~]*)\s*\(")
def name_of(line):
    line = line.split("//")[0]
    m = None
    for m in name_re.finditer(line): break
    if not m:
        m2 = re.search(r"([A-Za-z_]\w*)\s*(=|;|\[)", line)
        return m2.group(1) if m2 else None
    return m.group(1)
for p in src:
    L = open(p, encoding="latin-1").read().split("\n")
    for i, l in enumerate(L):
        m = re.search(r"orig (0x[0-9a-f]+)", l)
        if m and not l.lstrip().startswith("//"):
            n = name_of(l)
            if n: real.setdefault(int(m.group(1), 16), (n, os.path.relpath(p, root)))
        elif m and l.lstrip().startswith("//") and i + 1 < len(L) and "TEMP STUB" not in l:
            n = name_of(L[i + 1])
            if n and "(" in L[i + 1]: real.setdefault(int(m.group(1), 16), (n, os.path.relpath(p, root)))
        if "TEMP STUB" in l:
            addrs = [int(a, 16) for a in re.findall(r"\b0x([0-9a-f]{4,6})\b", l)]
            for j in range(i + 1, min(i + 4, len(L))):
                if L[j].strip() and not L[j].lstrip().startswith("//"):
                    n = name_of(L[j]); break
            else:
                n = None
            for a in addrs:
                stubs.append((a, n, os.path.relpath(p, root)))
for a, n, p in sorted(stubs, key=lambda x: (x[0], x[1] or "", x[2])):
    r = real.get(a)
    if r and n and r[0].split("::")[-1] != (n or "").split("::")[-1]:
        print("0x%x  stub %-34s (%s)  real %-34s (%s)" % (a, n, p, r[0], r[1]))
    elif r and n:
        print("0x%x  stub %-34s (%s)  real, same name    (%s)" % (a, n, p, r[1]))
