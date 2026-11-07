# scripts/vc6 - Visual C++ 6 reference builds

RC2's `rc2.dll` and `client.dll` carry no symbols. Their Rich headers say both were compiled
with VC6's C/C++ compiler 12.00.8168, the build Visual Studio 6.0 shipped with (service packs
1 to 4 kept it; SP5 moved to 8804). Half-Life SDK 2.3 built with that same compiler and the
flags of its own project files gives byte-identical copies of every stock function RC2 didn't
change, and that is how those functions get their names in the originals. These builds are
for reading the originals only; nothing built here ships.

Measured 2026-11-07, a function counted only when its whole body matches with relocations
masked:

```
library      SDK 2.3 functions   found in RC2's library
rc2.dll      3058 (hl.dsp)       1391 (45%)
client.dll   1542 (cl_dll.dsp)    775 (50%)
```

## Setting up the compiler

Visual Studio 6.0 Enterprise, CD 1, from archive.org item `en_vs6_ent`
(`en_vs6_ent_cd1.iso`, SHA-1 `25e382cabcf6c648a327db3b90ff38962bfc0e84`). Only `VC98/` and
`MSPDB60.DLL` are used; nothing is installed.

```sh
mkdir -p /media/Data3/Software/vc6 && cd /media/Data3/Software/vc6
curl -L -o en_vs6_ent_cd1.iso https://archive.org/download/en_vs6_ent/en_vs6_ent_cd1.iso
echo "25e382cabcf6c648a327db3b90ff38962bfc0e84  en_vs6_ent_cd1.iso" | sha1sum -c
7z x -o. en_vs6_ent_cd1.iso 'VC98/BIN/*' 'VC98/INCLUDE/*' 'VC98/LIB/*' 'COMMON/MSDEV98/BIN/MSPDB60.DLL'
cp COMMON/MSDEV98/BIN/MSPDB60.DLL VC98/BIN/
WINEPREFIX=$PWD/wineprefix wineboot -i
WINEPREFIX=$PWD/wineprefix wine VC98/BIN/CL.EXE      # prints "Version 12.00.8168 for 80x86"
```

Undo: delete `/media/Data3/Software/vc6`.

## Building SDK 2.3

The pristine SDK 2.3 is the first commit of goldsrc-net/hlsdk (`c026655`). RC2's PDB path,
`...\dlls\Releasehl\hl.pdb`, is the Release configuration of the singleplayer `hl.dsp`, so that
is the server project used.

```sh
mkdir -p build-vc6
git -C ../hlds64-buildchain/hlsdk archive c026655 singleplayer | tar -x -C build-vc6
scripts/vc6/build_dsp.py build-vc6/singleplayer/dlls   hl.dsp     build-vc6/hl      # ~6 min
scripts/vc6/build_dsp.py build-vc6/singleplayer/cl_dll cl_dll.dsp build-vc6/client  # ~5 min
scripts/vc6/rc2_layout.py build-vc6/singleplayer build-vc6/layout-rc2 --entity-pad b
scripts/vc6/build_dsp.py build-vc6/layout-rc2/dlls hl.dsp build-vc6/hl-rc2           # the reference the tables use
```

Each writes the `.dll`, its `.map` (every function's name and address) and a `.pdb`.
`build-vc6/` is gitignored.
