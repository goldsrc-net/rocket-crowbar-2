# Rocket Crowbar 2 (RE)

A fan-made reconstruction of Rocket Crowbar 2 beta 1.1, the 2004 Half-Life deathmatch mod, as C++
source on the Half-Life SDK. It rebuilds both of the mod's libraries, the server's `rc2.dll` and the
client's `client.dll`, from RC2's shipped binaries, so that they drop into a stock RC2 install and play
the same: the rocket crowbar and crowbarang, forklifts and shoulder riding, racing, the player
transforms and attribute point-buy, the shield, the grenades and mines, the nuclear missile, the
per-map `.lcs` files and the rc2.ini prefs. Unlike RC 1.9, RC2 has its own client library, so players
need the rebuilt client as well.

This reconstruction is not affiliated with or endorsed by Rocket Crowbar 2's authors. Rocket Crowbar 2,
its name, maps, models, sounds and configs are theirs; this replaces only the code. See
[CREDITS.md](CREDITS.md).

## Installing

The rebuild is a server library and a client library for a stock RC2 beta 1.1 install, packaged as
`reRC2-<rev>`: its `rc2/` folder goes over the install's, with the server under RC2's own name
(`rc2.dll`, and `rc2_i386.so` with its aliases for Linux), the Windows and Linux clients, the 64-bit
servers, and RC2's `liblist.gam` with the line a Linux server needs. [INSTALL.md](INSTALL.md) covers
servers, players, the test setup that runs the original and the rebuild side by side, and going back.

## Servers

The rebuild reads RC2's cvars, rc2.ini and per-map `maps/lcs/*.lcs` files unchanged; its admin files
take Steam IDs where RC2's took WON ids, which no engine issues now.
[docs/SERVER.md](docs/SERVER.md) lists every cvar, command, admin command, vote and file RC2 adds to
Half-Life, [docs/GAMEPLAY.md](docs/GAMEPLAY.md) what RC2 changes in play, and
[docs/RC_INI.md](docs/RC_INI.md) every rc2.ini key with its default. Where it deliberately does
something RC2 does not, it is listed in [reference/DEVIATIONS.md](reference/DEVIATIONS.md); the bugs
found in the original, and which ones it keeps, are in
[reference/ORIGINAL_BUGS.md](reference/ORIGINAL_BUGS.md).

## Building

The Linux libraries build in the [goldsrc-net/buildchain](https://github.com/goldsrc-net/buildchain)
Docker image (`scripts/build.sh i386 all` for the server and client, `amd64` or `aarch64` for a 64-bit
server), the Win32 ones with clang-cl on Linux (`scripts/build-win.sh`) or MSVC, and
`scripts/package.sh` packages them. CI builds every arch and the package on each push.
[BUILDING.md](BUILDING.md) has the steps.

## How it was made

RC2 never had a Linux build, so unlike RC 1.9 there is no library with symbols to read. Half-Life SDK
2.3 built with RC2's own compiler (Visual C++ 6) gives byte-identical copies of the stock functions RC2
didn't change, which names them in the originals; vtable positions and the save tables name more, and
RC2's own functions have names of ours. Both libraries were decompiled with Ghidra and rewritten on top
of [Half-Life Updated](https://github.com/twhl-community/halflife-updated), then checked by building,
running and comparing against the original, which runs under Wine in the Windows dedicated server.
`reference/` holds the originals and the derived inventories; `scripts/` builds and checks them, and
`tools/rig/` runs the two side by side.

## License

The rebuild is built on the Half-Life SDK and is distributed under its license ([LICENSE](LICENSE)): free
of charge only. Half-Life Updated's own changes are in [CHANGELOG.md](CHANGELOG.md) and
[FULL_UPDATED_CHANGELOG.md](FULL_UPDATED_CHANGELOG.md), and its contributors in [CREDITS.md](CREDITS.md).
