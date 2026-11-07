# Installing

The rebuild replaces two files of a Rocket Crowbar 2 beta 1.1 install, its server library and its client
library, and the manifest that points at them. Everything else (rc2.ini, the maps, models, sounds,
sprites and events) is RC2's own and stays as it is.
Both libraries are Linux builds; there is no Windows build of the rebuild yet (BUILDING.md).

## A server

1. Install RC2 beta 1.1 on the server as `rc2/` next to `valve/` (the installer's `rc2` folder).
2. Copy the rebuild's `hl.so` (`build-i386/dlls/hl.so` for a 32-bit HLDS, `build-amd64/dlls/hl.so` for a
   64-bit one) to `rc2/dlls/hl.so`.
3. Replace `rc2/liblist.gam` with this repository's [game/rc2/liblist.gam](game/rc2/liblist.gam), which
   adds `gamedll_linux "dlls/hl.so"` (RC2's has only `gamedll "dlls\rc2.dll"`).
4. Start the server with `-game rc2`, e.g. `./hlds_linux -game rc2 +deathmatch 1 +map rc_sandcrab`.

Admin levels in `adminprim.cfg` and `sv_tempadmins` take Steam IDs (`STEAM_0:1:2`) where RC2's took WON
ids ([reference/DEVIATIONS.md](reference/DEVIATIONS.md)), so a server whose admins should work runs
with `sv_lan 0`; `-nomaster` keeps it off Valve's server list.

Going back: put RC2's own `liblist.gam` back. The original only runs on a Windows server.

## A player

RC2 has its own client library, so players install the mod too:

1. Install RC2 beta 1.1 as `rc2/` in the Half-Life folder.
2. Copy the rebuild's `build-i386/cl_dll/client.so` to `rc2/cl_dlls/client.so`, and use this
   repository's `liblist.gam` as above.
3. Join with `steam -applaunch 70 -game rc2 +connect <server>`.

The client is 32-bit, as Half-Life's Linux client is. The Linux client can't load RC2's Windows
`client.dll`, so on Linux the rebuilt client is the only one; the side-by-side setup below uses it
against both the original and the rebuilt server.

## Running the original and the rebuild side by side

`tools/rig/setup.sh` sets up two servers on one machine, the original under Wine in the Windows HLDS on
port 27031 and the rebuild in a 32-bit Linux HLDS on 27032, both as `rc2`, and the client's mod folder;
`tools/rig/run.sh original|ours` starts them. Join with:

```
steam -applaunch 70 -game rc2 +connect 127.0.0.1:<port>
```

[tools/rig/README.md](tools/rig/README.md) has the details and how to undo it.
