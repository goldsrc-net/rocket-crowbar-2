# Audit: halflife-updated's changes inside RC2's stock functions

RC2 has 1,576 functions byte for byte as SDK 2.3 (`reference/names-rc2.tsv`, matched by
`scripts/vc6/match.py` against SDK 2.3 built with RC2's own compiler). So for those functions SDK
2.3's source is the original's behavior. Our tree is a fork of halflife-updated, which changed
many of them. `scripts/stock_drift.py --sdk build-vc6/singleplayer --loose` lists the 400 whose
body still differs from SDK 2.3 after folding halflife-updated's restyling: `sdk-drift.tsv` here
(rva, file, name). Each package `aN` owns the files in `aN.files`.

Decided by Steven, 2026-11-07: read all 400; put SDK 2.3's behavior back wherever it shows in
multiplayer play; keep the rest, documented.

## For each function in your files

```sh
python3 scripts/stock_drift.py --sdk build-vc6/singleplayer --loose --diff   # every diff; take yours
```

SDK 2.3's file is `build-vc6/singleplayer/<same path>`. Read both bodies, not only the diff, and
put each difference in one class:

```
class  what                                                        action
style  same behavior: restyling, bool returns, KeyValue returning   leave
       bool, SetThink(&C::F), helper renames, std::string paths,
       engine API spelling
guard  a null, bounds or sequence check that avoids a crash         keep (RC 1.9's policy: every
                                                                    crash gets guarded)
sp     can't show in multiplayer: save/restore, level transitions,  keep
       singleplayer-only paths, the HL25 singleplayer extras
engine needed by today's engine, client, 64-bit or Linux            keep
play   changes what a multiplayer player or server admin sees,      put SDK 2.3's back
       hears or can do (damage, timing, movement, sounds, messages,
       entity behavior, map logic)
```

A `play` revert is written in the surrounding halflife-updated style (`bool`, `0 !=`, nullptr), with
SDK 2.3's logic, and a comment at the change: `// SDK 2.3's (stock in RC2); halflife-updated <what
it did>`. Keep the function's existing tags (`// orig 0x...`). Where a revert would bring back a
crash, keep the guard and revert only the rest.

Not yours to change, already decided (read `reference/DEVIATIONS.md` in full first):
halflife-updated's message sizes for today's client (`Health`, `ScoreInfo`, ...), its kept cvars and
their readers, `func_vehicle`, and every "Crashes in the original" guard. If one of your functions
touches these, classify it `engine` and point at the DEVIATIONS.md entry.

When it isn't clear whether a change shows in play, say so (`unclear`), keep halflife-updated's,
and give the reason; Steven decides those.

## Rules

- Work only in your worktree `~/Projects/goldsrc.net/mod-rc2-wt/<aN>` and only in your files (a
  shared header only if a revert forces it; say so). No commits: commits are gpg-signed, the merge
  commits.
- Don't edit `reference/DEVIATIONS.md`: put the lines you propose for it in your report (below).
- Style and tags as `reference/dossiers/AGENT_BRIEF.md`.

## Finish line

- `scripts/build.sh i386 all` and `scripts/build.sh amd64` pass (one build at a time; build.sh
  takes a lock).
- `python3 scripts/check_coverage.py` stays 2468 of 2468; `--lib client` stays 1158 of 1466 or
  more.
- `reference/audit/<aN>.md`: one row per function (rva, file, name, class, what halflife-updated
  changed in one line, action); then "For DEVIATIONS.md": one line per kept `guard`, `sp`,
  `engine` or `unclear` change worth a reader's notice (not each style one); then anything you
  found odd.
- A short report back: counts per class, the `play` reverts in a line each, the `unclear` ones.
