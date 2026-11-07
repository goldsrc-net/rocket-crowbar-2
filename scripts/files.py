#!/usr/bin/env python3
"""files.py - which source file each function of an RC2 library comes from.

The linker lays the object files out in project order, so the functions named from the VC6
reference build (each with its object file, from the reference .map) mark where each SDK
file sits. A function between two named ones from the same object is in that object. A run
between two different objects is a file SDK 2.3 doesn't have: one of RC2's own. SDK 2.3's
project lists its sources alphabetically, so each new run is named "new: <after> .. <before>",
with the classes of the exported methods inside it (RC2's EXPORT think/touch/use functions).
Inline functions (the "i" in the .map) are left out as evidence: one copy of an inline is
kept, wherever the linker meets it first.

Writes reference/files-<lib>.tsv: rva, size, name, file. Prints the files in .text order.
Usage: scripts/files.py build-vc6/hl/hl.map reference/rc2.dll
"""
import collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def obj_name(o):
    o = re.sub(r"^LIBCMT:", "libcmt:", o)
    return re.sub(r"^_+", "", o).replace(".obj", "")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    ref_map, target = sys.argv[1:]
    lib = os.path.splitext(os.path.basename(target))[0]
    obj_of, inline = {}, set()
    for line in open(ref_map, encoding="latin-1"):
        m = re.match(r"\s*0001:[0-9a-f]{8}\s+(\S+)\s+[0-9a-f]{8}\s+f\s+(i\s+)?(\S+)", line)
        if m:
            obj_of.setdefault(m.group(1), obj_name(m.group(3)))
            if m.group(2):
                inline.add(m.group(1))
    named = {}
    for suffix in (".tsv", "-slots.tsv"):
        p = os.path.join(ROOT, "reference", "names-%s%s" % (lib, suffix))
        if os.path.exists(p):
            for line in open(p).read().split("\n")[1:]:
                if line:
                    f = line.split("\t")
                    named[int(f[0], 16)] = f[2]
    pe = PE(target)
    exported = {r: n for n, r in pe.exports().items() if n.startswith("?")}
    funcs = []
    for line in open(os.path.join(ROOT, "reference", "functions-%s.tsv" % lib)).read().split("\n")[1:]:
        if line:
            rva, size, name = line.split("\t")
            funcs.append([int(rva, 16), int(size), name, None])
    # evidence
    for f in funcs:
        n = named.get(f[0])
        if n and n in obj_of and n not in inline:
            f[3] = obj_of[n]
    # fill: between two of the same object, that object; else a new file
    idx = [i for i, f in enumerate(funcs) if f[3]]
    for a, b in zip([-1] + idx, idx + [len(funcs)]):
        left = funcs[a][3] if a >= 0 else "start"
        right = funcs[b][3] if b < len(funcs) else "end"
        for i in range(a + 1, b):
            funcs[i][3] = left if left == right else "new: %s .. %s" % (left, right)
    # the classes exported methods name, per new run
    classes = collections.defaultdict(collections.Counter)
    for f in funcs:
        n = exported.get(f[0], "")
        m = re.match(r"\?[^@]+@([^@]+)@@", n)
        if m and f[3].startswith("new"):
            classes[f[3]][m.group(1)] += 1
    out = os.path.join(ROOT, "reference", "files-%s.tsv" % lib)
    with open(out, "w") as w:
        w.write("rva\tsize\tname\tfile\n")
        for rva, size, name, fl in funcs:
            w.write("0x%x\t%d\t%s\t%s\n" % (rva, size, name, fl))
    # summary in .text order
    runs = []
    for f in funcs:
        if runs and runs[-1][0] == f[3]:
            runs[-1][1] += 1
            runs[-1][2] += f[1]
        else:
            runs.append([f[3], 1, f[1], f[0]])
    sdk_files = len({r[0] for r in runs if not r[0].startswith("new") and not r[0].startswith("libcmt")})
    new = [r for r in runs if r[0].startswith("new")]
    print("%s: %d functions; %d SDK files placed, %d runs of RC2's own code (%d functions, %d bytes) -> %s" % (
        lib, len(funcs), sdk_files, len(new), sum(r[1] for r in new), sum(r[2] for r in new),
        os.path.relpath(out, ROOT)))
    for r in new:
        cls = ", ".join(c for c, _ in classes[r[0]].most_common(6))
        print("  0x%-7x %4d fns %7d bytes  %s  [%s]" % (r[3], r[1], r[2], r[0][5:], cls))


if __name__ == "__main__":
    main()
