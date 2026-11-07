# The virtuals RC2 added to SDK 2.3's base classes

Found 2026-11-07 by `scripts/slots.py` (each stock class's vtable paired with SDK 2.3's) and
read off rc2.dll. None is named yet. They come first in the reconstruction, before anything is
split by file, as RC 1.9's did in mod-rockcrowbar.

Slot numbers are RC2's (byte offset = slot * 4). SDK 2.3's `CBaseEntity` has 57 slots (0-56);
RC2's has 79. Offsets 0xc0 and 0xe8-0x138 are used by nothing else in rc2.dll's vtables, so every
`call [reg + off]` there is a call to one of these; "calls" counts them. "Overridden by" is every
entity class whose vtable has something other than `CBaseEntity`'s default in that slot.

## CBaseEntity

```
slot  default body                    calls  overridden by (classes)
 48   return 0 (between Respawn and      6   31: the weapons and ammo, CHealthKit, ...; called from
      UpdateOwner)                            CItem::ItemTouch, CBasePlayerAmmo::DefaultTouch,
                                              CHealthKit::Precache. The overrides have real bodies
 58   return 0, 4 arguments (ret 0x10)   14   CBaseMonster CImpailer CSuckMine
 59   return 1.0f                         4   none
 60   real body (0xa9ba0)                21   none; called from think functions (MonsterThink,
                                              ControlThink, the mines and grenades)
 61   return a float global (0x192e00)    1   none
 62   return a float global (0x193d40)   17   CBaseMonster CGargantua CTentacle CTripmineGrenade
 63   return a float global (0x1932c0)    3   CApache CGargantua COsprey CTentacle
 64   return a float global (0x192658)    5   CApache CGargantua CNihilanth COsprey CTentacle
 65   return a float global (0x192668)    1   CApache CGargantua CNihilanth COsprey CTentacle;
                                              63-65 read in CCrossbow::DelayedHurt. The big
                                              monsters return fixed floats: the shape of RC 1.9's
                                              ExtreemeDamageFract / VeryHeavyDamageFract
 66   return a float global (0x192158)    4   CBasePlayer CHoundeye
 67   return a float global (0x192170)    1   CBasePlayer CHoundeye
 68   return a float global (0x192178)    2   CApache CBasePlayer CBigMomma CGargantua CHoundeye
 69   void, 1 argument                    1   CBaseMonster CImpailer CSuckMine
 70   return 1                            1   CRocketMine CSphereMine CSuckMine
 71   return 56.0f                        2   CRocketMine CSphereMine CSuckMine
 72   real body (0x4d00, reads pev+0x16c) 2   none; called from CGravGrenade
 73   return 0                           90   CBasePlayer only: returns (member at +0x688 == 0).
                                              Called from HurtTouch, TakeDamage, Use functions
 74   return 0                            1   92 classes, the monsters and items, all return 1
 75   return 0, 2 arguments              32   41: monsters, players, the grenades and mines;
                                              called from the attach/teather touches
 76   return 0, 2 arguments              36   24
 77   real body (0x1a7e0)                 7   CBaseMonster CBasePlayer CSnarkCam CTripmineGrenade
                                              CVirtualHull; called from SetupVisibility
 78   return 0, 1 argument                3   CUltimaGrenade (returns 1)
```

The float globals at 0x192xxx-0x193xxx are past `.data`'s file data (uninitialized), so they are
set at run time, most likely from `rc2.ini` (`rccfgfile`), as RC 1.9 looked up its prefs.

## CBaseMonster

One slot, RC2 slot 147, right after `StopFollowing`; 8 calls (CAGrunt::PrescheduleThink,
CScientistMine::SciMineThink, four small functions at 0x7b60-0x7c50). The default (0x74d0)
compares `pev` floats at +0x144.

## CBasePlayerItem

Four slots after `AttachToPlayer` (RC2 slots 93-96), in 17 weapon classes. Their offsets
collide with `CBaseMonster`'s own virtuals (slot 93 is `CheckLocalMove` there), so call sites
can't be told apart by offset alone; read them from the weapon code.

## To name them

Read each slot's overrides and callers in `recovered/decomp/rc2/` (`scripts/ghidra/re.sh export
rc2`), and compare with RC 1.9's 23 (`../mod-rockcrowbar/dlls/cbase.h`, "the virtuals RC added to
CBaseEntity"): GetGuassHull, GetGuassDistOut, IsDriveent, the Bot* interface, ProMagApplyForce,
DeflectApply, ExtreemeDamageFract, VeryHeavyDamageFract, RemoveTouch, GetControllerName,
GetRealOwner, GetEffectedEnt, GetPullOrg, EntIsBeingRemoved, PlayerIsDissconecting,
PlayerWasKilled, GetRCCaps, HasRCCap. RC2 shares code with RC 1.9 (its LINK_ENTITY_TO_CLASS
still calls AddToPrecachedList), so some carry over.
