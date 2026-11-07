/***
*
*	Rocket Crowbar 2 (client): the client's side of two of the player movement's game hooks
*	(pm_rc2.h): who rides whose shoulder (the PlrStat message) and an entity's box. client.dll
*	0xaf980-0xafab0, between parsemsg.cpp and pm_debug.cpp; rc2.dll has its own versions in
*	dlls/rc2/playerride.cpp. File name and every name here are ours.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "pm_rc2.h"

// client .bss 0x1020d5b8: each player's ride info as PlrStat last sent it (low 7 bits the
// ridden player, bit 7 the left shoulder), [1] to [32]; [33] is what any other index gets
static unsigned char s_ucRideInfo[34];

unsigned char* PM_GetRideInfo(int iEnt) // orig client 0xaf980
{
	if (iEnt < 1 || iEnt > 32)
		return &s_ucRideInfo[33];
	return &s_ucRideInfo[iEnt];
}

// PlrStat (dlls/player.cpp sends it): byte the player, byte its ride info. Read straight from
// the buffer, as the original does.
int __MsgFunc_PlrStat(const char* pszName, int iSize, void* pbuf) // orig client 0xaf9b0 (not in files-client.tsv: Ghidra missed it)
{
	const unsigned char* pData = (const unsigned char*)pbuf;
	if ((unsigned int)iSize < 2 || !pData)
		return 0;

	const int iPlayer = pData[0];
	if (iPlayer < 1 || iPlayer >= 33)
		return 0;

	s_ucRideInfo[iPlayer] = pData[1];
	return 1;
}

// The client hands out the entity's angles where the server's hook gives its v_angle.
void PM_GetEntityBox(int iEntIndex, float* vecOrigin, float* vecVAngle, float* vecMins, float* vecMaxs) // orig client 0xaf9f0
{
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(iEntIndex);
	if (!pEnt)
		return;

	VectorCopy(pEnt->origin, vecOrigin);
	VectorCopy(pEnt->angles, vecVAngle);
	VectorCopy(pEnt->curstate.mins, vecMins);
	VectorCopy(pEnt->curstate.maxs, vecMaxs);
}

// TEMP STUB (c4-P): the real ones belong in view.cpp (client 0xe9400-0xebb00) and the client's
// prediction code (client 0x59d50), other packages'. Weak, so the real ones replace them.
__attribute__((weak)) int g_iPMSpecView;	  // client .bss 0x10227160
__attribute__((weak)) int g_bPMSetViewAngles; // client .bss 0x102270f0
__attribute__((weak)) int g_bPMSetViewOrigin; // client .bss 0x10227164
__attribute__((weak)) void PM_SetPredictedOrigin(int iEnt, const float* vecOrigin) {}
