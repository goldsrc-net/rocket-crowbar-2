# mod-rc2

**Mission: a functional, source-faithful reconstruction of the Rocket Crowbar 2 (beta 1.1) server
and client game libraries as modern C++ on the halflife-updated SDK, that builds for every arch the
buildchain targets, loads in the engine, and behaves identically to the original.**

RC2 (January 2004) is a near-total rewrite of RC 1.9: of each version's 48 custom classes with
exported callbacks, only `CCrowbarRocket` is shared. RC2 adds the forklifts, racing, a new grenade
and mine set, shotgun scientists and HEV suits, the crowbarang and the nuclear missile, and ships
its own client library.

## Method

Same as `../mod-ts` (reTS) and `../mod-rockcrowbar`: behavior parity on a modern compiler, with the
external interface (entities, keyvalues, cvars, commands, user messages, events, `rc2.ini` and the
`maps/lcs/*.lcs` configs) as the swap contract. Harder than either: there is no Linux build, so no
symbol table, and the originals only run under Windows or Wine. The names to start from are the
SDK's exports and save/restore field names (`reference/README.md`); stock SDK functions can be
named by Ghidra Function ID signatures from an MSVC build of the SDK.

## This repo

A fork of `goldsrc-net/halflife-updated` branch `64bit-demo`, like mod-ts.

- `upstream` = `git@github.com:goldsrc-net/halflife-updated.git`, tracked by `main`.
- No `origin` yet. When it gets one it is a private goldsrc-net repo (`../REPOS.md`).

| Path | What |
|---|---|
| `game/rc2/` | the mod's gamedir manifest (`liblist.gam`) |
| `reference/` | the original beta 1.1 and beta 1 libraries, export dumps, provenance; `names-<lib>.tsv` (stock functions named from the VC6 build) |
| `scripts/vc6/` | VC6 reference builds of SDK 2.3 (`build_dsp.py`, under Wine) and `match.py`; `README.md` has the setup |
| `scripts/ghidra/` | `re.sh` (one project, both libraries, in `recovered/ghidra/`, gitignored), `ApplyNames.java` |
| `scripts/build.sh` | the buildchain build (i386 hl + client, amd64 hl) |

Original content: Steven's install at `/media/Data3/Projects/mods/rc2/` (beta 1.1 plus extra maps).

## Status (2026-11-07)

Naming the originals (`../DECOMPILE.md` section 2a). Both were built with VC6 12.00.8168
(`scripts/rich.py`). SDK 2.3 built with that compiler names 1,013 functions of `rc2.dll` and
591 of `client.dll` byte for byte; after the Ghidra setup 1,618 of 3,030 and 2,450 of 4,276
have a name. Next: vtables from the 301 entity factories, then the call graph, then the work
list and the tables (cvars, save fields) as mod-rockcrowbar has them.

- The amd64 client doesn't link yet (no 64-bit VGUI; mod-ts links openvgui statically). The
  Half-Life client players run is 32-bit, so it waits.
- The originals run only on Windows; the Windows HLDS under Wine at
  `/media/Data3/Projects/goldsrc.net/hlds-win` is where to run them.
