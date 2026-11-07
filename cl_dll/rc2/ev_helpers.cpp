//========= Rocket Crowbar 2 (c1): client event helpers =========
//
// Three small client functions the event handlers (ev_hldm.cpp, cl_dll/rc2/ev_rc2.cpp) call. In
// client.dll they sit at the end of egon.cpp's object, before ev_hldm (reference/dossiers/c1.md).
// Names ours, as the events reconstruction declares them.

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "pm_defs.h"

unsigned char* PM_GetRideInfo(int iEnt); // rc2/pm_client.cpp, orig client 0xaf980: player iEnt's riding byte

// orig client 0xdd40: the entity a player's events come from: the one it rides, if any
int EV_PlayerForEntity(int idx)
{
	unsigned char* pRiding = PM_GetRideInfo(idx);
	if (*pRiding != 0)
		return *pRiding & 0x7f;
	return idx;
}

// orig client 0xdd80: the height of the water surface between minz and maxz above position's x and
// y (the server's UTIL_WaterLevel, on the client's point contents)
float EV_WaterLevel(float* position, float minz, float maxz)
{
	Vector midUp(position[0], position[1], minz);

	if (gEngfuncs.PM_PointContents(midUp, NULL) != CONTENTS_WATER)
		return minz;

	midUp.z = maxz;
	if (gEngfuncs.PM_PointContents(midUp, NULL) == CONTENTS_WATER)
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff / 2.0;
		if (gEngfuncs.PM_PointContents(midUp, NULL) == CONTENTS_WATER)
			minz = midUp.z;
		else
			maxz = midUp.z;
		diff = maxz - minz;
	}

	return midUp.z;
}

// orig client 0xde90: the server's UTIL_PackNormal undone: 9 bits of size and a sign bit per
// component (x bits 0-9, y 10-19, z 20-29), normalized
Vector UTIL_UnpackNormal(int iPacked)
{
	Vector v;
	v.x = (iPacked & 0x1ff) * (1.0 / 511.0) * ((iPacked & 0x200) != 0 ? -1.0 : 1.0);
	v.y = ((iPacked >> 10) & 0x1ff) * (1.0 / 511.0) * (((iPacked >> 10) & 0x200) != 0 ? -1.0 : 1.0);
	v.z = ((iPacked >> 20) & 0x1ff) * (1.0 / 511.0) * (((iPacked >> 20) & 0x200) != 0 ? -1.0 : 1.0);
	VectorNormalize(v);
	return v;
}
