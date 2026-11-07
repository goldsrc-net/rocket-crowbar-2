// TEMP STUB (c4 fork V): what in_camera, input, inputw32, tri and transform_hud.cpp use from
// files other packages reconstruct. Each answers as "nothing there". Remove each one when its real
// definition lands; the file goes when the last one does.

#include "hud.h"
#include "cl_util.h"
#include "pm_defs.h"

// TEMP STUB (c4): real ones belong in the client tent code (client.dll 0x7bff0, 0x8ebf0, package c2)
int g_bViewLockTent = 0;
int g_bViewLockEffect = 0;

// TEMP STUB (c4): real one belongs in view.cpp (client.dll .data 0x101768b8, package c5)
bool g_bDrawClientTents = true;

// TEMP STUB (c4): real ones belong in the client tent code (client.dll 0x52280-0x52650, package c2)
void ClientTents_DrawNormal() {}
void ClientTents_DrawTransparent() {}
void ClientTents_Clear() {}
void ClientTents_Remove(int iEnt, int iType) {}
void GetEntityRenderOrigin(float* vecOut, int iEnt)
{
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(iEnt);
	if (pEnt)
	{
		VectorCopy(pEnt->origin, vecOut);
	}
	else
	{
		vecOut[0] = vecOut[1] = vecOut[2] = 0;
	}
}

// TEMP STUB (c4): real one belongs in client.dll 0xd6b00 (package c5)
void RC2_DrawTransparentExtra() {}

// TEMP STUB (c4): real one belongs in vgui_int.cpp area (client.dll 0xd9180, package c5)
void VGui_ShutdownState() {}

// TEMP STUB (c4): real one belongs in ammo.cpp (client.dll 0x4a900, package c1)
void Ammo_RestoreCrosshair() {}

// TEMP STUB (c4): real one belongs in ev_hldm.cpp (client.dll 0xee70, package c1)
float EV_TraceVisible(Vector vecStart, Vector vecEnd, Vector vecDir, float flRange, cl_entity_t* pTarget, pmtrace_t* ptr, int iDepth)
{
	return 0;
}
