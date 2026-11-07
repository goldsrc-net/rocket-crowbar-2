#!/usr/bin/env python3
"""dropstub.py <file> <symbol> [...]: remove a TEMP STUB definition of each symbol from file.

A stub is the definition (a function with its braces, or a one-line variable) whose line
contains the symbol followed by '(' or ' =' or ';', together with the comment lines right above
it that mention TEMP STUB. Leaves a one-line note saying where the real one is."""
import re, sys
p, syms = sys.argv[1], sys.argv[2:]
lines = open(p).read().split("\n")
for sym in syms:
    idx = None
    for i, l in enumerate(lines):
        if re.match(r"\s*(?:static\s+|inline\s+|const\s+|unsigned\s+)*[A-Za-z_][\w:<>]*[\s*&]+(?:[A-Za-z_]\w*::)?%s\s*(\(|=|;|\[)" % re.escape(sym.split("::")[-1]), l) and not l.lstrip().startswith(("//", "extern", "return", "if", "#")) and not re.search(r"\b%s\s*\([^)]*\)\s*;\s*(//.*)?$" % re.escape(sym), l):
            idx = i; break
    if idx is None:
        print("%s: %s not found" % (p, sym)); continue
    end = idx
    if "{" in lines[idx] or (idx + 1 < len(lines) and lines[idx + 1].strip() == "{"):
        depth = 0; started = False
        for j in range(idx, len(lines)):
            depth += lines[j].count("{") - lines[j].count("}")
            if "{" in lines[j]: started = True
            if started and depth <= 0:
                end = j; break
    start = idx
    while start > 0 and lines[start - 1].lstrip().startswith("//") and "TEMP STUB" in "".join(lines[max(0,start-4):idx]):
        start -= 1
        if "TEMP STUB" in lines[start]: break
    print("%s: dropping %s (lines %d-%d)" % (p, sym, start + 1, end + 1))
    lines[start:end + 1] = ["// %s: the real one is reconstructed elsewhere now" % sym]
open(p, "w").write("\n".join(lines))
