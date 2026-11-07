/***
*
*	Rocket Crowbar 2 client, work package c3: temporary definitions of what c3's files use
*	from package c1's range (client.dll below 0x4ec50, still being reconstructed), so the
*	client links. Each block goes when the real one lands; none of them carries the
*	original's address tag.
*
****/

#include "c3_needs.h"

// TEMP STUB (c3): real one belongs in "new: hl_hl_weapons .. common_interface" (orig 0x3b630, c1); no spread
Vector RC2_SpreadVector(Vector vecDir, Vector vecRight, Vector vecUp, float flSpread)
{
	return vecDir.Normalize();
}

// TEMP STUB (c3): real one belongs in "new: start .. ev_hldm" (orig 0xdd80, c1); no water
float RC2_WaterLevel(const float* vecPos, float flMinZ, float flMaxZ)
{
	return flMinZ;
}
