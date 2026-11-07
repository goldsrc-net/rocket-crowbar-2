/***
*
*	Temporary definitions, so the server links, of what the transform files use from other
*	packages. Each says where the real one belongs; at merge it goes.
*	Declarations and addresses: p8_needs_transform.h, player.h's p8 block.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "p8_needs_transform.h"

// TEMP STUB (p8): real ones belong in player.cpp (CBasePlayer::Precache sets them)
unsigned short g_usTAssassinGun;
unsigned short g_usTAssassinKick;
unsigned short g_usTHoundeyeBeam;
unsigned short g_usTHoundeyeSonic;
unsigned short g_usTControllerThrow;
unsigned short g_usTBatBite;

// TEMP STUB (p8): real ones belong in player.cpp (LinkUserMessages)
// gmsgCLTE: its TEMP STUB is in p8_stubs_tripmine.cpp (both halves of p8 use it)
int gmsgTformStat = 0;

// TEMP STUB (p8): real one belongs in util.cpp
entvars_t* UTIL_NamedInflictor(const char* pszName)
{
	return VARS(INDEXENT(0));
}

// TEMP STUB (p8): real one belongs in util.cpp
CBaseEntity* UTIL_FindEntityNearSphere(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecClosest)
{
	return NULL;
}

// TEMP STUB (p8): real one belongs in util.cpp
int UTIL_PackNormal(Vector vecNormal)
{
	return 0;
}

// TEMP STUB (p8): real ones belong in util.cpp
void UTIL_PushEntFlags()
{
}

bool UTIL_TestEntFlag(int iEntIndex)
{
	return false;
}

void UTIL_SetEntFlag(int iEntIndex)
{
}

void UTIL_PopEntFlags()
{
}

// TEMP STUB (p8): real one belongs in util.cpp
int UTIL_PunchTrace(Vector vecStart, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iDepth)
{
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pentIgnore, ptr);
	return 0;
}

// TEMP STUB (p8): real one belongs in the gamerules package (g_pGameRules' slot 58)
bool RC2_GameRulesCanKnock(CBasePlayer* pAttacker, CBaseEntity* pVictim)
{
	return true;
}

// TEMP STUB (p8): real ones belong in player.cpp
void CBasePlayer::AddMoveLock(int bits)
{
}

void CBasePlayer::RemoveMoveLock(int bits)
{
}

void CBasePlayer::AddPhysFlags(int bits)
{
}

void CBasePlayer::RemovePhysFlags(int bits)
{
}
