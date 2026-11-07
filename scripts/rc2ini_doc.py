#!/usr/bin/env python3
"""rc2ini_doc.py - every rc2.ini key, with its type, default, clamps and where it is read.

Reads the key table from dlls/rc2prefs_table.cpp (scripts/gen_prefs.py writes it from
reference/prefs-rc2.tsv, with the original's own type codes) and scans dlls/, cl_dll/ and
pm_shared/ for each key's RC2P_<Heading>_<Key> name to say which files read it. With --ini,
adds the value a shipped rc2.ini gives each key, the value the loader actually keeps after
its type range and flag clamps (dlls/rc2prefs.cpp, RC2Prefs_ParseValue), and lists the
file's lines that match no key.

Writes docs/RC_INI.md.
Usage: scripts/rc2ini_doc.py [--ini path/to/rc2.ini] [--check]
  --check  exit 1 if docs/RC_INI.md is not what this run would write
"""
import argparse, collections, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(ROOT, "dlls", "rc2prefs_table.cpp")
TSV = os.path.join(ROOT, "reference", "prefs-rc2.tsv")
SCAN = ["dlls", "cl_dll", "pm_shared"]
SKIP = {"rc2prefs_table.cpp", "rc2prefs_table.h"}
OUT = os.path.join(ROOT, "docs", "RC_INI.md")

# The type codes, as the original's "rccfg findkey" names them (dlls/rc2prefs.h).
TYPES = {0: "float", 1: "smallfloat", 2: "double", 3: "int", 4: "short", 5: "ushort", 6: "byte", 7: "bool"}
# The flag bits, in the order findkey prints them.
FLAGS = [
    (0x1, "POS_ONLY"),
    (0x2, "THREE_DIGITS"),
    (0x4, "NOT_ZERO"),
    (0x8, "GREATER_THEN_ONE"),
    (0x10, "ZERO_TO_ONE"),
    (0x20, "NEGATIVE_ONE_TO_ONE"),
    (0x40, "FOUR_DIGITS"),
    (0x40000000, "REGISTER_ITEM_INFO"),
]
NETWORK_SYNC = 0x80000000
ROW = re.compile(r'^\s*\{(\d+), "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", 0x([0-9a-f]+)\}, // (\d+)$')
NAME = re.compile(r"\bRC2P_\w+")


def read_table():
    keys = []
    for line in open(TABLE, encoding="latin-1"):
        m = ROW.match(line)
        if m:
            t, head, key, default, flags, i = m.groups()
            assert int(i) == len(keys), "table out of order at %s" % i
            keys.append({"type": int(t), "head": head, "key": key, "default": default, "flags": int(flags, 16)})
    tsv_count = sum(1 for line in open(TSV).read().split("\n")[1:] if line)
    if tsv_count != len(keys):
        sys.exit("dlls/rc2prefs_table.cpp has %d keys, reference/prefs-rc2.tsv %d: run scripts/gen_prefs.py" % (len(keys), tsv_count))
    return keys


def scan_readers():
    """RC2P_ name -> the files that name it; client files get a (cl) mark."""
    readers = collections.defaultdict(list)
    for top in SCAN:
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, top)):
            dirnames.sort()
            for name in sorted(filenames):
                if not name.endswith((".cpp", ".h")) or name in SKIP:
                    continue
                src = open(os.path.join(dirpath, name), encoding="latin-1").read()
                label = name + ("(cl)" if top == "cl_dll" else "")
                for n in sorted(set(NAME.findall(src))):
                    if label not in readers[n]:
                        readers[n].append(label)
    return readers


def c_atof(s):
    m = re.match(r"\s*([+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?)", s)
    return float(m.group(1)) if m else 0.0


def c_atoi(s):
    m = re.match(r"\s*([+-]?\d+)", s)
    return int(m.group(1)) if m else 0


def clamp_flags(flags, v, unsigned_char=False):
    """RC2's ApplyFlags. On an unsigned char the below-0 and above-255 tests never fire."""
    if flags & 0x1 and v < 0:
        v = 0
    if flags & 0x2:
        v = 0 if v < 0 else min(v, 999)
    if flags & 0x10:
        v = 0 if v < 0 else min(v, 1)
    if flags & 0x20:
        v = max(-1, min(v, 1))
    if flags & 0x40:
        v = 0 if v < 0 else min(v, 9999)
    if flags & 0x8 and v < 1:
        v = 1
    if flags & 0x4 and v == 0:
        v = 1
    return v


def parse(k, text):
    """The value RC2Prefs_ParseValue keeps for text, as a number (bool as 0/1)."""
    t, fl = k["type"], k["flags"]
    if t in (0, 2):
        return clamp_flags(fl, c_atof(text))
    if t == 1:
        return clamp_flags(fl, max(0.0, min(c_atof(text), 655.35)))
    if t == 3:
        return clamp_flags(fl, c_atoi(text))
    if t == 4:
        return clamp_flags(fl, max(-0x7fff, min(c_atoi(text), 0x7fff)))
    if t == 5:
        # held to 0..65535, then clamped as a signed short and printed unsigned
        v = max(0, min(c_atoi(text), 0xffff))
        s = v - 0x10000 if v >= 0x8000 else v
        return clamp_flags(fl, s) & 0xffff
    if t == 6:
        return clamp_flags(fl, max(0, min(c_atoi(text), 0xff)), True)
    if t == 7:
        on = text.strip().lower() in ("true", "yes") or c_atoi(text) != 0
        return clamp_flags(fl, 1 if on else 0, True)
    raise ValueError(t)


def raw_value(k, text):
    """The number the text says, before any range or flag clamp."""
    if k["type"] in (0, 1, 2):
        return c_atof(text)
    if k["type"] == 7:
        return 1 if text.strip().lower() in ("true", "yes") or c_atoi(text) != 0 else 0
    return c_atoi(text)


def fmt(k, v):
    if k["type"] == 7:
        return "true" if v else "false"
    if k["type"] in (0, 1, 2):
        return "%g" % v
    return "%d" % v


def read_ini(path):
    """(heading, key) lowercased -> (value text, line); and the lines outside any key."""
    ini, head, odd = collections.OrderedDict(), None, []
    for line in open(path, encoding="latin-1"):
        line = line.rstrip()
        if len(line) <= 2:
            continue
        if line.startswith("[") and line.endswith("]"):
            head = line[1:-1]
        elif head is None:
            odd.append("outside all headings: " + line)
        elif "=" not in line:
            odd.append("no '=': [%s] %s" % (head, line))
        else:
            k, v = line.split("=", 1)
            ini[(head.lower(), k.lower())] = (v, "[%s]%s" % (head, k))
    return ini, odd


def table(cols, body):
    widths = [max(len(c), *(len(r[i]) for r in body)) for i, c in enumerate(cols)]
    out = ["```", "  ".join(c.ljust(widths[i]) for i, c in enumerate(cols)).rstrip()]
    for r in body:
        out.append("  ".join(v.ljust(widths[i]) for i, v in enumerate(r)).rstrip())
    out.append("```")
    return out


def render(keys, readers, ini, odd):
    out = []
    w = out.append
    w("# rc2.ini keys")
    w("")
    w("Generated by `scripts/rc2ini_doc.py`; don't edit by hand. Every key of RC2's prefs table")
    w("(`dlls/rc2prefs_table.cpp`, %d keys in %d headings), in the table's order, which is also the" % (len(keys), len({k["head"] for k in keys})))
    w("order `rccfg save` writes them in. How the file is found, loaded and changed in game is in")
    w("[SERVER.md](SERVER.md#rc2ini).")
    w("")
    w("Columns:")
    w("")
    w("- **type**: how the value is parsed and held. `smallfloat` is a float held to 0..655.35 (it")
    w("  goes to clients as hundredths); `short` is held to -32767..32767, `ushort` to 0..65535,")
    w("  `byte` to 0..255; `bool` is true for `true`, `yes` or a nonzero number.")
    w("- **default**: the value the key has when the file doesn't set it. Every key goes back to")
    w("  its default at each map start, before the file is read.")
    if ini is not None:
        w("- **shipped**: what RC2 beta 1.1's own `rc2.ini` sets, as the file writes it. Blank: the")
        w("  file doesn't set it. A value the loader changes is shown as `text -> kept`.")
    w("- **flags**: the clamps applied after parsing, in this order: POS_ONLY (below 0 becomes 0),")
    w("  THREE_DIGITS (held to 0..999), ZERO_TO_ONE (0..1), NEGATIVE_ONE_TO_ONE (-1..1), FOUR_DIGITS")
    w("  (0..9999), GREATER_THEN_ONE (below 1 becomes 1), NOT_ZERO (0 becomes 1). On a `byte` or")
    w("  `bool` the below-0 and above-255 tests can't fire; a `ushort` is clamped as a signed")
    w("  short, so 32768 and up count as negative. REGISTER_ITEM_INFO: changing the key in game")
    w("  re-reads the 14 stock weapons' item info (slots, maximum ammo).")
    w("- **sent**: `yes` for the NETWORK_SYNC keys, which the client library also reads. A")
    w("  joining client gets every one that isn't at its default, and `rccfg setkey` sends the")
    w("  new value to everyone.")
    w("- **read in**: the files that name the key (`(cl)`: the client library). Blank: no code")
    w("  in the rebuild reads it.")
    w("")

    by_head = collections.OrderedDict()
    for i, k in enumerate(keys):
        by_head.setdefault(k["head"], []).append((i, k))

    changed, unread = [], []
    for head, rows in by_head.items():
        w("## [%s]" % head)
        w("")
        cols = ["key", "type", "default"] + (["shipped"] if ini is not None else []) + ["flags", "sent", "read in"]
        body = []
        for i, k in rows:
            r = [k["key"], TYPES[k["type"]], k["default"]]
            if ini is not None:
                got = ini.get((k["head"].lower(), k["key"].lower()))
                if got is None:
                    r.append("")
                else:
                    text = got[0]
                    kept = parse(k, text)
                    if raw_value(k, text) != kept if k["type"] not in (0, 1, 2) else abs(raw_value(k, text) - kept) > 1e-6:
                        r.append("%s -> %s" % (text, fmt(k, kept)))
                        changed.append("[%s]%s=%s is kept as %s" % (k["head"], k["key"], text, fmt(k, kept)))
                    else:
                        r.append(text)
            r.append(" ".join(n for bit, n in FLAGS if k["flags"] & bit))
            r.append("yes" if k["flags"] & NETWORK_SYNC else "")
            name = "RC2P_%s_%s" % (k["head"], k["key"])
            r.append(" ".join(readers.get(name, [])))
            if name not in readers:
                unread.append("[%s] %s" % (k["head"], k["key"]))
            body.append(r)
        out.extend(table(cols, body))
        w("")

    w("## Keys no code reads")
    w("")
    if unread:
        w("In the table, so `rc2.ini` may set them and `rccfg` lists them, but no code in the rebuild")
        w("names them. Whether the original reads any of them some other way is unverified.")
        w("")
        w("```")
        out.extend(unread)
        w("```")
    else:
        w("None.")
    w("")

    if ini is not None:
        known = {(k["head"].lower(), k["key"].lower()) for k in keys}
        extra = [line for hk, (v, line) in ini.items() if hk not in known]
        w("## Shipped values the loader changes")
        w("")
        if changed:
            w("```")
            out.extend(changed)
            w("```")
        else:
            w("None.")
        w("")
        w("## Shipped lines that match no key")
        w("")
        w("The loader prints a console line for each and ignores it.")
        w("")
        if extra or odd:
            w("```")
            out.extend(extra + odd)
            w("```")
        else:
            w("None.")
        w("")
        missing = [k for k in keys if (k["head"].lower(), k["key"].lower()) not in ini]
        w("## Keys the shipped file doesn't set")
        w("")
        if missing:
            w("```")
            out.extend("[%s] %s" % (k["head"], k["key"]) for k in missing)
            w("```")
        else:
            w("None.")
        w("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ini")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    keys = read_table()
    ini, odd = read_ini(a.ini) if a.ini else (None, [])
    text = render(keys, scan_readers(), ini, odd)
    if a.check:
        cur = open(OUT).read() if os.path.exists(OUT) else ""
        if cur != text:
            print("docs/RC_INI.md is stale: run scripts/rc2ini_doc.py" + (" --ini " + a.ini if a.ini else ""))
            sys.exit(1)
        return
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "w").write(text)
    print("%d keys in %d headings -> %s" % (len(keys), len({k["head"] for k in keys}), os.path.relpath(OUT, ROOT)))


if __name__ == "__main__":
    main()
