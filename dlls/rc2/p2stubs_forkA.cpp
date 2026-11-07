/***
*
*	More of work package p2's temporary stubs (the forklift, explode.cpp and the freeze timer):
*	just enough to link while the files these belong to are reconstructed by other packages.
*	None carries an orig address; each says where the real one belongs. Delete each at merge.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"

// TEMP STUB (p2): real one belongs in player.cpp (LinkUserMessages registers "ViewMode")
int gmsgViewMode = 0;

// TEMP STUB (p2): real one belongs in weapons.cpp (W_Precache, 0xf9360, precaches events/explode.sc)
unsigned short g_usRC2Explode = 0;

// TEMP STUB (p2): real one belongs in pm_shared's pm_math (0xa0260)
void VectorsToAngles(const Vector& vecForward, const Vector& vecRight, const Vector& vecUp, Vector& vecAngles)
{
	vecAngles = UTIL_VecToAngles(vecForward);
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf5920)
int UTIL_PackDirection(Vector vecDir)
{
	return 0;
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf3900)
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, entvars_t* pevIgnore, Vector vecSrc, float flRadius, float flDamage, float flScale, int bitsDamageType, int iFlag)
{
}

class CBasePlayer;

// TEMP STUB (p2): real ones belong in player.cpp (0x9a800, 0x9a820: CBasePlayer +0x648 hold bits)
void RC2PlayerAddHold(CBasePlayer* pPlayer, int iBits)
{
}

void RC2PlayerRemoveHold(CBasePlayer* pPlayer, int iBits)
{
}
