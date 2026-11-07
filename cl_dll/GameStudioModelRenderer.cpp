//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <assert.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "Exports.h"

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

// The renderer object, created on the stack.
// orig client 0x9f250 (its construction), orig client 0x9f270 (atexit of its destruction) and
// orig client 0x9f290 (not in files-client.tsv: Ghidra missed it; the destruction itself):
// compiler-made for this global
CGameStudioModelRenderer g_StudioRenderer;
/*
====================
CGameStudioModelRenderer

====================
*/
CGameStudioModelRenderer::CGameStudioModelRenderer()
{
}

// orig client 0x9f300: CGameStudioModelRenderer's deleting destructor (compiler-made; Ghidra
// names it CStudioModelRenderer's). It runs ~CStudioModelRenderer (orig client 0xc0870)

////////////////////////////////////
// Hooks to class implementation
////////////////////////////////////

/*
====================
R_StudioDrawPlayer

====================
*/
// orig client 0x9f330 (not in files-client.tsv: Ghidra missed it). As SDK 2.3's
int R_StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
	return static_cast<int>(g_StudioRenderer.StudioDrawPlayer(flags, pplayer));
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel(int flags)
{
	return static_cast<int>(g_StudioRenderer.StudioDrawModel(flags));
}

/*
====================
R_StudioInit

====================
*/
// orig client 0x9f380. Not byte-identical to SDK 2.3's (the renderer's address), but it reads the same
void R_StudioInit()
{
	g_StudioRenderer.Init();
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
	{
		STUDIO_INTERFACE_VERSION,
		R_StudioDrawModel,
		R_StudioDrawPlayer,
};

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
int DLLEXPORT HUD_GetStudioModelInterface(int version, struct r_studio_interface_s** ppinterface, struct engine_studio_api_s* pstudio)
{
	//	RecClStudioInterface(version, ppinterface, pstudio);

	if (version != STUDIO_INTERFACE_VERSION)
		return 0;

	// Point the engine to our callbacks
	*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy(&IEngineStudio, pstudio, sizeof(IEngineStudio));

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
