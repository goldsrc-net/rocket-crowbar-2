/***
*
*	Rocket Crowbar 2: what the client's studio renderer (StudioModelRenderer.cpp, c4) uses from
*	the rest of client.dll. Every name here is ours (client.dll keeps none); the address is the
*	original's. The real definitions belong to other packages; until they land,
*	studio_rc2_stubs.cpp has weak temporary ones.
*
****/

#pragma once

// g_bEntityHidden (c4_needs.h, .data 0x101f24c4): set while a client-side temporary entity
// holds the entity (FUN_1007bff0 sets it, FUN_1007c1f0 clears it). The renderer doesn't
// interpolate such an entity and draws it with a white shell.
#include "rc2/c4_needs.h"

// Per-entity flag (.bss 0x101edc7c): draw the entity with the magenta aftex.spr shell (set
// and cleared by FUN_100522f0, FUN_10052370, FUN_10083520, FUN_10083720). 1024 bytes: the
// next global is at 0x101ee07c.
extern unsigned char g_rgbEntAftexShell[1024];

// The client's frame counter (.bss 0x101f6a08), incremented once a frame (FUN_1004ff90).
extern unsigned int g_iRC2FrameCount;

// Set while the view chases an entity (.bss 0x10227991, FUN_100e9ee0, the view code): the
// local player's own model and the players riding with him are then drawn in first person too.
extern bool g_bViewChasing;

// cl_visiblejetpack (hud.h, registered in CHud::Init): draw models/jetpack.mdl on players with it.

// FUN_10059b90: remember where the entity was drawn this frame (with g_iRC2FrameCount).
void RC2_SetEntityDrawOrigin(int iEntIndex, Vector vecOrigin);

// FUN_100598d0: the coloured shell the player is drawn with, if any (colour 0-255, thickness).
// Its flag table (.bss 0x101edc58) is only 0x24 bytes, so iEntIndex is a player's.
bool RC2_GetEntityShellColor(int iEntIndex, float* vecColor, float* pflAmount);

// FUN_1004f4c0: the player's RC2 state bits (0 outside 1-32); value 8 = wears the jetpack.
int RC2_GetPlayerStateBits(int iPlayer);
