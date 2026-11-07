# Deviations from Rocket Crowbar 2 beta 1.1

Everything this rebuild does differently from the original `rc2.dll` and `client.dll`, on
purpose or forced by the build, with what it changes. Same convention as mod-rockcrowbar's
`reference/DEVIATIONS.md`.

## Cvars

The original's set is `reference/cvars-rc2.tsv` (`scripts/cvars.py`).

- **Cvars kept from halflife-updated** that SDK 2.3 and RC2 don't have are listed by
  `scripts/cvars.py` against our build once the work list reaches `game.cpp`.

## Client

- **The controller charge ball's trails (CLTE 0x49) leak without the bad delete.** The original
  (client 0x95e50 Think, 0x971c0 EntityGone) frees trail 0, then calls plain `operator delete` on
  the `new[]` array's pointer (block + 4). MSVC6's release heap rejects that silently and the
  trails leak; glibc would abort. We leave the delete out and keep the leak
  (`cl_dll/rc2/tcontroller_tents.cpp`, `reference/dossiers/c3.md`).
