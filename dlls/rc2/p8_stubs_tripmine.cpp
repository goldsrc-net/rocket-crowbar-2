/***
*
*	Temporary definitions for tripmine.cpp (work package p8); each says where the real one
*	belongs and goes at merge. Declarations: p8_needs_tripmine.h.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "p8_needs_tripmine.h"

// TEMP STUB (p8): real one belongs in weapons.cpp
int g_iAllowWeaponReplace = 1;

// TEMP STUB (p8): real one belongs in util.cpp (this one follows the original's body)
bool UTIL_RayHitsBox(Vector vecStart, Vector vecDir, Vector vecMins, Vector vecMaxs, float* pflDist)
{
	if (vecStart.x < vecMaxs.x && vecStart.x >= vecMins.x && vecStart.y < vecMaxs.y && vecStart.y >= vecMins.y && vecStart.z < vecMaxs.z && vecStart.z >= vecMins.z)
	{
		if (pflDist)
			*pflDist = 0;
		return true;
	}

	if (vecDir == g_vecZero)
		return false;

	float flFar = 9999.0;
	float flNear = -9999.0;
	for (int i = 0; i < 3; i++)
	{
		if (vecDir[i] == 0)
		{
			if (vecMaxs[i] < vecStart[i] || vecStart[i] < vecMins[i])
				return false;
		}
		else
		{
			float t1 = (vecMins[i] - vecStart[i]) / vecDir[i];
			float t2 = (vecMaxs[i] - vecStart[i]) / vecDir[i];
			if (t2 < t1)
			{
				float t = t1;
				t1 = t2;
				t2 = t;
			}
			if (flNear < t1)
				flNear = t1;
			if (t2 < flFar)
				flFar = t2;
			if (flFar < flNear)
				return false;
			if (flFar < 0)
				return false;
		}
	}
	if (pflDist)
		*pflDist = flNear;
	return true;
}

// TEMP STUB (p8): real one belongs in util.cpp
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal)
{
	return vecDir + vecNormal * 2 * -DotProduct(vecDir, vecNormal);
}

// TEMP STUB (p8): real one belongs in ggrenade.cpp (the original's takes no damage bits)
void RC2_GrenadeExplode(CGrenade* pGrenade, TraceResult* pTrace)
{
	pGrenade->Explode(pTrace, DMG_BLAST);
}

// TEMP STUB (p8): real one belongs in player.cpp (LinkUserMessages registers "CLTE")
int gmsgCLTE = 0;

// TEMP STUB (p8): becomes a read of CBasePlayer's member at orig +0x778 once player.h declares it
bool RC2_PlayerShieldOn(CBaseEntity* pPlayer)
{
	return false;
}
