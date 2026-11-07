# w2-stubs: wave 2, the wave-1 stubs resolved

Wave 1's nine packages each declared what they needed from other packages in a glue header and
gave it a TEMP STUB to link. This pass points every caller at the real function or global,
deletes the stubs, and moves each declaration next to its definition. Nothing new was
reconstructed: every stub had a real counterpart in the tree, under its own name or another
(found by its `orig 0x` address or, for data, by the address in the comments).

## 1. Scope

- Every `TEMP STUB` marker in dlls/, pm_shared/ and cl_dll/ (112 at the start), and the stub
  bodies the merges left behind without a marker (next to the "the real one is reconstructed
  elsewhere now" comments, and the weak definitions in ggrenade.cpp and glockgibs.cpp).
- Deleted: dlls/rc2/p2shared.h, p2stubs.cpp, p2stubs_forkA.cpp, p2stubs_forkC.cpp, p3b_temp.h,
  p3d_temp.h, p4_needs.h, p4_stubs.cpp, p5_externs.h, p6stubs.h, p6stubs.cpp, p7_extern.h,
  p7_stubs.cpp, p8_needs.h, p8_needs_transform.h, p8_needs_tripmine.h, p8_stubs.cpp,
  p8_stubs_transform.cpp, p8_stubs_tripmine.cpp, p9_needs.h, p9_stubs.cpp. dlls/CMakeLists.txt
  globs dlls/rc2, so it needed no edit.
- New headers (one per RC2 file whose functions others call): dlls/rc2/shield.h,
  maintainent.h, vote.h, killmessages.h, transform.h, racecontroller.h, freezetimer.h,
  antifrictimer.h, crossbowbolts.h.
- Headers extended: util.h (nothing new needed), player.h, pm_shared/pm_rc2.h (now included by
  player.h), weapons.h, UserMessages.h, explode.h, rc2prefs.h, dlls/rc2/virtualhull.h.

## 2. Status

- `grep -rn 'TEMP STUB' dlls pm_shared cl_dll`: nothing.
- `scripts/build.sh i386 all` and `scripts/build.sh amd64`: both pass.
- `python3 scripts/check_coverage.py`: 2567 of 2610, as at the start.
- Built only. Nothing has run in an engine.

## 3. Stub map

Each line: the stub (its package), what replaced it.

### util.cpp (p9)

| Stub | Real one |
|---|---|
| UTIL_Reflect (p2, p7, p1's crossbowbolts) | UTIL_ReflectVector (0xf5800) |
| UTIL_PackDirection (p4, p7, p1, p5's nuke.cpp copy), UTIL_RC2PackNormal (p6), weak UTIL_PackNormal (p3 ggrenade.cpp) | UTIL_PackNormal (0xf5920) |
| UTIL_NamedInflictor (p4, p7, p8), UTIL_DummyInflictor (p5), RC2NamedInflictor (p3) | UTIL_RC2DamageInflictor (0xf58a0) |
| UTIL_KillTrail (p2), UTIL_RC2KillEntityEffects (p6), weak UTIL_KillEntityEffects (p3) | UTIL_KillEntityEffects (0xf55b0) |
| UTIL_PushEntityMask / PopEntityMask (p4; empty bodies in combat.cpp), RC2EntMark* (p3 B), UTIL_RC2PushHitSet / PopHitSet / MarkHit / WasHit (p6), UTIL_PushEntFlags / PopEntFlags / SetEntFlag / TestEntFlag (p8), UTIL_EntityMarked (p5 nuke.cpp) | UTIL_PushEntityMarks, UTIL_PopEntityMarks, UTIL_MarkEntity, UTIL_IsEntityMarked (0xf4c70, 0xf4d40, 0xf4d90, 0xf4dd0) |
| RC2RaiseTime (p3), UTIL_RC2ExtendTime (p6), UTIL_SetMaxTime (p8, by reference), UTIL_DelayAttack (p5) | UTIL_ExtendTime (0xf5900, util.h inline; it has a float& overload) |
| RC2CountEntities (p3 weapons.cpp), UTIL_CountEntitiesByClassname (p5) | UTIL_CountEntitiesByClass (0xf30f0) |
| UTIL_FindRandomEntityByClassname (p3 B) | UTIL_RandomEntityByClassname (0xf31a0) |
| UTIL_IsValidEdict (p4) | UTIL_IsValidEntity (0xf2fe0) |
| UTIL_FindEntityInSphereDist (p4), weak UTIL_FindEntityInSphereEx (p3 C), UTIL_FindEntityNearSphere (p8), RC2FindEntityNear (p3 B, its unused 4th argument is the distance out pointer) | UTIL_FindEntityNearestInSphere (0xf33b0) |
| UTIL_PunchTrace (p8) | UTIL_BlastReaches (0xf5160) |
| UTIL_RayHitsBox (p8) | UTIL_RayIntersectsBox (0xf3200) |
| UTIL_PersistentClassname (p4), GetPrecachedClassname (p1) | GetPrecachedName (0xf5bf0) |
| RC2ResetUtil4c50 (p3 world.cpp) | UTIL_ClearEntityMarks (0xf4c50) |
| RC2ResetPrecachedList (p3 world.cpp) | ResetPrecachedList (0xf5c60) |
| weak RC2RadiusDamage (p3) | RC2RadiusDamage (0xf3900) |
| UTIL_PlayerPartIsGround (p7) | IsOnVirtualHull (0xf7780, virtualhull.cpp; the caller casts to CBasePlayer*) |

### player.cpp (p5)

| Stub | Real one |
|---|---|
| RC2PlayerAttribute (p1 combat.cpp, p3 D), RC2_PlayerAttribute (p4) | PlayerAttr (0x95890), renamed PM_GetAttributeScale (pm_rc2.h's name; see section 4) |
| RC2PlayerButtons (p2, p1), RC2PlayerStateBits (p3 C) | PlayerAttrFlags (0x958b0), renamed PM_GetAbilityFlags |
| RC2PlayerWaterWeapons (p2, p3 B), PlayerCanFireUnderwater (p6), RC2_PlayerFiresUnderwater (p4) | PlayerHasAttrFlag200 (0x95860) |
| RC2PlayerAddHold / RC2PlayerRemoveHold (p2), CBasePlayer::RemoveMoveLock (p8, empty) | CBasePlayer::AddPhysFlag2Source / RemovePhysFlag2Source (0x9a800, 0x9a820) |
| CBasePlayer::RC2StartObserver (p1 client.cpp, empty) | CBasePlayer::GoToSpectatorView (0x9b990) |
| g_fGivingItem (p2, 0x18f3c8) | gEvilImpulse101 (SDK; the disassembly's writes and reads of 0x1018f3c8 match it) |
| g_fRC2InDamageReflect (p3 world.cpp, 0x18f4dc) | was player.cpp's static s_bReflecting in TakeDamage; now a player.cpp global under world.cpp's name, declared in player.h, so CWorld::Precache clears the real one |
| gmsg* stubs: gmsgCLTE (p4, p6), gmsgWeaponSync (p7), gmsgViewMode (p7), gmsgPullerCtrl (p6), gmsgRaceStat (p6), gmsgTformStat (p8), gmsgVoteBegin / gmsgVoteUpdate (p9) | UserMessages.h's (LinkUserMessages, 0x94240). The strong stub definitions were overriding the inline ones; every user now includes UserMessages.h |
| g_usTAssassinGun and the other transform events (p8, marked but already defined) | player.h's inline globals, set in CBasePlayer::Precache |

### weapons.cpp, weapons_shared.cpp (p3)

| Stub | Real one |
|---|---|
| g_runfuncs (p6, server), g_iRC2WeaponFrame (p4), g_iWeaponRunFuncs (p7), g_iPlayWeaponEvents (p8) | g_fRC2PlayWeaponEvents (0x181434, weapons_shared.cpp). The server halves of crowbar, crossbow, egon and python now test it; the client halves still test the client's g_runfuncs |
| g_fItemTimeout (p2), g_bRC2EquipFlag (p5), g_iAllowWeaponReplace (p8) | g_fRC2Randomize (0x181430) |
| g_usRC2Explode (p2), g_usRC2ExplodeEvent (p6) | g_usExplodeEvent (0x1afd18) |
| ReplaceWithRandomItem (p4, empty) | RC2RandomizeItem (0xfa100) |
| ReadSyncShort (p4) | RC2ReadStateShort (0xfb790) |
| RC2_GrenadeExplode (p8 tripmine) | CGrenade::RadialExplode (0x50cc0, ggrenade.cpp) |

### The shield (p7, dlls/rc2/shield.cpp)

RC2ShieldDamage (p2, p1 combat.cpp, p3 B and C), ShieldAbsorbDamage (p4, p5 nuke.cpp),
RC2ShieldAbsorb (p6), RC2_ShieldFilterDamage (p8): all are ShieldDeflect (0xc45b0), declared in
dlls/rc2/shield.h. The callers that passed `(float*)&vec` for the out vectors now pass `&vec`.
RC2_PlayerShieldOn (p8) became a read of CBasePlayer::m_fShieldOn (orig +0x778).

### maintainent.cpp (p1)

| Stub | Real one |
|---|---|
| GetAimedEntity (p4) | Admin_GetAimedEntity (0x2660), no longer static |
| AdvCreateEntity, EditEntity, MaintainEdit (p4) | LCS_CreateWithScript, LCS_RunOnEntity, LCS_MaintainOnEntity (0x4f00, 0x4eb0, 0x5040); their script argument is const now (ALT_SplitArgs takes const) |
| Admin_FindPlayer (p9, by index only) | Admin_FindPlayer (0x25e0), no longer static |
| IsTempAdmin (p6) | Admin_IsTempAdmin (0x1160), no longer static |
| ClientFlags1050, PlayerSpawned50a0, IsListenHost1000 (p5) | Admin_GetLevel, LCS_RunPlayerScript, Admin_IsListenHost |
| g_szRC2SpawnCommand (p3 world.cpp, p4 lcs.cpp; 0x18be80) | g_szLCSPlayerScript |
| g_iRC2SpawnCommandRun (p4, 0x136078) | g_iLCSPlayerScriptFirstArg |
| g_iRC2AlterPlayer, g_iRC2AlterEntity (p4, 0x149c68, 0x149c6c) | g_iLCSCallerIndex, g_iLCSSelectedIndex, no longer static |

maintainent.cpp's own stubs (p1, unmarked): LCS_ParseTokens, LCS_EvalFloat, LCS_EvalVector became
alteration.cpp's ALT_SplitArgs, ALT_EvalFloat, ALT_EvalVector (0x73fb0, 0x71c60, 0x72a90).

### Other RC2 files

| Stub | Real one |
|---|---|
| IsPlayersViewEnt (p2) | PlayerHasRider (0xb28b0, playerride.cpp) |
| XBowTeatherDie (p1, a wrapper with no original) | the crossbow calls CXBowTeather::DieThink (0x29aa0) itself; CXBowTeather's declaration moved to dlls/rc2/crossbowbolts.h |
| VectorsToAngles (p1 crossbowbolts.cpp, p2 controlforklift.cpp; the stub dropped the roll) | VectorAnglesWithRoll (0xa0260, pm_math.cpp) |
| RC2AddPullPoint / RC2RemovePullPoint (p3 D, inline, did nothing) | AddPullPoint / RemovePullPoint (0xaa950, 0xaaa20, pullers.cpp) |
| g_rgiRC2ForceState (p3 world.cpp, 0x1915c8, 0xc0 ints) | g_vecPullPointOrg (pullers.h, 64 Vectors at 0x1915c8); CWorld::Precache clears it |
| ResetPMTables, SendWeaponSync (p5) | CBasePlayer::ResetTethers, CBasePlayer::SendPullPoints (0xaaa60, 0xaac20) |
| RemoveFreezeTimers (p5) | FreezeTimer_Thaw (0x41e80, freezetimer.h) |
| RemoveAntiFricTimers (p5) | StopAntiFriction (0xaa40, antifrictimer.h) |
| TransformSetAnimation, TransformMovement, TransformPostThink, TransformKilled, TransformDeathSound, TransformTookDamage, TransformFinish, TransformRevert, TransformClassify (p5) | CBasePlayer::Transform_SetAnimation, Transform_Think, Transform_FixAngle, Transform_Killed, Transform_DeathSound, Transform_PainSound, FinishTransform, FinishUntransform (0xe1fa0, 0xe2000, 0xe2040, 0xe2070, 0xe2080, 0xe20c0, 0xe2830, 0xe2f90), Transform_Classify (0xe3750) |
| UntransformPlayer (p6 racecontroller.cpp) | CBasePlayer::FinishUntransform (0xe2f90) |
| PrecacheTransforms (p1 client.cpp) | Transform_Precache (0xe3450) |
| Transform6Think (p5) | RaceUseFrame (0xae540, racecontroller.cpp; its comment said nothing calls it, PreThink does) |
| RacePath_* (declared in p6stubs.h) | racecontroller.cpp's, now declared in racecontroller.h |
| SendPrefsReset, SendPrefsChanged (p5) | RC2Prefs_SendResetToClient, RC2Prefs_SendToClient (0xaf370, 0xaf270) |
| RC2ResetPrefs, RC2LoadPrefs (p3 world.cpp) | RC2Prefs_ResetToDefaults, RC2Prefs_LoadConfig (0xb1150, 0xaf030) |
| RC2LoadLCS (p3 world.cpp) | LCS_Load (0x6d4d0, lcs.cpp) |
| g_iScoreMode (p5 multiplay_gamerules.cpp, 0x1afd38) | g_iRC2ScoreMode (world.cpp, set in CWorld::Spawn) |
| GetDeathMessageIndex, GetDeathMessageText (p5) | KillMsg_Pick, KillMsg_Get (0x31fe0, 0x32540, killmessages.h) |
| Vote_Command, VoteMap_Command, VoteKick_Command (p1 client.cpp) | ClientVote, ClientVoteMap, ClientVoteKick (0xf8520, 0xf8620, 0xf89d0, vote.h) |
| UpdateVirtualHulls (p1 client.cpp, free function) | RemoveOrphanVirtualHulls (0xf7600, virtualhull.h) |
| SnarkCam_TakeView (p8) | CSnarkCam::TakeControl (0xcbfe0) |
| SqueakGrenade_Owner (p8), TripmineGrenade_GetPlacer (p9) | reads of CSqueakGrenade::m_hOwner (+0x2dc) and CTripmineGrenade::m_hOwner (+0x304); both class declarations moved to weapons.h (server only) so squeak.cpp and tripmine_weapon.cpp can read them as the original does |
| RC2_GameRulesCanKnock (p8), RC2GameRulesAllowGaussTeleport (p3 B) | CGameRules::PlayerCanAffect (slot 58, gamerules.h) |
| CBaseAnimating::RC2FrameAdvance (p4) | CBaseAnimating::StudioFrameAdvance (0x9380, which holds a frozen entity's frame itself); cbase.h's p4 block removed |
| DeactivateSatchels (p7) | deleted with its declaration in weapons.h: nothing calls it now that p5 reconstructed PlayerKilled |

## 4. Shared-file edits and naming decisions

- **pm_shared hooks.** pm_shared calls player.cpp, transform.cpp, tbat.cpp, theadcrab.cpp and
  virtualhull.cpp functions directly (pm_rc2.h gives each one's address), and on the server
  pm_shared.cpp had stubs for all of them. pm_shared needs the same names on both sides, so the
  real definitions took pm_rc2.h's names and everything else now calls them by those:
  PlayerAttr, PlayerAttrFlags, SetPlayerAttrInt, GetPlayerAttrInt, PlayerEnergy, UsePlayerEnergy,
  PlayerEnergyEffect, PlayerHideViewEntities, PlayerShowViewEntities became PM_GetAttributeScale,
  PM_GetAbilityFlags, PM_SetPlayerTimer, PM_GetPlayerTimer, PM_GetEnergy, PM_UseEnergy,
  PM_JetboostEffect, PM_HideViewers, PM_RestoreViewers; Transform_OfPlayer became
  PM_GetPlayerMode; VirtualHulls_Hide / Show became PM_HideOwnedEnts / PM_RestoreOwnedEnts;
  TBat_GetRoost / TBat_Unroost became PM_GetBatPerch / PM_BatUnperch; THeadcrab_SetLeaping /
  IsLeaping / LeapHit became PM_SetHeadcrabLeap / PM_GetHeadcrabLeap / PM_HeadcrabLeapHit.
  Signatures follow pm_rc2.h: PM_JetboostEffect takes float* (its vectors are unused),
  PM_GetBatPerch writes a float[3], the owned-ent hooks take PM_SETENTFUNC. player.h includes
  pm_rc2.h and keeps only the attribute functions pm_shared doesn't call.
- **Duplicated globals merged** (two packages each defined the same original variable, so one
  side never saw the other's writes):
  - 0x1af2a8: client.cpp's g_iConnectedClients (set on connect) and util's g_iRC2ClientBits
    (read by UTIL_IsValidEntity and RemoveOrphanVirtualHulls). Kept g_iRC2ClientBits.
  - 0x1afd30: world.cpp's g_pevRC2WorldDummy (made in CWorld::Precache) and util's
    g_pevRC2DamageInflictor (what UTIL_RC2DamageInflictor hands out). Kept the latter.
  - 0x18f4dc, 0x18be80, 0x136078, 0x149c68/c, 0x1915c8, 0x181430, 0x181434, 0x1afd18,
    0x1afd38, 0x18f3c8: in the tables above.
- **player.h**: the phys-flag methods declared once (SetPhysFlags, ClearPhysFlags,
  AddPhysFlag2Source, RemovePhysFlag2Source, SetPhysFlag10For), with their addresses; the
  duplicate p8 block and RC2StartObserver gone; RC2_IN_CROWBARANG (ability 0x40) and
  g_fRC2InDamageReflect declared.
- **weapons.h**: CSqueakGrenade (made public; it was private in its .cpp) and CTripmineGrenade,
  under `#ifndef CLIENT_DLL`.
- **UserMessages.h**: RC2_CLTE_KILL (0xff). p2's other CLTE types and GetRCCaps bits went into
  the files that use them (crowbarang.cpp, crowbar.cpp), as the other packages keep theirs.
- **rc2prefs.h**: RC2PrefWord (p8's, a short key read zero-extended).
- **explode.h**: ExplodeAt. **virtualhull.h**: RemoveOrphanVirtualHulls, IsOnVirtualHull.
- Local `extern`s for world.cpp's g_iRC2ScoreMode and g_iRC2GibEvent stay in combat.cpp,
  player.cpp and multiplay_gamerules.cpp: world.cpp has no header in the SDK, and they point at
  the real definitions.
- Left alone: the "pN needs" member blocks in cbase.h, basemonster.h, player.h, weapons.h,
  talkmonster.h and gamerules.h (real members, not stubs); playerride.h and pullers.h still
  repeat some pm_rc2.h declarations (identical, harmless).

## 5. What now behaves differently

Behavior is the same wherever a stub already did what the real one does (the UTIL_ExtendTime,
UTIL_PackNormal and UTIL_ReflectVector copies, the always-set weapon-events flags). Where a stub
was inert, the real code now runs. The ones a tester would notice:

- The player shield absorbs damage from every weapon, monster and blast (all stubs returned
  the full damage).
- rc2.ini is loaded and reset each map (RC2LoadPrefs and RC2ResetPrefs were empty), and the
  prefs are sent to clients.
- The admin commands, temp admins (sv_tempadmins), the LCS scripts (create, edit, maintain,
  setspawncommand) and the map's player script run; maintainent.cpp's parser stubs did nothing.
- Votes (vote, votemap, votekick) work; the client commands called empty stubs.
- Death messages have text; the player's transforms, the race and the pull points (grav and
  suck grenades) are wired to the player.
- Area damage in combat.cpp hits each entity once per blast (its mark stack push and pop were
  empty, so marks leaked across blasts).
- Death notices name the weapon: UTIL_RC2DamageInflictor now gets the stand-in entity CWorld
  makes. mp_random leaves crowbarang pickups and tripmine drops alone.
- Weapons with ability 0x200 fire under water.

## 6. Client stubs kept

These exist because the client compiles server headers and weapon files; RC2's client.dll is
its own library and isn't reconstructed yet:

- cl_dll/hl/hl_baseentity.cpp: the CBaseEntity, CBasePlayer, CBasePlayerItem and weapon bodies,
  RC2RandomizeItem, g_fRC2Randomize, g_usExplodeEvent, RC2ReadState*.
- pm_shared/pm_shared.cpp `#ifdef CLIENT_DLL`: the client's PM_* hooks and g_rc2PrefValues.
- The `#ifdef CLIENT_DLL` halves of crowbar.cpp, crossbow.cpp, gauss.cpp and glock.cpp.

## 7. VERIFY list

No new VERIFY marks. Settled here: p1's "naming clash" (UTIL_IsValidEdict is UTIL_IsValidEntity),
virtualhull.cpp's "what pfnSetHull is" (pm_shared's 0xa8d10 callback, per pm_rc2.h), and
racecontroller.cpp's "who calls RaceUseFrame" (PreThink). Open, from wave 1 and unchanged:
multiplay_gamerules.cpp's reading of 0x181430 as "no ammo of their own" is now a comment on
g_fRC2Randomize; p5's and p2's readings differ and only weapons.cpp tests the flag.

## 8. Behavior worth testing

Everything in section 5, against the original under Wine: shielded hits from each weapon, an
.lcs file with each line type, a map vote and a kick vote, the death messages, a transform and
a race, and a blast that reaches one entity through two others (each hit once).

## 9. Original bugs

None new.
