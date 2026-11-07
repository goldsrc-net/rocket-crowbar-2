#!/usr/bin/env python3
"""dedupe.py <repo> <build log>: for each symbol the log says is defined twice (compile
"redefinition of" or link "multiple definition of"), drop the definition that is a TEMP STUB.
Prints what it drops and what it can't decide."""
import os, re, subprocess, sys
root, log = sys.argv[1], open(sys.argv[2]).read()
syms = set(re.findall(r"redefinition of '([^'(]+)", log)) | set(m.split("(")[0] for m in re.findall(r"multiple definition of `([^']+)'", log))
syms = {s.strip() for s in syms}
def defs(sym):
    short = sym.split("::")[-1]
    out = subprocess.run(["git", "-C", root, "grep", "-n", "-E", r"^[A-Za-z_][^;/(]*[\s*&]((\w+::)?%s)\s*(\(|=|;|\[)" % re.escape(short), "--", "dlls", "cl_dll", "pm_shared"], capture_output=True, text=True).stdout
    res = []
    for line in out.split("\n"):
        if not line: continue
        f, n, txt = line.split(":", 2)
        if txt.lstrip().startswith(("extern", "//", "return")) or re.search(r"\)\s*;\s*(//.*)?$", txt) and "=" not in txt.split("(")[0]:
            continue
        if "::" in sym and sym.split("::")[0] + "::" not in txt: continue
        res.append((f, int(n), txt))
    return res
def is_stub(f, n):
    L = open(os.path.join(root, f), encoding="latin-1").read().split("\n")
    ctx = "\n".join(L[max(0, n - 6):n])
    return "TEMP STUB" in ctx or "stub" in os.path.basename(f).lower()
for sym in sorted(syms):
    d = defs(sym)
    if len(d) < 2:
        print("?? %s: %d definitions found %s" % (sym, len(d), [(f, n) for f, n, _ in d])); continue
    stubs = [x for x in d if is_stub(x[0], x[1])]
    real = [x for x in d if x not in stubs]
    drop = stubs if real else stubs[1:]
    if not drop:
        print("!! %s: no stub among %s" % (sym, [(f, n) for f, n, _ in d])); continue
    for f, n, _ in drop:
        print("drop %s %s:%d" % (sym, f, n))
