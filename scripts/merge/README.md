# Merge helpers

For merging a package's worktree branch into main, when packages each wrote stubs or their own
signatures for something another package owns. Run them from anywhere; `<repo>` is the checkout
being merged into.

| Script | What it does |
|---|---|
| `stubmap.py <repo>` | lists TEMP STUBs whose address now has a real `// orig`-tagged function, with both names and files |
| `dropstub.py <file> <symbol>...` | removes a TEMP STUB definition (function or variable) and its TEMP STUB comment, leaving a note pointing at the real one; it matches definitions only, so check the diff anyway |
| `dedupe.py <repo> <build log>` | for each symbol the build log says is defined twice, drops the copy that is a TEMP STUB; prints the cases it can't decide |
| `fix_rccaps.sh <repo>` | normalizes `GetRCCaps`/`GetRCCaps2` to `(CBaseEntity* pOther, CBaseEntity* pAsker)` and removes old int casts (an int-typed pointer truncates on amd64) |

After any of them: `scripts/build.sh i386`, `scripts/build.sh amd64`, `scripts/build.sh i386 client`,
then `scripts/check_coverage.py` to make sure the count didn't drop.
