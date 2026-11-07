/***
*
*	Rocket Crowbar 2: what work package p8's files use from code other packages reconstruct.
*
*	Each is RC2's own function or global, declared here under our name with its original
*	address. The ones nothing defines yet have a temporary definition in p8_stubs.cpp, marked
*	TEMP STUB; at merge each moves to its real file and the stub goes.
*	reference/dossiers/p8.md, "Cross-file needs".
*
****/

#pragma once

#include "rc2prefs.h"

class CBaseEntity;

// rc2.ini's short keys are read zero-extended (movzx) in the original.
inline int RC2PrefWord(rc2pref_e index) // name ours
{
	return (unsigned short)RC2Pref(index).s;
}

// at 0x181434, set to 1 at the top of CBasePlayerWeapon::ItemPostFrame and initialized to 1:
// the weapon code plays its events only while it is set. Name ours. Real one: weapons.cpp.
extern int g_iPlayWeaponEvents;

// at 0xf5900 (util): push flTime out to flBase + flAdd unless it is already later. Name ours.
void UTIL_SetMaxTime(float& flTime, float flBase, float flAdd);

// at 0xfb780 (weapons): the server's stand-in for the client's byte reader that
// ReadWeaponState uses; returns 0. Name ours. VERIFY: argument types.

// User messages (player.cpp's LinkUserMessages)
extern int gmsgWeaponSync; // "WeaponSync", at 0x18f4c0

// at 0xcbfe0 (new: skill .. sound, CSnarkCam): the owner's view switches to the cam. Name ours.
void SnarkCam_TakeView(CBaseEntity* pCam);

// at 0xc45b0 (new: scripted .. shotgun): what is left of flDamage after pHit's (or the entity
// it stands for, slot 47) HEV shield takes its share; the original's twelve arguments, by its
// reading: two vectors by value, two optional float* outputs, a bit set. Name ours. VERIFY: the
// owner package's reading of the arguments.
float RC2_ShieldFilterDamage(CBaseEntity* pHit, float flDamage, float flScale, Vector vecSrc, Vector vecDir, float* pvecOut, float* pflOut, int bits);

// CSqueakGrenade::m_hOwner (orig +0x2dc). CSqueakGrenade is declared inside squeakgrenade.cpp
// (another package's file), so p8 reads it through this. Ours; the original read the member.
CBaseEntity* SqueakGrenade_Owner(CBaseEntity* pSnark);
