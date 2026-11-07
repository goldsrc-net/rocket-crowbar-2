#!/usr/bin/env python3
"""check_coverage.py - which of rc2.dll's functions our source reconstructs.

Every function the VC6 SDK 2.3 build doesn't match (reference/names-rc2*.tsv) is RC2's to
reconstruct, and a reconstructed one carries its address in a comment, "// orig 0x<rva>"
(the RVA as in reference/files-rc2.tsv, e.g. orig 0xaea60; a VA such as 0x100aea60 counts
the same). This lists, per source file of the original (reference/files-rc2.tsv), how many
there are and how many have a counterpart in dlls/, pm_shared/, game_shared/ and common/.

Usage:
  scripts/check_coverage.py                    summary per file, worst first
  scripts/check_coverage.py <file> --missing   the missing ones in files whose name contains <file>
"""
import collections, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 0x10000000


def load(p):
    return [l.split("\t") for l in open(p).read().split("\n")[1:] if l]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    missing_only = "--missing" in sys.argv
    ref = os.path.join(ROOT, "reference")
    matched = set()
    # only byte-for-byte matches are stock; a function named by its vtable slot
    # (names-rc2-slots.tsv) has the stock name but didn't match, so its body is RC2's
    matched = {int(r[0], 16) for r in load(os.path.join(ref, "names-rc2.tsv"))}
    covered = collections.defaultdict(list)
    for d in ("dlls", "pm_shared", "game_shared", "common"):
        for dp, _, fs in os.walk(os.path.join(ROOT, d)):
            for f in fs:
                if f.endswith((".cpp", ".h", ".c")):
                    path = os.path.join(dp, f)
                    for n, line in enumerate(open(path, encoding="latin-1"), 1):
                        for m in re.finditer(r"orig (0x[0-9a-fA-F]+)", line):
                            a = int(m.group(1), 16)
                            covered[a - BASE if a >= BASE else a].append("%s:%d" % (os.path.relpath(path, ROOT), n))
    per = collections.OrderedDict()
    for rva, size, name, fl in load(os.path.join(ref, "files-rc2.tsv")):
        rva = int(rva, 16)
        if fl.startswith("libcmt") or rva in matched:
            continue
        e = per.setdefault(fl, [0, 0, []])
        e[0] += 1
        if rva in covered:
            e[1] += 1
        else:
            e[2].append((rva, int(size), name))
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
    print("rc2.dll: %d of %d functions to reconstruct have a counterpart (%d%%)" % (tc, tn, 100 * tc // max(tn, 1)))
    for fl, (n, c, _) in sorted(per.items(), key=lambda kv: kv[1][1] - kv[1][0])[:40]:
        print("  %-44s %4d of %4d" % (fl, c, n))


if __name__ == "__main__":
    main()
