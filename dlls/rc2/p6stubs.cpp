/***
*
*	Rocket Crowbar 2: TEMP STUBs for package p6 (see p6stubs.h). None of these is the
*	original's behavior; each belongs to another package and goes at merge.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "p6stubs.h"

// TEMP STUB (p6): real one belongs in util.cpp
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, edict_t* pentSelf, Vector vecSrc, float flRadius,
	float flDamage, float flFalloffExponent, int bitsDamageType, int iLast)
{
}

// TEMP STUB (p6): real one belongs in util.cpp
void UTIL_RC2KillEntityEffects(edict_t* pent)
{
}

// TEMP STUB (p6): real one belongs in util.cpp
int UTIL_RC2PackNormal(Vector vecNormal)
{
	return 0;
}

// TEMP STUB (p6): real one belongs in weapons.cpp
unsigned short g_usRC2ExplodeEvent = 0;

// TEMP STUB (p6): real ones belong in util.cpp
void UTIL_RC2PushHitSet()
{
}

void UTIL_RC2PopHitSet()
{
}

void UTIL_RC2MarkHit(int iEntIndex)
{
}

bool UTIL_RC2WasHit(int iEntIndex)
{
	return false;
}

// TEMP STUB (p6): real one belongs in util.cpp
entvars_t* UTIL_RC2DamageInflictor(const char* szClassname)
{
	return VARS(INDEXENT(0));
}

// TEMP STUB (p6): real one belongs in util.cpp
void UTIL_RC2WaterDischarge(Vector vecSrc, float flDamage, entvars_t* pevInflictor, entvars_t* pevAttacker)
{
}

// TEMP STUB (p6): real one belongs in the run between scripted and shotgun (p7)
float RC2ShieldAbsorb(CBaseEntity* pEnt, float flDamage, float flShieldFraction, Vector vecHitPos, Vector vecDir,
	float* pvecOut1, float* pvecOut2, int bitsDamageType)
{
	return flDamage;
}

// TEMP STUB (p6): real one belongs in UserMessages.cpp (REG_USER_MSG("PullerCtrl", -1))
int gmsgPullerCtrl = 0;

// TEMP STUB (p6): real one belongs in util.cpp (this is the body read at rc2.dll 0xf5900)
void UTIL_RC2ExtendTime(float* pflTime, float flBase, float flDelay)
{
	if (*pflTime < flBase + flDelay)
		*pflTime = flBase + flDelay;
}

// TEMP STUB (p6): real one belongs in weapons.cpp (1 on the server; ItemPostFrame sets it again)
bool g_runfuncs = true;
