#!/usr/bin/env python3
"""check_clientdata.py - every clientdata field halflife-updated's UpdateClientData fills, ours fills.

Players run Valve's client, which predicts their movement from the clientdata the server sends:
a field we stop filling (cd->maxspeed, lost to an edit on 2026-11-07) makes HL25 predict with the
wrong value, and the player jitters ("pred. error" with cl_showerror 1). Compares the `cd->field`
assignments in dlls/client.cpp's UpdateClientData with upstream's (`git show upstream/64bit-demo`).
Exit 1 if any upstream field is never assigned here.

Usage: scripts/check_clientdata.py
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def fields(src):
    m = re.search(r"void UpdateClientData\s*\(const edict_t\s*\*\s*ent.*?\n\}", src, re.S)
    if not m:
        sys.exit("UpdateClientData not found")
    body = re.sub(r"//[^\n]*", "", m.group(0))
    return set(re.findall(r"\bcd->(\w+)(?:\.\w+|\[\w+\])?\s*=[^=]", body))


def main():
    ours = fields(open(os.path.join(ROOT, "dlls", "client.cpp"), encoding="latin-1").read())
    up = fields(subprocess.run(["git", "-C", ROOT, "show", "upstream/64bit-demo:dlls/client.cpp"],
                               capture_output=True, text=True, check=True).stdout)
    missing = sorted(up - ours)
    for f in missing:
        print("UpdateClientData never sets cd->%s (upstream does)" % f)
    print("%d clientdata fields, %d missing" % (len(up), len(missing)))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
