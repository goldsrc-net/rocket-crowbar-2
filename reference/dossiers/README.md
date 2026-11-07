# Dossiers

One per reconstruction work package (a set of the original's source files), written by whoever
reconstructed it. A dossier is the record a later session needs to trust, check or continue the
work without redoing the RE. Keep it factual: what was done, how it was checked, what is open.

## Sections

1. **Scope.** The original source files, and the files written or touched here.
2. **Status.** `scripts/check_coverage.py <file> --missing` result per file (built = compiles, links,
   every original function has its orig counterpart). Nothing is "done" until its behavior is checked in an engine
   against the original; say which.
3. **Function map.** Every original function in scope: original address, our function (file),
   and `ok` or `VERIFY` with a short reason.
4. **Classes.** Each class: base (from `hierarchy.tsv`), members with the original's offsets,
   which came from the save table and which were recovered from code (and how the name was chosen).
5. **Cross-file needs.** Anything used from outside the package: SDK functions RC changed, other RC
   files' functions, user messages (`gmsg*`), members added to shared classes, temporary stubs and
   where they are. These are the merge work.
6. **VERIFY list.** Every uncertain spot, with what would settle it (a disassembly read, a runtime
   test, another file's reconstruction).
7. **Behavior worth testing.** What to try in-game against the original once it can run.
8. **Original bugs.** Each bug or oddity of the original you found: where, what it does, and
   whether you kept it or changed it. The coordinator copies them into `reference/ORIGINAL_BUGS.md`.
