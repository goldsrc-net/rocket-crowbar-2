#!/usr/bin/env python3
"""slots.py - name RC2's stock virtual functions by their vtable position.

For every stock class in reference/vtables-<lib>.tsv (named from SDK 2.3's
LINK_ENTITY_TO_CLASS), its SDK 2.3 vtable comes from the VC6 reference build's .map
(??_7<Class>@@6B@, every slot named). The two slot lists are paired with the functions
named in both as anchors; between two anchors whose gaps are the same length the slots pair
one to one, and where RC2's gap is longer it inserted virtuals there, which stay unnamed.
Each RC2 function then takes the SDK name it is paired with most often across classes (an
inherited body sits in many vtables); a tie, or a name already used, leaves it alone.

Writes reference/names-<lib>-slots.tsv in the names-<lib>.tsv format (ref_object "vtable"),
and prints where RC2's vtables are longer than SDK 2.3's.
Usage: scripts/slots.py build-vc6/hl/hl.map build-vc6/hl/hl.dll reference/rc2.dll
"""
import collections, os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def reference_vtables(ref_map, ref_dll):
    pe = PE(ref_dll)
    vt, fn = {}, {}
    for line in open(ref_map, encoding="latin-1"):
        m = re.match(r"\s*\d{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if not m:
            continue
        rva = int(m.group(2), 16) - pe.base
        if m.group(1).startswith("??_7") and m.group(1).endswith("@@6B@"):
            vt[m.group(1)[4:-5]] = rva
        else:
            fn.setdefault(rva, m.group(1))
    starts = sorted(vt.values())

    def slots(cls):
        out, a = [], vt[cls]
        nxt = next((s for s in starts if s > a), None)
        while nxt is None or a < nxt:
            w = pe.u32(a)
            if w is None or not pe.in_section(w - pe.base, ".text"):
                break
            out.append(fn.get(w - pe.base, ""))
            a += 4
        return out

    return vt, slots


def pair(sdk, rc2):
    """[(j, k)] pairing SDK slot j with RC2 slot k, plus RC2 slots inserted (unpaired)."""
    pos = collections.defaultdict(list)
    for k, n in enumerate(rc2):
        if n:
            pos[n].append(k)
    anchors = [(-1, -1)]
    for j, n in enumerate(sdk):
        if n and len(pos.get(n, [])) == 1 and pos[n][0] > anchors[-1][1]:
            anchors.append((j, pos[n][0]))
    anchors.append((len(sdk), len(rc2)))
    pairs, inserted = [], []
    for (j0, k0), (j1, k1) in zip(anchors, anchors[1:]):
        if j0 >= 0:
            pairs.append((j0, k0))
        gj, gk = j1 - j0, k1 - k0
        if gj == gk:
            pairs += [(j0 + d, k0 + d) for d in range(1, gj)]
        elif gk > gj:
            inserted.append((j0, k0, gk - gj))
    return pairs, inserted


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    ref_map, ref_dll, target = sys.argv[1:]
    lib = os.path.splitext(os.path.basename(target))[0]
    vt, sdk_slots = reference_vtables(ref_map, ref_dll)
    named = set()
    tsv = os.path.join(ROOT, "reference", "names-%s.tsv" % lib)
    taken_names = set()
    for line in open(tsv).read().split("\n")[1:]:
        if line:
            f = line.split("\t")
            named.add(int(f[0], 16))
            taken_names.add(f[2])
    rows = [l.split("\t") for l in open(os.path.join(ROOT, "reference", "vtables-%s.tsv" % lib)).read().split("\n")[1:] if l]
    votes = collections.defaultdict(collections.Counter)
    insertions = collections.Counter()
    for r in rows:
        if r[2] != "sdk" or r[1] not in vt:
            continue
        sdk = sdk_slots(r[1])
        entries = [e.split(":", 1) for e in r[6].split()]
        rc2_rvas = [int(a, 16) for a, _ in entries]
        rc2_names = [n for _, n in entries]
        pairs, inserted = pair(sdk, rc2_names)
        for j0, k0, n in inserted:
            insertions[(re.sub(r"@@.*", "", sdk[j0]) if j0 >= 0 else "start", n)] += 1
        for j, k in pairs:
            if sdk[j] and not rc2_names[k] and rc2_rvas[k] not in named:
                votes[rc2_rvas[k]][sdk[j]] += 1

    out_rows, ties = [], 0
    for rva, c in votes.items():
        (best, n), *rest = c.most_common()
        if rest and rest[0][1] == n:
            ties += 1
            continue
        if best in taken_names:
            continue
        taken_names.add(best)
        out_rows.append((rva, best))
    out_rows.sort()
    dm = {}
    if out_rows:
        r = subprocess.run(["llvm-undname"], input="\n".join(n for _, n in out_rows) + "\n",
                           capture_output=True, text=True)
        lines = r.stdout.split("\n")
        dm = {lines[i]: lines[i + 1] for i in range(0, len(lines) - 1, 3)}
    out = os.path.join(ROOT, "reference", "names-%s-slots.tsv" % lib)
    with open(out, "w") as f:
        f.write("rva\tsize\tname\tdemangled\tref_object\n")
        for rva, name in out_rows:
            f.write("0x%x\t0\t%s\t%s\tvtable\n" % (rva, name, dm.get(name, name)))
    print("%s: %d more functions named by vtable position (%d ties skipped) -> %s" % (
        lib, len(out_rows), ties, os.path.relpath(out, ROOT)))
    print("RC2 virtuals inserted after (SDK slot, count): classes")
    for (after, n), c in insertions.most_common(12):
        print("  after %-45s +%d  in %d classes" % (after, n, c))


if __name__ == "__main__":
    main()
