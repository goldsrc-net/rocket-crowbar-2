/***
*
*	Rocket Crowbar 2, work package p4: temporary definitions of what p4's files use from
*	other packages (declared in p4_needs.h), so p4 links on its own. Each block goes when the
*	real one lands; none of them carries the original's address.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "p4_needs.h"

// TEMP STUB (p4): real one belongs in the p7 file holding 0xc45b0 ("new: scripted .. shotgun")
float ShieldAbsorbDamage(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecSrc, Vector vecDir, Vector* pvecHit, Vector* pvecNormal, int iSize)
{
	if (pvecHit)
		*pvecHit = vecSrc;
	return flDamage;
}

// TEMP STUB (p4): real one belongs in util.cpp (p9)
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal)
{
	return vecDir - vecNormal * (2 * DotProduct(vecDir, vecNormal));
}

// TEMP STUB (p4): real one belongs in UserMessages.h/.cpp (registered as "CLTE")
int gmsgCLTE = 0;

// TEMP STUB (p4): real one belongs in util.cpp (p9); the client-mask check is left out
int UTIL_IsValidEdict(edict_t* pent)
{
	if (!pent || pent->free || (pent->v.flags & FL_KILLME) != 0 || !pent->pvPrivateData)
		return 0;
	return ENTINDEX(pent) != 0 ? 1 : 0;
}

// TEMP STUB (p4): real one belongs in util.cpp (p9)
int UTIL_PackDirection(Vector vecDir)
{
	vecDir = vecDir.Normalize();
	auto pack = [](float f) { int i = (int)(fabs(f) * 511); return f < 0 ? (i | 0x200) : i; };
	return pack(vecDir.x) | (pack(vecDir.y) << 10) | (pack(vecDir.z) << 20);
}

// TEMP STUB (p4): real ones belong in util.cpp (p9)
static unsigned char* s_pEntityMasks[128];
static int s_iEntityMask;
void UTIL_PushEntityMask()
{
	if (s_pEntityMasks[s_iEntityMask] && ++s_iEntityMask > 127)
		s_iEntityMask = 127;
	const int iBytes = (int)ceil(gpGlobals->maxEntities * 0.125);
	s_pEntityMasks[s_iEntityMask] = (unsigned char*)calloc(iBytes > 0 ? iBytes : 1, 1);
}
void UTIL_PopEntityMask()
{
	free(s_pEntityMasks[s_iEntityMask]);
	s_pEntityMasks[s_iEntityMask] = NULL;
	if (s_iEntityMask > 0)
		s_iEntityMask--;
}
void UTIL_MarkEntity(int iEntity)
{
	if (s_pEntityMasks[s_iEntityMask])
		s_pEntityMasks[s_iEntityMask][iEntity / 8] |= 1 << (iEntity % 8);
}
bool UTIL_IsEntityMarked(int iEntity)
{
	return s_pEntityMasks[s_iEntityMask] && (s_pEntityMasks[s_iEntityMask][iEntity / 8] & (1 << (iEntity % 8))) != 0;
}

// TEMP STUB (p4): real one belongs in util.cpp (p9)
void UTIL_ExtendTime(float* pflTime, float flBase, float flDelay)
{
	if (*pflTime < flBase + flDelay)
		*pflTime = flBase + flDelay;
}

// TEMP STUB (p4): real one belongs in util.cpp (p9); this one always answers the world
entvars_t* UTIL_NamedInflictor(const char* pszName)
{
	return VARS(INDEXENT(0));
}

// TEMP STUB (p4): real one belongs in weapons.cpp (p3), which sets it in ItemPostFrame
int g_iRC2WeaponFrame = 0;

// TEMP STUB (p4): real one belongs in the p5 player file holding 0x95890
float RC2_PlayerAttribute(int iAttr, int iPlayer)
{
	return 1.0;
}

// TEMP STUB (p4): real one belongs in the p9 file holding 0xfa100; this one keeps the item
void ReplaceWithRandomItem(CBaseEntity* pItem)
{
}

// TEMP STUB (p4): real ones belong in util.cpp (p9)
void AddToPrecachedList(const char* pszClassname)
{
}
const char* UTIL_PersistentClassname(const char* pszClassname)
{
	return STRING(ALLOC_STRING(pszClassname));
}

// TEMP STUB (p4): real ones belong in the p1 file holding 0x2660 and 0x4eb0-0x5040
CBaseEntity* GetAimedEntity(entvars_t* pevPlayer)
{
	return NULL;
}
void AdvCreateEntity(const char* pszClassname, const char* pszArgs, int iFlag)
{
}
void EditEntity(edict_t* pent, const char* pszArgs, int iFlag)
{
}
void MaintainEdit(edict_t* pent, const char* pszArgs, int iFlag)
{
}
char g_szRC2SpawnCommand[512];
int g_iRC2SpawnCommandRun;

// TEMP STUB (p4): real one belongs in the p1 file holding 0x9380 (animating); this one
// doesn't hold the frame
float CBaseAnimating::RC2FrameAdvance(float flInterval)
{
	return StudioFrameAdvance(flInterval);
}

// TEMP STUB (p4): real one belongs in util.cpp (p9); this one measures to the center
CBaseEntity* UTIL_FindEntityInSphereDist(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecSpot)
{
	CBaseEntity* pEntity = UTIL_FindEntityInSphere(pStart, vecCenter, flRadius);
	if (pEntity)
	{
		*pvecSpot = pEntity->Center();
		*pflDist = (*pvecSpot - vecCenter).Length();
	}
	return pEntity;
}

// TEMP STUB (p4): real one belongs in UserMessages.h/.cpp (registered as "WeaponSync")
int gmsgWeaponSync = 0;

// TEMP STUB (p4): real one belongs in the p5 player file holding 0x95860
bool RC2_PlayerFiresUnderwater(CBasePlayer* pPlayer)
{
	return false;
}

// TEMP STUB (p4): real one belongs in weapons.cpp (p3); the server's returns 0 too
short ReadSyncShort(void** ppData, int* piSize)
{
	return 0;
}

// TEMP STUB (p4): real ones belong in the p1 file holding 0x4eb0-0x5040
int g_iRC2AlterPlayer;
int g_iRC2AlterEntity;
