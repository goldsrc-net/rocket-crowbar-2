#!/usr/bin/env python3
"""match.py - name the stock functions of an original RC2 library from a VC6 reference build.

The reference is Half-Life SDK 2.3 built with VC6 12.00.8168 (scripts/vc6/README.md). Each
function in the reference's .map becomes a byte pattern: its body with trailing int3/nop
padding cut, and every 4-byte operand that is an absolute address in the image or a call/jmp
displacement masked. A function is named when its pattern occurs exactly once in the
original's .text and no other reference function has the same pattern. Everything else is
left for the next steps (exports, vtables, call graph).

Writes reference/names-<lib>.tsv: rva, size, mangled name, demangled name, reference object.
Usage:
  scripts/vc6/match.py build-vc6/hl/hl.map build-vc6/hl/hl.dll reference/rc2.dll
  scripts/vc6/match.py build-vc6/client/cl_dll.map build-vc6/client/cl_dll.dll reference/client.dll
  --out <file> writes there instead (to compare reference builds)
"""
import collections, os, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MIN = 16  # shorter bodies match in too many places to mean anything


def text_section(path):
    pe = open(path, "rb").read()
    e = struct.unpack_from("<I", pe, 0x3c)[0]
    n = struct.unpack_from("<H", pe, e + 6)[0]
    opt = struct.unpack_from("<H", pe, e + 20)[0]
    base = struct.unpack_from("<I", pe, e + 52)[0]
    for i in range(n):
        o = e + 24 + opt + 40 * i
        if pe[o:o + 8].rstrip(b"\0") == b".text":
            vs, va, rs, ro = struct.unpack_from("<IIII", pe, o + 8)
            return base, va, pe[ro:ro + min(vs, rs)]
    sys.exit("no .text in " + path)


def map_functions(path):
    funcs = []
    for line in open(path, encoding="latin-1"):
        m = re.match(r"\s*0001:([0-9a-f]{8})\s+(\S+)\s+[0-9a-f]{8}\s+f\s+(?:i\s+)?(\S+)", line)
        if m:
            funcs.append((int(m.group(1), 16), m.group(2), m.group(3)))
    funcs = sorted(set(funcs))
    return funcs


def pattern(body, base):
    b = bytearray(body)
    while b and b[-1] in (0xcc, 0x90):
        b.pop()
    if len(b) < MIN:
        return None
    out, j = [], 0
    while j < len(b):
        if j + 4 <= len(b) and ((struct.unpack_from("<I", b, j)[0] >> 24) == (base >> 24) or
                                (j >= 1 and b[j - 1] in (0xe8, 0xe9))):
            out.append(b"....")
            j += 4
        else:
            out.append(re.escape(bytes([b[j]])))
            j += 1
    return b"".join(out)


def demangle(names):
    """llvm-undname prints each input line, its demangling, then a blank line."""
    try:
        r = subprocess.run(["llvm-undname"], input="\n".join(names) + "\n", capture_output=True, text=True)
    except FileNotFoundError:
        return {}
    lines = r.stdout.split("\n")
    return {lines[i]: lines[i + 1] for i in range(0, len(lines) - 1, 3)}


def main():
    args = sys.argv[1:]
    out = None
    if "--out" in args:
        i = args.index("--out")
        out = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    ref_map, ref_dll, target = args
    rbase, _, rtext = text_section(ref_dll)
    tbase, tva, ttext = text_section(target)
    funcs = map_functions(ref_map)

    pats = collections.defaultdict(list)
    for i, (off, name, obj) in enumerate(funcs):
        end = funcs[i + 1][0] if i + 1 < len(funcs) else len(rtext)
        p = pattern(rtext[off:end], rbase)
        if p:
            pats[p].append((name, obj, end - off))

    found, ambiguous, multi = [], 0, 0
    for p, owners in pats.items():
        hits = [m.start() for m, _ in zip(re.finditer(p, ttext, re.S), range(2))]
        if not hits:
            continue
        if len(owners) > 1:
            ambiguous += 1
            continue
        if len(hits) > 1:
            multi += 1
            continue
        name, obj, size = owners[0]
        found.append((tva + hits[0], len(re.match(p, ttext[hits[0]:], re.S).group(0)), name, obj))

    found.sort()
    dm = demangle([f[2] for f in found if f[2].startswith("?")])
    lib = os.path.splitext(os.path.basename(target))[0]
    out = out or os.path.join(ROOT, "reference", "names-%s.tsv" % lib)
    with open(out, "w") as f:
        f.write("rva\tsize\tname\tdemangled\tref_object\n")
        for rva, size, name, obj in found:
            f.write("0x%x\t%d\t%s\t%s\t%s\n" % (rva, size, name, dm.get(name, name), obj))
    sdk = sum(1 for x in found if not x[3].startswith("LIBCMT"))
    print("%s: %d reference patterns, %d named (%d SDK, %d C runtime); %d skipped as shared by "
          "several reference functions, %d as found more than once -> %s"
          % (lib, len(pats), len(found), sdk, len(found) - sdk, ambiguous, multi, os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
