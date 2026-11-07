// TEMP STUB (c4 fork V): what transform_hud.cpp uses from package c1's range (client.dll below
// 0x4ec50, still being reconstructed). Each answers as "nothing there". Remove each one when its
// real definition lands; the file goes when the last one does.

#include "hud.h"
#include "cl_util.h"
#include "pm_defs.h"

// TEMP STUB (c4): real one belongs in ammo.cpp (client.dll 0x4a900, package c1)
void Ammo_RestoreCrosshair() {}

// TEMP STUB (c4): real one belongs in ev_hldm.cpp (client.dll 0xee70, package c1)
float EV_TraceVisible(Vector vecStart, Vector vecEnd, Vector vecDir, float flRange, cl_entity_t* pTarget, pmtrace_t* ptr, int iDepth)
{
	return 0;
}
