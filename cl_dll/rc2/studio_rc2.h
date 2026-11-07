/***
*
*	Rocket Crowbar 2: what the client's studio renderer (StudioModelRenderer.cpp, c4) uses from
*	the rest of client.dll, declared under the names of the real definitions (all ours: client.dll
*	keeps none). The address is the original's.
*
****/

#pragma once

// g_bEntityFrozen (.bss 0x101f24c4, rc2/clte.cpp; c4_needs.h): set while CLTE 0x2f holds the
// entity frozen (FUN_1007bff0 sets it, FUN_1007c1f0 clears it). The renderer doesn't
// interpolate such an entity and draws it with a white shell.
#include "rc2/c4_needs.h"

// Per-entity flag (.bss 0x101edc7c, rc2/clte.cpp): CLTE 0x35 made the entity frictionless; it is
// drawn with the magenta aftex.spr shell (set and cleared by FUN_100522f0, FUN_10052370,
// FUN_10083520, FUN_10083720). 1024 bytes: the next global is at 0x101ee07c.
extern unsigned char g_abRC2AntiFriction[1024];

// The client's frame counter (.bss 0x101f6a08, rc2/clte.cpp), incremented once a frame
// (FUN_1004ff90, HUD_Frame).
extern unsigned int g_iClientFrame;

// Set while the view is another entity's (.bss 0x10227991, view.cpp's V_CalcNormalRefdef,
// FUN_100e9ee0): the local player's own model and the players riding with him are then drawn in
// first person too.
extern bool g_bViewEntityOverride;

// cl_visiblejetpack (hud.h, registered in CHud::Init): draw models/jetpack.mdl on players with it.

// FUN_10059b90 (rc2/clte.cpp): remember where the entity was drawn this frame (with g_iClientFrame).
void CLTE_SetOrigin(int iEnt, float x, float y, float z);

// FUN_100598d0 (rc2/clte.cpp): the player's override, if one is set: for the renderer the
// coloured shell he is drawn with (colour 0-255, thickness). Its flag table (.bss 0x101edc58) is
// only 0x24 bytes, so iPlayer is a player's.
bool CLTE_GetPlayerOverride(int iPlayer, float* vecOut, float* pflValue);

// FUN_1004f4c0 (rc2/attributes.cpp, pm_rc2.h): the player's RC2 ability bits (0 outside 1-32);
// 8 = wears the jetpack.
int PM_GetAbilityFlags(int iEnt);
