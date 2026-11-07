/***
*
*	Rocket Crowbar 2: the race. A race_controller (target = the name its race_path points
*	share, exit_target, track_radius) collects the race_path points in map order; using it
*	turns the player into the mini forklift on the track and sends the client the path
*	(RaceStat); using it with USE_OFF puts the player back at exit_target. pm_shared steers a
*	racing player along the points through the RacePath_* functions. rc2.dll keeps no file
*	names; this file is ours, after the class. Functions 0xadad0-0xae830.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "rc2prefs.h"
#include "rc2/p6stubs.h"

extern int gmsgCLTE;
extern int gmsgRaceStat; // "RaceStat", RC2's own (orig .bss 0x18f4d0)

void UntransformPlayer(CBasePlayer* pPlayer); // name ours: orig 0xe2f90 (TEMP STUB below)

// One point of the track, 16 bytes as in the original.
struct racepoint_t // name ours
{
	Vector vecOrigin;
	float flRadius; // the race_path's radius (its pev->health)
};

class CRaceController : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	int ObjectCaps() override { return 0; } // orig 0xadb40
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT InitPath();

	racepoint_t* m_pPoints;	  // orig +0x70, malloc'd. Not saved; names ours
	int m_iPointCount;		  // orig +0x74
	float m_flTrackRadius;	  // orig +0x78: "track_radius"
};

LINK_ENTITY_TO_CLASS(race_controller, CRaceController); // orig 0xadad0

void CRaceController::Spawn() // orig 0xadb50
{
	Precache();
	SetThink(&CRaceController::InitPath);
	pev->nextthink = gpGlobals->time + 1.5;
	m_pPoints = NULL;
	m_iPointCount = 0;
}

void CRaceController::Precache() // orig 0xadb80
{
	PRECACHE_MODEL("models/miniforklift.mdl");
}

// The points: every entity named pev->target, in the order the engine finds them, as long
// as each one's own target is not our target... see the loop.
void CRaceController::InitPath() // orig 0xadb90
{
	CBaseEntity* pPoint = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
	if (!pPoint)
	{
		UTIL_Remove(this);
		return;
	}

	// count: the first, then each next one found by the previous one's target, until one is
	// named like our target again. VERIFY: the original compares the found point's
	// targetname (always our target, so the loop ends at the second point found); kept literal
	m_iPointCount = 1;
	for (pPoint = UTIL_FindEntityByTargetname(pPoint, STRING(pPoint->pev->target)); pPoint;
		 pPoint = UTIL_FindEntityByTargetname(pPoint, STRING(pPoint->pev->target)))
	{
		if (strcmp(STRING(pPoint->pev->targetname), STRING(pev->target)) == 0)
			break;
		m_iPointCount++;
	}

	if (m_iPointCount < 2)
	{
		UTIL_Remove(this);
		return;
	}

	m_pPoints = (racepoint_t*)malloc(m_iPointCount * sizeof(racepoint_t));

	pPoint = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
	if (!pPoint)
	{
		UTIL_Remove(this);
		return;
	}
	m_pPoints[0].vecOrigin = pPoint->pev->origin;
	m_pPoints[0].flRadius = pPoint->pev->health;

	int i = 1;
	for (pPoint = UTIL_FindEntityByTargetname(pPoint, STRING(pPoint->pev->target)); pPoint;
		 pPoint = UTIL_FindEntityByTargetname(pPoint, STRING(pPoint->pev->target)))
	{
		if (strcmp(STRING(pPoint->pev->targetname), STRING(pev->target)) == 0)
			return;
		m_pPoints[i].vecOrigin = pPoint->pev->origin;
		m_pPoints[i].flRadius = pPoint->pev->health;
		i++;
	}
}

// Start a race (any use but USE_OFF), or leave it (USE_OFF). Only a live player who isn't
// transformed, shrunk, riding or ridden, and has the transform's energy.
void CRaceController::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) // orig 0xadde0
{
	if (!pActivator || !pActivator->IsPlayer())
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;
	if (pPlayer->m_iTransformType != 0 || !pPlayer->IsAlive() || pPlayer->pev->deadflag != DEAD_NO)
		return;
	if (pPlayer->m_iShrunk != 0 || pPlayer->m_bFrozen) // base byte +0x66
		return;
	if (pPlayer->m_flEnergy < RC2Pref(RC2P_Attributes_TransformEnergyCost).f)
		return;
	if (pPlayer->m_bTransforming || pPlayer->m_iPhysKeyFlags2 != 0)
		return;
	if (pPlayer->m_iRidingIndex != 0 || pPlayer->m_iRightRider != 0 || pPlayer->m_iLeftRider != 0)
		return;

	if (useType == USE_OFF)
	{
		// out of the race: to exit_target, or a spawn spot
		if (!FStringNull(pev->netname))
		{
			CBaseEntity* pExit = UTIL_FindEntityByTargetname(NULL, STRING(pev->netname));
			if (pExit)
			{
				UTIL_SetOrigin(pPlayer->pev, pExit->pev->origin);
				pPlayer->pev->v_angle = g_vecZero;
				pPlayer->pev->velocity = g_vecZero;
				pPlayer->pev->angles = pExit->pev->angles;
				pPlayer->pev->punchangle = g_vecZero;
				pPlayer->pev->fixangle = 1;
				UntransformPlayer(pPlayer);
				return;
			}
		}
		g_pGameRules->GetPlayerSpawnSpot(pPlayer);
		UntransformPlayer(pPlayer);
		return;
	}

	pPlayer->m_iRC2State4ac |= 0x10;
	pPlayer->m_fShieldOn = false;

	if (pPlayer->m_bRC2State650)
	{
		pPlayer->m_bRC2State650 = false;
		pPlayer->ClearPhysKeyFlags(1);
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(4);
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	if (pPlayer->m_pActiveItem)
	{
		pPlayer->m_pActiveItem->Holster(); // SDK 2.3: Holster(0)
		pPlayer->pev->weaponmodel = 0;
		pPlayer->pev->viewmodel = 0;
	}

	if (pPlayer->m_pTank != NULL)
	{
		pPlayer->m_pTank->Use(pPlayer, pPlayer, USE_OFF, 0);
		pPlayer->m_pTank = NULL;
	}

	if (pPlayer->FlashlightIsOn())
		pPlayer->FlashlightTurnOff();
	pPlayer->pev->effects &= ~EF_DIMLIGHT;

	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->renderamt = 0;
	pPlayer->pev->rendercolor = g_vecZero;
	pPlayer->pev->skin = 0;

	pPlayer->m_flTransformIdleTime = gpGlobals->time - 120;
	pPlayer->m_iTransformIdleAnim = 0;
	pPlayer->m_flPreTransformHealth = pPlayer->pev->health;
	pPlayer->m_flPreTransformMaxHealth = pPlayer->pev->max_health;
	pPlayer->pev->health = 1;
	pPlayer->pev->max_health = 1;

	pPlayer->m_bTransforming = false;
	pPlayer->m_iTransformType = 6;
	pPlayer->pev->playerclass = 6;
	pPlayer->pev->skin = 0;

	const float flStart = gpGlobals->time + 0.25;
	pPlayer->m_flTransformLastAttack = -1;
	pPlayer->m_flTransformNextAttack4 = flStart;
	pPlayer->m_flTransformNextAttack3 = flStart;
	pPlayer->m_flTransformNextAttack2 = flStart;
	pPlayer->m_flTransformNextAttack1 = flStart;
	pPlayer->m_flTransformVar1 = 0;
	pPlayer->m_flTransformVar2 = pPlayer->m_flTransformVar3 = pPlayer->m_flTransformVar4 = 0;
	pPlayer->m_iTransformState1 = 0;
	pPlayer->m_iTransformState2 = pPlayer->m_iTransformState3 = pPlayer->m_iTransformState4 = 0;
	pPlayer->m_flTransformTimer1 = pPlayer->m_flTransformTimer2 = pPlayer->m_flTransformTimer3 = pPlayer->m_flTransformTimer4 = -1;
	pPlayer->m_vecTransformVec = g_vecZero;
	pPlayer->m_pentRaceController = edict();
	pPlayer->m_flTransformVar1 = m_flTrackRadius;

	UTIL_SetOrigin(pPlayer->pev, m_pPoints[0].vecOrigin); // VERIFY: as the original, no check that InitPath ran
	pPlayer->pev->view_ofs = Vector(0, 0, 0);
	SET_MODEL(pPlayer->edict(), "models/miniforklift.mdl");
	pPlayer->pev->sequence = 0;
	pPlayer->ResetSequenceInfo();
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->fuser2 = 0;
	pPlayer->pev->fuser1 = 0;

	// the track to the racer's client: a header, then every point
	MESSAGE_BEGIN(MSG_ONE, gmsgRaceStat, NULL, pPlayer->edict());
	WRITE_BYTE(1);
	WRITE_SHORT(m_iPointCount);
	WRITE_LONG(*(int*)&m_flTrackRadius); // the float's bits
	MESSAGE_END();

	for (int i = 0; i < m_iPointCount; i++)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgRaceStat, NULL, pPlayer->edict());
		WRITE_BYTE(2);
		WRITE_SHORT(i);
		WRITE_LONG(*(int*)&m_pPoints[i].vecOrigin.x);
		WRITE_LONG(*(int*)&m_pPoints[i].vecOrigin.y);
		WRITE_LONG(*(int*)&m_pPoints[i].vecOrigin.z);
		WRITE_LONG(*(int*)&m_pPoints[i].flRadius);
		MESSAGE_END();
	}
}

bool CRaceController::KeyValue(KeyValueData* pkvd) // orig 0xae370
{
	if (strcmp(pkvd->szKeyName, "exit_target") == 0)
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	if (strcmp(pkvd->szKeyName, "track_radius") == 0)
	{
		m_flTrackRadius = atof(pkvd->szValue);
		return true;
	}
	// the original doesn't fall back to CBaseEntity::KeyValue
	return false;
}

// A point of the race track; its "radius" goes in pev->health. The class name is ours
// (rc2.dll has none for race_path).
class CRacePath : public CPointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	int ObjectCaps() override { return 0; } // orig 0xae4b0
};

LINK_ENTITY_TO_CLASS(race_path, CRacePath); // orig 0xae440

bool CRacePath::KeyValue(KeyValueData* pkvd) // orig 0xae4c0
{
	if (strcmp(pkvd->szKeyName, "radius") == 0)
	{
		pev->health = atof(pkvd->szValue);
		return true;
	}
	return false;
}

// While racing: +use gives up the race at a spawn spot. Name ours; nothing in rc2.dll calls it
// directly (VERIFY: the caller, likely the transforms' per-frame table in another package).
void RaceUseFrame(CBasePlayer* pPlayer) // orig 0xae540
{
	if ((pPlayer->m_afButtonPressed & IN_USE) != 0)
	{
		g_pGameRules->GetPlayerSpawnSpot(pPlayer);
		UntransformPlayer(pPlayer);
	}
}

// The racer's controller and its points, or NULL; the checks each RacePath_* function makes.
static CRaceController* RacerController(int iPlayer, CBasePlayer** ppPlayer) // name ours, inlined in the original
{
	if (iPlayer <= 0 || iPlayer > gpGlobals->maxClients)
		return NULL;
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return NULL;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer || pPlayer->m_iTransformType != 6 || !UTIL_IsValidEntity(pPlayer->m_pentRaceController))
		return NULL;
	*ppPlayer = pPlayer;
	return (CRaceController*)CBaseEntity::Instance(pPlayer->m_pentRaceController);
}

void RacePath_GetCurrent(int iPlayer, Vector* pvecPoint, float* pflRadius) // orig 0xae570
{
	CBasePlayer* pPlayer;
	CRaceController* pController = RacerController(iPlayer, &pPlayer);
	if (!pController)
		return;
	const int i = pPlayer->m_iTransformState1;
	if (i < 0 || i >= pController->m_iPointCount)
		return;
	if (pvecPoint)
		*pvecPoint = pController->m_pPoints[i].vecOrigin;
	if (pflRadius)
		*pflRadius = pController->m_pPoints[i].flRadius;
}

// The point iOffset from the current one, wrapping round the track.
void RacePath_GetRelative(int iPlayer, int iOffset, Vector* pvecPoint, float* pflRadius) // orig 0xae660
{
	CBasePlayer* pPlayer;
	CRaceController* pController = RacerController(iPlayer, &pPlayer);
	if (!pController)
		return;
	int i = iOffset + pPlayer->m_iTransformState1;
	const int iCount = pController->m_iPointCount;
	while (true)
	{
		while (i < 0)
			i += iCount;
		if (i < iCount)
			break;
		i -= iCount;
	}
	if (pvecPoint)
		*pvecPoint = pController->m_pPoints[i].vecOrigin;
	if (pflRadius)
		*pflRadius = pController->m_pPoints[i].flRadius;
}

void RacePath_Advance(int iPlayer) // orig 0xae760
{
	CBasePlayer* pPlayer;
	CRaceController* pController = RacerController(iPlayer, &pPlayer);
	if (!pController)
		return;
	pPlayer->m_iTransformState1++;
	if (pPlayer->m_iTransformState1 < 0)
		pPlayer->m_iTransformState1 = 0;
	if (pPlayer->m_iTransformState1 >= pController->m_iPointCount)
		pPlayer->m_iTransformState1 = 0;
}

// The track radius the race began with.
void RacePath_GetHeight(int iPlayer, float* pflHeight) // orig 0xae830
{
	CBasePlayer* pPlayer;
	CRaceController* pController = RacerController(iPlayer, &pPlayer);
	if (!pController || !pflHeight)
		return;
	*pflHeight = pPlayer->m_flTransformVar1;
}


// TEMP STUB (p6): real one belongs in UserMessages.cpp (LinkUserMessages registers "RaceStat")
int gmsgRaceStat = 0;

// TEMP STUB (p6): real one belongs in the package with 0xe2f90 (p8): back to the player
// model, m_iTransformType and m_bTransforming 0, health scaled back to the saved max health
void UntransformPlayer(CBasePlayer* pPlayer)
{
}
