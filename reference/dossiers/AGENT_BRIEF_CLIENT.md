# Brief addendum: reconstructing a Rocket Crowbar 2 client package

Read `AGENT_BRIEF.md` first; everything there applies. This is what differs for client.dll,
RC2's client library (VC6, stripped, built 2004-01-11). The server (rc2.dll) is already
reconstructed in this tree (`dlls/`), so the client can read what the server sends.

## Where things are

- **Your functions:** `reference/files-client.tsv`, `python3 scripts/check_coverage.py --lib
  client [<file label> --missing]`. Your package's `.text` range is in the prompt and
  `reference/dossiers/PACKAGES-client.md`.
- **The decompile:** `~/Projects/goldsrc.net/mod-rc2/recovered/decomp/client/` (read-only).
- **Tables:** `names-client.tsv` (byte-identical to SDK 2.3's client: stock),
  `names-client-slots.tsv` (stock virtuals by vtable position: RC2's bodies), the exports
  `exports-client.raw` / `signatures-client.txt` (VGUI templates and the client entry points).
- **SDK 2.3's client source:** `~/Projects/goldsrc.net/mod-rc2/build-vc6/singleplayer/cl_dll/`
  (pristine), built with VC6 as `build-vc6/client/` (`cl_dll.map` names every function).

## What is different

- **The tag is `// orig client 0x<rva>`.** Files compiled into both libraries (the weapons,
  `pm_shared`) would make a plain `orig` ambiguous; check_coverage --lib client counts only this
  form. Never write `orig client 0x` for anything but a reconstructed client function.
- **client.dll was linked incrementally.** The start of `.text` is a table of 5-byte `jmp`
  thunks; every call and every vtable slot goes through one. Follow the `jmp` to the function.
  The thunks themselves are the linker's: nothing to reconstruct.
- **RC2's own client files go in `cl_dll/rc2/`** (globbed by `cl_dll/CMakeLists.txt`). Changed
  stock functions stay in their SDK file in `cl_dll/`.
- **The server side is done:** the user messages RC2 sends are registered in
  `dlls/UserMessages.cpp` (names, sizes); `dlls/rc2prefs.*` is rc2.ini, whose NETWORK_SYNC keys
  the server sends in the RCCFG message for the client to keep its own copy; the weapon classes in
  `dlls/` are what the client's prediction compiles (`#ifdef CLIENT_DLL` parts are the client's).
  Read the server's sending side when you reconstruct a message handler.
- **RC2's client works with the Half-Life engine of 2004**, through the stock `cl_enginefuncs`
  interface; halflife-updated's client in this tree runs on today's engine. Where they differ,
  keep halflife-updated's engine interface and reconstruct RC2's behavior on it; say so in the
  dossier.
- **VGUI 1** (`utils/vgui/include`): RC2 adds its own panels (menus, the attribute and
  menuconfig screens); halflife-updated's VGUI code is the base.

## Checking

```sh
scripts/build.sh i386 client      # the client is 32-bit; amd64 has no VGUI yet
scripts/build.sh i386             # the server must still build (shared headers)
python3 scripts/check_coverage.py --lib client <file> --missing
```
