/***
*
*	Work package p8's temporary definitions of what other packages reconstruct, so the
*	server links. Each says where the real one belongs; at merge it goes.
*	Declarations and addresses: p8_needs.h.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "p8_needs.h"

// TEMP STUB (p8): real one belongs in weapons.cpp
int g_iPlayWeaponEvents = 1;

// TEMP STUB (p8): real one belongs in util.cpp
void UTIL_SetMaxTime(float& flTime, float flBase, float flAdd)
{
	if (flTime < flBase + flAdd)
		flTime = flBase + flAdd;
}

// TEMP STUB (p8): real one belongs in weapons.cpp
unsigned char WeaponSync_ReadByte(void** ppbuf, int* piSize)
{
	return 0;
}

// TEMP STUB (p8): real one belongs in player.cpp (LinkUserMessages registers "WeaponSync")
int gmsgWeaponSync = 0;

// TEMP STUB (p8): real one belongs in the snark cam's file (new: skill .. sound, CSnarkCam)
void SnarkCam_TakeView(CBaseEntity* pCam)
{
}

// TEMP STUB (p8): real one belongs in the shotgun scientist / HEV file (new: scripted .. shotgun)
float RC2_ShieldFilterDamage(CBaseEntity* pHit, float flDamage, float flScale, Vector vecSrc, Vector vecDir, float* pvecOut, float* pflOut, int bits)
{
	return flDamage;
}

// TEMP STUB (p8): real one belongs in squeakgrenade.cpp (return m_hOwner)
CBaseEntity* SqueakGrenade_Owner(CBaseEntity* pSnark)
{
	return NULL;
}

// TEMP STUB (p8): real one belongs in the file of the code at 0x74d0 (new: start .. aflock),
// which maps pev->scale to a pitch offset
int CBaseMonster::ScalePitchShift()
{
	return 0;
}
