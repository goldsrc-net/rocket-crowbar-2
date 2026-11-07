/***
*
*	TEMP STUB (p3 B): what gargantua.cpp, gauss.cpp and gaussbecon.cpp use from RC2 files
*	other packages reconstruct. Inline so that the three files share one copy and a real
*	definition elsewhere replaces them at merge (delete this header then).
*
****/

#pragma once

#include "UserMessages.h"

// Real ones belong in the run "new: plats .. player" (orig 0x94240 registers them): RC2's
// user messages "CLTE" (orig .data 0x18f4b8, client temp entities: byte type, then by type) and
// "WeaponSync" (0x18f4c0, byte weapon id, then by weapon).
// gmsgCLTE, gmsgWeaponSync: UserMessages.h (p5)

// Real one belongs in the run "new: scripted .. shotgun" (orig 0xc45b0, 1832 bytes): the damage
// left after pEntity's shield (an RC2 player member at orig +0x778 says it is up), scaled by
// flShieldFraction when it is hit; *pvecHitOut gets the hit point moved onto the shield. iFlags
// 0..15 plus bit 0x10 internally. Names ours. Without it nothing is shielded.
inline float RC2ShieldDamage(CBaseEntity* pEntity, float flDamage, float flShieldFraction, Vector vecHit, Vector vecDir, Vector* pvecHitOut, Vector* pvecOther, int iFlags)
{
	if (pvecHitOut)
		*pvecHitOut = vecHit;
	return flDamage;
}

// Real ones belong in util.cpp (orig 0xf4c70, 0xf4d40, 0xf4d90, 0xf4dd0): a stack (128 deep) of
// bit sets with a bit per entity index, so an area effect hurts each entity once. Names ours.
inline void RC2EntMarkPush() {}
inline void RC2EntMarkPop() {}
inline void RC2EntMarkSet(int iIndex) {}
inline bool RC2EntMarkTest(int iIndex) { return false; }

// Real one belongs in util.cpp (orig 0xf5900): *pflTime = max(*pflTime, flBase + flDelay), how
// RC2's weapons push their four attack timers. Name ours.
inline void RC2RaiseTime(float* pflTime, float flBase, float flDelay)
{
	if (*pflTime < flBase + flDelay)
		*pflTime = flBase + flDelay;
}

// Real one belongs in util.cpp (orig 0xf58a0): an entvars for a fake inflictor whose classname
// is pszName (a static entity the util code keeps; the world when it has none), so death notices
// name the weapon. Name ours. The stub returns the world's.
inline entvars_t* RC2NamedInflictor(const char* pszName)
{
	return VARS(INDEXENT(0));
}

// Real one belongs in player.cpp (orig 0x95860, via 0x958b0/0x956c0): bit 0x200 of an RC2
// player member at orig +0x708 (looked up by index). With it the gauss works under water. Name
// ours; the stub says no, which is stock Half-Life's rule.
inline bool RC2PlayerWaterWeapons(CBaseEntity* pPlayer) { return false; }

// Real one belongs in util.cpp (orig 0xf33b0, 1357 bytes): the next entity after pStart (not a
// BSP model's, or a BSP model by its box) whose nearest point to vecCenter is within flRadius;
// that point goes to *pvecNearest. Name ours; the stub finds nothing.
inline CBaseEntity* RC2FindEntityNear(CBaseEntity* pStart, Vector vecCenter, float flRadius, void* pUnused, Vector* pvecNearest) { return NULL; }


// Real one belongs in gamerules.cpp: RC2's CGameRules virtual at orig slot 58 (vtable +0xe8),
// asked (pPlayer, pEntity) before the gauss moves pEntity; false refuses. Name ours.
inline bool RC2GameRulesAllowGaussTeleport(CBasePlayer* pPlayer, CBaseEntity* pEntity) { return true; }

// Real one belongs in util.cpp (orig 0xf31a0, counting with 0xf30f0): a random entity of the
// class (RANDOM_LONG(0, count) steps along the list, wrapping past its end). Name ours; this
// stub does what the original does.
inline CBaseEntity* UTIL_FindRandomEntityByClassname(const char* pszName)
{
	int iCount = 0;
	CBaseEntity* p = NULL;
	while ((p = UTIL_FindEntityByClassname(p, pszName)) != NULL)
		iCount++;
	if (iCount < 1)
		return NULL;
	int iSteps = RANDOM_LONG(0, iCount);
	p = UTIL_FindEntityByClassname(NULL, pszName);
	for (int i = 0; i < iSteps || (p == NULL && i < 1000); i++)
		p = UTIL_FindEntityByClassname(p, pszName);
	return p;
}
