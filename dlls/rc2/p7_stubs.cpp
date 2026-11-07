/***
*
*	Rocket Crowbar 2, work package p7: temporary definitions of what p7's files use from other
*	packages, so the library links. None of these is a reconstruction (none carries a provenance
*	comment); each goes when its real one arrives at the merge. Declarations: p7_extern.h.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "p7_extern.h"
#include <vector>

// gmsgCLTE: defined in dlls/rc2/playerride.cpp (p6)
// TEMP STUB (p7): real one belongs in player.cpp (p5), registered as "WeaponSync"
int gmsgWeaponSync = 0;

// TEMP STUB (p7): real ones belong in util.cpp (p9). Simplified: one set, not a stack.
static std::vector<unsigned char> s_entityMarks; // name ours
// UTIL_IsEntityMarked: the real one is reconstructed elsewhere now
// UTIL_MarkEntity: the real one is reconstructed elsewhere now

// UTIL_FindEntityNearestInSphere: the real one is reconstructed elsewhere now

// UTIL_BlastReaches: the real one is reconstructed elsewhere now

// TEMP STUB (p7): real one belongs in util.cpp (p9). Approximation of the packing read at 0xf5920.
int UTIL_PackDirection(Vector vecDir)
{
	vecDir = vecDir.Normalize();
	auto pack = [](float f)
	{ int i = (int)(fabs(f) * 511); return f < 0 ? (i | 0x200) : i; };
	return pack(vecDir.z) << 20 | pack(vecDir.x) | pack(vecDir.y) << 10;
}

// TEMP STUB (p7): real one belongs in util.cpp (p9); the body read at 0xf5800
Vector UTIL_Reflect(Vector vecDir, Vector vecNormal)
{
	const float flDot = -DotProduct(vecDir, vecNormal);
	return vecNormal * 2 * flDot + vecDir;
}

#include "player.h"
// CBasePlayer::SetPhysFlag10For: the real one is reconstructed elsewhere now

// TEMP STUB (p7): real one belongs in player.cpp (p5), registered as "ViewMode"
int gmsgViewMode = 0;

// RC2RadiusDamage: the real one is reconstructed elsewhere now

// TEMP STUB (p7): real one belongs in util.cpp (p9). Returns the world's pev, as the original does when unset.
entvars_t* UTIL_NamedInflictor(const char* pszName) { return VARS(INDEXENT(0)); }

// TEMP STUB (p7): real one belongs in util.cpp (p9); needs the player's array at +0x7a8 (p5).
bool UTIL_PlayerPartIsGround(CBaseEntity* pPlayer, edict_t* pent) { return false; }

// TEMP STUB (p7): real one belongs in player.cpp (p5), the method at 0xb1f50 ("no longer riding")
// CBasePlayer::StopRiding is p6's (dlls/rc2/playerride.cpp)

// TEMP STUB (p7): real one belongs in weapons.cpp (p3); ItemPostFrame sets it to 1 there
int g_iWeaponRunFuncs = 1;
