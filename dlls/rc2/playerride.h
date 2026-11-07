/***
*
*	Rocket Crowbar 2: one player riding on another's shoulder (dlls/rc2/playerride.cpp).
*
****/

#pragma once

// The byte pm_shared reads for a player: the ridden player's index in bits 0-6, bit 7 set on
// the left shoulder. A static zero byte when iEntIndex isn't a player.
unsigned char* PM_GetRideInfo(int iEntIndex); // name ours

// An entity's v.origin, v.v_angle, v.mins and v.maxs by index, for pm_shared; leaves the
// outputs alone when the index has no valid entity.
void PM_GetEntityBox(int iEntIndex, float* vecOrigin, float* vecVAngle, float* vecMins, float* vecMaxs); // name ours

// True when pRider is on one of pPlayer's shoulders.
bool PlayerHasRider(edict_t* pPlayer, edict_t* pRider); // name ours

// The four bytes at CBaseEntity +0x64..+0x67 (cbase.h's m_iRC2Base64): RC2 uses them one by one.
#define RC2_BASE_BYTE(pEnt, iOffset) (((unsigned char*)&(pEnt)->m_iRC2Base64)[(iOffset)-0x64])
