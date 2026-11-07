#!/usr/bin/env python3
"""stock_drift.py <base commit> [names-tsv] [--diff]: functions the original has byte for byte as SDK 2.3
(reference/names-rc2.tsv) whose body in our tree differs from <base> (the halflife-updated
commit we forked from). A stock function should be upstream's code; a difference there is either
a halflife-updated fix kept on purpose (none expected since the base is upstream) or an edit
made under a wrong reading that RC2 changed it. Comments and whitespace are ignored.
Prints: rva  file  name  differs|not in base; --diff adds the unified diff of each.
A free function is looked up by its name alone, so a static helper of the same name elsewhere
can be reported instead; check those by hand."""
import difflib, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = [a for a in sys.argv[1:] if a != "--diff"]
show = "--diff" in sys.argv
base = args[0]
tsv = args[1] if len(args) > 1 else os.path.join(ROOT, "reference", "names-rc2.tsv")


def norm(t):
    t = re.sub(r"//[^\n]*", "", t)
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"\s+", " ", t).strip()


def body(text, cls, meth):
    pat = (re.escape(cls) + r"\s*::\s*" if cls else r"(?<![\w:])") + re.escape(meth) + r"\s*\([^;{]*\)\s*(const\s*)?\{"
    for m in re.finditer(pat, text):
        i, depth = m.end() - 1, 0
        for j in range(i, len(text)):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    return text[m.start():j + 1]
    return None


files = subprocess.run(["git", "-C", ROOT, "ls-files", "dlls", "pm_shared", "game_shared", "common"],
                       capture_output=True, text=True).stdout.split()
files = [f for f in files if f.endswith((".cpp", ".h"))]
cur = {f: open(os.path.join(ROOT, f), encoding="latin-1").read() for f in files}
old = {}
for f in files:
    r = subprocess.run(["git", "-C", ROOT, "show", "%s:%s" % (base, f)], capture_output=True)
    old[f] = r.stdout.decode("latin-1") if r.returncode == 0 else None
seen = set()
for line in open(tsv).read().split("\n")[1:]:
    if not line:
        continue
    rva, size, name, dem, obj = line.split("\t")
    if obj.startswith("LIBCMT"):
        continue
    m = re.search(r"(?:(\w+)::)?(~?\w+|operator\S+)\(", dem)
    if not m or (m.group(1), m.group(2)) in seen:
        continue
    cls, meth = m.group(1), m.group(2)
    seen.add((cls, meth))
    for f in files:
        b = body(cur[f], cls, meth)
        if b is None:
            continue
        o = body(old[f], cls, meth) if old[f] else None
        if o is None:
            print("%s\t%s\t%s::%s\tnot in base" % (rva, f, cls, meth))
        elif norm(o) != norm(b):
            print("%s\t%s\t%s::%s\tdiffers" % (rva, f, cls, meth))
            if show:
                for d in difflib.unified_diff(o.split("\n"), b.split("\n"), lineterm="", n=1):
                    if not d.startswith(("---", "+++")):
                        print("    " + d)
        break
