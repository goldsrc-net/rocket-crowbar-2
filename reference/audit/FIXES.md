# halflife-updated's bug fixes in the audits' play reverts

Decided by Steven, 2026-11-07, after the stock audits (mod-rc2 `a1.md`-`a6.md`, mod-rockcrowbar
`r1.md`-`r6.md`): halflife-updated's evident bug fixes are kept in both projects; a revert stays only
where halflife-updated changed the game's design or balance, or added a feature. Every play revert of
both audits is read here once, with the replaced code from the audit merge commits and the reason from
halflife-updated's commit (`git log upstream/64bit-demo -S<code> -- <file>`;
`~/stignored/rc2-tools/fixes/why.sh`). The same function or the same fix gets the same verdict in both
projects.

- fix: halflife-updated's code is restored, the comment reads `// halflife-updated's fix, kept: ...`.
- design: the revert stays.
- unclear: the revert stays until Steven decides (listed at the end).

"seed" is halflife-updated's first commit de70fad, Valve's 2013 SDK; "SDK 2.2+" is a change Valve made
after SDK 2.1, so RC2's SDK 2.3 already has it. Where one row splits, each part has its own verdict.

## mod-rc2

```
rva       file                    function                           what halflife-updated changed                                   verdict  reason
0x35950   doors.cpp               CBaseDoor::DoorGoUp                no moving sound replay while already moving (seed)              fix      the loop restarted on every reverse or retrigger
0x35d00   doors.cpp               CBaseDoor::DoorGoDown              same                                                            fix      same
0x36ac0   doors.cpp               CMomentaryDoor::Use                clamp at 0, skip a move to the current goal, stop sound on      fix      the moving loop never stopped, and a negative value
                                                                     arrival (DoorMoveDone, StopMoveSound) (seed)                             drove the door past its start
0x87840   nodes.cpp               CTestHull::DropDelay               "Node Graph out of Date" center print commented out (seed)      design   drops a message; neither is buggy
0x9c2d0   weapons_shared.cpp      CBasePlayer::SelectLastItem        m_ForceSendAnimations around Deploy (42f45bd, #154)             fix      lastinv's deploy animation never reached the
                                                                                                                                              predicting client
0xc0690   scientist.cpp           CScientist::Scream                 FOkToSpeak check dropped (Marphy, 57306ca)                      design   SDK's check (no talking with an enemy in view) is
                                                                                                                                              deliberate; a content retune
0xc0950   scientist.cpp           CScientist::RunTask                scared scream 8/32 cut to 1/64, second scream removed           design   the retune that goes with Scream
0xdd2f0   tentacle.cpp            CTentacle::Spawn                   m_EFlags |= EFLAG_SLERP (a02eb83, halflife#3228)                fix      works only through halflife-updated's AddToFullPack
                                                                                                                                              (m_EFlags), which RC2's own lacks; there SDK's slerp
                                                                                                                                              test already covers the tentacle, so it is inert
0xfebe0   zombie.cpp              CZombie::IdleSound                 uses its pitch local (95-104) (022b5ea, HL25)                   fix      SDK computed the pitch and never used it
0xfec40   zombie.cpp              CZombie::AttackSound               pitch 95-104 (same commit)                                      fix      same commit, makes the two consistent
0xacc0    apache.cpp              CApache::Spawn, NullThink          max_health, NullThink + FCheckAITrigger (398591d, halflife#3262) design   AI trigger rework (Steven)
0xaf20    apache.cpp              CApache::Killed                    deadflag DEAD_DYING (same)                                      design   same
0xb030    apache.cpp              CApache::DyingThink                FCheckAITrigger (same)                                          design   same
0xb960    apache.cpp              CApache::HuntThink                 monster state from the enemy, Listen, FCheckAITrigger (same)    design   same
0xd620    apache.cpp              CApache::TakeDamage                light/heavy damage conditions (same)                            design   same
0x51a80   ggrenade.cpp            CGrenade::BounceTouch              ResetSequenceInfo on landing (2ee36b4), frame 0 when it slows   fix      the new sequence never got its timing; the frame
                                                                     (b31de2b, #238)                                                          froze mid-tumble
0x521b0   ggrenade.cpp            CGrenade::ShootTimed               ResetSequenceInfo after SET_MODEL (2ee36b4)                     fix      the tumble sequence's timing was never set up
0x75f40   monsters.cpp            CBaseMonster::SetActivity          m_Activity set before SetYawSpeed (ec1f016, #176)               unclear  the fix (SetYawSpeed read the old activity) brought a
                                                                                                                                              bug of its own: `oldActivity = NewActivity`, so the
                                                                                                                                              walk/run frame test compares the new activity with
                                                                                                                                              itself and idle-to-walk keeps the old frame
0x783e0   monsters.cpp            CBaseMonster::ChangeYaw            turn step from the time since the last turn (343bbb2,           fix      frametime is the server frame, not the monster's
                                                                     halflife#2458; 95fdebe)                                                  interval, so turn speed followed the server's fps
0x91970   plats.cpp               CFuncTrackTrain::UpdateSound       fabs for abs on a float (e19fe47)                               fix      the C abs() on a float truncates to int (type bug)
0xd51b0   subs.cpp                CBaseToggle::LinearMoveDone        moves on at speed 100 while > 1/32 unit short (seed)            unclear  SDK's snap isn't evidently wrong; the creep has no
                                                                                                                                              stated bug and changes every mover's arrival
0xd6cb0   talkmonster.cpp         CTalkMonster::TakeDamage           skips a friend not DEAD_NO (e306d7a)                            fix      reset a dying friend's death schedule
0x67870   houndeye.cpp            CHoundeye::GetScheduleOfType       idle leader gets slHoundGuardPack 1 in 15 (Marphy)              design   restores unused content
0x68800   ichthyosaur.cpp         CIchthyosaur::GetScheduleOfType    SCHED_DIE when DEAD_DEAD goes to slFloat (a47bddd, #152)        fix      the death sequence restarted
0x69360   ichthyosaur.cpp         CIchthyosaur::ChangePitch          step from the time since the last call (343bbb2)                fix      fixed 0.1 s step whatever the call rate (time base)
0x69400   ichthyosaur.cpp         CIchthyosaur::ChangeYaw            same, for the roll                                              fix      same
0x79f20   monsterstate.cpp        CBaseMonster::RunAI                gag test on spawnflags (15c059d, halflife#3076)                 fix      SDK tested pev->flags: gagged monsters talked,
                                                                                                                                              swimmers (FL_SWIM) never did (Steven)
0x808d0   nihilanth.cpp           CNihilanth::Spawn                  FL_FLY (29e93ca, halflife#3228)                                 fix      as the tentacle: the interpolation is in halflife-
                                                                                                                                              updated's AddToFullPack, not RC2's; alone FL_FLY only
                                                                                                                                              changes engine movement
0x83840   nihilanth.cpp           CNihilanth::CommandUse             no enemy (notarget): touches the first live player (seed)       fix      the ending never fired without an enemy
0x8c770   osprey.cpp              COsprey::Spawn                     FL_FLY (2bf0ea1, halflife#3228)                                 fix      as nihilanth's
0x8c770   osprey.cpp              COsprey::Spawn                     max_health (765c1f5, halflife#3261)                             design   osprey AI trigger rework (Steven)
0x8d320   osprey.cpp              COsprey::HoverThink                Update(): Look, Listen, FCheckAITrigger (765c1f5)               design   same
0x8d610   osprey.cpp              COsprey::FlyThink                  same (m_pGoalEnt guard stays)                                   design   same
0x8dc00   osprey.cpp              COsprey::Killed                    deadflag DEAD_DYING (765c1f5)                                   design   same
0x8ddb0   osprey.cpp              COsprey::DyingThink                Update() (765c1f5)                                              design   same
-         osprey.cpp              COsprey::TakeDamage, Update        the attacker becomes the enemy (765c1f5)                        design   same
0x8e780   osprey.cpp              COsprey::TraceAttack               smoke puff on the engine that was hit (seed)                    fix      SDK puffed the other engine (wrong field)
0xe4930   triggers.cpp            PlayCDTrack                        track -1 sends "cd stop", not "cd pause" (seed)                 unclear  a bug only if today's engine resumes a paused track
                                                                                                                                              (cd resume) on its own; not tested
0xe5b90   triggers.cpp            CChangeLevel::Spawn                error line for a changelevel to the current map (b9adea9)       design   an added diagnostic
0xe7860   triggers.cpp            CTriggerCamera::FollowTarget       avelocity * 0.01 for * frametime (91944d1)                      fix      frametime applied twice to a per-second velocity
0xfb0e0   weapons.cpp             CBasePlayerWeapon::IsUseable       secondary ammo, WEAPON_NOCLIP, CanDeploy (a9b1d9c               fix      MP5 auto-switched away with grenades left;
                                                                     halflife#3001, c4a67d4 #160)                                             tripmine-to-RPG switch broke
0xfb330   weapons.cpp             CBasePlayerWeapon::PlayEmptySound  EMIT_SOUND_PREDICTED (063b8a5, halflife#3231)                   fix      the predicting shooter heard it twice; cl_lw 0 heard it
0xfb740   weapons.cpp             CBasePlayerWeapon::RetireWeapon    deferred to DoRetireWeapon (c4a67d4, #160)                      fix      wrong animation on the auto-switch
0x9d4c0   player.cpp              CBasePlayer::GiveAmmo              gives the exhaustible weapon with its ammo (6951bf1, #153 #157) fix      ammo for grenades, satchels... without the weapon
                                                                     (+ AddAmmoNameToAmmoRegistry's weapon name, ffa8e5b)                     (Steven)
0x9d150   player.cpp              CBasePlayer::AddPlayerItem         insert before ExtractAmmo, then SendSingleAmmoUpdate (6951bf1)  fix      the pickup flow GiveAmmo's fix needs (ownership
                                                                                                                                              checks see the weapon); CBasePlayerWeapon::AddToPlayer
                                                                                                                                              no longer extracts
0x9d150   player.cpp              CBasePlayer::AddPlayerItem         WeapPickup for every non-exhaustible weapon unless spawning     design   which weapons announce a pickup is the original's
                                                                                                                                              (the eight AddToPlayer below; Steven); m_bIsSpawning
                                                                                                                                              stays out
0x2ae70.. crossbow, egon, gauss,  ::AddToPlayer                      WeapPickup moved to AddPlayerItem                               design   same
          mp5, python, rpg,
          shotgun, hornetgun
0x9c180   player.cpp              CBasePlayer::SelectItem            m_ForceSendAnimations (42f45bd, #154)                           fix      as SelectLastItem
0x9eac0   player.cpp              CBasePlayer::SwitchWeapon          same                                                            fix      same
0xfaf30   weapons.cpp             CBasePlayerWeapon::SendWeaponAnim  sends to the predicting client while m_ForceSendAnimations     fix      same
0xc3df0   scripted.cpp            CScriptedSentence::AcceptableSpeaker  a non-monster that IsAllowedToSpeak speaks (func_button)     design   what scripted_sentence does (Steven)
0x17af0   buttons.cpp             CBaseButton::ButtonSpark           next spark on pev->ltime (#1681), at absmin (d7dd2a8,           fix      a pusher thinks on ltime, so sparks came late or not
                                                                     halflife#3269)                                                           at all; sparks at the model's origin corner
0x17f30   buttons.cpp             CBaseButton::ButtonBackHome        no spark restart for func_rot_button (0d101ca, halflife#3270);  fix      its X Axis flag is the spark bit; ltime as above
                                                                     ltime
0x40f80   flyingmonster.cpp       CFlyingMonster::ChangeYaw          roll step from the time since the last call (343bbb2), then     unclear  the time base fix is a fix, but upstream's code steps
                                                                     220 * 220 * delta (95fdebe, HL25)                                        by 220 * 220 * delta, so the roll snaps; restoring it
                                                                                                                                              brings that in. 343bbb2's 220 * delta is neither
0x41ed0   func_break.cpp          CBreakable::KeyValue               "explosion" "1" is directed (50e1811, #24)                      fix      the FGD's value never selected directed
0x42d40   func_break.cpp          CBreakable::Die                    no second break once SOLID_NOT (473e9ea)                        fix      double gibs and items
0x42d40   func_break.cpp          CBreakable::Die                    fabs(health) in the volume (e19fe47)                            fix      as UpdateSound
0x42d40   func_break.cpp          CBreakable::Die                    directed gibs along -g_vecAttackDir (50e1811)                   unclear  right for a break by damage (g_vecAttackDir points at
                                                                                                                                              the attacker), but Use sets it to the breakable's
                                                                                                                                              angle, so a triggered break now throws them backwards
0x7e090   multiplay_gamerules.cpp CHalfLifeMultiplay::RefreshSkillData  357 damage 50 (ebaf86d, HL25)                                design   balance (Steven)
```

Same fix, outside the audits' rows (mod-rc2's earlier reconstructions of e19fe47, restored so both
projects match mod-rockcrowbar's r3 and r6 rows):

```
0x136f0   bmodels.cpp             CFuncRotating::RampPitchVol        fabs (e19fe47)                                                  fix      as UpdateSound (IntAbs)
0x137f0   bmodels.cpp             CFuncRotating::SpinUp              fabs (e19fe47)                                                  fix      same
0xb2d60   roach.cpp               CRoach::MonsterThink               fabs (e19fe47)                                                  fix      same
```

Counts (rows above, a split row counted once per part): fix 34 (31 audit rows and the 3 above), design 21,
unclear 8.

## mod-rockcrowbar (verdicts only; applied after the go-ahead)

rva is the `.so`'s, as `sdk-drift.tsv`. "RC's own" parts of a play+missed row stay whatever the verdict.

```
rva        file                     function                           what halflife-updated changed                                verdict  reason
0x1cacf0   monsterstate.cpp         CBaseMonster::RunAI                gag test on spawnflags (15c059d)                             fix      as mod-rc2
0x1eabc0   player.cpp               CBasePlayer::PackDeadPlayerItems   reloads the active weapon before packing (9d4ce15, HL25)     design   a dropped gun comes loaded: gameplay
0x1eabc0   player.cpp               CBasePlayer::PackDeadPlayerItems   packs nothing for an empty active satchel (e0ba8ce, HL25)    fix      a box with an empty satchel gave a weapon with nothing
                                                                                                                                         to throw
0x1eaf20   player.cpp               CBasePlayer::RemoveAllItems        lets go of a func_tank (seed)                                fix      the tank stayed controlled by a stripped player (mod-
                                                                                                                                         rc2 keeps it)
0x1eaf20   player.cpp               CBasePlayer::RemoveAllItems        empty CurWeapon dropped (83c18d3, halflife#692)              design   the fix moved it into UpdateClientData, which this tree
                                                                                                                                         doesn't have; dropping it alone brings #692 back (mod-
                                                                                                                                         rc2 keeps the message too)
0x1eb6a0   player.cpp               CBasePlayer::Killed                zeroes the death-frame count (ac6b664, HL25 corpses)         design   HL25's multiplayer corpse change
0x1ec080   player.cpp               CBasePlayer::WaterMove             drown damage counts health lost (3718cf2)                    fix      god mode gained health on surfacing
0x1ec080   player.cpp               CBasePlayer::WaterMove             swim sounds only with the head under (c2902f8,               fix      breathing sounds in shallow water
                                                                       halflife#3110)
0x1ec080   player.cpp               CBasePlayer::WaterMove             no server water friction (SDK 2.2+)                          fix      pm_shared applies it too: twice on the server, and the
                                                                                                                                         client mispredicts
0x1f02a0   player.cpp               CBasePlayer::UpdatePlayerSound     UTIL_MakeVectors / v_forward.z = 0 commented out (SDK 2.2+)  unclear  a global side effect either way; no bug stated, and
                                                                                                                                         later code may read the vectors (RC's sv_ignoreplayer
                                                                                                                                         stays)
0x1f0520   player.cpp               CBasePlayer::PostThink             m_afButtonLast set on every path (seed)                      fix      the early exits left it stale, so held buttons read as
                                                                                                                                         pressed again
0x1f2c70   player.cpp               CBasePlayer::ImpulseCommands       impulse 204 dropped (SDK 2.2+)                               design   a removed command (fullupdate replaces it)
0x1f4140   player.cpp               CBasePlayer::GiveAmmo              gives the exhaustible weapon with its ammo (6951bf1)         fix      as mod-rc2 (RC's count test stays; the pickup flow
                                                                                                                                         is already halflife-updated's here)
0x1f42b0   player.cpp               CBasePlayer::ItemPostFrame         returns when PlayerUse just started a tank (4fe7e6e,         fix      the weapon fired on the frame the tank was taken
                                                                       halflife#3345)
0x1f4fd0   player.cpp               CBasePlayer::GetAutoaimVector      ResetAutoaim when sv_aim is 0 (beb9f35, halflife#966)        fix      autoaim angles stuck when disabled on a target
0x1f4fd0   player.cpp               CBasePlayer::GetAutoaimVector      sv_allow_autoaim test (c567ed8, HL25)                        design   a new cvar
0x1f53d0   player.cpp               CBasePlayer::AutoaimDeflection     sv_allow_autoaim test (c567ed8)                              design   same
0x1f5b30   player.cpp               CBasePlayer::DropPlayerItem        refuses when no next weapon (seed)                           design   what a player may drop; neither is buggy
0x1090c0   combat.cpp               CBaseEntity::FireBullets           no tracer flag (b0e5d2d, #163), no player bullet cases       design   SDK's "tracers don't decal" is deliberate, and RC's
                                                                       (SDK 2.2+), buckshot decals                                       callers need the player damages
0x11b980   doors.cpp                CBaseDoor::Blocked                 stops the moving sound before relinking (SDK 2.2+)           fix      CHAN_STATIC loops stacked on a blocked reverse (mod-
                                                                                                                                         rc2's SDK 2.3 has it)
0x11c5a0   doors.cpp                CMomentaryDoor::Use                as mod-rc2                                                   fix      as mod-rc2
0x1fb430   roach.cpp                CRoach::Spawn                      2 units tall, not 1 (SDK 2.2+)                               unclear  Valve's hull change with no stated reason
0x1fb700   roach.cpp                CRoach::MonsterThink               fabs (e19fe47)                                               fix      as mod-rc2
0x1c9170   monsters.cpp             CBaseMonster::ReportAIState        "PlayDead" in the names                                      fix      off by one from PLAYDEAD on (mod-rc2 keeps it)
0x21d9e0   subs.cpp                 CBaseToggle::LinearMoveDone        as mod-rc2                                                   unclear  as mod-rc2
0x18a2c0   func_break.cpp           CBreakable::KeyValue               as mod-rc2                                                   fix      as mod-rc2
0x18b440   func_break.cpp           CBreakable::Die                    re-break, fabs                                               fix      as mod-rc2
0x18b440   func_break.cpp           CBreakable::Die                    gib direction                                                unclear  as mod-rc2
0x18c3c0   func_break.cpp           CPushable::Move                    HL25 pushable rework, sv_pushable_fixed_tick_fudge (4f0d502) design   new cvar, changed push controls and speed limits
                                                                                                                                         (mod-rc2's is RC2's own)
0x2261f0   triggers.cpp             CTargetCDAudio::Think              UTIL_GetLocalPlayer (f2c8e23, a refactor)                    design   no bug fixed; with slot 1 empty it stopped thinking
                                                                                                                                         for good (mod-rc2 has SDK's too)
0x226730   triggers.cpp             CBaseTrigger::HurtTouch            no healing a dead player in multiplayer (6ae5366, HL25)      fix      healed corpses (RC's CLIENTONLYFIRE test stays)
0x227400   triggers.cpp             CChangeLevel::Spawn                as mod-rc2                                                   design   as mod-rc2
0x229790   triggers.cpp             CTriggerCamera::FollowTarget       as mod-rc2                                                   fix      as mod-rc2
0x0f2c40   bmodels.cpp              CFuncRotating::RampPitchVol        fabs (e19fe47)                                               fix      as mod-rc2
0x0f2d90   bmodels.cpp              CFuncRotating::SpinUp              fabs (e19fe47)                                               fix      as mod-rc2
0x0f8460   buttons.cpp              CBaseButton::ButtonSpark           as mod-rc2                                                   fix      as mod-rc2
0x0f89f0   buttons.cpp              CBaseButton::ButtonBackHome        as mod-rc2                                                   fix      as mod-rc2
0x0f93f0   buttons.cpp              CMomentaryRotButton::Use           sends the value of the predicted destination (SDK 2.2+)      fix      retriggering sent the target the wrong way (mod-rc2's
                                                                                                                                         is RC2's own change, stays)
0x17f480   effects.cpp              CBlood::BloodPosition              UTIL_GetLocalPlayer                                          fix      SDK bled at a free slot 1's stale origin (mod-rc2
                                                                                                                                         keeps it)
0x18df40   func_tank.cpp            CFuncTank::StartControl            clears the view model (seed)                                 fix      the holstered weapon's view model stayed up (mod-rc2
                                                                                                                                         keeps it)
0x18e020   func_tank.cpp            CFuncTank::StopControl             EquipWeapon (3c1e84a, #134)                                  fix      an empty weapon came back invisible (mod-rc2 keeps it)
0x1ce970   multiplay_gamerules.cpp  CHalfLifeMultiplay::RefreshSkillData  357 damage 50                                            design   as mod-rc2
0x1d3c20   nihilanth.cpp            CNihilanth::CommandUse             as mod-rc2                                                   fix      as mod-rc2
```

Same fix outside the rows: `CFuncTrackTrain::UpdateSound` (plats.cpp, orig 0x1e67c0, r5's RC-own
row) gets fabs back and keeps RC's 0-255 clamp.

Counts: fix 28 (27 rows and UpdateSound), design 12, unclear 4.

## Unclear, for Steven

- (decided, Steven 2026-11-07: restored in both, with the AddToFullPack side) `CTentacle::Spawn`, `CNihilanth::Spawn`, `COsprey::Spawn` (mod-rc2): halflife-updated's interpolation
  fix (halflife#3228) lives in its AddToFullPack, which RC2's own AddToFullPack doesn't have. Restore
  the flags with that AddToFullPack change, or leave both out.
- `CFlyingMonster::ChangeYaw` (mod-rc2): restore upstream's (time base, but 220 * 220 * delta, the roll
  snaps), restore 343bbb2's 220 * delta, or keep SDK 2.3's.
- `CBaseMonster::SetActivity` (mod-rc2): restore upstream's as it is (its `oldActivity` is the new
  activity), restore it with `oldActivity = m_Activity` (what the commit meant), or keep SDK 2.3's.
- `CBreakable::Die`'s gib direction (both): halflife-updated's negation fixes damage breaks and
  reverses triggered ones; restoring it would want Use's direction negated too.
- `CBaseToggle::LinearMoveDone` (both): the 2013 SDK's creep at speed 100, or the original's snap.
- `PlayCDTrack` (mod-rc2): "cd stop" or "cd pause" for track -1; decided by whether today's engine
  resumes a paused track by itself.
- `CBasePlayer::UpdatePlayerSound` (mod-rockcrowbar): SDK 2.1's UTIL_MakeVectors side effect.
- `CRoach::Spawn` (mod-rockcrowbar): 1 or 2 units tall.

## mod-rc2 functions outside the audits with a halflife-updated change that a mod-rockcrowbar verdict calls design

Not reverts (RC2's own functions, reconstructed keeping halflife-updated's code), so not changed here:
`CBasePlayer::PackDeadPlayerItems` (reload before packing), `CBasePlayer::Killed` (death-frame count),
`CBasePlayer::DropPlayerItem` (refusal), `CBasePlayer::UpdatePlayerSound` (vectors commented out).
