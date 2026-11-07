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
0x1eb6a0   player.cpp               CBasePlayer::Killed                zeroes the death-frame count (ac6b664, HL25 corpses)         fix      HL25's multiplayer corpse change
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

### mod-rockcrowbar wave 2 (review of 2026-11-07, applied)

RC 1.9's wave-2 packages (the SDK files RC changed, `reference/dossiers/w2-*.md`) were rebuilt
before Steven's rule, under a brief that put the original's behavior back wherever it differed.
Each place they replaced halflife-updated's code in a stock function, read against SDK 2.1 (a
difference SDK 2.1 already has is RC's own, not a revert) and the wave-2 merge commits 8e0edbe,
1b8034e, e29e0c5 and 8aa67cf. "-" is a table or member, not a function.

```
rva        file                     function                           what halflife-updated changed                                verdict  reason
0x239e50   weapons.cpp              CBasePlayerItem::FallThink         think stops once a player holds it (e0ba8ce, HL25)           fix      as ad50584: could run while attached and give twice
0x23a730   weapons.cpp              CBasePlayerItem::AttachToPlayer    SetThink(NULL) (ad50584)                                     fix      double pickup removed the weapon and crashed
0x239fc0   weapons.cpp              CBasePlayerItem::AttemptToMaterialize  nextthink is FlWeaponTryRespawn's absolute time (f14827a) fix      the original added the time twice: respawn far off
0x23ac50   weapons.cpp              CBasePlayerWeapon::IsUseable       as mod-rc2                                                   fix      as mod-rc2
0x23af90   weapons.cpp              CBasePlayerWeapon::SecondaryAmmoIndex  m_iSecondaryAmmoType, not -1 (34ce0e3, halflife#3029)    fix      a secondary-ammo weapon read m_rgAmmo[-1] (the mp5's
                                                                                                                                         override stays)
0x23b250   weapons.cpp              CBasePlayerWeapon::ExtractAmmo     |= the secondary result (d42b905, halflife#3027)             fix      the weapon was left behind when its ammo was taken
0x23b370   weapons.cpp              CBasePlayerWeapon::RetireWeapon    as mod-rc2                                                   fix      as mod-rc2
0x23af00   weapons.cpp              CBasePlayerWeapon::PlayEmptySound  as mod-rc2                                                   unclear  see the list below
0x23a250   weapons_shared.cpp       CBasePlayerWeapon::ItemPostFrame   TabulateAmmo, m_flLastFireTime (SDK 2.2+)                    design   prediction bookkeeping; nothing reads it here
0x19c7d0   ggrenade.cpp             CGrenade::Explode                  source 1 unit lower against RadiusDamage's + 1 (d6accf1,     fix      damage through the brush a grenade hit from below
                                                                       halflife#3244)                                                            (RC's own pull-out distance stays)
0x19d500   ggrenade.cpp             CGrenade::BounceTouch              as mod-rc2                                                   fix      as mod-rc2
0x19dc60   ggrenade.cpp             CGrenade::ShootTimed               as mod-rc2                                                   fix      as mod-rc2 (after RC's later SET_MODEL)
0x1c4e00   monsters.cpp             CBaseMonster::MonsterUse           no MONSTERSTATE_ALERT (08975b8, halflife#3061)               fix      resurrected a dying monster (mod-rc2 keeps it)
0x1c6080   monsters.cpp             CBaseMonster::SetActivity          as mod-rc2                                                   fix      as mod-rc2 (Steven's oldActivity = m_Activity)
0x1c7e30   monsters.cpp             CBaseMonster::IRelationship        HUMANPASSIVE fears as R_HT (75bf517)                         fix      R_FR does nothing (mod-rc2 keeps it); RC's PLAYER
                                                                                                                                         row stays
0x1c8730   monsters.cpp             CBaseMonster::ChangeYaw            as mod-rc2                                                   fix      as mod-rc2
0x1c9570   monsters.cpp             CBaseMonster::KeyValue, DropItem   allow_item_dropping (b624a61)                                design   a new keyvalue
0x1cab20   basemonster.h            CBaseMonster::CanPlaySentence,     no speaking while dying unless scripted (85467c5 #171,       fix      dying monsters spoke scripted sentences; RC's
           monsters.cpp, talk-      IsAllowedToSpeak, PlaySentence     5ee9711 #180)                                                             sentence code stays in CBaseMonster
           monster.cpp              (0x1c9e00), CTalkMonster::
                                    PlaySentence (0x21fae0)
-          (same)                   sentences in CBaseToggle, CCycler  HL25 sentence rework (3d8b933)                               design   restructure and a cycler that speaks
                                    IsAllowedToSpeak
0x21e5e0   talkmonster.cpp          CTalkMonster::RunTask              player looked up once, task fails without one (55a1e89,      fix      stare read an unset player; free slot 1 (as
                                                                       halflife#3177; f2c8e23)                                                   BloodPosition)
0x21fcd0   talkmonster.cpp          CTalkMonster::GetScheduleOfType    UTIL_GetLocalPlayer (f2c8e23)                                fix      watched a free slot 1's stale origin (as BloodPosition)
0x21f190   talkmonster.cpp          CTalkMonster::Touch                push only when the schedule allows it (d667a24)              fix      turned a talker away from a scripted facing
0x21fc30   talkmonster.cpp          CTalkMonster::TakeDamage           as mod-rc2                                                   fix      as mod-rc2
0x2203f0   talkmonster.cpp          CTalkMonster::CanFollow            ideal state SCRIPT too (3f68a77, #200)                       fix      +use broke a scripted_sequence
0x2069c0.. scientist.cpp            CScientist::Scream, StartTask      as mod-rc2 (Marphy)                                          design   as mod-rc2
           (0x206a90), RunTask
           (0x206d30)
0x207ba0   scientist.cpp            CScientist::GetSchedule,           Marphy disregard-enemy changes                               design   Marphy content, mod-rc2 has SDK 2.3's too
                                    GetIdealState
0x207490   scientist.cpp            CScientist::TalkInit               % NUM_SCIENTIST_HEADS (4, HL25's head)                       design   mod-rc2 keeps % 3 too
0x2076d0   scientist.cpp            CScientist::ISoundMask             smells meat, carcass, garbage (SDK 2.2+)                     unclear  see the list below
0x2073f0.. scientist.cpp            CScientist::Precache, Spawn,       _sv_override_scientist_mdl                                   unclear  see the list below
                                    CDeadScientist, CSittingScientist
-          houndeye.cpp             HOUNDEYE_EYE_FRAMES 3, not 4       (e5f9c39, Marphy)                                            fix      houndeye.mdl has 3 skin families: skin 3 isn't
                                                                                                                                         there (mod-rc2 keeps 3)
0x1b6870   houndeye.cpp             CHoundeye::GetScheduleOfType,      as mod-rc2 (leaderlook, c718785)                             design   as mod-rc2
                                    tlHoundGuardPack
0x1ad4b0   hgrunt.cpp               CHGrunt::HandleAnimEvent           drop only while holding a gun (bc7d6e3)                      fix      dropped it again after a save and load
-          hgrunt.cpp               tlGruntVictoryDance                TASK_SET_ACTIVITY ACT_IDLE first (ff37029)                   fix      kept firing after the last enemy died
0x10b590   controller.cpp           CController::RunTask               faces only with an enemy (271d503)                           fix      turned to a stale spot out of combat
0x1947b0   gargantua.cpp            CStomp::Think (+ save table)       time since the last think (cb09dc7, halflife#2876)           fix      the stomp followed the server's frame rate
0x1ba8e0.. islave.cpp               CISlave::ArmBeam, WackBeam         SF_BEAM_TEMPORARY (ac808a8, halflife#3104)                   fix      beams stayed forever over a level change
           (0x1badf0), ZapBeam
           (0x1bafa0)
0x242750   zombie.cpp               CZombie::IdleSound, AttackSound    as mod-rc2                                                   fix      as mod-rc2
           (0x2427e0)
0x1b7a90   ichthyosaur.cpp          CIchthyosaur::GetScheduleOfType    as mod-rc2 (with slFloat, StartTask 0x1b7bb0, SetActivity)   fix      as mod-rc2 (RC's corpse fade stays)
0x1b85c0   ichthyosaur.cpp          CIchthyosaur::ChangePitch,         as mod-rc2                                                   fix      as mod-rc2
                                    ChangeYaw (0x1b8670)
0x1898b0   flyingmonster.cpp        CFlyingMonster::ChangeYaw          as mod-rc2                                                   fix      as mod-rc2 (343bbb2's 220 * delta)
0x1e2570   osprey.cpp               COsprey::TraceAttack               as mod-rc2                                                   fix      as mod-rc2
0x1e10b0   osprey.cpp               COsprey::Flight                    no move while m_dTime is 0 (2e0607e, halflife#3260)          fix      flew to a NaN origin with no target
0x1dfd00.. osprey.cpp               COsprey Spawn max_health, Hover-   as mod-rc2 (765c1f5)                                         design   as mod-rc2
                                    Think, FlyThink, Killed, DyingThink
0x0e8900.. apache.cpp               CApache (398591d)                  as mod-rc2                                                   design   as mod-rc2
0x1dae90   nodes.cpp                CTestHull::DropDelay               as mod-rc2                                                   design   as mod-rc2
0x20b670   scripted.cpp             CScriptedSentence::AcceptableSpeaker  as mod-rc2                                                design   as mod-rc2
0x0e3770   aflock.cpp               CFlockingFlyer::SpawnCommonCode    flocks take damage, health 10 (ab037f6)                      design   whether flyers can be shot
0x0e56d0   agrunt.cpp               CAGrunt::HandleAnimEvent,          ag_fire moved from the hornet to the agrunt (SDK 2.2+)       design   one sound per hornet either way (moved for the
           hornet.cpp               CHornet::Spawn (0x1af5d0)                                                                                    predicted hivehand)
-          h_cycler.cpp             CWreckage::m_flStartTime           float, not int (cb51706)                                     fix      a time kept in an int (type bug)
0x1ab180   healthkit.cpp            CHealthKit::MyTouch                no kit for a dead player (SDK 2.2+)                          fix      as HurtTouch: the dead took health (rc.ini amount
                                                                                                                                         stays)
0x1ec5b0   player.cpp               CBasePlayer::PlayerDeathThink      4 s, not 120 think frames (ac6b664, HL25)                    fix      the frames followed the client's command rate
0x1ec5b0   player.cpp               CBasePlayer::PlayerDeathThink      grounded corpse MOVETYPE_NONE (SDK 2.2+)                     fix      the body went to a player's head walking over it
0x1ec5b0   player.cpp               CBasePlayer::PlayerDeathThink      body copied at once in multiplayer (ac6b664, with            design   HL25's corpses (RC fades its own)
                                                                       CopyToBodyQue's modelindex test)
0x1ef420   player.cpp               CBasePlayer::PreThink              m_bResetViewEntity (5fd0acb, halflife#3031)                  fix      a camera's view lost on a save load (singleplayer)
0x1f44a0   player.cpp               CBasePlayer::UpdateClientData      health held to a short (5a0c6c1, #2149), 0-1 sent as 1       fix      wrap past 32767; a live player's HUD read 0
                                                                       (SDK 2.2+)
0x1eb6a0   player.cpp               CBasePlayer::Killed                holsters the active weapon (SDK 2.2+)                        unclear  see the list below
0x1eb6a0   player.cpp               CBasePlayer::Killed                pev->modelindex = player model, "don't use eyes" (SDK 2.2+)  unclear  see the list below
0x0fd490   client.cpp               ClientDisconnect                   lets go of a func_tank (de2751c, halflife#2594)              fix      as RemoveAllItems' tank release
0x102cc0   client.cpp               ClientUserInfoChanged              % in a new name becomes a space (SDK 2.2+)                   fix      a name worked as a format string where printed
0x0fd7e0   client.cpp               Host_Say, ClientPutInServer,       chat flood timer, spectator, voice mute, log lines           design   features (spectator mode, logs)
                                    ClientCommand
-          multiplay_gamerules.cpp  Think, GoToIntermission, FShould-  mp_chattime, cl_autowepswitch, func_vehicle credit, log      design   features
                                    SwitchWeapon, PlayerKilled, ...    lines, motdfile and ServerName
0x1cf840   multiplay_gamerules.cpp  CHalfLifeMultiplay::DeathNotice    HLTV director event (SDK 2.2+)                               unclear  see the list below
0x220d90.. teamplay_gamerules.cpp   CHalfLifeTeamplay::InitHUD,        TeamNames, ScoreInfo on a team change, TeamInfo resend       unclear  see the list below
                                    ChangePlayerTeam (0x221020),       (SDK 2.2+)
                                    RecountTeams (0x221b70)
0x1a7810   handgrenade.cpp          CHandGrenade::Holster              clears m_flStartThrow (70dd419, halflife#3251)               fix      a primed grenade was thrown on the next deploy
0x21b280   squeakgrenade.cpp        CSqueak::Deploy                    first idle at 1.7 s (33385ad, halflife#2495)                 fix      cut the deploy animation short
0x2036e0   satchel.cpp              CSatchel::Deploy                   first idle at 2 s (33385ad)                                  fix      same commit (RC's own models and rc.ini delay stay)
0x2010c0   rpg.cpp                  CRpg::WeaponIdle                   fidget idle 6.1 s, not 3 (33385ad)                           fix      cut the fidget short
0x210150   shotgun.cpp              CShotgun::PrimaryAttack, Weapon-   pump on the last shell too, played from an ItemPostFrame     fix      no pump while holding attack or on the last shell
                                    Idle (0x2118f0), ItemPostFrame     override (a65846e, halflife#617)
-          satchel, rpg, squeak-    HL25 satchel controls and live     (1c202f5, fd97136, 2353ca4, e9986c0)                         design   features and balance
           grenade                  limit, RPG NOAUTOSWITCHTO, snark
                                    throw and world-brush test
-          crossbow, hornetgun      CCrossbow::Reload's ammo_bolts,    (SDK 2.2+, f347954)                                          design   prediction mirrors
                                    CHgun::ReloadAmmo's TabulateAmmo
```

Not reverts (RC 1.9's own, since SDK 2.1 already has halflife-updated's code there):
`CBasePlayerWeapon::UpdateClientData` and `SendWeaponAnim` (SDK 2.1 resends only the old and new
weapon and sets weaponanim; the original does neither), `CHalfLifeMultiplay::ChangeLevel` (SDK
2.1 has the mapcycle_t code), and the weapon bodies RC rewrote whole (the gauss, tripmine laser,
egon, crowbar, python, mp5, glock fire code), whose halflife-updated fixes are for code RC
dropped.

Counts (rows above, a multi-function row once): fix 45, design 18, unclear 7.

Unclear, for Steven (kept as the original until decided):
- `CBasePlayerWeapon::PlayEmptySound`: mod-rc2's verdict is fix, but EMIT_SOUND_PREDICTED skips a
  client with cl_lw 1 on the assumption that it predicts the sound. This server has no
  CLIENT_WEAPONS and sends no weapon data, so such a client may hear nothing. Settle with an HL25
  client, cl_lw 1, dry-firing an empty weapon against our build.
- `CBasePlayer::Killed`'s Holster of the active weapon (SDK 2.2+, "to allow it to cleanup"): no
  stated bug, and RC's Holster bodies have side effects (snark cam, zoom). Settle by reading what
  each RC Holster would do on a death, or keep.
- `CBasePlayer::Killed`'s modelindex reset to the player model ("don't use eyes", SDK 2.2+): no
  stated bug; matters only for a player whose model index was changed (RC's observer or drive
  states). Settle with RC's observer and drive deaths in an engine run.
- `CScientist::ISoundMask`'s scents (SDK 2.2+): Valve's AI change with no stated reason, as the
  roach's height was.
- `_sv_override_scientist_mdl` (four scientist functions): a feature by the rule, but Steven kept
  it in mod-rc2 (its DEVIATIONS.md). Parity or the original.
- `CHalfLifeMultiplay::DeathNotice`'s HLTV director event: a feature, but r7 kept the same message
  in `CBasePlayer::TakeDamage`.
- teamplay's TeamNames message, ScoreInfo on a team change and TeamInfo resend (InitHUD,
  ChangePlayerTeam, RecountTeams): today's client's scoreboard may want them, as it wanted the
  9-byte ScoreInfo. Settle with mp_teamplay 1 on an HL25 client.

## Unclear, decided by Steven 2026-11-07 (each line says how)

- (decided, Steven 2026-11-07: restored in both, with the AddToFullPack side) `CTentacle::Spawn`, `CNihilanth::Spawn`, `COsprey::Spawn` (mod-rc2): halflife-updated's interpolation
  fix (halflife#3228) lives in its AddToFullPack, which RC2's own AddToFullPack doesn't have. Restore
  the flags with that AddToFullPack change, or leave both out.
- (decided, Steven 2026-11-07: 343bbb2's 220 * delta, restored) `CFlyingMonster::ChangeYaw` (mod-rc2): restore upstream's (time base, but 220 * 220 * delta, the roll
  snaps), restore 343bbb2's 220 * delta, or keep SDK 2.3's.
- (decided, Steven 2026-11-07: restored with `oldActivity = m_Activity`) `CBaseMonster::SetActivity` (mod-rc2): restore upstream's as it is (its `oldActivity` is the new
  activity), restore it with `oldActivity = m_Activity` (what the commit meant), or keep SDK 2.3's.
- (decided, Steven 2026-11-07: the original's direction, both) `CBreakable::Die`'s gib direction (both): halflife-updated's negation fixes damage breaks and
  reverses triggered ones; restoring it would want Use's direction negated too.
- (decided, Steven 2026-11-07: the original's snap, both) `CBaseToggle::LinearMoveDone` (both): the 2013 SDK's creep at speed 100, or the original's snap.
- (decided, Steven 2026-11-07: the original's "cd pause" until today's engine is shown to resume it) `PlayCDTrack` (mod-rc2): "cd stop" or "cd pause" for track -1; decided by whether today's engine
  resumes a paused track by itself.
- (decided, Steven 2026-11-07: the original's call) `CBasePlayer::UpdatePlayerSound` (mod-rockcrowbar): SDK 2.1's UTIL_MakeVectors side effect.
- (decided, Steven 2026-11-07: halflife-updated's 2 units, restored) `CRoach::Spawn` (mod-rockcrowbar): 1 or 2 units tall.

## mod-rc2 functions outside the audits with a halflife-updated change that a mod-rockcrowbar verdict calls design

RC2's own functions, reconstructed keeping halflife-updated's code. Checked against RC2's decompile
afterwards (2026-11-07, same verdicts in both projects):
- `CBasePlayer::PackDeadPlayerItems` (orig 0x95280): RC2's has no reload before packing; removed in
  mod-rc2 (design, as mod-rockcrowbar). The empty-satchel check stays in both (fix).
- `CBasePlayer::DropPlayerItem` (orig 0x9e630): RC2's drops whatever GetNextBestWeapon returns; the
  refusal is Valve's 2013 SDK (halflife-updated's seed), what a player may drop: removed in mod-rc2
  (design, as mod-rockcrowbar).
- `CBasePlayer::Killed`: the respawn count reset at death is kept in both (fix; the SDK code it
  replaced reset m_iRespawnFrames there too, so it guards a stale count). mod-rockcrowbar's row 139
  was design; changed to fix and restored there.
- `CBasePlayer::UpdatePlayerSound`: the vectors stay as they are (mod-rockcrowbar keeps the original's
  call, decided by Steven).

## mod-ozdm

Oz Deathmatch 3.02 (`~/Projects/goldsrc.net/mod-ozdm`), built on SDK 2.3 as RC2 is, so a change
Valve made by SDK 2.3 is the original's and not a call. These are the calls the seven package
dossiers made (`reference/dossiers/*.md`, halflife-updated tables), gathered 2026-11-07. rva is the
`hl_i386.so` file address (`reference/funcfile.tsv`, the code's `// orig` tags); "-" where the dossier
names no single function. "as mod-rc2" or "as mod-rockcrowbar" marks a call that follows that
project's verdict for the same function. The bots package has none (all OZ's own files), and the egon
is OZ's own body, which replaces halflife-updated's whole. The seven calls the packages left unclear
follow Steven's rule of 2026-11-07 (unclear goes to halflife-updated's version, design reverts
already made stay) and are marked so.

```
rva       file                    function                           what halflife-updated changed                                   verdict  reason
0x9daa0   combat.cpp              CBaseEntity::FireBullets           no tracer flag, decals on every shot (b0e5d2d, #163)            design   as mod-rockcrowbar: SDK 2.3's restored
0x9daa0   combat.cpp              CBaseEntity::FireBullets           BULLET_PLAYER_BUCKSHOT case (c73c240)                           design   as mod-rockcrowbar ("no player bullet cases"); OZ's
                                                                                                                                              FireBulletsPlayer has the player types
0x7b370   airtank.cpp             CAirtank::Save, m_SaveData         m_state int to bool, FIELD_BOOLEAN (998469a)                    fix      a type refactor, as mod-rc2 and mod-rockcrowbar keep
                                                                                                                                              it; the original saves FIELD_INTEGER
0x13e0c0  items.cpp               CItemSuit::MyTouch and HasSuit     the suit bit in the 64-bit m_WeaponBits (c633af8)               fix      the same test; pev->weapons is mirrored
0xf15e0   player.cpp              CBasePlayer::PackDeadPlayerItems   reloads the active weapon before packing (9d4ce15, HL25)        design   as mod-rockcrowbar
0xf15e0   player.cpp              CBasePlayer::PackDeadPlayerItems   packs nothing for an empty active satchel (e0ba8ce, HL25)       fix      as mod-rockcrowbar
0xf1940   player.cpp              CBasePlayer::RemoveAllItems        lets go of a func_tank (seed)                                   fix      as mod-rockcrowbar
0xf1940   player.cpp              CBasePlayer::RemoveAllItems        empty CurWeapon moved to UpdateClientData (83c18d3)             design   as mod-rockcrowbar; the original's message is back
0xf1af0   player.cpp              CBasePlayer::Killed                non-solid corpse in multiplayer (df5605d, HL25)                 design   HL25's corpses
0xf1af0   player.cpp              CBasePlayer::Killed                zeroes the death-frame count (ac6b664)                          fix      as mod-rockcrowbar (row 139, changed to fix)
0xf2920   player.cpp              CBasePlayer::PlayerDeathThink      body copied when the death animation ends (ac6b664, HL25)       design   as mod-rockcrowbar
0xf2920   player.cpp              CBasePlayer::PlayerDeathThink      a time to respawn, not think frames (ac6b664)                   fix      as mod-rockcrowbar
0xf2920   player.cpp              CBasePlayer::PlayerDeathThink      spectator return through iuser1                                 design   OZ's observer is SDK 2.2's
0xf3120   player.cpp              CBasePlayer::StartObserver,        halflife-updated's spectator mode                               design   a feature; OZ's own observer reconstructed
                                  Observer_*
0xf4190   player.cpp              CBasePlayer::PreThink              m_bResetViewEntity (5fd0acb, halflife#3031)                     fix      as mod-rockcrowbar
0xf4190   player.cpp              CBasePlayer::PreThink              func_vehicle controls (HL25)                                    fix      (Steven 2026-11-07: unclear goes to
                                                                                                                                              halflife-updated); mod-rc2 and mod-rockcrowbar keep
                                                                                                                                              func_vehicle too (their DEVIATIONS.md)
0xf4190   player.cpp              CBasePlayer::PreThink              node graph pointers set here, not in Precache (seed)            design   the original sets them in Precache
0xf4f00   player.cpp              CBasePlayer::PostThink             m_afButtonLast set on every path (seed)                         fix      as mod-rockcrowbar
0xf23f0   player.cpp              CBasePlayer::WaterMove             drown damage counts health lost (3718cf2); swim sounds only     fix      as mod-rockcrowbar
                                                                     with the head under (c2902f8)
0xf4600   player.cpp              CBasePlayer::CheckTimeBasedDamage  fabs (e19fe47)                                                  fix      as mod-rc2
0xf59d0   player.cpp              EntSelectSpawnPoint                random walk by the spot count (b61c0be, HL25)                   design   RANDOM_LONG(1, 5) back; a balance change
0xf77d0   player.cpp              CBasePlayer::ImpulseCommands       impulse 204 dropped                                             design   as mod-rockcrowbar
0xf7a90   player.cpp              CBasePlayer::CheatImpulseCommands  texture type in impulse 107                                     design   the original prints the plain "Texture:" line
0xf7a90   player.cpp, client.cpp, CBasePlayer::CheatImpulseCommands, sv_cheats read on demand (04f8b91, halflife#2511)               fix      the cached value missed a later change (no FIXES
          world.cpp               ClientCommand (give, fov),                                                                                  row; the player dossier says as mod-rc2)
                                  CWorld::Spawn
0xf84a0   player.cpp              CBasePlayer::AddPlayerItem         insert before ExtractAmmo (6951bf1)                             fix      as mod-rc2
0xf84a0   player.cpp              CBasePlayer::AddPlayerItem         WeapPickup for every non-exhaustible weapon                     design   as mod-rc2
0xf8780   player.cpp              CBasePlayer::GiveAmmo              gives the exhaustible weapon with its ammo (6951bf1)            fix      as mod-rc2
0xf88f0   player.cpp              CBasePlayer::ItemPostFrame         returns when PlayerUse just started a tank (4fe7e6e)            fix      as mod-rockcrowbar
0xf9550   player.cpp              CBasePlayer::GetAutoaimVector      ResetAutoaim when sv_aim is 0 (beb9f35)                         fix      as mod-rockcrowbar
0xf9550   player.cpp              CBasePlayer::GetAutoaimVector,     sv_allow_autoaim (c567ed8)                                      design   as mod-rockcrowbar
                                  AutoaimDeflection (0xf9970)
0xfa0e0   player.cpp              CBasePlayer::DropPlayerItem        refuses when no next weapon (seed)                              design   as mod-rockcrowbar
0xf6d00   player.cpp              CBasePlayer::SelectItem,           m_ForceSendAnimations (42f45bd, #154)                           fix      as mod-rc2
                                  SwitchWeapon (0xfa540)
0xf8ae0   player.cpp              CBasePlayer::UpdateClientData      status bar, Weapons message, empty CurWeapon                    design   the original sends none of them
0xf8ae0   player.cpp              CBasePlayer::UpdateClientData      0-1 health sent as 1, resend after a restore                    fix      as mod-rockcrowbar for the health
0xf8ae0   player.cpp              CBasePlayer::UpdateClientData      Health as a short (5a0c6c1, #2149)                              fix      as mod-rockcrowbar
0xf8ae0   player.cpp              CBasePlayer::UpdateClientData,     room type sent from here, reset at spawn (bc3add9)              fix      goes with CEnvSound::Think below
                                  Spawn
0xf7770   player.cpp              CBasePlayer::ForceClientDllUpdate  extra resends                                                   fix      (Steven 2026-11-07: unclear goes to
                                                                                                                                              halflife-updated)
0xf5e70   player.cpp              CBasePlayer::Spawn                 m_flStartCharge reset                                           fix
0xf5e70   player.cpp              CBasePlayer::Spawn                 "bj" physinfo (sv_allowbunnyhopping), chat time                 fix      (Steven 2026-11-07: unclear goes to
                                                                                                                                              halflife-updated); at their defaults they do what
                                                                                                                                              the original does
0x950d0   client.cpp              ClientDisconnect                   lets go of a func_tank (de2751c, halflife#2594)                 fix      as mod-rockcrowbar
0x950d0   client.cpp              ClientDisconnect,                  null name guards (seed)                                         fix
                                  CHalfLifeMultiplay::InitHUD
                                  (0xdafc0)
0x95490   client.cpp              Host_Say                           Q_UnicodeValidate for the isprint loop (seed)                   fix
0x959e0   client.cpp              ClientCommand                      fullupdate, closemenus, the % filter on "Unknown command"       fix      as mod-rockcrowbar (per the rules dossier; no FIXES
                                                                     (seed)                                                                   row)
0x991a0   client.cpp              AddToFullPack                      MAX_EDICTS test; the FL_FLY/m_EFlags slerp rule                 fix      the slerp rule as decided for mod-rc2 (FIXES,
                                                                                                                                              decided list)
0x97480   client.cpp              ServerActivate                     loop bounds                                                     fix
0x9a370   client.cpp              UpdateClientData                   fov from m_iFOV (6f8aac9, halflife #3044)                      fix      one FOV; OZ's zooms set m_iFOV (Steven 2026-11-07:
                                                                                                                                              unclear goes to halflife-updated)
                                                                                                                                              m_iFOV, so m_iFOV wins
0x9a370   client.cpp              UpdateClientData                   m_WeaponBits sync                                               fix
0xda7a0   multiplay_gamerules.cpp CHalfLifeMultiplay ctor            no servercfgfile exec                                           fix      left to the engine (Steven 2026-11-07)
0xf0350   UserMessages.cpp        LinkUserMessages                   Health 2 bytes (5a0c6c1, halflife#2149)                         fix      as mod-rockcrowbar
0x12d3a0  world.cpp               CWorld::Spawn                      duplicate worldspawn guard                                      fix
0x12cc40  world.cpp               CopyToBodyQue                      modelindex 0 test (ac6b664)                                     fix      mod-rockcrowbar dropped it (see below)
0x131800  voice_gamemgr.cpp       CVoiceGameMgr::ClientCommand       vban sscanf check (3e9a8f0)                                     fix
0x134df0  pm_shared.cpp           PM_FlyMove                         reflects off the first plane only (854187a, HL25)               fix      mod-rc2 p6 restored the original (see below)
0x136690  pm_shared.cpp           PM_CheckStuck, PM_PlayerMove       nudge loop on a server, duck to get unstuck (854187a, 1d0831a)  fix      mod-rc2 p6 dropped the duck (see below)
                                  (0x138e80)
0x138ae0  pm_shared.cpp           PM_CheckParamters                  +use on the ground cuts maxspeed to a third (4f0d502, HL25)     fix      mod-rc2 p6 dropped it (see below)
0x139780  pm_shared.cpp           PM_Move                            friction reset in multiplayer too (1d5610c, halflife#1542)      fix      a func_friction's value stayed on the player
0x138010  pm_shared.cpp           PM_Jump                            no jump sound while frozen (f3810c7)                            fix      mod-rockcrowbar's server plays it (see below)
0x1398e0  pm_shared.cpp           PM_Init, PM_GetHullBounds (not in  hull bounds filled at init (c3c5576, halflife#3229)             fix
                                  the original)
0x137380  pm_shared.cpp           PM_LadderMove                      model trace from a cleared trace (f2f5ecc, halflife#3283)       fix      as mod-rc2 (its PM_TraceModel)
0x137f80  pm_shared.cpp           PM_PreventMegaBunnyJumping         sv_allowbunnyhopping (a1cf2d3)                                  fix      (Steven 2026-11-07: unclear goes to
                                                                                                                                              halflife-updated); at its default 0 it caps, as the
                                                                                                                                              original
0xda900   multiplay_gamerules.cpp CHalfLifeMultiplay::               357 damage 50 (ebaf86d, HL25)                                   design   as mod-rc2
                                  RefreshSkillData
0xbf930   gamerules.cpp           CGameRules::RefreshSkillData       sk_plr_hornet_dmg (460802c)                                     design   OZ's fixed 7
0xdad10   multiplay_gamerules.cpp CHalfLifeMultiplay::               cl_autowepswitch and prefs (e7bb9d7)                            design   as mod-rockcrowbar (features)
                                  FShouldSwitchWeapon,
                                  ClientUserInfoChanged
0x95490   client.cpp              Host_Say                           chat flood timer, "(SPEC)" chat, voice mute                     design   as mod-rockcrowbar
0x959e0   client.cpp              ClientCommand                      spectate, specmode, follownext                                  design   as mod-rockcrowbar
0x95440   client.cpp              ClientPutInServer                  iuser1/iuser2 reset                                             design   as mod-rockcrowbar
0x9a370   client.cpp              UpdateClientData                   OBS_IN_EYE data                                                 design   spectator feature
0x991a0   client.cpp              AddToFullPack                      EFLAG_FLESH_SOUND                                               design
0xc0160   gamerules.cpp           InstallGameRules                   game.cfg exec                                                   design
0xc0160   gamerules.cpp           InstallGameRules                   teamplay.value > 0 for mp_teamplay == 1                         design
0xdbf90   multiplay_gamerules.cpp CHalfLifeMultiplay::PlayerKilled   func_vehicle kill credit                                        design   as mod-rockcrowbar
0xda9c0   multiplay_gamerules.cpp CHalfLifeMultiplay::Think,         mp_chattime 1-120                                               design   as mod-rockcrowbar; OZ clamps to 10-45
                                  GoToIntermission (0xdd000)
0x9a940   client.cpp              AllowLagCompensation               lag compensation on                                             design   OZ returns 0
0x137380  pm_shared.cpp           PM_LadderMove                      maxspeed cap, ducked climb speed (seed)                         design   as mod-rc2 (p6)
0xe0b60   nodes.cpp               CTestHull::DropDelay               "Node Graph out of Date" center print commented out (seed)      design   as mod-rc2
0x11bc70  triggers.cpp            CLadder::KeyValue,                 pass-through overrides dropped (seed)                           design   restored; no behavior
                                  CTriggerPush::KeyValue (0x11bda0)
0xd6740   monsters.cpp            CBaseMonster::KeyValue, DropItem   allow_item_dropping (b624a61)                                   design   as mod-rockcrowbar
0xd4fa0   monsters.cpp            CBaseMonster::IRelationship        HUMANPASSIVE fears AMIL and APRED as R_HT/R_DL (75bf517)        fix      as mod-rockcrowbar (Steven)
0xd31f0   monsters.cpp            CBaseMonster::SetActivity          m_Activity set before SetYawSpeed (ec1f016)                     fix      as mod-rc2 (Steven's oldActivity = m_Activity)
0xd5870   monsters.cpp            CBaseMonster::ChangeYaw            turn step from the time since the last turn (343bbb2)           fix      as mod-rc2
0xb5980   flyingmonster.cpp       CFlyingMonster::ChangeYaw          roll step from the time since the last call (343bbb2, 95fdebe)  fix      as mod-rc2 (343bbb2's 220 * delta)
0xd1ed0   monsters.cpp            CBaseMonster::MonsterUse           no MONSTERSTATE_ALERT (08975b8)                                 fix      as mod-rockcrowbar
0xd6320   monsters.cpp            CBaseMonster::ReportAIState        "PlayDead" in the names (3dc5750)                               fix      as mod-rockcrowbar
0xd70d0   monsters.cpp,           CBaseMonster::PlaySentence,        IsAllowedToSpeak, no speaking while dying (85467c5)             fix      as mod-rockcrowbar
          talkmonster.cpp         CTalkMonster::PlaySentence
                                  (0x113420)
0xd70d0   monsters.cpp,           CBaseMonster::PlaySentence,        sentences moved to CBaseToggle (3d8b933, HL25)                  fix      kept; mod-rockcrowbar calls it design (see below)
          talkmonster.cpp,        PlayScriptedSentence,
          subs.cpp                SentenceStop,
                                  CTalkMonster::PlaySentence
0x119910  triggers.cpp            PlayCDTrack, CTargetCDAudio::Think UTIL_GetLocalPlayer (f2c8e23)                                   design   as mod-rockcrowbar; INDEXENT(1) back
                                  (0x119c80)
0xb01e0   effects.cpp,            CBlood::BloodPosition,             UTIL_GetLocalPlayer (f2c8e23)                                   fix      as mod-rockcrowbar: a free slot 1's stale edict, or
          triggers.cpp,           CMessage::Use (0xb1630),                                                                                    a NULL Instance
          talkmonster.cpp         CChangeLevel::ChangeLevelNow
                                  (0x11b150), CTriggerCamera::Use
                                  (0x11cf10), CTalkMonster tasks
0x119910  triggers.cpp            PlayCDTrack                        track -1 sends "cd stop" (seed)                                 design   as mod-rc2 (Steven: "cd pause")
0x11aed0  triggers.cpp            CChangeLevel::Spawn                error for a changelevel to the current map (b9adea9)            design   as mod-rc2
0x11a1e0  triggers.cpp            CBaseTrigger::HurtTouch            no healing a dead player in multiplayer (6ae5366)               fix      as mod-rockcrowbar
0x11d280  triggers.cpp            CTriggerCamera::FollowTarget       avelocity (91944d1; 66b1f30 reverted by 95741e9)                fix      as mod-rc2
0xb8850   func_break.cpp          CPushable::Move                    HL25 pushable rework (4f0d502)                                  design   as mod-rockcrowbar
0xb6390   func_break.cpp          CBreakable::KeyValue               "explosion" "1" is directed (50e1811)                           fix      as mod-rc2
0xb7710   func_break.cpp          CBreakable::Die                    no second break (473e9ea), fabs (e19fe47)                       fix      as mod-rc2
0xb7710   func_break.cpp          CBreakable::Die                    gibs along -g_vecAttackDir (50e1811)                            design   as mod-rc2 (Steven: the original's direction)
0xed040   plats.cpp               CFuncTrackTrain::UpdateSound       fabs (e19fe47)                                                  fix      as mod-rc2
-         func_break.cpp          CPushable::DamageDecal             no glass decal unless breakable (484911d)                       fix      a wrong decal
0x10a250  sound.cpp               CEnvSound::KeyValue, m_SaveData    room type an int (fdc159c)                                      fix      as mod-rc2 and mod-rockcrowbar (Steven 2026-11-07)
0x10a6b0  sound.cpp               CEnvSound::Think                   room type sent from UpdateClientData, saved, reset on a level   fix      a client kept a stale room type (halflife#1144,
                                                                     change (bc3add9)                                                         #2936)
0x10af10  sound.cpp               SENTENCEG_Init                     off by one (51a1fc2)                                            fix
0x10c1e0  sound.cpp               CSpeaker::SpeakerThink             invalid preset (a8824ba)                                        fix
0xe2e50   nodes.cpp               CGraph::FLoadGraph, FSaveGraph     node graphs through the engine filesystem, GAMECONFIG only,     fix      a port; the same paths
                                  (0xe32f0)                          out-of-date check (1f2f27a, 88df0a2, 609eafd, 04c63d0)
0xe2e50   nodes.cpp               CGraph::FLoadGraph, FSaveGraph     .nod64 on 64-bit builds (870fe1c)                               fix      64-bit only: the graph layout differs
0x908e0   buttons.cpp             CBaseButton::ButtonSpark,          sparks (ecd7ef9, d7dd2a8, 0d101ca, 2ca64e1)                     fix      as mod-rc2
                                  ButtonBackHome (0x90eb0)
0x919b0   buttons.cpp             CMomentaryRotButton::Use           sends the predicted destination's value (SDK 2.2+)              fix      as mod-rockcrowbar
0xbac20   func_tank.cpp           CFuncTank::StartControl,           view model cleared (seed), EquipWeapon (3c1e84a)                fix      as mod-rockcrowbar
                                  StopControl (0xbad20)
0xbbc80   func_tank.cpp           CFuncTank::AdjustAnglesForBarrel   acos domain clamp (90500cf)                                     fix
0xbafd0   func_tank.cpp           CFuncTank::Use                     usable again (1315fd4)                                          fix      its own regression
0x115a70  tentacle.cpp            CTentacle::Spawn                   EFLAG_SLERP (a02eb83)                                           fix      as mod-rc2 (Steven)
0x113580  talkmonster.cpp         CTalkMonster::TakeDamage           skips a dying friend (e306d7a)                                  fix      as mod-rc2
0x113da0  talkmonster.cpp         CTalkMonster::CanFollow            ideal state SCRIPT too (3f68a77)                                fix      as mod-rockcrowbar
-         talkmonster.cpp         CTalkMonster (m_pCine)             null m_pCine (3c7d4ed)                                          fix
0x112aa0  talkmonster.cpp         CTalkMonster::Touch                push only when the schedule allows it (d667a24)                 fix      as mod-rockcrowbar
0x111e70  talkmonster.cpp         CTalkMonster::RunTask              player looked up once (55a1e89)                                 fix      as mod-rockcrowbar
0xa0dc0   controller.cpp          CController::RunTask               faces only with an enemy (271d503)                              fix      as mod-rockcrowbar
0x920f0   effects.cpp             CEnvSpark                          no level transition (70f3b23)                                   fix
-         effects.cpp             CBeam                              attachments restored (497b213)                                  fix      save/restore
-         effects.cpp             trip beam touch                    touch reference (5831d2e)                                       fix      the same function either way
0xcffa0   maprules.cpp            CGamePlayerEquip::EquipPlayer,     null activator, KeyValue, save table (6b649da, 4122fcd,         fix
                                  KeyValue (0xcfce0), save table     c20feca)
0xcdfe0   maprules.cpp            CGameScore::Use                    null deref (fcb7096)                                            fix
-         maprules.cpp            CGamePlayerZone                    no level transition (61e18e3)                                   fix
0xc7310   h_cycler.cpp            CWreckage                          time as a float (cb51706)                                       fix      as mod-rockcrowbar
0xc6ca0   h_cycler.cpp            CCyclerSprite                      saved (d02b687)                                                 fix
0xea970   plats.cpp               CPlatTrigger::Touch                no trigger after the plat is removed (ffb0716)                  fix
-         (all)                   BOOL members                       bool, FIELD_BOOLEAN (c5b1932)                                   fix      the field type follows the member
0xa5750   crowbar.cpp             CCrowbar::Swing                    swing event only on fFirst (6528126)                            fix      SwingAgain replayed the event
0xa5750   crowbar.cpp             CCrowbar::Swing                    next attack set before the dead-target return (511c6ef)         fix      a corpse took swings with no delay
0xa5750   crowbar.cpp             CCrowbar::Swing                    Smack think on gpGlobals->time (c90351f)                        fix      UTIL_WeaponTimeBase() + 0.2 decalled at once
0xa5750   crowbar.cpp             CCrowbar::Swing                    <= for a full swing (a542e4a)                                   fix      inert in multiplayer
0xfd3a0   python.cpp              CPython::PrimaryAttack             dry fire (75bc5af)                                              fix      a repeated dry-fire sound
0xfd2a0   python.cpp,             CPython::SecondaryAttack,          pev->fov replaced by m_iFOV (6f8aac9); m_fInZoom kept           fix      the zoom moved onto m_iFOV (Steven 2026-11-07);
          crossbow.cpp            CCrossbow::SecondaryAttack                                                                                  m_fInZoom (d4b0e77) stays, OZ's zoom state
                                  (0xa4b20)
0xa47c0   crossbow.cpp            CCrossbow::FireBolt,               event ammo int 0                                                design   the original's reserve count
                                  FireSniperBolt (0xa4500)
0x106420  shotgun.cpp             CShotgun::PrimaryAttack,           pump from ItemPostFrame and on the last shell (a65846e)         fix      as mod-rockcrowbar
                                  WeaponIdle (0x1076f0)
0x129920  weapons.cpp             CBasePlayerItem::                  absolute time (f14827a)                                         fix      as mod-rockcrowbar
                                  AttemptToMaterialize
0x12a200  weapons.cpp             CBasePlayerItem::AttachToPlayer    SetThink(NULL) (ad50584)                                        fix      as mod-rockcrowbar
0x1297b0  weapons.cpp             CBasePlayerItem::FallThink         stops once a player holds it (e0ba8ce)                          fix      as mod-rockcrowbar (its busters branch inert)
0x12a630  weapons.cpp             CBasePlayerWeapon::AddPrimaryAmmo  sound test (3d13792)                                            fix      goes with 6951bf1
0x12a7b0  weapons.cpp             CBasePlayerWeapon::IsUseable       secondary ammo, WEAPON_NOCLIP, CanDeploy                        fix      as mod-rc2
0x12ab20  weapons.cpp             CBasePlayerWeapon::PlayEmptySound  EMIT_SOUND_PREDICTED (063b8a5)                                  fix      as mod-rc2 (mod-rockcrowbar: unclear, see below)
0x12b020  weapons.cpp             CBasePlayerWeapon::RetireWeapon    deferred to DoRetireWeapon (c4a67d4)                            fix      as mod-rc2
0x12a530  weapons.cpp             CBasePlayerWeapon::SendWeaponAnim  m_ForceSendAnimations (42f45bd)                                 fix      as mod-rc2
0x12a300  weapons.cpp             CBasePlayerWeapon::AddToPlayer     no extract (6951bf1)                                            fix      as mod-rc2
0x12aa40  weapons_shared.cpp      CBasePlayerWeapon::DefaultReload   forwards body (a3783be)                                         fix      inert: weapons-a's callers pass 0
-         weapons.cpp             GetNextAttackDelay (crowbar,       the 2013 SDK's fire-rate compensation (seed)                    fix      (Steven 2026-11-07: unclear goes to
                                  glock, mp5, shotgun, crossbow)                                                                              halflife-updated); weapons-b's design revert stays
0xfdf00   rpg.cpp                 CRpgRocket::RocketTouch,           launcher decrement in the destructor, EHANDLE launcher          fix      a rocket that blew up in flight locked the reload
                                  ~CRpgRocket                        (40d3849, 8092ab5)
0xfe1a0   rpg.cpp                 CRpgRocket::FollowThink            launcher release at 8192 units or 6 s, sky detonate (9b9d6c3,   design
                                                                     HL25)
0xfefa0   rpg.cpp                 CRpg::PrimaryAttack, WeaponIdle    ResetEmptySound placement (c31c918, 9b9d6c3)                    design   client prediction; the server gets the original
                                  (0xff320)
0xff320   rpg.cpp                 CRpg::WeaponIdle                   fidget idle 6.1 s, not 3 (33385ad)                              fix      as mod-rockcrowbar
0xff450   rpg.cpp                 CRpg::UpdateSpot                   no laser while reloading (9784339)                              fix
0xfea50   rpg.cpp                 CRpg::Reload                       reads only this reload's result (c82c3fa)                       fix
0xfed20   rpg.cpp                 CRpg::GetItemInfo                  ITEM_FLAG_NOAUTOSWITCHTO (2353ca4, HL25)                        design   as mod-rockcrowbar
-         rpg, gauss, egon,       weapons-b's attacks                GetNextAttackDelay for UTIL_WeaponTimeBase() + x (seed)         design   as mod-rc2 and mod-rockcrowbar (made before the
          hornetgun, ...                                                                                                                      rule; stays)
0xfedf0   rpg, gauss, egon,       ::AddToPlayer (0xc0750, 0xb2d20,   WeapPickup moved to AddPlayerItem                               design   as mod-rc2
          hornetgun               0xca390)
0xc1220   gauss.cpp               CGauss::StartFire                  V_min(m_flStartCharge, time) (8c5d4ab, HL25)                    fix
0xc14c0   gauss.cpp               CGauss::Fire                       hitgroup cleared on a self-hit (8c5d4ab)                        fix      (Steven 2026-11-07: unclear goes to
                                                                                                                                              halflife-updated)
0xc0b40   gauss.cpp               CGauss::SecondaryAttack            SendStopEvent in water and on overcharge (fd35528)              fix
0xc1c80   gauss.cpp               CGauss stop event                  FEV_GLOBAL (52b6a8a)                                            fix
0xc0b40   gauss.cpp               CGauss::SecondaryAttack            no ammo burn at 0 (0e8d3ad)                                     fix      uranium went to -1
0xca640   hornetgun.cpp           CHgun::PrimaryAttack, Reload       0.3 s multiplayer recharge, empty delays (37ed1d9, HL25)        design
                                  (0xcaf90)
0xca980   hornetgun.cpp           CHgun::SecondaryAttack             iparam1 FIREMODE_FAST to 0, ammo_hornets test (f347954)         design   as mod-rockcrowbar: prediction mirrors
0xcaf90   hornetgun.cpp           CHgun::Reload                      TabulateAmmo after the recharge (f347954, halflife#556)         fix      mod-rockcrowbar calls it design (see below)
0xca0a0   hornet.cpp              CHornet::DieTouch                  owner guard (e7fb734)                                           fix
0xc7b60   handgrenade.cpp         CHandGrenade::Holster              clears m_flStartThrow (70dd419, halflife#3251)                  fix      as mod-rockcrowbar; mod-rc2 left it out (see below)
0xc7da0   handgrenade.cpp         CHandGrenade::WeaponIdle           m_flReleaseThrow kept after the throw (4bb1f3c)                 fix
0xc7da0   handgrenade.cpp         CHandGrenade::WeaponIdle           6.5 multiplier, 1000 cap (ae352a6, HL25)                        design   balance
0xc2d20   ggrenade.cpp            CGrenade::BounceTouch              ResetSequenceInfo, frame 0 (2ee36b4, b31de2b)                   fix      as mod-rc2
0xc34b0   ggrenade.cpp            CGrenade::ShootTimed               ResetSequenceInfo (2ee36b4)                                     fix      as mod-rc2
0xc2270   ggrenade.cpp            CGrenade::Explode(TraceResult*)    source 1 unit lower (d6accf1)                                   fix      as mod-rockcrowbar
0xc2270   ggrenade.cpp            CGrenade::Explode(TraceResult*)    pull-out 0.6, was (dmg - 24) * 0.6 (078e787)                    fix      (Steven 2026-11-07: unclear goes to
                                                                                                                                              halflife-updated)
0x1000d0  satchel.cpp             CSatchel::PrimaryAttack            MODERN_SATCHEL_CONTROLS (1c202f5, HL25)                         design   as mod-rockcrowbar
0xffbd0   satchel.cpp             CSatchel::AddDuplicate             live charge limit (fd97136, HL25)                               design   as mod-rockcrowbar
0xfff10   satchel.cpp             CSatchel::Deploy                   first idle at 2 s (33385ad)                                     fix      as mod-rockcrowbar
0x11e4f0  tripmine.cpp            CTripmineGrenade::PowerupThink,    SF_BEAM_TEMPORARY, PointsInit (7d84906, 819a9ac)                fix
                                  MakeBeam (0x11ea20)
0x11ed50  tripmine.cpp            CTripmineGrenade::BeamBreakThink   owner retrace (6f0ae55, 79f9e95)                                fix
0x11f470  tripmine.cpp            CTripmine::Deploy, WeaponIdle      pev->body = 0 (68dc517, c10b998)                                fix
                                  (0x11fbe0)
0x10fa00  squeakgrenade.cpp       CSqueak::PrimaryAttack,            aim-down throw, FL_WORLDBRUSH bite test (e9986c0, HL25)         design   as mod-rockcrowbar
                                  CSqueakGrenade::SuperBounceTouch
                                  (0x10f260)
0x10e630  squeakgrenade.cpp       CSqueakGrenade::Spawn              movetype assignment (d6629ed)                                   fix
0x10f850  squeakgrenade.cpp       CSqueak::Deploy                    first idle at 1.7 s (33385ad)                                   fix      as mod-rockcrowbar
```

Counts (rows above): fix 126 (7 of them unclear until Steven's rule), design 52.

### Where an OZ verdict differed from mod-rc2 or mod-rockcrowbar

- `PM_FlyMove`'s first-plane reflect, the duck to get unstuck (`PM_CheckStuck`/`PM_PlayerMove`) and
  `PM_CheckParamters`' +use slowdown (854187a, 4f0d502): fix in OZ and mod-rockcrowbar; mod-rc2's p6
  restored the original for all three. Resolved: halflife-updated's in all three.
- `CopyToBodyQue`'s modelindex 0 test (ac6b664): fix in OZ; mod-rockcrowbar dropped it with
  `PlayerDeathThink`'s HL25 body copy (wave 2 row 0x1ec5b0). Resolved: halflife-updated's in all
  three.
- The HL25 sentence rework (3d8b933, sentences on `CBaseToggle`): OZ's world package kept it;
  mod-rockcrowbar calls the restructure design and keeps the sentence code in `CBaseMonster`. Both
  keep 85467c5's IsAllowedToSpeak. Resolved: halflife-updated's in all three.
- `CHgun::Reload`'s TabulateAmmo after the recharge (f347954): fix in OZ (weapons-b); design in
  mod-rockcrowbar wave 2 ("prediction mirrors"). Resolved: halflife-updated's in all three.
- `CBasePlayerWeapon::PlayEmptySound` (063b8a5): fix in OZ and mod-rc2; unclear in mod-rockcrowbar
  wave 2. Resolved: halflife-updated's in all three.
- `CHandGrenade::Holster` clearing m_flStartThrow (70dd419): fix in OZ and mod-rockcrowbar; mod-rc2
  keeps RC2's, which doesn't clear it (`dlls/handgrenade.cpp`). Resolved: halflife-updated's in all
  three.
- `PM_Jump`'s silent jump while frozen (f3810c7): fix in OZ; mod-rockcrowbar's server build plays the
  sound as the original does (`pm_shared.cpp`; its client build has halflife-updated's). Resolved:
  halflife-updated's in all three.
- `CEnvSound`'s room type as an int (fdc159c): OZ's world package had put back the original's float.
  Resolved: halflife-updated's int in all three (Steven 2026-11-07).
- client.cpp `UpdateClientData`'s fov (6f8aac9): OZ's rules and weapons-a packages differed.
  Resolved: halflife-updated's m_iFOV; OZ's zooms (glock, python, mp5, crossbow, DefaultDeploy)
  set m_iFOV alone. mod-rockcrowbar still sends pev->fov (its DEVIATIONS.md): to be brought to
  m_iFOV the same way.

### The unclear calls in mod-ozdm, applied

Each was unclear in its dossier, or kept there without a call; all now have halflife-updated's
version (rows above), in `reference/DEVIATIONS.md` where it differs from the original:
- weapons-a's `GetNextAttackDelay`: the crowbar, glock, mp5, shotgun and crossbow set the next primary
  attack through it where halflife-updated's do (the python has none upstream). weapons-b's design
  revert of the same stays.
- `CGauss::Fire` clears the hitgroup on a self-hit (8c5d4ab).
- `CGrenade::Explode` pulls out of the wall by 0.6 units (078e787).
- `CBasePlayer::ForceClientDllUpdate`'s extra resends: kept.
- The multiplay constructor's servercfgfile exec: left to the engine, as before; the VERIFY is gone.
- `CBasePlayer::PreThink`'s func_vehicle controls, `CBasePlayer::Spawn`'s "bj" physinfo and chat
  time, `PM_PreventMegaBunnyJumping`'s sv_allowbunnyhopping: kept, as they were.
- The user-message set: this tree's registrations, as before (not a FIXES row; the rules dossier).
