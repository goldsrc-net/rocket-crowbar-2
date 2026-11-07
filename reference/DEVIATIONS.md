# Deviations from Rocket Crowbar 2 beta 1.1

Everything this rebuild does differently from the original `rc2.dll` and `client.dll`, on
purpose or forced by the build, with what it changes. Same convention as mod-rockcrowbar's
`reference/DEVIATIONS.md`.

## Cvars

The original's set is `reference/cvars-rc2.tsv` (`scripts/cvars.py`).

- **`mp_allowmonsters` and `mp_falldamage` are still registered.** RC2 has neither. The SDK
  code that reads them (the multiplayer rules' FAllowMonsters and FlPlayerFallDamage) is among
  the functions RC2 changed, and they go when that code is reconstructed.
- **Cvars kept from halflife-updated** that SDK 2.3 and RC2 don't have are listed by
  `scripts/cvars.py` against our build once the work list reaches `game.cpp`.
