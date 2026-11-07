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
extern bool g_bMainView;			// view.cpp .data 0x101768b8: false while V_CalcRefdef draws its second view
extern void CLTE_DrawNormal();		// client.dll 0x52580 (rc2/clte.cpp)
extern void CLTE_DrawTransparent(); // client.dll 0x52650 (rc2/clte.cpp)
extern void DrawDebugEntityBox();	// client.dll 0xd6b00 (rc2/vgui_DebugPanel.cpp): the debug panel's entity box

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

	if (g_bMainView)
		CLTE_DrawNormal();
}


/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
// orig client 0xcba70. RC2: its client tents and the debug panel's entity box. halflife-updated's particle
// manager kept.
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	//	RecClDrawTransparentTriangles();

	if (g_bMainView)
	{
		CLTE_DrawTransparent();
		DrawDebugEntityBox();
	}

	if (g_pParticleMan)
		g_pParticleMan->Update();
}
