/***
*
*	Rocket Crowbar 2: what tripmine.cpp (work package p8) uses from code other packages
*	reconstruct. Names ours, addresses the original's. Temporary definitions are in
*	p8_stubs_tripmine.cpp (TEMP STUB); at merge each moves to its real file.
*
****/

#pragma once

#include "p8_needs.h"

class CGrenade;

// at 0x181430 (weapons.cpp's): initialized to 1; while 0, a weapon created with
// CBaseEntity::Create isn't swapped by RC2's weapon replacement (0xfa100), which the
// tripmine clears around making the weapon_tripmine it drops. Name ours.
extern int g_iAllowWeaponReplace;

// at 0xf3200 (util): does the ray vecStart + t*vecDir (t >= 0) cross the box
// vecMins..vecMaxs (slab test); *pflDist gets the entry t (0 when vecStart is inside).
// A zero vecDir never hits from outside. Name ours.
bool UTIL_RayHitsBox(Vector vecStart, Vector vecDir, Vector vecMins, Vector vecMaxs, float* pflDist);

// at 0xf5800 (util): vecDir mirrored off the plane with normal vecNormal. Name ours.
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal);

// at 0x50cc0 (ggrenade): RC2's one-argument grenade explosion at the trace's end. Name ours.
void RC2_GrenadeExplode(CGrenade* pGrenade, TraceResult* pTrace);

// User messages (player.cpp's LinkUserMessages)
extern int gmsgCLTE; // "CLTE", at 0x18f4b8: RC2's client-side temp entities

// CBasePlayer's byte at orig +0x778: the HEV shield is up (RC2_ShieldFilterDamage and the
// beams read it). player.h is another p8 fork's to edit, so this reads it for now. Ours.
bool RC2_PlayerShieldOn(CBaseEntity* pPlayer);
