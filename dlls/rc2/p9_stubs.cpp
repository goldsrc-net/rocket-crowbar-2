/***
*
*	Rocket Crowbar 2, work package p9: temporary definitions of what p9's files use from other
*	packages (rc2/p9_needs.h), so that p9's worktree links. None of these is original code.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "rc2/p9_needs.h"

// TEMP STUB (p9): real one belongs in player.cpp (p5), registered as "WeaponSync"
int gmsgWeaponSync = 0;

// TEMP STUB (p9): real one belongs in player.cpp (p5), registered as "CLTE"
int gmsgCLTE = 0;

// TEMP STUB (p9): real ones belong in player.cpp (p5), registered as "VoteBegin" and "VoteUpdate"
int gmsgVoteBegin = 0;
int gmsgVoteUpdate = 0;

// TEMP STUB (p9): real one belongs in p1's maintainent.cpp (0x25e0, static there). By index only.
CBasePlayer* Admin_FindPlayer(const char* pszName)
{
	return (CBasePlayer*)UTIL_PlayerByIndex(atoi(pszName));
}

// TEMP STUB (p9): real one belongs in p7's file for 0xc45b0 (the shield). Lets everything through.
float ShieldDeflect(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecHitPos, Vector vecDir, Vector* pvecDeflectPos, Vector* pvecDeflectDir, int iEffect)
{
	return flDamage;
}

// TEMP STUB (p9): real one belongs in tripmine.cpp (p8): CTripmineGrenade's EHANDLE at orig +0x304
CBaseEntity* TripmineGrenade_GetPlacer(CBaseEntity* pMine)
{
	return NULL;
}

