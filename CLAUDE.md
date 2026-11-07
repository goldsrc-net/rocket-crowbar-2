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
(`scripts/rich.py`). rc2.dll: 2,738 of 3,807 functions named (SDK 2.3 matched byte for byte,
then stock virtuals by vtable position); client.dll: 2,450 of 4,276. Tables in `reference/`:
`vtables-rc2.tsv` (292 entity classes, size, vtable), `savefields-rc2.tsv` (57 save tables, all
stock), `cvars-rc2.tsv` (278, 11 RC2's own; `mp_allowmonsters` and `mp_falldamage` gone).

RC2 added virtuals to the SDK bases, which come first, as in mod-rockcrowbar: one between
`CBaseEntity::Respawn` and `UpdateOwner` and 21 at the end of `CBaseEntity`, one at the end
of `CBaseMonster`, four after `CBasePlayerItem::AttachToPlayer`. Each factory also calls
RC 1.9's `AddToPrecachedList`, so code is shared with RC 1.9 beyond `CCrowbarRocket`.

The work list is `reference/RECONSTRUCTION-rc2.md` (`scripts/inventory.py`): 881 functions
(296 KB) of RC2's own code between SDK files and 920 (278 KB) changed inside them; the
decompile is `recovered/decomp/rc2/` (`scripts/ghidra/re.sh export rc2`, about a minute).
Next: name the added base virtuals (`reference/BASE_VIRTUALS.md` has what is known of each),
then client.dll's vtables (its weapons are static objects).

- The amd64 client doesn't link yet (no 64-bit VGUI; mod-ts links openvgui statically). The
  Half-Life client players run is 32-bit, so it waits.
- The originals run only on Windows; the Windows HLDS under Wine at
  `/media/Data3/Projects/goldsrc.net/hlds-win` is where to run them.
