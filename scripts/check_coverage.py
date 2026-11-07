#!/usr/bin/env python3
"""check_coverage.py - which of rc2.dll's (or client.dll's) functions our source reconstructs.

Every function the VC6 SDK 2.3 build doesn't match (reference/names-rc2*.tsv) is RC2's to
reconstruct, and a reconstructed one carries its address in a comment, "// orig 0x<rva>"
(the RVA as in reference/files-rc2.tsv, e.g. orig 0xaea60; a VA such as 0x100aea60 counts
the same). This lists, per source file of the original (reference/files-rc2.tsv), how many
there are and how many have a counterpart in dlls/, pm_shared/, game_shared/, common/ and public/.

client.dll (--lib client) works the same from reference/*-client.tsv, with its own tag,
"// orig client 0x<rva>", since files compiled into both libraries (the weapons, pm_shared)
would otherwise be ambiguous; it scans cl_dll/ as well.

Usage:
  scripts/check_coverage.py [--lib client]                    summary per file, worst first
  scripts/check_coverage.py [--lib client] <file> --missing   the missing ones in files whose name contains <file>
"""
import collections, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 0x10000000


def load(p):
    return [l.split("\t") for l in open(p).read().split("\n")[1:] if l]


def main():
    argv = sys.argv[1:]
    lib = "rc2"
    if "--lib" in argv:
        i = argv.index("--lib")
        lib = argv[i + 1]
        del argv[i:i + 2]
    args = [a for a in argv if not a.startswith("--")]
    missing_only = "--missing" in argv
    tag = re.compile(r"orig client (0x[0-9a-fA-F]+)" if lib == "client" else r"orig (0x[0-9a-fA-F]+)")
    dirs = ("dlls", "pm_shared", "game_shared", "common", "public") + (("cl_dll",) if lib == "client" else ())
    ref = os.path.join(ROOT, "reference")
    # only byte-for-byte matches are stock; a function named by its vtable slot
    # (names-<lib>-slots.tsv) has the stock name but didn't match, so its body is RC2's
    matched = {int(r[0], 16) for r in load(os.path.join(ref, "names-%s.tsv" % lib))}
    covered = collections.defaultdict(list)
    for d in dirs:
        for dp, _, fs in os.walk(os.path.join(ROOT, d)):
            for f in fs:
                if f.endswith((".cpp", ".h", ".c")):
                    path = os.path.join(dp, f)
                    for n, line in enumerate(open(path, encoding="latin-1"), 1):
                        for m in tag.finditer(line):
                            a = int(m.group(1), 16)
                            covered[a - BASE if a >= BASE else a].append("%s:%d" % (os.path.relpath(path, ROOT), n))
    # an incremental link's thunks (client.dll was linked that way: 1398 five-byte "jmp rel32" at
    # the start of .text) are the linker's, not code to reconstruct
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from pe import PE
    pe = PE(os.path.join(ref, "%s.dll" % lib))

    def is_thunk(rva, size):
        # only client.dll was linked incrementally; in rc2.dll a lone jmp is a real function
        # that tail-calls another
        if lib == "client" and size == 5 and (pe.read(rva, 1) or b"") == b"\xe9":
            return True
        # an import's "jmp [__imp_X]" (ff 25 <IAT slot>), the linker's for each DLL function called
        # directly (client.dll: vgui.dll's methods, wsock32)
        return size == 6 and (pe.read(rva, 2) or b"") == b"\xff\x25"

    per = collections.OrderedDict()
    for rva, size, name, fl in load(os.path.join(ref, "files-%s.tsv" % lib)):
        rva = int(rva, 16)
        if is_thunk(rva, int(size)):
            continue
        # the C runtime, and the stretches between two of its objects, aren't RC2's code
        if fl.startswith("libcmt") or (fl.startswith("new: libcmt:") and (" .. libcmt:" in fl or fl.endswith(" .. end"))) or rva in matched:
            continue
        e = per.setdefault(fl, [0, 0, []])
        e[0] += 1
        if rva in covered:
            e[1] += 1
        else:
            e[2].append((rva, int(size), name))
    # code Ghidra's function list misses (scripts/gaps.py): counted under the file of the function
    # before it, as "+gap" entries
    gtsv = os.path.join(ref, "gaps-%s.tsv" % lib)
    if os.path.exists(gtsv):
        before = {}
        for rva, size, name, fl in load(os.path.join(ref, "files-%s.tsv" % lib)):
            before[name] = fl
        for rva, size, kind, refname, after in load(gtsv):
            if kind != "code":
                continue
            rva, fl = int(rva, 16), before.get(after, "?")
            if fl.startswith("libcmt"):
                continue
            e = per.setdefault(fl, [0, 0, []])
            e[0] += 1
            if rva in covered:
                e[1] += 1
            else:
                e[2].append((rva, int(size), "+gap after " + after))
    if args:
        for fl, (n, c, miss) in per.items():
            if args[0] in fl:
                print("%-40s %d of %d reconstructed" % (fl, c, n))
                if missing_only:
                    for rva, size, name in miss:
                        print("   0x%-7x %6d  %s" % (rva, size, name))
        return
    tn = sum(v[0] for v in per.values())
    tc = sum(v[1] for v in per.values())
    print("%s.dll: %d of %d functions to reconstruct have a counterpart (%d%%)" % (lib, tc, tn, 100 * tc // max(tn, 1)))
    for fl, (n, c, _) in sorted(per.items(), key=lambda kv: kv[1][1] - kv[1][0])[:40]:
        print("  %-44s %4d of %4d" % (fl, c, n))


if __name__ == "__main__":
    main()
