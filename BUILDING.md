# Building

The libraries build on Linux in the [goldsrc-net/buildchain](https://github.com/goldsrc-net/buildchain)
Docker image (Debian 10, glibc 2.28), so the result runs on any Linux with glibc 2.28 or newer. Build the
image once from a buildchain checkout:

```sh
( cd ../hlds64-buildchain && make image )      # -> hlds64-buildchain:debian10
```

Then, from this repository:

```sh
scripts/build.sh i386 all    # -> build-i386/dlls/hl.so and build-i386/cl_dll/client.so
scripts/build.sh amd64       # -> build-amd64/dlls/hl.so, for a 64-bit HLDS (the server only)
scripts/build.sh i386 client # the client alone; the second argument is hl (the default), client or all
BUILDTYPE=Debug scripts/build.sh i386 all   # -O0 with debug info
```

`scripts/build.sh` only consumes the image; this repository isn't a buildchain submodule. CMake is the
build system (`dlls/CMakeLists.txt`, `cl_dll/CMakeLists.txt`); RC2's own files in `dlls/rc2/` and
`cl_dll/rc2/` are globbed, so a new file there needs no CMake change. The weapon files in `dlls/` are
compiled into both libraries, as in the SDK.

The server builds and boots on i386 and amd64. The client builds on i386 only: it links Valve's 32-bit
`linux/release/vgui.so`, and there is no 64-bit VGUI in the tree, so a 64-bit client isn't done yet.

## Windows

There is no Windows build of the rebuild yet. The CI workflow inherited from goldsrc-net/halflife-updated
has a Win32 job for `projects/vs2019/projects.sln`, whose projects don't list RC2's own sources, and a
Win64 job that builds the `hl` target with CMake and globs them as the Linux build does, but has never
run. A Windows server can still run the original `rc2.dll` with the original `liblist.gam`.

`scripts/vc6/` builds Half-Life SDK 2.3 with RC2's own compiler (Visual C++ 6, under Wine). Those builds
are for naming the stock functions in the originals; nothing built there ships.
[scripts/vc6/README.md](scripts/vc6/README.md) has the setup.

## Checking a build

```sh
scripts/check_coverage.py                            # every original rc2.dll function has a counterpart
scripts/check_coverage.py --lib client               # the same for client.dll
scripts/check_client_engfuncs.py                     # the client's weapon code calls no engine entry it leaves NULL
scripts/rc2ini_doc.py --ini <RC2's rc2.ini> --check  # docs/RC_INI.md matches the key table and shipped values
scripts/check_docs.py                                # every cvar and command is in docs/SERVER.md
tools/rig/smoke.py ours out.log                      # boot, change maps, quit (tools/rig/README.md)
```

`scripts/` also regenerates every table in `reference/` from the original libraries (see each script's
header, and the parent goldsrc.net folder's `DECOMPILE.md`, section 2a, for the order).
