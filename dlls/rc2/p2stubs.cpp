/***
*
*	Work package p2's temporary stubs: just enough for the server to link while the files these
*	belong to are reconstructed by other packages. None carries an orig address; each says where
*	the real one belongs (reference/dossiers/p2.md, "Cross-file needs"). Delete each at merge.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "p2shared.h"

// TEMP STUB (p2): real one belongs in weapons.cpp (orig .data 0x181434, initialised 1)
bool g_runfuncs = true;

// TEMP STUB (p2): real one belongs in player.cpp (orig .data 0x18f3c8)
bool g_fGivingItem = false;

// TEMP STUB (p2): real one belongs in weapons.cpp (orig .data 0x181430, initialised 1)
bool g_fItemTimeout = true;

// TEMP STUB (p2): real ones belong in player.cpp (LinkUserMessages registers "WeaponSync", "CLTE")
int gmsgWeaponSync = 0;
int gmsgCLTE = 0;

// TEMP STUB (p2): real one belongs in util.cpp (0xf5900)
void UTIL_ExtendTime(float* pflTime, float flBase, float flDelay)
{
	if (*pflTime < flBase + flDelay)
		*pflTime = flBase + flDelay;
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf5630)
Vector UTIL_RandomSpreadDir(Vector vecForward, Vector vecRight, Vector vecUp, float flSpread)
{
	return vecForward;
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf5800)
Vector UTIL_Reflect(Vector vecDir, Vector vecNormal)
{
	return vecDir - vecNormal * 2 * DotProduct(vecDir, vecNormal);
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf55b0)
void UTIL_KillTrail(edict_t* pent)
{
}

// TEMP STUB (p2): real one belongs in the run between scripted and shotgun (0xc45b0)
float RC2ShieldDamage(CBaseEntity* pTarget, float flDamage, float flShieldFraction, Vector vecPos, Vector vecDir, Vector* pvecHit, float* pflOut, int iFlags)
{
	if (pvecHit)
		*pvecHit = vecPos;
	return flDamage;
}

// TEMP STUB (p2): real one belongs in its RC2 run (0xb28b0)
bool IsPlayersViewEnt(edict_t* pPlayer, edict_t* pEnt)
{
	return false;
}

// TEMP STUB (p2): real one belongs in player.cpp (0x958b0)
int RC2PlayerButtons(int iIndex)
{
	return 0;
}

// TEMP STUB (p2): real one is CXBowTeather::DieThink (0x29aa0), p1's crossbow bolts
void XBowTeatherDie(CBaseEntity* pTeather)
{
}

// TEMP STUB (p2): real ones belong in weapons.cpp (0xfb780, 0xfb7b0)
unsigned char WeaponSync_ReadByte(void** ppBuf, void** ppSize)
{
	return 0;
}

float WeaponSync_ReadFloat(void** ppBuf, void** ppSize)
{
	return 0;
}
