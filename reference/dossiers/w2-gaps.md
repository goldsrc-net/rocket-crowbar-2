# w2-gaps: the code Ghidra's function list missed

## 1. Scope

Every stretch of rc2.dll's .text (up to the C runtime) that no function in
`reference/functions-rc2.tsv` covers: `reference/gaps-rc2.tsv`, made by `scripts/gaps.py`. Wave 1
left 43 "code" and 107 "data" stretches; this package split them into pieces, identified each
and reconstructed or tagged it in the file it belongs to.

Files written or touched: `scripts/gaps.py`, `reference/gaps-rc2.tsv` (regenerated),
`dlls/game.cpp`, `game.h`, `gamerules.h`, `multiplay_gamerules.cpp`, `singleplay_gamerules.cpp`,
`teamplay_gamerules.cpp`, `teamplay_gamerules.h`, `rc2prefs.cpp`, `util.cpp`, `player.cpp`,
`nodes.cpp`, `flyingmonster.cpp`, `ggrenade.cpp`, `triggers.cpp`, `world.cpp`, `xen.cpp`,
`func_tank.cpp`, `aflock.cpp`, `animating.cpp`, `client.cpp`, `plats.cpp`, `schedule.cpp`,
`scripted.cpp`, `spectator.cpp`, `talkmonster.h`, `monsters.h`, `rc2/pullers.cpp`,
`common/mathlib.h`, `game_shared/voice_gamemgr.cpp`, `pm_shared/pm_debug.cpp`,
`public/interface.cpp`. Most are one-line `// orig` tags on stock functions; the real code
changes are listed in section 3.

### gaps.py, rewritten

The old version took each whole stretch as one item and called it "code" when it started on a
16-byte boundary, so a stretch holding several functions counted once, and switch jump tables
that happened to be aligned counted as functions (14 of the 43). It now walks each stretch
instruction by instruction (capstone) and splits it into pieces: a piece ends where padding
follows a ret/jmp or runs to a 16-byte boundary, and a jump table (VC6's `8d 49 00`/`8b ff`
filler, aligned addresses pointing back into the code, then the byte index table) is its own
piece. Kinds: `stock`, `code`, `jumptable`, `thunk` (the linker's `jmp [import]` stubs), `tail`
(starts exactly where the listed function before it ends), `data`. check_coverage still counts
`code` only. Pieces of any size are listed now (the old version skipped stretches under 16
bytes), which is why the count grew. It needs the RC2-layout reference build at
`build-vc6/hl-rc2` (a symlink to the main checkout's `build-vc6` works; it exits otherwise).

Result: 24 stock, 189 code, 125 jumptable, 1 thunk, 2 tail, 0 data.

## 2. Status

`python3 scripts/check_coverage.py`: 2730 of 2730 (100%), 0 missing in every file (was 2567 of
2610; the code pieces outside the C runtime's files replace the old 43 stretches, 14 of which
were jump tables).
`scripts/build.sh i386 all` and `scripts/build.sh amd64` pass. Nothing run in an engine.

Each piece was checked against the RC2-layout reference build (hl.map/hl.dll) with an
instruction-level comparison (addresses masked, string literals compared, vtable call offsets
normalized, since the reference lacks RC2's gamerules slots 29, 57 and 58). "stock" below means
identical by that comparison.

## 3. Function map

Reconstructed (RC2's own or changed):

```
0x470a0  GameDLLInit (game.cpp)                       ok: reached only through gFunctionTable. Order fixed
                                                          (mp_random, mp_scoremode before mp_teamannoy) and the
                                                          sv_maxvelocity pointer added (g_psv_maxvelocity, orig
                                                          .data 0x18c500); hlu's extra cvars, SV_InitServer,
                                                          sv_allow_autoaim/sv_cheats pointers kept
0x7f6c0  CHalfLifeMultiplay::CanHavePlayerItem        ok: weaponstay + already carried -> tries m_iShoulderRider
0xdcbc0  CHalfLifeTeamplay::DeathNotice               ok: RC2 dropped the "teammate" message
0xdc9a0  CHalfLifeTeamplay::ClientUserInfoChanged     ok: log line back to <%u> WON id, as client.cpp does
0xdd130  CHalfLifeTeamplay::PlayerAllowsRiding        ok: new override (slot 57)
0xdd1b0  CHalfLifeTeamplay::PlayerCanAffect           ok: new override (slot 58), mp_teamannoy
0x946a0  CBasePlayer::Pain                            ok: empty in RC2 (nothing calls it)
0xb1260  RC2Prefs_PackedSize (rc2prefs.cpp)           ok: name ours; uncalled in rc2.dll
0xb1340  RC2Prefs_PackValues                          ok: name ours; uncalled
0xb1520  RC2Prefs_UnpackValues                        ok: name ours; uncalled
0xf4c00  UTIL_TraceEffectedHit (util.cpp)             ok: name ours; uncalled
0xf4e20  UTIL_TestAndMarkEntity                       ok: name ours; uncalled
0xf5530  UTIL_DebugLog, g_pRC2DebugLog (.bss 0x1af2b0) ok: name ours; uncalled; debuglog.txt
```

Stock, tagged where they are:

```
teamplay      0xdc300 GetGameDescription, 0xdc310 Think, 0xdc650 InitHUD, 0xdcc10 IsTeamplay,
              0xdccf0 ShouldAutoAim, 0xdcd50 IPointsForKill, 0xdce40 IsValidTeam
multiplay     0x7e400 IsMultiplayer, 0x7e410 IsDeathmatch, 0x7e420 IsCoOp, 0x7ecc0 FPlayerCanRespawn,
              0x7ecd0 FlPlayerSpawnTime, 0x7ed00 IPointsForKill, 0x7f5b0 PlayerGotWeapon,
              0x7f7c0 CanHaveItem, 0x7f7d0 PlayerGotItem, 0x7f840 PlayerGotAmmo, 0x7f850 IsAllowedToSpawn,
              0x7f8c0..0x7f8f0 FlHealth/FlHEVChargerRechargeTime, DeadPlayerWeapons/Ammo,
              0x7f950 PlayerRelationship, 0x805b0 GetTeamID, 0x805c0 PlayTextureSounds,
              0x805d0 EndMultiplayerGame
CGameRules    (inline defaults, gamerules.h) 0x80510..0x80590 IsTeamplay, GetGameDescription,
              ShouldAutoAim, ClientUserInfoChanged, GetTeamIndex, GetIndexedTeamName, IsValidTeam,
              ChangePlayerTeam, SetDefaultPlayerTeam; 0x805a0 PlayerCanAffect; 0xca7b0..0xca820
              UpdateGameMode, FPlayerCanTakeDamage, ClientCommand, FlHEVChargerRechargeTime,
              PlayerAllowsRiding (0xca7f0: return false), PlayTextureSounds, PlayFootstepSounds,
              EndMultiplayerGame
singleplay    33 CHalfLifeRules one-liners 0xca4c0..0xca7a0, FAllowFlashlight 0xca830, GetTeamID 0xca840
nodes         0x85a30 CGraph::LinkEntForLink, 0x8a8d0 CGraph::TestRoutingTables, 0x88d70 CStack::CStack,
              0x88de0 CStack::Top, 0x85860/0x85870 WorldGraph's initializer
flyingmonster 0x41180 CFlyingMonster::MoveExecute, 0x41550 CFlyingMonster::CeilingZ
player        0x946b0 VecVelocityForDamage, 0x9c360 SelectPrevItem, 0x9c460 CSprayCan::Think,
              0x9ce20 CSprayCan::ObjectCaps, 0x9c5b0 CBloodSplat::Spawn, 0x9c650 CBloodSplat::Spray
others        0x52530 CGrenade::UseSatchelCharges, 0xe6510 NextLevel, 0xbe580 CBaseMonster::FHaveSchedule,
              0x98a0 CBaseAnimating::GetAutomovement, 0x8fe20 CPlatTrigger::ObjectCaps,
              0xc2250 CTalkMonster::DeclineFollowing, 0xc2750/0xc2760 CCineMonster::Die/Pain,
              0xcfa70 CBaseSpectator::SpectatorDisconnect, 0x9f340 PM_ShowClipBox,
              0x1d790 ParmsNewLevel, 0x1f370 CreateInstancedBaselines, 0xf7970 CVoiceGameMgr::SetHelper,
              0x5700 CFlockingFlyerFlock::PrecacheFlockSounds (Ghidra had it inside Precache)
interface     0x6a6f0 InterfaceReg::InterfaceReg, 0x6a790 Sys_LoadModule, 0x6a7a0 Sys_UnloadModule,
              0x6a7b0 Sys_GetFactoryThis, 0x6a7c0 Sys_GetFactory (public/interface.cpp)
statics       compiler-generated initializers (jump + body): 0x43ba0 gTankSpread, 0x525f0/0x52600
              g_vecAttackDir, 0x52610 g_vecZero, 0x7deb0/0x7dee0/0x7def0/0x7df50 the voice manager and
              helper in multiplay, 0xa96f0/0xa9700 g_vecPullPointOrg, 0xf77c0..0xf7880 voice_gamemgr's
              five, 0xfbfa0/0xfbfb0 gGlobalState, 0xfe510/0xfe520 CXenSporeLarge::m_hullSizes
util          0xf5dc0: SDK 2.3's default CSaveRestoreBuffer(), uncalled; hlu has none (m_data is a
              reference), so the tag is a comment
```

Not code anyone writes: the 125 jump tables (each one checked to be the target of a
`jmp [reg*4+table]` in the function before it), the import thunks at 0xff2b0, and the two tails:
0x4276b is the rest of CBreakable::DamageSound (stock, matched at 408 bytes in names-rc2.tsv)
and 0x5723 the rest of PrecacheFlockSounds.

The old 107 "data" stretches: 100 were jump tables only, the rest held functions after all:
0x1d385 (two client.cpp functions wave 1 had tagged), 0x4276b (the DamageSound tail),
0x946a0 (Pain, VecVelocityForDamage), 0x9cd9c (CSprayCan::ObjectCaps after a jump table),
0xb123c (the three prefs functions), 0xca4c0 and 0xca590 (the single player rules' one-liners).

## 4. Classes

No new members. CHalfLifeTeamplay gains the PlayerAllowsRiding and PlayerCanAffect overrides.
New globals: `g_psv_maxvelocity` (game.h, orig .data 0x18c500; the egon, satchel and
shotgun projectile code still read the cvar by name, same value), `g_pRC2DebugLog` (util.cpp).

## 5. Cross-file needs

Shared files edited: `game.cpp`/`game.h` (GameDLLInit), `gamerules.h` (tags; two VERIFY notes
on the riding/affect defaults replaced by the read addresses), `monsters.h` and
`common/mathlib.h` (comment tags only). No stubs. No new user messages.

## 6. VERIFY list

None added. Two removed: the CGameRules defaults of PlayerAllowsRiding (false, 0xca7f0) and
PlayerCanAffect (true, 0x805a0) are now read; PlayerCanAffect's name stays ours.

## 7. Behavior worth testing

- Teamplay: a teammate kill shows the normal death notice, not "teammate".
- Teamplay riding: only teammates can ride; mp_riding 0 blocks it; a player with no
  `allowriding` key can be ridden (in plain deathmatch he can't).
- Teamplay with mp_teamannoy 0: weapons that ask PlayerCanAffect leave teammates alone.
- mp_weaponstay 1: touching a weapon you already carry gives it to the player on your shoulder.
- The cvar list order (`cvarlist`) against the original.

## 8. Original bugs

- CHalfLifeMultiplay::CanHavePlayerItem: the rider's UTIL_PlayerByIndex result isn't checked
  before the virtual call (crashes on a stale index). Kept. Each rider check calls
  CanHavePlayerItem on the rider, which checks its own riders in turn.
- RC2Prefs_UnpackValues: a SMALLFLOAT is read back multiplied by 100 again instead of divided.
  Kept (nothing calls it).
- Teamplay PlayerAllowsRiding allows riding when the ridden player has no infobuffer or no
  allowriding key; the multiplayer version refuses. Kept.

## Notes for the coordinator

- `scripts/vc6/match.py`'s `pattern()` masks any 4 bytes whose top byte is 0x10, scanning byte by
  byte, so it can mask a misaligned window (`00 ff 15 10` inside `push 0; call [0x100e3710]`),
  leave part of the real address unmasked and miss a stock function. UseSatchelCharges,
  LinkEntForLink, TestRoutingTables, MoveExecute, CeilingZ and NextLevel are identical to the
  reference by instruction but weren't matched for this reason; names-rc2.tsv is likely short
  of other stock functions too. Not changed here.
- The helper scripts used for this (gap references, reference comparison) were scratch; the
  comparison method is described in section 2.
