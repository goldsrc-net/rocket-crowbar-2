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

// gmsgViewMode: the real one is reconstructed elsewhere now

// TEMP STUB (p2): real one belongs in weapons.cpp (W_Precache, 0xf9360, precaches events/explode.sc)
unsigned short g_usRC2Explode = 0;

// VectorsToAngles: the real one is reconstructed elsewhere now

// UTIL_PackDirection: the real one is reconstructed elsewhere now
// RC2RadiusDamage: util.cpp (p9); its third argument is an edict_t*

class CBasePlayer;

// TEMP STUB (p2): real ones belong in player.cpp (0x9a800, 0x9a820: CBasePlayer +0x648 hold bits)
void RC2PlayerAddHold(CBasePlayer* pPlayer, int iBits)
{
}

void RC2PlayerRemoveHold(CBasePlayer* pPlayer, int iBits)
{
}
