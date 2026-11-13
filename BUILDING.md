# Building

CMake builds both libraries. The Linux builds run in the
[goldsrc-net/buildchain](https://github.com/goldsrc-net/buildchain) Docker image (Debian 10, glibc 2.28),
so the result runs on any Linux with glibc 2.28 or newer. Build the image once from a buildchain checkout:

```sh
( cd ../hlds64-buildchain && make image )      # -> hlds64-buildchain:debian10
```

Then, from this repository:

```sh
scripts/build.sh i386 all    # -> build-i386/dlls/hl.so and build-i386/cl_dll/client.so
scripts/build.sh amd64       # -> build-amd64/dlls/hl.so, for a 64-bit HLDS (the server only)
scripts/build.sh aarch64     # -> build-aarch64/dlls/hl.so, for a 64-bit ARM server
scripts/build.sh i386 client # the client alone; the second argument is hl (the default), client or all
BUILDTYPE=Debug scripts/build.sh i386 all   # -O0 with debug info
```

`scripts/build.sh` only consumes the image; this repository isn't a buildchain submodule. CMake is the
build system (`dlls/CMakeLists.txt`, `cl_dll/CMakeLists.txt`); RC2's own files in `dlls/rc2/` and
`cl_dll/rc2/` are globbed, so a new file there needs no CMake change. The weapon files in `dlls/` are
compiled into both libraries, as in the SDK.

The server builds for every arch. The client builds for i386 and Win32 only: it links Valve's 32-bit
VGUI (`linux/release/vgui.so`, `utils/vgui/lib/win32_vc16/vgui.lib`), and there is no 64-bit VGUI in the
tree, so a 64-bit client isn't done yet.

## Windows

`scripts/build-win.sh` builds the Win32 server and client on Linux with clang-cl and lld-link, which
give MSVC's ABI (so Valve's win32 `vgui.lib` links), against the MSVC runtime and Windows SDK that
xwin downloads (about 300 MB, once):

```sh
cargo install xwin
xwin --accept-license --arch x86 splat --output ~/.cache/xwin/splat-x86
scripts/build-win.sh       # -> build-win32-clangcl/dlls/hl.dll and build-win32-clangcl/cl_dll/client.dll
```

On Windows, MSVC builds the same from the same CMake files: `cmake -B build -A Win32`, then
`cmake --build build --config Release --target hl --target client` (`-A x64` and `--target hl` for
Win64). CI does that. The Win32 server boots a map in the Windows HLDS
(`tools/rig/boot-package.sh`); the Win32 client has not been run in a game yet.

`scripts/vc6/` builds Half-Life SDK 2.3 with RC2's own compiler (Visual C++ 6, under Wine). Those builds
are for naming the stock functions in the originals; nothing built there ships.
[scripts/vc6/README.md](scripts/vc6/README.md) has the setup.

## Packaging

`scripts/package.sh` builds the i386 and Win32 libraries at the current commit and lays out
`reRC2-<rev>` in `dist/`: a patch for a stock RC2 beta 1.1 install, with the server under RC2's own
name (`rc2/dlls/rc2.dll`, and `rc2.so`, `rc2_i386.so`, `rc2_i686.so` for Linux), the clients
(`rc2/cl_dlls/client.dll`, `client.so`), RC2's `liblist.gam` with a `gamedll_linux` line added (RC2
shipped for Windows only), a MANIFEST.tsv of sha256s, and a zip of the lot. The README.txt in it says
which engine loads which name. It refuses a tree with uncommitted changes, so a package is a commit;
`<rev>` is the `v*` tag from `git describe`, or the commit. The Linux libraries are stripped to their
dynamic exports. `scripts/package.sh --debug` builds the Debug profile instead, not stripped and with
the dlls' PDBs, as `reRC2-<rev>-debug`.

`scripts/assemble-dropin.sh` is the layout step alone, from libraries already built; CI gives it the
64-bit servers as well (`rc2_amd64.so`, `rc2_arm64.so`, `rc2_amd64.dll`, the names Xash builds from
the liblist).

`tools/rig/boot-package.sh win|linux <package>/rc2 <RC2's rc2 folder> <map>` boots a package's server
in the Windows HLDS under Wine or on the 32-bit Linux rig, and passes when it answers a server query.

## CI

`.github/workflows/ci-cd.yml` builds Linux i386 (server and client), amd64 and aarch64 (server) in the
goldsrc-net build image (`ghcr.io/goldsrc-net/build-containers/debian10`, the buildchain's), and Win32
(server and client) and Win64 (server) with MSVC, on every push that touches more than docs. The
package job then runs `scripts/assemble-dropin.sh` on them and uploads `reRC2-<commit>` (the patch,
every arch) and `rc2-gamelibs-all` (the bare libraries). Run it by hand (Actions, CI/CD, Run workflow)
with debug ticked to get the Debug package too.

`.github/workflows/release.yml`: pushing a tag (`git tag v0.1.0 && git push origin v0.1.0`) builds that
commit through ci-cd.yml and creates a GitHub release with `reRC2-<tag>.zip`; a tag with a `-` is a
prerelease. `cmake/Version.cmake` stamps the tag, or `git describe`, into the Windows version
resources.

## Checking a build

```sh
scripts/check_coverage.py                            # every original rc2.dll function has a counterpart
scripts/check_coverage.py --lib client               # the same for client.dll
scripts/check_client_engfuncs.py                     # the client's weapon code calls no engine entry it leaves NULL
scripts/rc2ini_doc.py --ini <RC2's rc2.ini> --check  # docs/RC_INI.md matches the key table and shipped values
scripts/check_docs.py                                # every cvar and command is in docs/SERVER.md
scripts/check_clientdata.py                          # UpdateClientData sets every field upstream's does
tools/rig/smoke.py ours out.log                      # boot, change maps, quit (tools/rig/README.md)
```

`scripts/` also regenerates every table in `reference/` from the original libraries (see each script's
header, and the parent goldsrc.net folder's `DECOMPILE.md`, section 2a, for the order).
