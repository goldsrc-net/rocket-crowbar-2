# Deviations from Rocket Crowbar 2 beta 1.1

Everything this rebuild does differently from the original `rc2.dll` and `client.dll`, on
purpose or forced by the build, with what it changes. Same convention as mod-rockcrowbar's
`reference/DEVIATIONS.md`.

## Cvars

The original's set is `reference/cvars-rc2.tsv` (`scripts/cvars.py`).

- **Cvars the original does not have, kept from halflife-updated:** `allow_spectators`,
  `sk_plr_hornet_dmg1`-`3`, `sv_allowbunnyhopping`, `sv_busters`, `sv_pushable_fixed_tick_fudge`.
  Every other cvar is the original's, registered in its order (`GameDLLInit`, orig 0x470a0). At
  their defaults none changes anything: nothing in our game code reads `allow_spectators`,
  `sv_busters` or `sv_pushable_fixed_tick_fudge`; `PM_PreventMegaBunnyJumping` runs
  unconditionally, as in SDK 2.3, so `sv_allowbunnyhopping` doesn't either; and
  `CGameRules::RefreshSkillData` (stock in RC2, where SDK 2.3 sets the player hornet damage to 7)
  reads `sk_plr_hornet_dmg`, which `CHalfLifeMultiplay::RefreshSkillData` then overwrites with 10, as
  the original does. What does show: RC2's `skill.cfg` doesn't set `sk_plr_hornet_dmg`, so our
  server prints `** GetSkillCVar Got a zero for sk_plr_hornet_dmg1 **` on every map load
  (`tools/rig/smoke.py` finds no other console difference).

## Client

- **The controller charge ball's trails (CLTE 0x49) leak without the bad delete.** The original
  (client 0x95e50 Think, 0x971c0 EntityGone) frees trail 0, then calls plain `operator delete` on
  the `new[]` array's pointer (block + 4). MSVC6's release heap rejects that silently and the
  trails leak; glibc would abort. We leave the delete out and keep the leak
  (`cl_dll/rc2/tcontroller_tents.cpp`, `reference/dossiers/c3.md`).
