# Brief: reconstructing a Rocket Crowbar 2 work package

You are rebuilding part of the original Rocket Crowbar 2 beta 1.1 server game library
(`rc2.dll`, 2004, Visual C++ 6, stripped) as modern C++ on the halflife-updated SDK. Your
package is a set of the original's source files; other agents do the other files in parallel,
each in its own git worktree. The bar is identical behavior. Exact binary layout is NOT a goal.
The same was done for Rocket Crowbar 1.9 in `~/Projects/goldsrc.net/mod-rockcrowbar`: its
`dlls/` is a good model for style, and RC2 shares code with it (its LINK_ENTITY_TO_CLASS still
calls RC 1.9's AddToPrecachedList; CCrowbarRocket is in both).

## Where things are

- **Your worktree** (edit only here): `~/Projects/goldsrc.net/mod-rc2-wt/<package>`, branch
  `wt/<package>`. The main checkout is `~/Projects/goldsrc.net/mod-rc2`; don't edit it.
- **Read first, in full:** the worktree's `CLAUDE.md`, `reference/README.md`,
  `reference/DEVIATIONS.md`, `reference/BASE_VIRTUALS.md`, `reference/dossiers/README.md` (the
  dossier format), and your files' lines of `reference/RECONSTRUCTION-rc2.md`.
- **The decompile** (read-only, shared): `~/Projects/goldsrc.net/mod-rc2/recovered/decomp/rc2/`,
  one `<Class>.c` per class, unnamed functions in `_free.c` as `FUN_<address>`. Each function is
  headed with its address and size. Do NOT run Ghidra (`scripts/ghidra/re.sh`): one project, one
  lock, many agents.
- **Your functions:** `reference/files-rc2.tsv` lists every function with its RVA, size, Ghidra
  name and the source file it belongs to (by link order). `python3 scripts/check_coverage.py
  <file> --missing` lists the ones of a file that still need reconstructing. A function is yours
  when it is in your files there.
- **Tables** in `reference/`:
  - `names-rc2.tsv`: functions byte-identical to SDK 2.3 (stock; nothing to do).
  - `names-rc2-slots.tsv`: stock virtuals named by vtable position. These have the SDK name but
    RC2's body: reconstruct them.
  - `vtables-rc2.tsv`: every entity class, its factory, size and vtable slots.
  - `savefields-rc2.tsv`: save tables, with real member names and the original's offsets.
  - `prefs-rc2.tsv`: rc2.ini's 1,113 keys.
  - `cvars-rc2.tsv`, `exports-rc2.raw` / `signatures-rc2.txt`: the EXPORT think/touch/use
    methods, with their real class and method names and full signatures.
- **SDK 2.3**, which RC2 was built on: `~/Projects/goldsrc.net/mod-rc2/build-vc6/singleplayer/`
  (pristine). When RC2 changed a stock function, diff your reading against it.
- **The binary** when the decompile is unclear:
  `objdump -d --no-show-raw-insn --start-address=0x<VA> --stop-address=0x<VA> reference/rc2.dll`
  (VA = RVA + 0x10000000), or capstone through `scripts/pe.py`.

## Reading the decompile

- MSVC layout: the vtable pointer is at `+0`, `pev` at `+4`. A call `(**(code **)(*p + o))(...)`
  is vtable slot `o/4` of `p`'s class: `vtables-rc2.tsv` has every slot. Ghidra prints some
  offsets in decimal (`+ 300` is 0x12c).
- `DAT_1018c504` is `g_pGameRules`; calls through it are CGameRules virtuals. `DAT_1018c798` is
  `gpGlobals`. `(*DAT_1018c6xx)(...)` are `g_engfuncs` slots: write the SDK macro.
- RC2's added base virtuals have their slots and our names in `reference/BASE_VIRTUALS.md` and
  `dlls/cbase.h`.
- `fld dword ptr [0x10191bc8 + 8*i]` reads rc2.ini key `i`: write `RC2Pref(RC2P_<Heading>_<Key>)`
  (`dlls/rc2prefs.h`, `prefs-rc2.tsv` maps the address to the key).
- RC2 added members to the base classes. Against SDK 2.3: CBaseEntity has 28 more bytes, so
  every stock member of CBaseDelay and below sits 28 (0x1c) further on; CBaseMonster 4 more (32
  in total for its subclasses), CTalkMonster 4 more before `m_useTime`, CBasePlayerWeapon 8 more
  before `m_flTimeWeaponIdle`, CBasePlayer about 2.5 KB after its stock members. Subtract the
  shift to find the stock member an offset means.
- `CONCAT31(extraout_var, b)` is a bool or byte return; take the low byte. Ghidra sometimes loses
  a function's returns or splits one; read the disassembly then, don't guess.

## Names

rc2.dll keeps names only for classes (from the exported methods), the EXPORT methods themselves,
saved members, entity classnames, strings, cvars and rc2.ini keys. Use those everywhere, typos
included. Everything else (unexported methods, unsaved members, globals, free functions) has no
original name: name it from what it does, in RC 1.9's style, and if RC 1.9 has the same thing
(`../mod-rockcrowbar/dlls/`), use RC 1.9's name. Mark each name of ours `// name ours` the first
time it appears.

## How to write it

- Same file names as the original (your package's `.cpp` files exist as placeholders, already in
  `dlls/CMakeLists.txt`). For a file of RC2's own the name is ours too: the brief for your package
  gives it.
- Modern SDK style, matching the surrounding halflife-updated code: `bool` returns, `override`,
  `SetThink`/`SetTouch`, `pev->`, `LINK_ENTITY_TO_CLASS`, `IMPLEMENT_SAVERESTORE` with the
  original's fields and types (`savefields-rc2.tsv`).
- Every function gets a one-line provenance comment with its RVA, `// orig 0x<rva>` (as in
  files-rc2.tsv). That comment is what `check_coverage.py` counts, so write `orig 0x` for nothing
  else. Mark every uncertain spot `// VERIFY: <why>`.
- A stock function RC2 changed: start from halflife-updated's version in this tree and make it do
  what RC2's does. Keep halflife-updated's fixes unless RC2's behavior differs; say which in the
  dossier.
- Keep the original's bugs (its crashes too, unless told otherwise) and list each in the dossier's
  "Original bugs" section.
- Never invent behavior. If the decompile and the disassembly don't settle it, write the most
  literal reading, mark it VERIFY, and put it in the dossier.
- Already in the tree (don't redo): RC2's 11 cvars (`game.h`), the 22 virtuals RC2 added to
  `CBaseEntity` (`cbase.h`), the rc2.ini table and `RC2Pref()` (`rc2prefs.h`).

## Shared files

You own only your package's files and any new header you create. Do not touch `CMakeLists.txt`,
`game.cpp`, `cbase.h`/`cbase.cpp` outside a marked block, or other packages' files. When you need
something that isn't there:

- **A member or virtual on a shared class** (CBaseEntity, CBaseMonster, CBasePlayer,
  CBasePlayerItem, ...): add it in a block `// <package> needs:` right before the class's closing
  `};`, with its original offset or slot, and list it in the dossier.
- **A function, global or user message from elsewhere**: declare it `extern` in your file and, if
  needed to link, give it a temporary definition at the bottom of your own `.cpp` in a block marked
  `// TEMP STUB (<package>): real one belongs in <file>`. A stub never carries `orig 0x`. List each
  in the dossier.

## Checking

```sh
cd ~/Projects/goldsrc.net/mod-rc2-wt/<package>
scripts/build.sh i386 all          # Docker; a few minutes; server and client
scripts/build.sh amd64
python3 scripts/check_coverage.py <file> --missing   # must reach 0 missing, or explain each
nm -D build-i386/dlls/hl.so | grep -w <classname>    # each entity factory exported
```

i386 (server and client) and amd64 (server) must build. Several agents share this machine: one
build at a time. Nothing runs in an engine yet, so "built" is the most you can claim.

## Finishing

- **Don't commit** (commits are gpg-signed by Steven's key and would prompt him). Leave your
  changes uncommitted in the worktree.
- Write `reference/dossiers/<package>.md` in the worktree, in the format of the dossier README.
- Your final message: the worktree path, check_coverage results per file, the build results, the
  shared-file edits and stubs you made, and the VERIFY count. Keep it short; the dossier has the
  detail.
