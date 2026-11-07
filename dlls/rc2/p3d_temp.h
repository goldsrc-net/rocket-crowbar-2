/***
*
*	TEMP STUB (p3 D): what h_battery.cpp, handgrenade.cpp and the RC2 hand grenades use from
*	RC2 files other packages reconstruct. Inline so the files share one copy; delete this
*	header at merge once the real definitions exist. p3b_temp.h has the ones group B needed too
*	(gmsgCLTE, gmsgWeaponSync, RC2ShieldDamage, RC2EntMark*, RC2RaiseTime, RC2FindEntityNear
*	for 0xf33b0, RC2GameRulesAllowGaussTeleport for gamerules slot 58).
*
****/

#pragma once

#include "UserMessages.h"

// Real ones belong in the run "new: pm_shared_pm_shared .. python" (orig 0xaa950, 0xaaa20): a
// table of 64 pull points that CBaseEntity::ApplyExternalForce (orig 0xa9ba0) draws entities
// towards. Add returns the slot, or -1 when the table is full; Remove frees it. Names ours.
inline int RC2AddPullPoint(Vector vecOrigin, float flRadius, float flPlayerConstant, float flMonsterConstant)
{
	return -1;
}
inline void RC2RemovePullPoint(int iPoint) {}

// Real one belongs in player.cpp (orig 0x95890, a wrapper of 0x95690): the player's attribute
// iAttribute (the float array at orig player +0x6e0), 1.0 when iPlayerIndex is no player. Name ours.
inline float RC2PlayerAttribute(int iAttribute, int iPlayerIndex)
{
	return 1.0f;
}
