#!/usr/bin/env python3
"""stock_drift.py <base commit>|--sdk <dir> [names-tsv] [--diff]: stock functions whose body differs.

A stock function is one the original has byte for byte as SDK 2.3 (reference/names-rc2.tsv).

<base commit> (the halflife-updated commit we forked from): stock functions our tree changed against
upstream. Each should be a deliberate return to SDK 2.3; anything else is an edit made under a wrong
reading that RC2 changed it.

--sdk <dir> (the pristine SDK 2.3 tree, build-vc6/singleplayer): stock functions whose body differs
from what RC2 actually shipped. That also finds halflife-updated's own changes to them, which the git
mode can't (they are in the base). halflife-updated restyled the SDK, so this mode normalizes
spelling that compiles the same (void parameter lists, TRUE/FALSE, NULL/nullptr, explicit 0
comparisons, BOOL, spacing); what is left still needs reading, and much of it is API change
(engine calls, std::string paths) with the same behavior. --loose also folds halflife-updated's
blanket restyles (SetThink(&C::F), ENT(pev), casts, KeyValue's bool return) and drops
parentheses: for triage, since a changed precedence would hide.

Comments and whitespace are ignored. Prints: rva  file  name  differs|not in base; --diff adds the
unified diff of each. A free function is looked up by its name alone, so a static helper of the
same name elsewhere can be reported instead; check those by hand."""
import difflib, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
argv = sys.argv[1:]
show = "--diff" in argv
loose = "--loose" in argv
argv = [a for a in argv if a not in ("--diff", "--loose")]
sdk = None
base = None
if "--sdk" in argv:
    i = argv.index("--sdk")
    sdk = argv[i + 1]
    del argv[i:i + 2]
else:
    base = argv.pop(0)
tsv = argv[0] if argv else os.path.join(ROOT, "reference", "names-rc2.tsv")


def norm(t):
    t = re.sub(r"//[^\n]*", "", t)
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    t = re.sub(r"\s+", " ", t).strip()
    if sdk is None:
        return t
    t = re.sub(r"\(\s*void\s*\)", "()", t)
    t = re.sub(r"\bTRUE\b", "true", t)
    t = re.sub(r"\bFALSE\b", "false", t)
    t = re.sub(r"\b(NULL|nullptr)\b", "0", t)
    t = re.sub(r"\bBOOL\b", "bool", t)
    t = re.sub(r"\b0 != ", "", t)
    t = re.sub(r" != 0\b", "", t)
    t = re.sub(r"\b0 == ([\w.>\[\]-]+)", r"!\1", t)
    t = re.sub(r"([\w.>\]-]+) == 0\b", r"!\1", t)
    t = re.sub(r"\s*([(){}\[\];,:<>=!+*/&|-])\s*", r"\1", t)
    if not loose:
        return t
    # --loose, for triage: also fold the restyles halflife-updated applied everywhere. It drops
    # every parenthesis, so a changed precedence would hide; read what it reports, not what it drops.
    t = re.sub(r"(Set(Think|Touch|Use|Blocked))\(&\w+::", r"\1(", t)
    t = re.sub(r"\bENT\(pev\)", "edict()", t)
    t = re.sub(r"static_cast<[^<>]+>", "", t)
    t = re.sub(r"\((int|float|bool|char\*|const char\*)\)", "", t)
    t = t.replace("pkvd->fHandled=true;", "return true;")
    t = re.sub(r"\belse(return )?(\w+::KeyValue\(pkvd\);)", r"return \2", t)
    t = re.sub(r"\b(void|int|bool)(?=[A-Za-z_]\w*:)", "", t)
    t = t.replace("(", "").replace(")", "")
    t = re.sub(r"!=0\b|\b0!=", "", t)
    t = re.sub(r"\bfalse\b", "0", t)
    t = re.sub(r"\btrue\b", "1", t)
    return t


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
    if sdk:
        p = os.path.join(sdk, f)
        old[f] = open(p, encoding="latin-1").read() if os.path.exists(p) else None
        continue
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
