#!/usr/bin/env python3
"""rich.py - print a PE file's Rich header: which Microsoft tools built it.

Each entry is a product id, its build number and how many objects it made. For VC6:
prodid 10 is the C compiler, 11 the C++ compiler, 4 the linker, 1 imported objects; build
8168 is VC6 as released (to SP4), 8804 SP5/SP6. RC2's rc2.dll and client.dll both say C/C++
8168, which is why scripts/vc6/ uses VC6 12.00.8168.

Usage: scripts/rich.py <file.dll> [...]
"""
import struct, sys


def rich(path):
    d = open(path, "rb").read()
    e = struct.unpack_from("<I", d, 0x3c)[0]
    end = d.find(b"Rich", 0, e)
    if end < 0:
        return None
    key = struct.unpack_from("<I", d, end + 4)[0]
    start = end - 4
    while start > 0x40 and struct.unpack_from("<I", d, start)[0] ^ key != 0x536e6144:  # "DanS"
        start -= 4
    words = [struct.unpack_from("<I", d, x)[0] ^ key for x in range(start + 16, end, 4)]
    return [(w >> 16, w & 0xffff, c) for w, c in zip(words[0::2], words[1::2])]


for path in sys.argv[1:] or sys.exit(__doc__):
    entries = rich(path)
    print("==", path)
    if entries is None:
        print("  no Rich header")
        continue
    for prodid, build, count in entries:
        print("  prodid %3d  build %5d  count %d" % (prodid, build, count))
