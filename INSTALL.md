# Installing

The rebuild replaces two libraries of a Rocket Crowbar 2 beta 1.1 install, its server and its client,
and the manifest that points at them. Everything else (rc2.ini, the maps, models, sounds, sprites and
events) is RC2's own and stays as it is. Both come in a `reRC2-<rev>` package (CI's artifact or
release, or `scripts/package.sh`, [BUILDING.md](BUILDING.md)), whose `rc2/` folder goes over the
install's, on servers and players alike.

## A server

1. Install RC2 beta 1.1 on the server as `rc2/` next to `valve/` (the installer's `rc2` folder).
2. Copy the package's `rc2/` folder over it. It holds the server under RC2's own name, `dlls/rc2.dll`
   for Windows and `rc2.so`, `rc2_i386.so` and `rc2_i686.so` for Linux (Steam's current engine loads
   `rc2.so`, older ones `rc2_i386.so`, Metamod-R `rc2_i686.so`), the 64-bit servers, the clients, and
   RC2's `liblist.gam` with `gamedll_linux "dlls/rc2_i386.so"` added (RC2's names only
   `gamedll "dlls\rc2.dll"`).
3. Start the server with `-game rc2`, e.g. `./hlds_linux -game rc2 +deathmatch 1 +map rc_sandcrab`.

Admin levels in `adminprim.cfg` and `sv_tempadmins` take Steam IDs (`STEAM_0:1:2`) where RC2's took WON
ids ([reference/DEVIATIONS.md](reference/DEVIATIONS.md)), so a server whose admins should work runs
with `sv_lan 0`; `-nomaster` keeps it off Valve's server list.

Going back: put RC2's own `rc2.dll`, `client.dll` and `liblist.gam` back, and delete `rc2.so`,
`rc2_i386.so`, `rc2_i686.so`, `client.so` and the 64-bit servers, which RC2 never had. The original
only runs on a Windows server.

## A player

RC2 has its own client library, so players install the mod too:

1. Install RC2 beta 1.1 as `rc2/` in the Half-Life folder.
2. Copy the package's `rc2/` folder over it: `cl_dlls/client.so` is the Linux client,
   `cl_dlls/client.dll` the Windows one.
3. Join with `steam -applaunch 70 -game rc2 +connect <server>`.

The client is 32-bit, as Half-Life's is. The Linux client can't load RC2's Windows `client.dll`, so on
Linux the rebuilt client is the only one; the side-by-side setup below uses it against both the
original and the rebuilt server. The rebuilt Windows client has been built but not yet played.

## Running the original and the rebuild side by side

`tools/rig/setup.sh` sets up two servers on one machine, the original under Wine in the Windows HLDS on
port 27031 and the rebuild in a 32-bit Linux HLDS on 27032, both as `rc2`, and the client's mod folder;
`tools/rig/run.sh original|ours` starts them. Join with:

```
steam -applaunch 70 -game rc2 +connect 127.0.0.1:<port>
```

[tools/rig/README.md](tools/rig/README.md) has the details and how to undo it.
