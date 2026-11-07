//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "tri.h"
extern IParticleMan* g_pParticleMan;

// Rocket Crowbar 2: its client tents draw here, on the frames the view lets them
extern bool g_bDrawClientTents;		  // view.cpp .data 0x101768b8 (V_CalcRefdef, package c5), name ours
extern void ClientTents_DrawNormal();	  // client.dll 0x52580 (package c2), name ours
extern void ClientTents_DrawTransparent(); // client.dll 0x52650 (package c2), name ours
extern void RC2_DrawTransparentExtra();	  // client.dll 0xd6b00 (package c5), name ours. VERIFY: what it draws

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
// orig client 0xcba40. RC2: then its client tents
void DLLEXPORT HUD_DrawNormalTriangles()
{
	//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();

	if (g_bDrawClientTents)
		ClientTents_DrawNormal();
}


/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
// orig client 0xcba70. RC2: its client tents and 0xd6b00's drawing. halflife-updated's particle
// manager kept.
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	//	RecClDrawTransparentTriangles();

	if (g_bDrawClientTents)
	{
		ClientTents_DrawTransparent();
		RC2_DrawTransparentExtra();
	}

	if (g_pParticleMan)
		g_pParticleMan->Update();
}
