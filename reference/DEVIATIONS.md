# Deviations from Rocket Crowbar 2 beta 1.1

Everything this rebuild does differently from the original `rc2.dll` and `client.dll`, on
purpose or forced by the build, with what it changes. Same convention as mod-rockcrowbar's
`reference/DEVIATIONS.md`.

## Cvars

The original's set is `reference/cvars-rc2.tsv` (`scripts/cvars.py`).

- **Cvars the original does not have, kept from halflife-updated:** `allow_spectators`,
  `sk_plr_hornet_dmg1`-`3`, `sv_allowbunnyhopping`, `sv_busters`, `sv_pushable_fixed_tick_fudge`.
  Every other cvar is the original's, registered in its order (`GameDLLInit`, orig 0x470a0). At
  their defaults none changes anything: nothing in our game code reads `allow_spectators` or
  `sv_pushable_fixed_tick_fudge`; `sv_busters` 1 would install halflife-updated's busters rules
  (`InstallGameRules`), and its default is 0; `PM_PreventMegaBunnyJumping` runs
  unconditionally, as in SDK 2.3, so `sv_allowbunnyhopping` doesn't either; and
  `CGameRules::RefreshSkillData` (stock in RC2, where SDK 2.3 sets the player hornet damage to 7)
  reads `sk_plr_hornet_dmg`, which `CHalfLifeMultiplay::RefreshSkillData` then overwrites with 10, as
  the original does. What does show: RC2's `skill.cfg` doesn't set `sk_plr_hornet_dmg`, so our
  server prints `** GetSkillCVar Got a zero for sk_plr_hornet_dmg1 **` on every map load
  (`tools/rig/smoke.py` finds no other console difference).

## Entities

- **`func_vehicle` and `func_vehiclecontrols` exist** (`dlls/vehicle.cpp`). halflife-updated
  added them; the original has neither (`reference/entities-rc2.txt`), so a map using them gets
  nothing there on RC2.

## halflife-updated changes kept in stock functions

RC2's stock functions (byte for byte SDK 2.3, `reference/names-rc2.tsv` and
`names-rc2-present.tsv`) have SDK 2.3's behavior back wherever it shows in multiplayer play
(audit of 2026-11-07, `reference/audit/a1.md`-`a6.md`, one row per function). What halflife-updated
changed and we keep, besides restyling and the crash guards listed under "Crashes in the original":

- **Node graphs go through the engine filesystem, in the mod directory only**
  (`maps/graphs/<map>.nod`, `.nod64` on amd64; `dlls/nodes.cpp`). The original's
  LOAD_FILE_FOR_ME also found a graph in the fallback game directory.
- **The dedicated server's `servercfgfile` is run by the engine, not the game rules**
  (`dlls/multiplay_gamerules.cpp`, `CHalfLifeMultiplay` constructor). Today's engine execs it once
  per map itself, so the original's call would run it twice. A listen server still runs
  `lservercfgfile` from the game.
- **The suit test and weapon bits go through `m_WeaponBits`** (`dlls/player.cpp` HasSuit,
  `dlls/weapons.cpp` SetWeaponBit), 64 bits for today's client, mirrored into `pev->weapons`
  below bit 32.
- **Monsters take an `allow_item_dropping` keyvalue** (`dlls/monsters.cpp`, from Opposing Force
  Updated). No RC2 map sets it, and unset it keeps the drops.
- **Save/restore is halflife-updated's** (`dlls/cbase.cpp`, `dlls/util.cpp`, `dlls/world.cpp`):
  save data held by reference and checked, bool and int64 fields, 64-bit function pointers, alien
  slave beams not saved (`dlls/islave.cpp`), and beams keeping their attachment when their entity
  is set again on restore (`dlls/effects.cpp` SetStartEntity, SetEndEntity). Multiplayer doesn't
  save.
- **Singleplayer-only paths are halflife-updated's** (`dlls/singleplay_gamerules.cpp` spserver.cfg,
  `dlls/triggers.cpp` trigger_camera's view entity and the changelevel transition code,
  `dlls/player.cpp` CStripWeapons' local player).
- **Scientists take their model from `_sv_override_scientist_mdl` when it is set**
  (`dlls/scientist.cpp`, four places). Nothing in our build registers that cvar, so they use
  `models/scientist.mdl`, as the original's hardcoded name. Kept by Steven, 2026-11-07.
- **`CanAttack` rounds to the millisecond** (`dlls/weapons_shared.cpp`, server and client):
  a weapon may fire when its next attack is under 1 ms away, halflife issue #1621, so the server
  agrees with the client's prediction. SDK 2.3 tests `<= 0`. Kept by Steven, 2026-11-07.
- **A held weapon stops its fall think** (`dlls/weapons.cpp` FallThink's `m_pPlayer` branch,
  AttachToPlayer's `SetThink(NULL)`). SDK 2.3 lets it run, so if the engine set FL_ONGROUND on a
  held (MOVETYPE_FOLLOW) item the weapon would turn back into a pickup and could be removed from
  the inventory. Kept as a guard by Steven, 2026-11-07.

## Client

- **The controller charge ball's trails (CLTE 0x49) leak without the bad delete.** The original
  (client 0x95e50 Think, 0x971c0 EntityGone) frees trail 0, then calls plain `operator delete` on
  the `new[]` array's pointer (block + 4). MSVC6's release heap rejects that silently and the
  trails leak; glibc would abort. We leave the delete out and keep the leak
  (`cl_dll/rc2/tcontroller_tents.cpp`, `reference/dossiers/c3.md`).

## Crashes in the original

Each crash the original has is guarded with the smallest check that avoids it (policy since
2026-11-07, as RC 1.9's). Where the original would crash, ours goes on; nothing else changes.
`reference/ORIGINAL_BUGS.md` lists each with its address, and the code marks each
`// Original bug (crash), guarded`.

Server (`rc2.dll`):

- **`CanHavePlayerItem` checks the rider it looks up** (`dlls/multiplay_gamerules.cpp`). With
  `mp_weaponstay` on, a rider slot with no player crashed the original.
- **A teather whose shooter isn't a player fails** (`dlls/rc2/crossbowbolts.cpp`). An LCS spawner's
  `xbow_teather` has the spawner as its shooter, and the original's `AddTether` wrote player
  members past the spawner's end.
- **The LCS string pool and the precache list stop at their ends** (`dlls/rc2/maintainent.cpp`
  `LCS_AllocString`, `dlls/util.cpp` `AddToPrecachedList`). A long string still runs on into the
  next slots or rows, as in the original, but no longer past the array.
- **`sv_tempadmins` and `say_private` are bounded** (`dlls/rc2/maintainent.cpp`,
  `dlls/client.cpp`): the original overran its stack with a long cvar or message.
- **`LCS_GetToken` copies at most 255 characters and stops at an unterminated quote or brace**
  (`dlls/rc2/lcs.cpp`). The original overran its callers' 256-byte buffers with a long token in a
  `.lcs` line, and read on past the line's end after an unterminated one.
- **`ALT_StripSpaces` doesn't write before the string** (`dlls/rc2/alteration.cpp`) when it is a
  single whitespace character.
- **The damage score checks the attacker's instance** (`dlls/player.cpp` `TakeDamage`, score mode
  1). A grenade whose thrower was removed crashed the original.
- **The python's chain doesn't set a monster's bit past its bitset** (`dlls/python.cpp`
  `FireChain`); only player bits are ever read.
- **`trigger_counter` takes a NULL activator** (`dlls/triggers.cpp` `CounterUse`), as a
  `func_breakable`'s death sends.
- **A tripmine's far crossing counts as none** (`dlls/tripmine.cpp`, plain and punch mines). A box
  crossing 9999 or more units away left the original with no target to read.
- **`GetSequenceInfo`, `GetSequenceFlags`, `SetBlending` and `GetAnimationEvent` keep
  halflife-updated's sequence range checks** (`dlls/animation.cpp`); an LCS `seq` token can set a
  negative one.

Server, in stock functions, halflife-updated's guards kept (not marked in the code nor in
`ORIGINAL_BUGS.md`; the audit rows have them):

- **`game_score` and `game_player_equip` take a NULL activator, and `game_player_equip`'s weapon
  keys are cut at 127 characters** (`dlls/maprules.cpp`).
- **Keyvalue and file parsing stay in their buffers**: `UTIL_StripToken` stops at its destination's
  size (`dlls/util.cpp`, with its callers in `dlls/triggers.cpp` CMultiManager and
  `dlls/maprules.cpp`), `UTIL_StringToVector` and `UTIL_StringToIntArray` copy at most 127
  characters (`dlls/util.cpp`), `SENTENCEG_Init` stops at the last sentence slot instead of one
  past it (`dlls/sound.cpp`), and mapcycle tokens are copied bounded
  (`dlls/multiplay_gamerules.cpp` ReloadMapCycleFile).
- **`TEXTURETYPE_Init` reads materials.txt from its start** (`dlls/sound.cpp`); the original left
  the file position uninitialized.
- **The nihilanth's target-name buffers are 128 bytes**, as its keyvalues (`dlls/nihilanth.cpp`
  NextActivity, HandleAnimEvent).
- **`CmdStart` and `CmdEnd` cast the player with `dynamic_cast`** (`dlls/client.cpp`), so their
  NULL check also catches an edict that isn't a CBasePlayer.
- **`func_plat`'s trigger holds its platform by handle** (`dlls/plats.cpp` SpawnInsideTrigger): a
  removed platform no longer leaves the trigger reading freed memory.
- **A hornet without an owner does no damage** (`dlls/hornet.cpp` DieTouch); the original passed
  the missing owner to TakeDamage.
- **`func_breakable` material 9 (`matNone`) precaches and sounds as wood** (`dlls/func_break.cpp`
  Precache, DamageSound); the original used an uninitialized model name and sound list.
- **`CHGrunt::CheckMeleeAttack1` fails with no enemy** (`dlls/hgrunt.cpp`) instead of reading an
  uninitialized pointer.
- **`CSoundEnt::ISoundsInList` counts 0 for an unknown list type** (`dlls/soundent.cpp`) instead of
  walking an uninitialized index.
- **`ReportAIState` names all nine monster states** (`dlls/monsters.cpp`); the original read past
  its list for a dead monster. Nothing calls it in our tree.
- **The osprey's flight checks its path corner** (`dlls/osprey.cpp` FlyThink, `m_pGoalEnt`).
- **`trigger_camera` returns when there is no player 1** (`dlls/triggers.cpp` CTriggerCamera::Use).
- **`RemovePlayerItem` also clears the last item when the active one goes** (`dlls/player.cpp`),
  where the original left it pointing at the removed item; `SwitchWeapon` takes a NULL weapon
  (`dlls/player.cpp`).
- **`UTIL_SetOrigin` skips a NULL edict** (`dlls/util.cpp`).

Client (`client.dll`):

- **CLTE 0xfc (`nukeclte`) skips empty effect slots** (`cl_dll/rc2/clte.cpp`). The original called
  through them and crashed whenever it was sent.
- **`WeaponSync` copies at most its 256-byte buffer** (`cl_dll/hl/hl_weapons.cpp`).
- **The frozen and antifriction tables are bounded** (`cl_dll/entity.cpp` `HUD_StudioEvent`,
  `cl_dll/StudioModelRenderer.cpp`, CLTE 0x2f and 0x35). Above index 1023 the original read the
  table after them and wrote past them; ours reads 0 and doesn't write.
- **Effects start their pointers at NULL** (CLTE 12, 15, 17, 20, 30, 0x2a, 0x37 in
  `cl_dll/rc2/`). The original left lights, flares and beams unset with `cl_detail` below 0 or when
  a sprite or tempent failed, and its NULL tests passed the garbage.
- **Beams the engine didn't make are skipped** (CLTE 13, the ultima's down beams, CLTE 0x49's
  leech beams). `R_Beam*` returns NULL when its beam list is full or the sprite is missing.
- **VGUI labels print their text with `"%s"`** (player ID, the spectator title and map line, the
  vote menu and vote update, `CommandButton` text, the debug panel). The original passed player
  names, team names and the vote text as `setText`'s printf format, so a `%s` or `%n` in a name
  crashed every client that showed it.
- **The vote menu is checked** (`MsgFunc_VoteBegin`): in demo playback the original had none.
- **`CreateCommandMenu` makes no submenu for a NULL button and frees the file it loaded**
  (`cl_dll/vgui_TeamFortressViewport.cpp`). The original passed NULL on for an unknown CUSTOM
  button before `{`, and freed the advanced parse pointer, a bad free when `MAX_MENUS` stopped the
  parse. At the file's end it freed NULL and leaked the file, which ours now frees.
- **The debug panel bounds the server's strings** (`cl_dll/rc2/vgui_DebugPanel.cpp`
  `MsgFunc_DBGEntInfo`): a map target or targetname over 127 characters overran the original's
  buffers.
- **`AttrMenu_LoadSlots` writes inside its slot** (`cl_dll/rc2/vgui_attributes.cpp`), and
  **`SayTextPrint` keeps halflife-updated's bounded copy** (`cl_dll/saytext.cpp`).
