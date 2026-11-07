/***
*
*	Rocket Crowbar 2: what package p5's player.cpp uses from other parts of rc2.dll that are
*	not reconstructed yet. Each has a temporary definition at the bottom of player.cpp (TEMP
*	STUB block) until its own package's file has it; reference/dossiers/p5.md lists them.
*	Every name here is ours.
*
****/

#pragma once

class CBaseEntity;
class CBasePlayer;

// world.cpp (CWorld::Spawn, orig 0xfc9d0): mp_scoremode as read at map start, 0 or 1
extern int g_iScoreMode;

// util.cpp, orig 0xf58a0: a shared entvars whose classname is set to pszName, for the death
// notice of damage nothing really inflicted ("water", "dmg_reflect"); the world's without one
extern entvars_t* UTIL_DummyInflictor(const char* pszName);
// util.cpp, orig 0xf55b0: TE_KILLBEAM and CLTE 0xff on the entity
extern void UTIL_KillEntityEffects(edict_t* pent);
// util .. voice_gamemgr, orig 0xf6fb0: keep the player's virtual hulls (m_rgpVirtualHulls) in place

// The freeze and antifriction timers (orig 0x41e80, 0xaa40): remove those the player owns
extern void RemoveFreezeTimers(edict_t* pOwner);
extern void RemoveAntiFricTimers(edict_t* pOwner);

// The transformations (new: tentacle .. triggers, orig 0xe1fa0-0xe3750)
extern void TransformSetAnimation(CBasePlayer* pPlayer, int playerAnim); // orig 0xe1fa0
extern void TransformTookDamage(CBasePlayer* pPlayer);					  // orig 0xe20c0
extern void TransformKilled(CBasePlayer* pPlayer);						  // orig 0xe2070
extern void TransformDeathSound(CBasePlayer* pPlayer);					  // orig 0xe2080
extern void TransformMovement(CBasePlayer* pPlayer);					  // orig 0xe2000
extern void TransformPostThink(CBasePlayer* pPlayer);					  // orig 0xe2040
extern void TransformFinish(CBasePlayer* pPlayer);						  // orig 0xe2830
extern void TransformRevert(CBasePlayer* pPlayer);						  // orig 0xe2f90
extern int TransformClassify(int iTransform);							  // orig 0xe3750

// Transform 6's think (new: python .. rat, orig 0xae540)
extern void Transform6Think(CBasePlayer* pPlayer);

// rc2.ini to the client (new: rat .. roach, orig 0xaf370, 0xaf270)
extern void SendPrefsReset(edict_t* pClient);
extern void SendPrefsChanged(edict_t* pClient);
// Riding (new: rat .. roach, orig 0xb1f50): get off the shoulders

// The pm_shared side (new: pm_shared .. python, orig 0xaaa60, 0xaac20)
extern void ResetPMTables(CBasePlayer* pPlayer);
extern void SendWeaponSync(CBasePlayer* pPlayer);

// The function at orig 0x1050 a new client's +0x674 comes from; the one at orig 0x50a0 Spawn
// ends with; the check at orig 0x1000 DBGCounts makes (all in new: start .. aflock, p1)
extern int ClientFlags1050(edict_t* pClient);
extern void PlayerSpawned50a0(edict_t* pClient);
extern bool IsListenHost1000(edict_t* pClient);

// gamerules: a random spawn point by classname (orig 0xf31a0, util)
extern CBaseEntity* UTIL_RandomEntityByClassname(const char* pszClassname);

// The global the cheat impulse and the multiplayer PlayerSpawn clear (orig .data 0x181430).
// VERIFY: what it gates
extern bool g_bRC2EquipFlag;

// util.cpp (orig 0xf5900): *pflTime = max(*pflTime, flBase + flDelay); stubbed in rc2/nuke.cpp
extern void UTIL_DelayAttack(float* pflNextAttack, float flBase, float flDelay);

// util.cpp (orig 0xf30f0): how many entities have that classname
extern int UTIL_CountEntitiesByClassname(const char* pszClassname);
