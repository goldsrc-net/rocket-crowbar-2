# Bugs in the original Rocket Crowbar 2

Bugs and oddities in `rc2.dll` and `client.dll` (beta 1.1) found while reconstructing them. The
rebuild keeps each one as the original has it unless the **Ours** column says otherwise. The
package's dossier (`reference/dossiers/<package>.md`, section 8) has the full reasoning. Add new
ones here as they turn up.

Status: **kept** = same behavior as the original; **guarded** = we avoid a crash the original has
(each also in `DEVIATIONS.md`, "Crashes in the original"); **changed** = we behave differently for
another reason. Of the changed ones only the CLTE 0x49 delete is in `DEVIATIONS.md` so far; the
rest are in their dossiers' deviation notes.

Policy (RC 1.9's, since 2026-11-07): every crash the original has is guarded with the smallest
check that avoids it, and nothing else changes. A crash is a null or out-of-range dereference, a
bad free, or an unbounded write or read past a buffer: anything that can bring down the server or
a client. Everything else the original does wrong is kept. Each guard is marked
`// Original bug (crash), guarded: <what>` in the code.

Addresses are the `// orig` tags' (RVAs, as in `reference/files-rc2.tsv` and `files-client.tsv`).
Files are under `dlls/` or `cl_dll/`.

## Crashes

### rc2.dll

```
Addr     Our file, function             What                                                  Ours
0x7f6c0  multiplay_gamerules.cpp        with mp_weaponstay, a rider index whose slot has no   guarded
         CanHavePlayerItem              player: UTIL_PlayerByIndex's NULL is called through
0x288d0  rc2/crossbowbolts.cpp          a teather an LCS spawner made has the spawner as its  guarded
         CXBowTeather::AttachTouch      shooter; AddTether writes player members past its end
0x1580   rc2/maintainent.cpp            strcpy into 64-byte slots; in the last slots a long   guarded
         LCS_AllocString                string runs past the pool into its counts (0x18be70)
0x1160   rc2/maintainent.cpp            strcpy's sv_tempadmins into a 128-byte stack buffer   guarded
         Admin_IsTempAdmin                                                                    (p1)
0x1b200  client.cpp SayPrivate          strncat's 192 chars a word into 256 bytes: stack      guarded
                                        overrun with a long message                           (p1)
0x6d390  rc2/lcs.cpp LCS_GetToken       copies a token unbounded from a 512-byte .lcs line    guarded
                                        into the callers' 256-byte buffers
0x6d390  rc2/lcs.cpp LCS_GetToken       steps past an unterminated quote or brace and reads   guarded
                                        on through stale bytes, past the line at its end
0x71c00  rc2/alteration.cpp             a one-character whitespace string writes psz[-1],     guarded
         ALT_StripSpaces                the heap header before ALT_Copy's block
0x949b0  player.cpp CBasePlayer::       damage score mode 1: IsPlayer through the attacker's  guarded
         TakeDamage                     NULL instance (a removed thrower's freed edict)
0xacea0  python.cpp CPython::FireChain  the hit bitset is sized by maxClients but a monster   guarded
                                        target sets its bit past the block (heap corruption)
0xe5590  triggers.cpp                   pActivator->IsActivePlayer() with no NULL test; a     guarded
         CBaseTrigger::CounterUse       func_breakable's Die fires its targets with none
0xe8d30  tripmine.cpp CTripmineGrenade  a box crossing 9999 or more away sets bFound with no  guarded
         ::BeamUpdate                   target, then pTarget is read through NULL
0xeabb0  tripmine.cpp CPunchTripmine::  the same, in the punch mine                           guarded
         BeamUpdate
0xf5a20  util.cpp AddToPrecachedList    strcpy into 128-byte rows; an LCS classname (up to    guarded
                                        255) in the last row runs past the list
0x9db0   animation.cpp GetSequenceInfo  only the upper bound is checked: a negative sequence  guarded
                                        (an LCS "seq" token) reads before the table           (HLU's)
```

### client.dll

```
Addr     Our file, function             What                                                  Ours
0x510b0  rc2/clte.cpp __MsgFunc_CLTE    CLTE 0xfc ("nukeclte") calls KillEntity through      guarded
         (0xfc)                         every slot, empty ones too: once any player is seen
0x3b900  hl/hl_weapons.cpp              WeaponSync copies iSize bytes into a 256-byte buffer  guarded
         __MsgFunc_WeaponSync           with no bound
0x9d880  entity.cpp HUD_StudioEvent     reads the 1024-byte frozen table by any entity index  guarded
                                        (the next table above 1023)
-        StudioModelRenderer.cpp        the renderer reads the frozen and antifriction        guarded
         (9 functions)                  tables the same way
0x7bff0  rc2/freeze_tents.cpp CLTE      writes the frozen table at the message's index        guarded
0x7c1f0  0x2f factory, KillEntity       unchecked
0x83520  rc2/antifriction_tents.cpp     writes the antifriction table at the message's index  guarded
0x83720  CLTE 0x35 ctor, KillEntity     unchecked
0x633a0  rc2/clte_effects3.cpp CLTE 13  Beam_Kill on a slot that is NULL when the beam wasn't guarded
         CTentChargeGlow::Update        made (plasma.spr missing, the beam list full)
0x66d50  rc2/clte_effects3.cpp CLTE 17  the flare is left unset (missing entity or sprite)    guarded
         CTentMP5Pulse ctor             and its readers' NULL tests pass the garbage
0x689b0  rc2/clte_effects3.cpp CLTE 20  the light is left unset with cl_detail below 0, then  guarded
         CLTE_CreateHEVFlame            written through
0x64c50  rc2/clte_effects3.cpp CLTE 15  lights unset below cl_detail 0; big flare and ring    guarded
         CTentBallLightning ctor        sprite unset when the small flare fails
0x62780  rc2/clte_effects2.cpp CLTE 12  flare, lights, shockwave unset when the gib or its    guarded
         CTentTransphasic ctor          tempent fails or cl_detail is below 0
0x6d5b0  rc2/clte_effects4.cpp CLTE 30  lights made only with cl_detail not negative, else    guarded
         CLTE_CreateBoltArmed           unset; Update and KillEntity write through them
0x76c40  rc2/nuke_tents.cpp CLTE 0x2a   lights unset below cl_detail 0; Think and EntityGone  guarded
         CNukeTent ctor                 write through them
0x85a20  rc2/ultima_tent.cpp CLTE 0x37  the down beams are used unchecked: NULL with a full   guarded
0x83dd0  CUltimaTent Update, ctor       beam list, unset when laserbeam.spr doesn't load
0x95e50  rc2/tcontroller_tents.cpp      the unused leech beams are written unchecked (NULL    guarded
         CLTE 0x49 Update               when zbeam2.spr fails or the beam list is full)
0xd4020  vgui_CustomObjects.cpp         button text (team and player names) passed as        guarded
         CommandButton::RecalculateText setText's printf format: %s or %n in a name
0xd8980  rc2/vgui_PlayerID.cpp          a player's name passed as setText's format            guarded
         PlayerID_Frame
0xe9350  rc2/vgui_VoteMenu.cpp          the server's vote text passed as setText's format     guarded
         CVoteMenuPanel::SetVoteText
0xe6a80  vgui_TeamFortressViewport.cpp  the vote text, in szText, passed as the format        guarded
         MsgFunc_VoteUpdate
0xe4c70  vgui_TeamFortressViewport.cpp  the spectated player's name passed as the format      guarded
         UpdateSpectatorMenu
-        vgui_TeamFortressViewport.cpp  the map name passed as the format (SDK 2.3's code)    guarded
         UpdateSpectatorPanel
0xe69e0  vgui_TeamFortressViewport.cpp  in demo playback ShowVGUIMenu makes no vote menu and  guarded
         MsgFunc_VoteBegin              m_pVoteMenu is read through NULL
0xe32c0  vgui_TeamFortressViewport.cpp  an unknown CUSTOM button followed by '{' passes NULL  guarded
         CreateCommandMenu              to CreateSubMenu (SDK 2.1's)
0xe32c0  vgui_TeamFortressViewport.cpp  frees the advanced parse pointer: a bad free when     guarded
         CreateCommandMenu              MAX_MENUS stops the parse mid-file (SDK 2.3's)
0xd6040  rc2/vgui_DebugPanel.cpp        strcpy's the server's strings into 112/128-byte       guarded
         CDebugPanel::MsgFunc_DBGEntInfo buffers and sprintf's four into 512 (a map's long
                                        targetname); the lines, names in them, as the format
0xcf930  rc2/vgui_attributes.cpp        writes psz[255] of a 255-byte slot: the last slot     guarded
         AttrMenu_LoadSlots             one byte past the array                               (c4)
0xbee40  saytext.cpp SayTextPrint       SDK 2.2's copies max(iBufSize - 1, 255) bytes into a  guarded
                                        256-byte line                                         (HLU's)
```

### Called a crash, but the path can't happen

Left as the original has it. The reason in each case is in a comment at the spot.

```
Addr     Our file, function             Why it can't crash
0x22070  combat.cpp CBaseMonster::      a trace only hits linked edicts, and every one has private data
         CheckTraceHullAttack
0x196c0  cbase.cpp DispatchUse,         the engine never calls Use; Blocked's other is an entity it is
         DispatchBlocked                pushing, which has private data
-        rc2/crossbowbolts.cpp          the teather's owner is only cleared by ReleasePull, which only
         FollowStuck (teather)          the gauss's teleport calls, and it never moves a brush
0x2b500  crossbow.cpp CCrossbow::       the hit passed UTIL_IsValidEntity, which needs private data
         PrimaryAttack
0x31190  crowbar.cpp CCrowbar::Swing    a trace hit, as above
0x41990  rc2/freezetimer.cpp            UTIL_IsValidEntity on the owner comes first
0x41cc0  StartFreeze, EndFreeze
0x38620  effects.cpp CBeam::BeamDamage  p2 says SDK 2.3's null test is gone; the original tests the
                                        hit edict's private data itself
0x64310  rc2/freezegrenade.cpp          pev->owner is set in AttachTouch and never cleared; a removed
         AttachedThink                  owner's edict is still readable memory (stale origin)
0x8b690  rc2/nuke.cpp CNuke::NukeThink  the controller must pass IsAlive; the only non-player owner, an
                                        LCS spawner, has no health, so the cast write never runs
0xb1f50  rc2/playerride.cpp StopRiding  EntSelectSpawnPoint never returns NULL (the world without spots)
0xa1b10  pm_shared.cpp                  the moveent overrun stays inside the pass-through .bss block,
         PM_TryPassThrough              and every index read back is in range
0xcb7d0  rc2/snarkcam.cpp ControlThink  the owner is cast to a player only while m_fControlling, which
                                        only the player's weapon sets
0xd5a10  talkmonster.cpp RunTask        INDEXENT(1) is never NULL
0xe9830  tripmine.cpp CBouncyTripmine:: m_ppBeams[i]: Spawn fills every slot (BeamCreate never fails) and
         BeamUpdate                     BeamUpdate stops with KillBeam
-        weapons.cpp, python.cpp,       CBaseEntity::Create of a registered classname never returns NULL
         snarkcam.cpp, util.cpp         (the engine stops with Sys_Error when edicts run out)
0xfc3f0  world.cpp CopyToBodyQue        only players reach it
0x59be0  rc2/clte.cpp CLTE_GetOrigin    GetEntityByIndex returns NULL only past the engine's entity
                                        list; callers pass server-sent indices
0x6a4c0  rc2/clte_effects4.cpp CLTE 24, the "missing" target or shooter is GetEntityByIndex's NULL, as
0x6b370  26, 31                         above
0x6dbc0
0x8dc00  rc2/transform_tents.cpp        GetLocalPlayer and GetEntityByIndex, as above
0x8e4e0  Update, KillEntity
-        ev_hldm.cpp nuke and conc      GetLocalPlayer never returns NULL in game (c1)
         explosions, far branch
0x5bac0  rc2/clte_effects1.cpp CLTE 1   the trail's members are unset only for entity 0; the server
         CLTE_CreateRocketTrail         always sends the rocket's index
0x61a60  rc2/clte_effects2.cpp          the recursion's depth is only counted down, but each step moves
         CTentTransphasic::TraceStep    on by at least a sixtieth of a stretch longer than 8 units
0x80cb0  rc2/gravgrenade_tents.cpp      nothing sends CLTE 0x32 (neither the server nor the client's
         CLTE 0x32 ctor                 loopback), so its unset beam and glows are never read
0x92e70  rc2/afterimage_tents.cpp       nothing sends CLTE 0x44 either
         CLTE 0x44 Update
0x8d8d0  rc2/forklift_tents.cpp         a dlight without an elight: both are made together, and the
         KillEntity                     engine's CL_AllocDlight/CL_AllocElight never return NULL
0x93220  rc2/tcontroller_tents.cpp      members unset for a player byte outside 1-32; Update ends the
0x94790  CLTE 0x45, 0x48 ctors          effect at once and nothing else reads them
-        rc2/ultima_tent.cpp            g_pClDebugGren is registered with cl_detail at init
0xa47f0  hud.cpp DrawHudNumber          10000 and up would read past the sprites; every caller clamps
0xa70b0  hud_spectator.cpp SetModes     no viewport check, but it only runs in game, after HUD_VidInit
                                        made the viewport
-        voice_status.cpp               the else branch's NULL m_pIcon is unreachable (c1)
         RepositionLabels
-        rc2/vgui_ServerBrowser.cpp     SDK 2.3's setText(server name) and sprintf into 32 bytes; our
                                        build runs no ServersThink, so the list never fills
```

## Other bugs and oddities

### rc2.dll: gameplay

```
Addr     Our file, function             What                                                  Ours
0x9d0c0  player.cpp                     with the first rider missing or without the item, the kept
         RidersHavePlayerItem           second check asks the first rider again
0x9d3d0  player.cpp RidersGiveAmmo      when the first rider takes nothing, iGiven stays -1   kept
                                        and the second is offered one more round than is left
0x98a20  player.cpp PreThink            health creation times against EnergyDrawTimeTaken (3) kept
                                        while the curve uses CreateHealthTimeTaken (4.5)
0x96020  player.cpp Killed              a shoulder rider who dies vanishes, with no death     kept
                                        think
0x95b80  player.cpp StartDyingCam       a spot found on the last of five tries is discarded   kept
0x7e130  multiplay_gamerules.cpp Think  no players and a frag limit put (int)1e16 into        kept
                                        mp_fragsleft
0xdd130  teamplay_gamerules.cpp         teamplay allows riding when the ridden player has no  kept
         PlayerAllowsRiding             infobuffer or no allowriding key (multiplay refuses)
0xb16b0  rc2/playerride.cpp             "ride right" picks a random shoulder, any other       kept
                                        argument the left
0xb1f50  rc2/playerride.cpp StopRiding  with no room, the player is put at the spawn spot and kept
                                        then UTIL_SetOrigin moves him to the last failed try
-        rc2/playerride.cpp             "Cannot get off right now" still lets the player off  kept
-        rc2/racecontroller.cpp InitPath takes only the first two race points unless the map   kept
         (0xadb90)                      chains them otherwise (VERIFY)
0xe21f0  rc2/transform.cpp              houndeye, bat and headcrab test m_iTransform (always  kept
         StartTransform                 0) for the room check and hold 1 s, not their time
0xe2cc0  rc2/transform.cpp              the houndeye uses the assassin's                      kept
         StartUntransform               TransformParalyzeTime
0xe1350  rc2/thoundeye.cpp locator      turning off is gated by +0x1120, on by +0x1124; a     kept
                                        press inside the gate charges the turn-on energy again
0xe1350  rc2/thoundeye.cpp sonic cancel uses BeamRefireTime and the beam's CLTE kill (0x41)   kept
0xdfdd0  rc2/thoundeye.cpp sonic        TakeDamage even when the shield left only a push      kept
0xded30  rc2/theadcrab.cpp SetAnimation tests the houndeye's turn sequences 25/26             kept
0xdf320  rc2/theadcrab.cpp leap         a negative JumpMinDamage heals the victim             kept
0xd7890  rc2/tassassin.cpp jump         on the jump button's release (the houndeye's on press)kept
-        rc2/tbat.cpp take-off          the hang animation plays backwards at framerate -3,   kept
                                        TBat_Unroost at -1
0xdb2d0  rc2/tcontroller.cpp ball       its malloc'd queue is freed only through LeechThink   kept
-        rc2/tcontroller.cpp            the charge isn't clamped to 1: past ChargeBallCharge- kept
         TController_ChargeLaunch       Time the ball is bigger than the client shows
-        crossbow.cpp TertiaryAttack    the zoomed charge event has bparam1 1, unlike the     kept
                                        others
-        rc2/crowbarang.cpp             the pull uses the unnormalized offset (distance       kept
                                        weighted until the speed clamp)
-        rc2/crowbarang.cpp Embed       clears pev->owner right after setting it: stuck in    kept
                                        anything but the world, it goes on its first think
0x31190  crowbar.cpp Swing              a body hit doesn't update m_trHit (Smack decals the   kept
                                        last world hit); Smack runs the next frame
-        crowbar.cpp Tertiary/Secondary uses the secondary attack's refire keys; the          kept
                                        secondary's event has bparam1 4
-        crowbar.cpp WeaponFrame        energy per step is charged on the charge before the   kept
                                        step
-        rc2/controlforklift.cpp        pev->impulse reset every think, so stuck handling     kept
                                        never passes 1; danger sound bits_SOUND_CARCASS
-        rc2/freezetimer.cpp            common/null.wav on its own channels; clears the       kept
                                        player's shield byte and never restores it
-        explode.cpp ExplodeAt          leaves its bare CBaseEntity with FL_KILLME            kept
-        egon.cpp                       GetItemInfo writes m_iId; ShrinkSelf checks the       kept
                                        imploder's costs, charges its own; a short quaternary
                                        extends only the fourth timer; BeginImplode ignores
                                        IsPlayer(); idle time without the time base
-        doors.cpp Blocked              SDK 2.3's: compares the other door's avelocity to     kept
                                        pev->velocity
-        weapons.cpp RandomItemOfKind   counts the long jumps and ignores the count; both     kept
                                        random switches have unreachable cases
-        weapons.cpp ExtractAmmo        result uninitialized without primary ammo (the        kept (true)
                                        register holds pWeapon: true)
-        weapons.cpp ItemPostFrame      SecondaryAmmoIndex is -1, so the secondary empty test changed
                                        reads m_rgAmmo[-1] (inside the player: m_pLastItem)   (HLU's)
-        gauss.cpp PrimaryAttack        with a beacon tests PrimaryTeleportCost, subtracts    kept
                                        PrimaryNoBeaconTeleportCost; both teleports use
                                        PrimaryTeleportRefire (key 509 never read)
-        gauss.cpp TertiaryFireBecon    tests and charges TertiaryNoBeaconBurnAmmount         kept
-        gargantua.cpp FlameDamage      SDK's clamp: dist < -radius sets dist = radius        kept
-        rc2/glockgibs.cpp PunchThrough ignores its recursion's result: only solid crossed on kept
                                        the first level counts toward MaxSolidPunch
-        glock.cpp QuaternaryAttack     gates on TransphasicAmmoCost but takes one round      kept
-        glock.cpp FireTransphasic      clears the charge only inside the play-events test,   kept
                                        and tests that flag twice
-        rc2/glockgibs.cpp              a sticky gib's Expire after its host is gone always   kept
                                        reports "not on world"
-        h_cycler.cpp CWreckage         its start time truncated to whole seconds (SDK int)   kept
-        ggrenade.cpp Detonate2         a TraceResult with three fields set (rest unread      changed
                                        stack); ours zeroes it                                (zeroed)
-        rc2/crossbowbolts.cpp impailer the shield hit position is uninitialized stack; ours  changed
                                        zero                                                  (zeroed)
-        handgrenade.cpp                refire keys crossed (antifriction uses Secondary*,    kept
                                        suck Tertiary*, gravity Primary*); only gravity
                                        pushes the quaternary timer
-        handgrenade.cpp                the quaternary attack is empty: ultima_grenade is     kept
                                        precached, never thrown
-        handgrenade.cpp Holster        doesn't clear m_flStartThrow (HLU's fix not taken)    kept
-        handgrenade.cpp WeaponFrame    reads the weapon's frozen byte, not the player's      kept
-        h_battery.cpp CRecharge::Use   reads attribute 6 twice and drops the value           kept
-        rc2/gravgrenade.cpp,           Spawn skips ResetSequenceInfo (the other two call it) kept
         rc2/afgrenade.cpp
-        rc2/freezegrenade.cpp          divides by flDamage/flFull even without a shield (NaN kept
         FreezeDamage                   at damage 0)
-        mp5.cpp                        idle time without the time base; the bouncy bolt's    kept
                                        sky event at the step's start; the blaster bolt shows
                                        no hit on the world; slot 115 zeroes the bolt counts
-        healthkit.cpp CWallHealth::Use the over-max charge runs in the tick of a successful  kept
                                        TakeHealth
-        python.cpp WeaponIdle          draws 10..15 where 0..1 is meant: always fidgets      kept
-        python.cpp ChainTo             skips the last client slot                            kept
-        python.cpp Holster             toggles zoom by m_fInZoom while the toggle reads fov  kept
-        python.cpp reload              its sound is dead on the server                       kept
-        rpg.cpp RC2's reload           drops SDK 2.3's rocket and laser checks               kept
-        rc2/spheremine.cpp             drag clamps a fraction against a speed; PreactThink   kept
                                        never updates m_flLastThink; the sm_project exemption
                                        checks the inflictor's classname; Killed sets
                                        takedamage twice; GetRCCaps asked inconsistently
-        rc2/suckmine.cpp               aims a new target along the last candidate's offset   kept
-        rc2/balllightning.cpp          the waterlevel > 1 branch is dead                     kept
-        rc2/pullers.cpp                ApplyExternalForce gives an unlifted walker no        kept
                                        velocity; AddPullPoint bounds-checks after searching;
                                        RemovePullPoint leaves radius and forces
-        rc2/scientistmine.cpp          its director event is SDK 2.2's HLTV form             kept
-        satchel.cpp CRoachCharge (p7)  under water velocity times 8*interval; FindTarget     kept
                                        reuses pev->dmgtime, so an armed roach keeps its target
-        rc2/shield.cpp ShieldDeflect   passes degrees to cos/sin                             kept
-        shotgun.cpp crate              ExpandThink sets scale 0 at full size (GoBoom reads 1);kept
                                        FireCrate's avelocity.y RANDOM_FLOAT(500, 500)
-        shotgun.cpp                    GetItemInfo max ammo a constant 36 (MaxCarryShells    kept
                                        ignored); idle time without the time base
-        scientist.cpp StepSound        can play npc_step4.wav, never precached               kept
-        squeakgrenade.cpp              SnarkBiteGainPopDamage never read; the mini grunt's   kept
                                        retarget dot is unnormalized; SDK 2.3's HuntThink
                                        MOVETYPE_FLY bug, pitch % 3, Activate's classname
-        rc2/shrinktimer.cpp ShrinkEnt  stops common/null.wav on its own channels              kept
-        rc2/snarkcam.cpp               HomeThink returns before updating velocity when its   kept
                                        guide is gone; FindNextCam's wrap skips maxClients+1
                                        and maxEntities-1; TakeDamage false for its own shot
0xd3cb0  squeakgrenade.cpp CSqueak::    tests FL_CONVEYOR on the surface hit                  kept
         TertiaryAttack
-        triggers.cpp ChangeLevelNow    deathmatch with sv_chglevel 1: changes to             kept
                                        st_szNextMap before anything fills it
-        tripmine.cpp BeamUpdate (all)  the crossing check loops players 1..maxClients-1     kept
-        tripmine.cpp CTargetTripmine   the dot score is always >= 1 past the blind spot      kept
                                        test; the zap chain reuses the first TraceResult
-        tripmine.cpp bouncy, punch     arrays freed only in KillBeam (leak otherwise)        kept
-        tripmine.cpp Spawn             a first trace miss calls Killed before takedamage is  kept
                                        set: it explodes at once
-        tripmine.cpp mine_activate     flags 1 (SDK 2.3's 1.0 pitch argument)                kept
-        tripmine.cpp MineAttack        "greater than": one mine more than MaxOut             kept
0xf2fe0  util.cpp UTIL_IsValidEntity    `!bits & (1 << (i - 1))`: no bits set makes slot 1    kept
                                        invalid, otherwise every client slot is valid
0xf5a20  util.cpp AddToPrecachedList    the duplicate test compares nothing: names are added  kept
                                        again until the 512 rows fill
0xf4c50  util.cpp                       UTIL_ClearEntityMarks drops sets without freeing;     kept
                                        PushEntityMarks leaks the set it overwrites
0xf4e70  util.cpp                       the shield's result is only tested: full damage dealt kept
         UTIL_RC2WaterDischarge
0xf31a0  util.cpp                       RANDOM_LONG(0, count) can pick one past the end; the  kept
         UTIL_RandomEntityByClassname   search wraps (up to 1000 tries)
0xf89d0  rc2/vote.cpp votekick          the second "already voted" bit is the caller's again; kept
                                        the kick passes the entity index where banid wants
                                        the user id
0xf8280  rc2/vote.cpp VoteThink         fails once the undecided (the caller among them) are  kept
                                        fewer than the votes needed (VERIFY)
-        client.cpp ClientCommand       weapon_ with +0x4ac bit 0x10 goes to the rules and    kept
                                        then "Unknown command"
-        client.cpp AddToFullPack       always sends spectator 0                              kept
-        client.cpp ServerActivate      skips entities below clientMax, the world too, and    kept
                                        activates the one at clientMax
-        buttons.cpp CMomentaryRotButton sends the current value                               kept
         ::Use
-        bmodels.cpp CFuncWall          the followent test runs at spawn                      kept
```

### rc2.dll: admin commands, LCS and rc2.ini

```
Addr     Our file, function             What                                                  Ours
0x1670   rc2/maintainent.cpp            "takedamagenoaim" never matches ("takedamage" is      kept
         LCS_EntityToken                tested first); "blend" reads the 4th character, so it
                                        is always blending[1]
0x1580   rc2/maintainent.cpp            full, it keeps overwriting slot 0; a long string runs kept
         LCS_AllocString                into the next slots
-        rc2/maintainent.cpp            "string" writes from the '=' on, "char" writes the    kept
         LCS_MessageToken               '=' (nothing calls it)
-        rc2/maintainent.cpp            skips every entity at or below maxClients             kept
         Admin_Remove
-        rc2/maintainent.cpp            keeps stale sscanf values on a malformed              kept
         Admin_GetLevel                 adminprim.cfg line (VERIFY)
0x71c00  rc2/alteration.cpp             clears psz[len-2] after a shift and skips the next    kept
         ALT_StripSpaces                character: each space costs the last character
                                        ("a b" is "a"), garbling c3a2a.lcs's vectors
-        rc2/alteration.cpp             only the first (...) or {...} group is evaluated      kept
         ALT_EvalFloat, ALT_EvalVector  (2*(3) is 3); no precedence (2*3+1 is 8)
0x6d4d0  rc2/lcs.cpp LCS_Load           LCS_Command writes "setspawncmd" but this looks for   kept
                                        "setspawncommand"; the leading-space skip never moves
-        rc2/lcs.cpp                    "Ussage" in the messages                              kept
0x6ce50  rc2/lcs.cpp CLCSSpawner::      clears a one-shot spawn's owner, then sets it to the  kept
         CreateThink                    spawner anyway
0xaee10  rc2prefs.cpp savediff          tests the heading's first key for every key: the      kept
                                        whole heading or nothing
0xaf150  rc2prefs.cpp                   DOUBLE keys are networked as the low dword twice      kept
-        rc2prefs.cpp                   UNSIGNED SHORT keys go through signed clamps:         kept
                                        32768..65535 become 0 with POS_ONLY
0xb1520  rc2prefs.cpp                   a SMALLFLOAT is read back multiplied by 100 again     kept
         RC2Prefs_UnpackValues          instead of divided (nothing calls it)
-        rc2prefs.cpp listkeys          an unknown heading prints "Page 1 of 0"               kept
0xaea60  rc2prefs.cpp                   fgets' result unchecked: at EOF the last line is      kept
                                        processed again
```

### rc2.dll and client.dll: pm_shared

```
Addr     Our file, function             What                                                  Ours
0xa1b10  pm_shared.cpp                  moveent origins go into the visent array and their    kept
         PM_TryPassThrough              solid and index at the visent count (emulated with
                                        the original's .bss layout, s_pmPass)
-        pm_shared.cpp PM_FlyMove       the allsolid return skips PM_RestorePassThrough       kept
-        pm_shared.cpp PM_WalkMove      modes 3 and 6 cap speed at cmd.forwardmove            kept
-        pm_shared.cpp PM_WaterMove     modes without a swim speed cap at the speed           kept
                                        attribute (about 1)
-        pm_shared.cpp PM_LadderMove    modes 3, 4, 6 climb at the jump attribute (~1 u/s)    kept
-        pm_shared.cpp PM_HeadcrabJump  past maxvelocity, speed scales to maxspeed            kept
-        pm_shared.cpp                  jetpack, controller and bat accelerate by             kept
                                        FrictionScale, not AccelScale
-        pm_shared.cpp                  a missing "phf" key keeps the previous move's flags   kept
-        pm_shared.cpp SpectatorMove    in-eye: uninitialised angles (ours zero); an unused   kept
                                        second target lookup; two identical branches (client)
-        rc2/pullers.cpp PM_RaceMove    the roll wrap uses (1 - n) * 360; client: the height  kept
                                        is unset when not the local player
-        pm_shared.cpp PM_ShowClipBox   acts only with pmove->server set: never on the client kept
```

### rc2.dll and client.dll: messages

```
Addr     Our file, function             What                                                  Ours
-        rc2/playerride.cpp StopRiding  sends CLTE 0xfe with ENTINDEX of the just-cleared     kept
                                        owner (0)
-        rc2/pullers.cpp SendPullPoints raw float bits through WRITE_LONG                    kept
-        rc2/pullers_client.cpp         PullerCtrl 4: the server clamps the radius to 0xff37  kept
                                        and the client reads it signed (over 32767 negative)
-        rc2/rc2prefs_client.cpp        RCCFG 0x7ffe block: a SMALLFLOAT multiplied by 100    kept
         RC2Prefs_ReadSyncBlock         again (nothing sends one); a DOUBLE drops its first
                                        long (high half zero, stack garbage in the original);
                                        returns 0 after applying a short block
-        hl/hl_weapons.cpp RC2ReadState* each reader checks *piSize but never lowers it: a    kept
                                        reader can run past the message (inside the buffer)
-        glock.cpp, tripmine.cpp zap    the HLTV message goes to svc 50 with byte 3 first,    kept
                                        not SDK 2.3's director form
0x9ce80  rc2/hud_energy.cpp             CHudEnergy::MsgFunc_Energy is void; its hook returns  changed
                                        whatever is in eax                                    (true)
```

### client.dll: effects (CLTE) and events

```
Addr     Our file, function             What                                                  Ours
0x971c0  rc2/tcontroller_tents.cpp      CLTE 0x49 frees trail 0, then plain delete on the     changed
0x95e50  EntityGone, Update             new[] array's pointer (block + 4): MSVC6 rejects it   (leak kept,
                                        silently and the trails leak                          no delete)
0x95e50  rc2/tcontroller_tents.cpp      the leech's shield test uses the ball's index, not    kept
                                        the victim's
-        rc2/clte_draw.cpp              CLTE_DrawBeamQuad lights its third vertex with the    kept
                                        start alpha; CLTE_DrawStripLoop (dead) counts the
                                        closing segment twice and writes pLengths[iCount]
-        rc2/clte.cpp CLTE_ShieldPoint  passes VectorAngles' degrees to cos/sin, and adds the kept
                                        step twice with a normal
-        rc2/clte_effects1.cpp CLTE 3   the spark gains a second plus the frame each update   kept
-        rc2/clte_effects4.cpp bolts    pass an entity index as EV_PlayerTrace's ignore_pe    kept
                                        (a physent index); so does the ultima's flash trace
-        rc2/clte_effects4.cpp CLTE 28, another mode leaves the time unset                    kept
         35
-        rc2/clte_effects4.cpp CLTE 30  writes the elight's radius and decay into the dlight kept
                                        and never ends on its own
-        rc2/clte_effects4.cpp CLTE 26  the self-type toggle round CLTE_KillByEntity does     kept
                                        nothing; 24 and 26 KillEntity set -1, not 0
-        rc2/clte_effects4.cpp CLTE 32  leftover sparks go into random slots                 kept
-        rc2/clte_effects4.cpp CLTE 34  adds the frame twice to the flare and light lives    kept
-        rc2/shrinker_tents.cpp 0x26-28 the shrinker shot's lights get die += 1 a frame; its  kept
                                        second spawn loop uses random slots, live ones too;
                                        0x28 sets the entity's scale every frame
-        rc2/nuke_tents.cpp CLTE 0x2a   never sets m_flBubbles (heap garbage); EntityGone     kept
                                        returns false even for its own entity
-        rc2/imploder_tents.cpp         CImplodeTent draws flares 0-149 of 250; the shot      kept
                                        updates each trail twice on a frame that adds a point
-        rc2/gravgrenade_tents.cpp 0x32 the debris timer is compared with itself; SpawnDebris kept
                                        ignores the free slot it found; EntityGone false
-        rc2/*_tents.cpp                every beam kill writes die = 0.1, an absolute time    kept
                                        long past (0x32-0x36, 0x3a, 0x29)
-        rc2/suckgrenade_tents.cpp 0x34 fades in its last 0.75 s, then snaps back to 1.0     kept
-        rc2/ultima_tent.cpp            DrawTransparent never tests bar trail 1 or three of   kept
                                        the rising trails; the rising-ring trails freeze
                                        between cycles
-        rc2/clte_particles.cpp         no effect's destructor runs: every CRC2Trail and      kept
                                        CRC2Particles user leaks
-        rc2/energyblast_tents.cpp 0x3d reads and normalizes a direction it never uses       kept
-        rc2/charge_tents.cpp 0x39      never touches its glow entity                         kept
0x8d8d0  rc2/forklift_tents.cpp         tests each dlight but kills the elight: dlights live  kept
                                        to their 10 s die time
0x8ebf0  rc2/transform_tents.cpp ctor   both durations switch on the from-type; the alpha    kept
                                        init writes index 1 every pass
0x8dc00  rc2/transform_tents.cpp        the forward/up ring pair is built once, never drawn   kept
0x92e70  rc2/afterimage_tents.cpp       copies 2-4 all sit where copy 1 was (CLTE 0x44,       kept
                                        unsent); the entity index is never cleared
0x98480  rc2/tripminezap_tent.cpp,      beam die an absolute 0.1: the beam dies at once       kept
0x99770  egontractor_tent.cpp
-        ev_hldm.cpp EV_DrawLightning   retraces a hidden point 16 times                      kept
-        ev_hldm.cpp EV_ChainLightning  never decrements its depth (bounded by the mask)     kept
-        ev_hldm.cpp EV_PythonChain     draws from the local player's attachment, whoever    kept
                                        fired
-        ev_hldm.cpp EV_GibVelocity     the 2000 clamp scales up as well                      kept
-        ev_hldm.cpp HornetGrenadeDet   passes the packed direction as an entity              kept
-        ev_hldm.cpp ConcChargeExplode  the message direction is the unnormalized eye delta  kept
                                        (overflows the short); scorches when nothing was hit
-        ev_hldm.cpp THoundeyeBeam      measures the miss from the world origin               kept
-        ev_hldm.cpp EgonTractor        the refire condition can never be true                kept
-        ev_hldm.cpp                    GravGrenadeGlow's no-op PM push/pop; CbarCharge and   kept
                                        HornetGunSetCharge skip player maxclients; GaussThird's
                                        first ring ignores cl_detail
-        ev_hldm.cpp                    unset trace ends and angles (RpgSphereMineImpact,     changed
                                        SnarkProjectHit, TeleportFlash, GravGrenadeDet) get   (zeroed)
                                        zero; unreachable default sounds take the last case
```

### client.dll: HUD, view, input and VGUI

```
Addr     Our file, function             What                                                  Ours
-        voice_status.cpp               SetPlayerBlockedState ignores its argument and       kept
                                        toggles; UpdateBanButton shows no icon for inaudible
                                        players; UpdateSpeakerStatus uses entindex < 32
-        voice_status.cpp destructor    asks the engine for the game dir at unload            changed
-        ammo.cpp CurWeapon, WeaponList dead tests (SDK 2.3's too)                            kept
0xa47f0  hud.cpp DrawHudNumber          indexes the digits by iNumber / 1000 (callers clamp)  kept
-        hud.cpp CHud::Redraw           the screenshot time kept as an int                    kept
-        hud.cpp CHud::Think            in multiplayer overwrites default_fov with 90 every   kept
                                        frame
-        hud.cpp                        cl_gibcount created and never read                    kept
-        status_icons.cpp               drawn from ScreenWidth / 2 without centring the icon  kept
-        StudioModelRenderer.cpp        writes into the shared model data: sequence 5 gets    kept
         StudioSetupTransformBones      STUDIO_LOOPING, hassassin.mdl's controllers point at
                                        a static array
-        StudioModelRenderer.cpp        exits the game (exit(-1)) on a model with more than   kept
         StudioCalcAttachments          4 attachments, now without the SDK's message
-        StudioModelRenderer.cpp        the houndeye's and headcrab's swim frame divides by   kept
                                        linearmovement[0] unchecked (an infinite frame)
-        StudioModelRenderer.cpp        StudioProcessGait adds 180 both ways; the constructor kept
                                        clears only the assassin's model pointer
-        input.cpp IN_JoyMove           runs only while the houndeye sleeps                   kept
-        input.cpp CL_AdjustAngles      lost SDK 2.3's roll clamp                             kept
-        rc2/vgui_attributes.cpp        CAttrSlotMenu sets its LineBorder twice (one leaks);  kept
                                        TGA streams closed but never deleted
-        vgui_CustomObjects.cpp         a bound key of (char)255 printed as a character       kept
         RecalculateText
-        vgui_TeamFortressViewport.cpp  a menu queued behind a non-class menu skips           kept
         ShowVGUIMenu                   UpdateCursorState
-        view.cpp V_StartViewShake      RandomFloat(1.2566, 1.0472), low above high           kept
-        view.cpp V_UpdateScreenTint    the clamp to 255 compares the truncated byte: never   kept
-        view.cpp V_GetInEyePosition    scales pitch by -pi (SDK 2.3: -3)                     kept
-        view.cpp V_CalcNormalRefdef    view mode 2 reuses the stale delta as the basis' axis kept
-        view.cpp V_CalcSpectatorRefdef the turn test's acos(dot) / (l1 l2)                   kept
```
