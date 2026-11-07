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


def resolve_thunk(pe, rva):
    """An incremental link's thunk (a lone jmp rel32) to the function it jumps to."""
    b = pe.read(rva, 5)
    if b and b[0] == 0xe9:
        import struct
        return rva + 5 + struct.unpack("<i", b[1:5])[0]
    return rva


def rows_by_overlap(target, names_tsv, vt, sdk_slots):
    """For client.dll, whose classes aren't built by exported factories and whose vtables go
    through incremental-link thunks: every vtable a constructor writes, its slots resolved
    through the thunks, given the SDK class with the most of its own methods (?Method@Class@@)
    in it. Rows in vtables.tsv's shape."""
    import vtables
    pe = PE(target)
    names = {}
    for line in open(names_tsv).read().split("\n")[1:]:
        if line:
            f = line.split("\t")
            names[int(f[0], 16)] = f[2]
    starts = sorted(vtables.all_vtable_stores(pe))
    own = {c: {n for n in sdk_slots(c) if re.match(r"\?[^@]+@%s@@" % re.escape(c), n)} for c in vt}
    rows, sdk_cache = [], {}
    for i, v in enumerate(starts):
        nxt = starts[i + 1] if i + 1 < len(starts) else None
        sl, a = [], v
        while nxt is None or a < nxt:
            w = pe.u32(a)
            if w is None or not pe.in_section(w - pe.base, ".text"):
                break
            sl.append(resolve_thunk(pe, w - pe.base))
            a += 4
        got = [names.get(x, "") for x in sl]
        # by position: how many slots hold the same named function in both, inherited defaults
        # included (the client's overrides are what RC2 changed, so its own methods rarely
        # match); a table of the same length breaks a tie
        score = []
        for c in vt:
            ref = sdk_cache.setdefault(c, sdk_slots(c))
            hits = sum(1 for k in range(min(len(ref), len(got))) if got[k] and got[k] == ref[k])
            if hits:
                score.append((hits, len(ref) == len(got), len(own[c] & set(got)), c))
        score.sort(reverse=True)
        if not score or score[0][0] < 3 or (len(score) > 1 and score[1][:3] == score[0][:3]):
            continue
        rows.append(["", score[0][3], "sdk", "", "0x%x" % v, str(len(sl)),
                     " ".join("0x%x:%s" % (x, names.get(x, "")) for x in sl)])
    return rows


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
    vtsv = os.path.join(ROOT, "reference", "vtables-%s.tsv" % lib)
    if os.path.exists(vtsv):
        rows = [l.split("\t") for l in open(vtsv).read().split("\n")[1:] if l]
    else:
        rows = rows_by_overlap(target, tsv, vt, sdk_slots)
        print("%s: no vtables-%s.tsv; %d vtables matched to SDK classes by the methods they share" % (
            lib, lib, len(rows)))
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
