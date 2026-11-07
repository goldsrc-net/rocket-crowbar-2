#!/usr/bin/env python3
"""check_client_engfuncs.py: engine calls the client's copy of the weapon code makes that the
client never fills in.

The weapon files in dlls/ are compiled into client.so too (prediction), where the server engine
table g_engfuncs is ours: HUD_InitClientWeapons (cl_dll/hl/hl_weapons.cpp) fills the entries the
client copy needs with stubs or the client engine's functions, and every other entry is NULL. A
call through one crashes the client the moment it runs (ENTINDEX in the crowbar's Deploy did,
2026-11-07). This lists, for the client-compiled part of each weapon file (code outside #ifndef
CLIENT_DLL), every engine macro or inline wrapper (enginecallback.h, util.h) or direct g_engfuncs call whose entry
HUD_InitClientWeapons doesn't set. Exit 1 when there is any.

STRING() and ENT()-style macros go through pfnSzFromIndex / pfnPEntityOfEntOffset only in a
64-bit build; the client is 32-bit, so those two aren't reported.
Usage: scripts/check_client_engfuncs.py
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = ("crossbow crowbar egon gauss glock handgrenade hornetgun mp5 python rpg satchel shotgun "
         "squeakgrenade tripmine weapons_shared").split()
SIXTY_FOUR_BIT_ONLY = {"pfnSzFromIndex", "pfnPEntityOfEntOffset"}


def read(p):
    return open(os.path.join(ROOT, p), encoding="latin-1").read()


def client_part(text):
    """The lines a CLIENT_DLL build compiles, by #ifdef/#ifndef CLIENT_DLL (others pass through)."""
    keep, stack = [], []
    for line in text.split("\n"):
        s = line.strip()
        if re.match(r"#\s*if(n?def)?\b", s):
            if re.match(r"#\s*ifndef\s+CLIENT_DLL\b", s) or re.match(r"#\s*if\s+!\s*defined\s*\(?\s*CLIENT_DLL", s):
                stack.append("off")
            elif re.match(r"#\s*ifdef\s+CLIENT_DLL\b", s) or re.match(r"#\s*if\s+defined\s*\(?\s*CLIENT_DLL", s):
                stack.append("on")
            else:
                stack.append("x")
            continue
        if re.match(r"#\s*(else|elif)\b", s) and stack:
            stack[-1] = {"off": "on", "on": "off", "x": "x"}[stack[-1]]
            continue
        if re.match(r"#\s*endif\b", s) and stack:
            stack.pop()
            continue
        if "off" not in stack:
            keep.append(line)
    return "\n".join(keep)


def main():
    eng = read("dlls/enginecallback.h") + read("dlls/util.h")
    macros = {}
    for m in re.finditer(r"#define\s+(\w+)\s*\(?\*?g_engfuncs\.(pfn\w+)\)?", eng):
        macros[m.group(1)] = m.group(2)
    # and inline wrappers (util.h's ENTINDEX, INDEXENT): the first g_engfuncs entry in the body
    for m in re.finditer(r"inline[^\n(;]*?\b(\w+)\s*\([^)]*\)\s*\{([^}]*)\}", eng):
        e = re.search(r"g_engfuncs\.(pfn\w+)", m.group(2))
        if e:
            macros.setdefault(m.group(1), e.group(1))
    filled = set(re.findall(r"g_engfuncs\.(pfn\w+)\s*=", read("cl_dll/hl/hl_weapons.cpp")))
    missing = {}
    for f in FILES:
        body = client_part(read("dlls/%s.cpp" % f))
        body = re.sub(r"//[^\n]*", "", body)
        for name, pfn in macros.items():
            if re.search(r"\b%s\s*\(" % name, body):
                missing.setdefault(pfn, set()).add(f)
        for m in re.finditer(r"g_engfuncs\.(pfn\w+)\s*\(", body):
            missing.setdefault(m.group(1), set()).add(f)
    bad = {k: v for k, v in missing.items() if k not in filled and k not in SIXTY_FOUR_BIT_ONLY}
    for pfn in sorted(bad):
        print("not filled on the client: %s, called in %s" % (pfn, ", ".join(sorted(bad[pfn]))))
    if not bad:
        print("every engine call in the client's weapon code has an entry")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
