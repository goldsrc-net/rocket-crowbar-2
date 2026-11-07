# mod-rc2

**Mission: a functional, source-faithful reconstruction of the Rocket Crowbar 2 (beta 1.1) server
and client game libraries as modern C++ on the halflife-updated SDK, that builds for every arch the
buildchain targets, loads in the engine, and behaves identically to the original.**

Not started. `../mod-rockcrowbar` (RC 1.9) comes first.

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
| `reference/` | the original beta 1.1 and beta 1 libraries, export dumps, provenance |

Original content: Steven's install at `/media/Data3/Projects/mods/rc2/` (beta 1.1 plus extra maps).

## Status

Scaffold only (2026-11-06).
