#!/usr/bin/env python3
"""vtables.py - every entity class of an RC2 library: its factory, size, vtable and slots.

Each LINK_ENTITY_TO_CLASS factory (an unmangled export that is a classname) allocates the
object through the engine with the class size pushed just before the call, then constructs
it, inline or through a constructor; the constructors write their vtable to offset 0, base
first. The last vtable written is the class's own. Its slots run until the first word that
isn't a .text address or the start of another known vtable.

A stock classname gets its class from SDK 2.3's LINK_ENTITY_TO_CLASS. One of RC2's own gets
the class of the exported methods (the EXPORT think/touch/use functions, the only mangled
exports) that its vtable's functions store, by vote. The slots run until the first word
that isn't a .text address or the start of any vtable a constructor writes.

Writes reference/vtables-<lib>.tsv: classname, class, how it was named, size, vtable rva,
slot count, then the slots (rva:name, name empty when unknown).
Usage: scripts/vtables.py reference/rc2.dll
"""
import collections, os, re, subprocess, sys

import capstone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False


def disasm(pe, rva, limit=600):
    code = pe.read(rva, limit)
    return list(md.disasm(code, pe.base + rva)) if code else []


def stores_and_size(pe, rva, depth=0, seen=None):
    """vtable addresses written to [reg] in program order, following direct calls."""
    seen = seen or set()
    if rva in seen or depth > 3:
        return [], None
    seen.add(rva)
    out, size, last_push = [], None, None
    for ins in disasm(pe, rva):
        m = re.fullmatch(r"dword ptr \[(e\w\w)\], (0x[0-9a-f]+)", ins.op_str)
        if ins.mnemonic == "mov" and m:
            v = int(m.group(2), 16) - pe.base
            if pe.in_section(v, ".rdata") and pe.in_section((pe.u32(v) or 0) - pe.base, ".text"):
                out.append(v)
        if ins.mnemonic == "push" and ins.op_str.startswith("0x"):
            last_push = int(ins.op_str, 16)
        if ins.mnemonic == "call" and ins.op_str.startswith("dword ptr [0x") and size is None and depth == 0:
            size = last_push  # the engine's PvAllocEntPrivateData(pev, size)
        if ins.mnemonic == "call" and ins.op_str.startswith("0x"):
            target = int(ins.op_str, 16) - pe.base
            if pe.in_section(target, ".text"):
                sub, _ = stores_and_size(pe, target, depth + 1, seen)
                out += sub
        if ins.mnemonic in ("ret", "jmp") and not ins.op_str.startswith("0x1"):
            break
        if ins.mnemonic == "ret":
            break
    return out, size


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    target = sys.argv[1]
    lib = os.path.splitext(os.path.basename(target))[0]
    pe = PE(target)
    exports = pe.exports()
    names = {}
    tsv = os.path.join(ROOT, "reference", "names-%s.tsv" % lib)
    if os.path.exists(tsv):
        for line in open(tsv).read().split("\n")[1:]:
            if line:
                f = line.split("\t")
                names[int(f[0], 16)] = f[2]
    for n, rva in exports.items():
        if n.startswith("?"):
            names[rva] = n

    factories = {n: r for n, r in exports.items()
                 if not n.startswith("?") and re.fullmatch(r"[a-z][a-z0-9_]*", n) and
                 n not in ("GiveFnptrsToDll", "CreateInterface")}
    found = {}
    for cn, rva in sorted(factories.items()):
        vts, size = stores_and_size(pe, rva)
        if vts:
            found[cn] = (vts[-1], size)
    # every vtable any constructor writes, abstract bases included, bounds the tables
    starts = sorted(all_vtable_stores(pe) | {v for v, _ in found.values()})

    def slots(vt):
        out, a = [], vt
        nxt = next((s for s in starts if s > vt), None)
        while nxt is None or a < nxt:
            w = pe.u32(a)
            if w is None or not pe.in_section(w - pe.base, ".text"):
                break
            out.append(w - pe.base)
            a += 4
        return out

    def owner(mangled):
        # ?Method@Class@@... -> Class
        m = re.match(r"\?[^@]+@([^@]+)@@", mangled)
        return m.group(1) if m else None

    exported = {r: n for n, r in exports.items() if n.startswith("?")}
    sdk = sdk_classes()
    sdk_set = set(sdk.values()) | {"CBaseEntity", "CBaseDelay", "CBaseAnimating", "CBaseToggle",
                                   "CBaseMonster", "CSquadMonster", "CTalkMonster", "CBasePlayerItem",
                                   "CBasePlayerWeapon", "CBasePlayerAmmo", "CGrenade", "CBaseButton",
                                   "CBasePlayer", "CItem", "CPointEntity", "CBaseTurret", "CFuncTank"}

    def stored_methods(rva):
        """classes of the exported methods a function stores (SetThink/SetTouch/SetUse)."""
        out = []
        for ins in disasm(pe, rva, 2000):
            if ins.mnemonic == "mov" and ins.op_str.startswith("dword ptr [") and ", 0x" in ins.op_str:
                v = int(ins.op_str.rsplit(", ", 1)[1], 16) - pe.base
                if v in exported and owner(exported[v]):
                    out.append(owner(exported[v]))
            if ins.mnemonic == "ret":
                break
        return out

    out = os.path.join(ROOT, "reference", "vtables-%s.tsv" % lib)
    how = collections.Counter()
    with open(out, "w") as f:
        f.write("classname\tclass\thow\tsize\tvtable\tslots\tslot entries (rva:name)\n")
        for cn in sorted(found):
            vt, size = found[cn]
            sl = slots(vt)
            cls, src = None, ""
            if cn in sdk:
                cls, src = sdk[cn], "sdk"
            else:
                # the class whose exported methods this vtable's own functions (and the
                # factory's constructor) store most; a base class's methods count less
                votes = collections.Counter()
                for s in sl:
                    for c in stored_methods(s):
                        votes[c] += 1
                # inherited functions store their own class's methods (CBaseMonster::MonsterThink),
                # so a class SDK 2.3 doesn't have wins over any it does
                own = [c for c, _ in votes.most_common() if c not in sdk_set]
                if own:
                    cls, src = own[0], "stored-methods"
                elif votes:
                    cls, src = votes.most_common(1)[0][0], "stored-methods, inherited only"
            how[src or "none"] += 1
            f.write("%s\t%s\t%s\t%s\t0x%x\t%d\t%s\n" % (
                cn, cls or "", src, "0x%x" % size if size else "", vt, len(sl),
                " ".join("0x%x:%s" % (s, names.get(s, "")) for s in sl)))
    print("%s: %d factories, %d with a vtable, %d vtables written by constructors; class from %s -> %s" % (
        lib, len(factories), len(found), len(starts), dict(how), os.path.relpath(out, ROOT)))


def all_vtable_stores(pe):
    """every .rdata address a mov [reg], [reg + off] or [absolute], imm in .text stores whose first
    word is a .text address."""
    md2 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md2.skipdata = True
    _, va, _, ro, rs = pe.section(".text")
    out = set()
    for ins in md2.disasm(pe.data[ro:ro + rs], pe.base + va):
        if ins.mnemonic != "mov":
            continue
        # [reg] for heap objects; [reg + off] and [absolute] for members and static objects (the
        # client's HUD elements and predicted weapons are globals)
        m = re.fullmatch(r"dword ptr \[(?:e\w\w(?: \+ 0x[0-9a-f]+)?|0x[0-9a-f]+)\], (0x[0-9a-f]+)", ins.op_str)
        if m:
            v = int(m.group(1), 16) - pe.base
            if pe.in_section(v, ".rdata") and pe.in_section((pe.u32(v) or 0) - pe.base, ".text"):
                out.add(v)
    return out


def sdk_classes():
    """{classname: C++ class} from SDK 2.3's LINK_ENTITY_TO_CLASS lines (goldsrc-net/hlsdk c026655)."""
    sdk = os.path.join(ROOT, "..", "hlds64-buildchain", "hlsdk")
    out = {}
    try:
        files = subprocess.run(["git", "-C", sdk, "grep", "-h", "LINK_ENTITY_TO_CLASS", "c026655", "--",
                                "singleplayer/dlls", "singleplayer/cl_dll"],
                               capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return out
    for m in re.finditer(r"LINK_ENTITY_TO_CLASS\(\s*(\w+)\s*,\s*(\w+)\s*\)", files):
        out.setdefault(m.group(1), m.group(2))
    return out


if __name__ == "__main__":
    main()
