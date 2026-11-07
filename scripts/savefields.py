#!/usr/bin/env python3
"""savefields.py - every save table in an RC2 library, with its class's real name.

A save table (m_SaveData) is an array of 16-byte TYPEDESCRIPTION records in .data:
{ FIELDTYPE type; char *name; int offset; short size; short flags; }. SDK 2.3's
IMPLEMENT_SAVERESTORE passes the class name as a literal, save.WriteFields("CFoo", this,
m_SaveData, count), so the function that pushes a table's address also pushes the class
name: that names the class, and the function is its Save (or Restore, ReadFields).

Writes reference/savefields-<lib>.tsv: class, table rva, Save rva, Restore rva, field,
type, offset, size, flags.
Usage: scripts/savefields.py reference/rc2.dll
"""
import collections, os, re, struct, sys

import capstone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIELD = re.compile(r"[A-Za-z_][\w.\[\]]{1,63}")
TYPES = ["FLOAT", "STRING", "ENTITY", "CLASSPTR", "EHANDLE", "EVARS", "EDICT", "VECTOR",
         "POSITION_VECTOR", "POINTER", "INTEGER", "FUNCTION", "BOOLEAN", "SHORT", "CHARACTER",
         "TIME", "MODELNAME", "SOUNDNAME"]


def strings_ok(pe, p):
    return pe.in_section(p - pe.base, ".data") or pe.in_section(p - pe.base, ".rdata")


def tables(pe):
    _, va, _, _, rs = pe.section(".data")
    out, cur, rva = [], [], va
    while rva < va + rs - 16:
        t, name, off, size, flags = struct.unpack("<IIIHH", pe.read(rva, 16))
        n = pe.cstr(name - pe.base) if t < len(TYPES) and strings_ok(pe, name) else None
        if n and FIELD.fullmatch(n) and off < 0x4000 and 0 < size <= 512:
            cur.append((rva, TYPES[t], n, off, size, flags))
            rva += 16
            continue
        # a real table names members m_* or entvars-style fields; schedules and tasks don't
        if cur and sum(1 for c in cur if c[2].startswith("m_")) * 2 >= len(cur):
            out.append(cur)
        cur, rva = [], rva + 4
    return out


def pushers(pe):
    """{pushed imm: [(function start guess, [imm pushed nearby])]} over .text, by linear sweep."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    _, va, _, ro, rs = pe.section(".text")
    code = pe.data[ro:ro + rs]
    pushes = []
    for ins in md.disasm(code, pe.base + va):
        if ins.mnemonic == "push" and ins.op_str.startswith("0x"):
            pushes.append((ins.address - pe.base, int(ins.op_str, 16) - pe.base))
    return pushes


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    pe = PE(sys.argv[1])
    lib = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    tabs = tables(pe)
    by_addr = {t[0][0]: t for t in tabs}
    pushes = pushers(pe)
    owner, users = {}, collections.defaultdict(list)
    for i, (at, imm) in enumerate(pushes):
        if imm not in by_addr:
            continue
        # WriteFields/ReadFields(pname, pBaseData, pFields, count): the name is pushed after the
        # table (arguments go right to left), within a few instructions
        for at2, imm2 in pushes[i + 1:i + 4]:
            if at2 - at > 24:
                break
            if strings_ok(pe, imm2 + pe.base):
                s = pe.cstr(imm2)
                if re.fullmatch(r"C\w+", s):
                    owner.setdefault(imm, s)
                    users[imm].append(at)
                    break
    out = os.path.join(ROOT, "reference", "savefields-%s.tsv" % lib)
    named = 0
    with open(out, "w") as f:
        f.write("class\ttable\tfield\ttype\toffset\tsize\tflags\n")
        for t in tabs:
            cls = owner.get(t[0][0], "")
            named += bool(cls)
            for rva, typ, name, off, size, flags in t:
                f.write("%s\t0x%x\t%s\t%s\t0x%x\t%d\t%d\n" % (cls, t[0][0], name, typ, off, size, flags))
    print("%s: %d save tables (%d fields), %d with their class name -> %s" % (
        lib, len(tabs), sum(len(t) for t in tabs), named, os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
