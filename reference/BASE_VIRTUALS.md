# The virtuals RC2 added to SDK 2.3's base classes

Found 2026-11-07 by `scripts/slots.py` (each stock class's vtable paired with SDK 2.3's) and read
off rc2.dll. They come first in the reconstruction, before anything is split by file, as RC 1.9's
did in mod-rockcrowbar.

**Names are ours.** Unlike RC 1.9's `.so`, rc2.dll keeps no names for these: only the classes and
their exported think/touch/use methods survive. A slot gets RC 1.9's name where it is clearly the
same virtual, otherwise a name from its behavior, and the reconstruction marks every such name as
ours.

Slot numbers are RC2's (byte offset = slot * 4; Ghidra prints some offsets in decimal, 0x12c as
`300`). SDK 2.3's `CBaseEntity` has 57 slots (0-56), RC2's 79. "Calls" counts virtual calls at that
offset in `recovered/decomp/rc2/` on anything but `g_pGameRules` (`DAT_1018c504`, whose own
virtuals share the offsets). "Overridden by" is every entity class whose vtable has something
other than `CBaseEntity`'s default there.

## CBaseEntity

One slot is inserted before SDK 2.3's `Respawn` (slot 47), 21 are appended after `FVisible`
(slots 58-78). So SDK 2.3's `Respawn`, `UpdateOwner`, `FBecomeProne` are RC2's 48, 49, 50, and
`Center` onward moves by one.

```
slot calls default             overridden by                 reading                       name
 47   183  return this         -                             the entity an effect really   GetEffectedEnt (RC 1.9's
                                                             lands on: CBaseEntity's code   name; its default there
                                                             forwards TakeDamage to it when returned NULL). Settled
                                                             it isn't this; used by every   shape; who overrides it is
                                                             projectile and grenade         still to read
 58     0  return 0, 4 args    CBaseMonster CImpailer        with 69, what the grabbing    tentative: GetPullOrg
                               CSuckMine                     classes do (RC 1.9: GetPullOrg)
 59     1  return 1.0f         -                             a force scale read by 60      tentative: ForceScale
 60    21  real body (0xa9ba0) -                             integrates an outside force   ours: ApplyExternalForce
                                                             into pev->velocity over the    (float interval)
                                                             interval (walk/toss only),
                                                             lifting off the ground; from
                                                             think functions
 61     1  [Glock] TransphasicImpactDamage (50000)  -          the damage each effect deals  ours, from the key:
 62     0  [Tripmine] GlobalBeamDamage (5000)  CBaseMonster  to this entity: the rc2.ini   TransphasicImpactDamage,
                               CGargantua CTentacle          value by default, a fixed     GlobalBeamDamage,
                               CTripmineGrenade              amount in the overrides (the  FartDamageInflict,
 63     1  [MP5] FartDamageInflict (5000)  CApache           big monsters take less).      ZoomedPrimaryDamage,
                               CGargantua COsprey CTentacle  64-65 read in                 ZoomedSecondaryDamage,
 64     3  [Crossbow] ZoomedPrimaryDamage (350)  + CNihilanth CCrossbow::DelayedHurt;      BeamCoreDamage,
 65     1  [Crossbow] ZoomedSecondaryDamage (50000)          66-68 together in the         BeamRadialMaxDamage,
                               + CNihilanth                  houndeye transform's beam     BeamViewDamage
 66     4  [TransformHoundeye] BeamCoreDamage (50000)
                               CBasePlayer CHoundeye
 67     1  [TransformHoundeye] BeamRadialMaxDamage (250)
                               CBasePlayer CHoundeye
 68     2  [TransformHoundeye] BeamViewDamage (5)
                               + CApache CBigMomma CGargantua
 69     1  void, 1 arg         CBaseMonster CImpailer        release, with 58              tentative
                               CSuckMine
 70     1  return 1            CRocketMine CSphereMine       gauss: hull to trace          GetGuassHull (RC 1.9)
                               CSuckMine
 71     2  return 56.0f        same three                    gauss: distance out (RC 1.9:   GetGuassDistOut (RC 1.9)
                                                             int 56)
 72     2  if takedamage:      -                             the gravity grenade's kill    ours, from the key:
           TakeDamage(a, b,                                  hit, [Grenade]                 GravGrenAnnihilate
           [Grenade] GravGren-                               GravGrenAnnihilateDamage       (inflictor, attacker)
           AnnihilateDamage,
           ALWAYSGIB|ENERGY-
           BEAM|BLAST)
 73    81  return 0            CBasePlayer: member at        "a player in play": replaces  ours: IsActivePlayer
                               +0x688 == 0                   SDK 2.3's IsPlayer()/"player"
                                                             classname checks in touches
                                                             (CAirtank::TankTouch,
                                                             CBaseButton::ButtonTouch,
                                                             CItem::ItemTouch)
 74     1  return 0            92 classes return 1           -                             open
 75    27  return 0, 2 args    41                            capability bits callers mask  tentative: GetRCCaps (RC
 76    34  return 0, 2 args    24                            (0x800, 0x20, 8, 2) before a   1.9's had no arguments);
                                                             mine, grenade or ball acts     two sets
 77     6  real body: player   CBaseMonster CBasePlayer      true when the player's        ours: IsViewOfPlayer
           arg; owner index ==  CSnarkCam CTripmineGrenade   members +0x69c/+0x6a4/+0x6a8  (CBaseEntity*)
           one of 3 player     CVirtualHull                  hold this entity's owner;
           members                                           from SetupVisibility
 78     3  return 0, 1 arg     CUltimaGrenade (returns 1)    -                             open
```

The rc2.ini values come from `reference/prefs-rc2.tsv` (`scripts/prefs.py`): a static table of
1,113 key records (type, heading, key, default) with a parallel array of 8-byte value slots at
0x191bc8, which is what these defaults read.

## CBaseMonster

One slot, RC2 slot 147, right after `StopFollowing`; 8 calls (CAGrunt::PrescheduleThink,
CScientistMine::SciMineThink, four small functions at 0x7b60-0x7c50). The default (0x74d0)
compares `pev` floats at +0x144.

## CBasePlayerWeapon

Four slots: two after `PlayEmptySound`, two after `RetireWeapon` (the slot pairing against the
RC2-layout reference build places them; they are not on `CBasePlayerItem`). In the vtables
before the RC2-layout correction they were listed as slots 93-96. Defaults in 17 of 18
weapons: one returns the member at +0xbc, one returns -1, one is a real function comparing
the player's members at +0x4d4 and +0x5ec, one returns `this`. The weapons package names and
declares them. Their offsets collide with `CBaseMonster`'s own virtuals, so their call sites
have to be read from the weapon code.

## Members

From the class sizes (each factory's allocation) and the save tables' offsets against SDK 2.3:

```
CBaseEntity         +28 bytes after m_fireState (dlls/cbase.h, neutral names until read)
CBaseMonster        +4 after m_pCine
CTalkMonster        +4 before m_useTime
CBasePlayerWeapon   +8 before m_flTimeWeaponIdle
CBasePlayer         +2576 after its last stock member
```

New saved fields: `m_iDefaultAmmo2` (weapons), `CFuncTank::m_pControllerEdict` and
`m_flIgniteTime`, `CIchthyosaur::m_flDeadTime`.

## RC 1.9's, for comparison

`../mod-rockcrowbar/dlls/cbase.h`, "the virtuals RC added to CBaseEntity": AskQuest, AnswerQuest,
AddMemeber, GetGuassHull, GetGuassDistOut, IsDriveent, GetBotShootOrg, GetBotTargetWeight,
BotTargetRealseable, ProMagApplyForce, DeflectApply, ExtreemeDamageFract, VeryHeavyDamageFract,
RemoveTouch, GetControllerName, GetRealOwner, GetEffectedEnt, GetPullOrg, EntIsBeingRemoved,
PlayerIsDissconecting, PlayerWasKilled, GetRCCaps, HasRCCap.
