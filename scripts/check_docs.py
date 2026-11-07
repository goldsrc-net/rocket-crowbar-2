#!/usr/bin/env python3
"""check_docs.py - every cvar and command the server registers is in docs/SERVER.md.

Collects the cvar names defined in dlls/game.cpp and game_shared/, the client commands
matched against pcmd anywhere in dlls/ and dlls/rc2/, the admin commands matched against
pszCmd (dlls/rc2/maintainent.cpp) and the rccfg ones against pszCommand (dlls/rc2prefs.cpp),
and the server commands added with pfnAddServerCommand, and fails if any of them does not
appear in docs/SERVER.md as a word.

Usage: scripts/check_docs.py
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "docs", "SERVER.md")

CVAR = re.compile(r"cvar_t\s+\w+\s*=\s*\{\s*\"([^\"]+)\"")
# RC2's ClientCommand compares with strcmp, halflife-updated's with FStrEq
CMD = re.compile(r"(?:FStrEq|strcmp|stricmp)\(\s*(?:pcmd|pszCmd|pszCommand)\s*,\s*\"([^\"]+)\"")
SRVCMD = re.compile(r"pfnAddServerCommand\(\s*\"([^\"]+)\"")
# voice_gamemgr.cpp matches its commands with stricmp
VOICE = re.compile(r"stricmp\(\s*cmd\s*,\s*\"([^\"]+)\"")


def read(*parts):
    return open(os.path.join(ROOT, *parts), encoding="latin-1").read()


def sources(folder):
    for name in sorted(os.listdir(os.path.join(ROOT, folder))):
        if name.endswith((".cpp", ".h")):
            yield name, read(folder, name)


def main():
    doc = read("docs", "SERVER.md")
    want = {}
    for folder in ("dlls", os.path.join("dlls", "rc2"), "game_shared"):
        for name, src in sources(folder):
            if name == "game.cpp" or folder == "game_shared":
                for n in CVAR.findall(src):
                    want.setdefault(n, "cvar (%s/%s)" % (folder, name))
            for rx, kind in ((CMD, "client command"), (SRVCMD, "server command"), (VOICE, "client command")):
                for n in rx.findall(src):
                    want.setdefault(n, "%s (%s/%s)" % (kind, folder, name))

    def documented(n):
        if re.search(r"(?<![\w-])" + re.escape(n) + r"(?![\w-])", doc, re.IGNORECASE):
            return True
        # sk_foo1..3 written as sk_foo1-3, and the sk_* family covered by the stock list
        m = re.fullmatch(r"(sk_\w+?)([123])", n)
        return bool(m) and (m.group(1) + "1-3" in doc or "`sk_*`" in doc)

    missing = sorted((k, v) for k, v in want.items() if not documented(k))
    for n, where in missing:
        print("not in docs/SERVER.md: %s, %s" % (n, where))
    print("%d cvars and commands, %d missing" % (len(want), len(missing)))
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
