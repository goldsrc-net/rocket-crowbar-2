"""pe.py - the little of the PE format the RC2 scripts need: sections, exports, reads by RVA."""
import struct


class PE:
    def __init__(self, path):
        self.data = d = open(path, "rb").read()
        e = struct.unpack_from("<I", d, 0x3c)[0]
        n = struct.unpack_from("<H", d, e + 6)[0]
        opt = e + 24
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        optsize = struct.unpack_from("<H", d, e + 20)[0]
        self.dirs = [struct.unpack_from("<II", d, opt + 96 + 8 * i) for i in range(16)]
        self.sections = []
        for i in range(n):
            o = opt + optsize + 40 * i
            name = d[o:o + 8].rstrip(b"\0").decode()
            vs, va, rs, ro = struct.unpack_from("<IIII", d, o + 8)
            self.sections.append((name, va, max(vs, rs), ro, rs))

    def section(self, name):
        return next(s for s in self.sections if s[0] == name)

    def in_section(self, rva, name):
        _, va, size, _, _ = self.section(name)
        return va <= rva < va + size

    def off(self, rva):
        for _, va, size, ro, rs in self.sections:
            if va <= rva < va + rs:
                return ro + rva - va
        return None

    def read(self, rva, n):
        o = self.off(rva)
        return None if o is None else self.data[o:o + n]

    def u32(self, rva):
        b = self.read(rva, 4)
        return None if b is None or len(b) < 4 else struct.unpack("<I", b)[0]

    def cstr(self, rva):
        o = self.off(rva)
        return self.data[o:self.data.index(b"\0", o)].decode("latin-1")

    def exports(self):
        """{name: rva} for every named export."""
        rva, _ = self.dirs[0]
        (_, _, _, _, _, _, nfuncs, nnames, afuncs, anames, aords) = struct.unpack("<IIHHIIIIIII", self.read(rva, 40))
        out = {}
        for i in range(nnames):
            name = self.cstr(self.u32(anames + 4 * i))
            ordinal = struct.unpack("<H", self.read(aords + 2 * i, 2))[0]
            out[name] = self.u32(afuncs + 4 * ordinal)
        return out
