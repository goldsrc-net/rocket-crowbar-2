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

// TEMP STUB (p7): real one belongs in player.cpp (p5), registered as "CLTE"
int gmsgCLTE = 0;
// TEMP STUB (p7): real one belongs in player.cpp (p5), registered as "WeaponSync"
int gmsgWeaponSync = 0;

// TEMP STUB (p7): real ones belong in util.cpp (p9). Simplified: one set, not a stack.
static std::vector<unsigned char> s_entityMarks; // name ours
void UTIL_PushEntityMarks() { s_entityMarks.assign((gpGlobals->maxEntities + 7) / 8, 0); }
void UTIL_PopEntityMarks() { s_entityMarks.clear(); }
bool UTIL_IsEntityMarked(int iEntIndex) { return iEntIndex >= 0 && (size_t)(iEntIndex / 8) < s_entityMarks.size() && (s_entityMarks[iEntIndex / 8] & (1 << (iEntIndex % 8))) != 0; }
void UTIL_MarkEntity(int iEntIndex)
{
	if (iEntIndex >= 0 && (size_t)(iEntIndex / 8) < s_entityMarks.size())
		s_entityMarks[iEntIndex / 8] |= 1 << (iEntIndex % 8);
}

// TEMP STUB (p7): real one belongs in util.cpp (p9). Approximation: SDK sphere search, origin distance.
CBaseEntity* UTIL_FindEntityNearestInSphere(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecNearest)
{
	CBaseEntity* pEntity = UTIL_FindEntityInSphere(pStart, vecCenter, flRadius);
	if (pEntity != NULL)
	{
		if (pflDist != NULL)
			*pflDist = (pEntity->pev->origin - vecCenter).Length();
		if (pvecNearest != NULL)
			*pvecNearest = pEntity->pev->origin;
	}
	return pEntity;
}

// TEMP STUB (p7): real one belongs in util.cpp (p9). Approximation: one trace, no punching.
bool UTIL_BlastReaches(Vector vecSrc, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iMaxSteps)
{
	UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, pentIgnore, ptr);
	return ptr->flFraction >= 1.0f || (pentTarget != NULL && ptr->pHit == pentTarget);
}

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
// TEMP STUB (p7): real one belongs in player.cpp (p5), the method at 0x9a860
void CBasePlayer::SetPhysicsFlag10(float flDuration) {}

// TEMP STUB (p7): real one belongs in player.cpp (p5), registered as "ViewMode"
int gmsgViewMode = 0;

// TEMP STUB (p7): real one belongs in util.cpp (p9). Approximation: the SDK's RadiusDamage, no falloff exponent.
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, edict_t* pentIgnore, Vector vecSrc, float flRadius, float flDamage, float flFalloffExp, int bitsDamageType, int iFlags)
{
	::RadiusDamage(vecSrc, pevInflictor, pevAttacker, flDamage, flRadius, CLASS_NONE, bitsDamageType);
}

// TEMP STUB (p7): real one belongs in util.cpp (p9). Returns the world's pev, as the original does when unset.
entvars_t* UTIL_NamedInflictor(const char* pszName) { return VARS(INDEXENT(0)); }

// TEMP STUB (p7): real one belongs in util.cpp (p9); needs the player's array at +0x7a8 (p5).
bool UTIL_PlayerPartIsGround(CBaseEntity* pPlayer, edict_t* pent) { return false; }

// TEMP STUB (p7): real one belongs in player.cpp (p5), the method at 0xb1f50 ("no longer riding")
void CBasePlayer::StopRiding(int iArg) {}

// TEMP STUB (p7): real one belongs in weapons.cpp (p3); ItemPostFrame sets it to 1 there
int g_iWeaponRunFuncs = 1;
