/***
*
*	Rocket Crowbar 2: one player riding on another's shoulder. The "ride" command climbs onto
*	the player in front of you within 64 units (or gets you off), and pm_shared keeps the
*	rider on the shoulder (PM_GetRideInfo, PM_GetEntityBox). rc2.dll keeps no file names; this
*	file is ours, after the feature. The functions are the original's 0x9f320 and
*	0xb16b0-0xb28b0.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "game.h"
#include "UserMessages.h"
#include "rc2/playerride.h"

extern edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer);


// orig .bss 0x18f4e8: what PM_GetRideInfo hands out for an index that isn't a player
static unsigned char s_ucNoRideInfo; // name ours

unsigned char* PM_GetRideInfo(int iEntIndex) // orig 0x9f320
{
	CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(iEntIndex);
	if (!pPlayer)
		return &s_ucNoRideInfo;
	return &pPlayer->m_ucRideInfo;
}

// The "ride" command: get off when riding, otherwise climb on the player most nearly in
// front within 64 units. Costs 40 energy (20 to get off a player).
void CBasePlayer::StartRiding() // orig 0xb16b0
{
	if (m_iTransformType != 0 || m_bTransforming)
	{
		CLIENT_PRINTF(edict(), print_console, "Can't ride players while transformed\n");
		return;
	}

	if (m_iRidingIndex != 0)
	{
		CBasePlayer* pRidden = (CBasePlayer*)UTIL_PlayerByIndex(m_iRidingIndex);
		if (pRidden)
		{
			// the original prints this and gets off anyway
			if (RC2_BASE_BYTE(pRidden, 0x66) != 0 || pRidden->m_iShrunk != 0)
				CLIENT_PRINTF(edict(), print_console, "Cannot get off right now\n");

			if (m_flEnergy < 20.0f)
			{
				CLIENT_PRINTF(edict(), print_console, "Not enough energy to get off\n");
				return;
			}
			m_flEnergy = m_flEnergy - 20.0f;
		}
		StopRiding(false);
		return;
	}

	if (m_iShoulderRider[0] != 0 || m_iShoulderRider[1] != 0)
	{
		CLIENT_PRINTF(edict(), print_console, "Cannot ride while being ridden\n");
		return;
	}

	if (m_iShrunk != 0 || RC2_BASE_BYTE(this, 0x66) != 0)
	{
		CLIENT_PRINTF(edict(), print_console, "Cannot ride right now\n");
		return;
	}

	if (m_flEnergy < 40.0f)
	{
		CLIENT_PRINTF(edict(), print_console, "Not enough energy to ride a player\n");
		return;
	}

	// pick the player most nearly in front, flat, within 64 units of our center
	float flBestDot = 0;
	bool bHostile = false;
	UTIL_MakeVectors(pev->v_angle);
	Vector2D vecForward = gpGlobals->v_forward.Make2D().Normalize();

	CBasePlayer* pBest = NULL;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pOther = (CBasePlayer*)UTIL_PlayerByIndex(i);
		if (!pOther || pOther->edict() == edict() || !pOther->IsActivePlayer())
			continue;
		if (pOther->m_iTransformType != 0 || pOther->m_bTransforming)
			continue;
		if (pOther->m_iShrunk != 0 || RC2_BASE_BYTE(pOther, 0x66) != 0)
			continue;
		if ((pOther->m_iShoulderRider[0] != 0 && pOther->m_iShoulderRider[1] != 0) || pOther->m_iRidingIndex != 0)
			continue;

		Vector vecDelta = pOther->Center() - Center();
		if (vecDelta.Length() > 64)
			continue;

		Vector2D vecDir = vecDelta.Make2D().Normalize();
		float flDot = DotProduct(vecForward, vecDir);
		if (flDot < flBestDot)
			continue;

		// the original traces between the two and never looks at the result
		TraceResult tr;
		UTIL_TraceLine(Center(), pOther->Center(), ignore_monsters, edict(), &tr); // VERIFY: unused trace kept

		if (!g_pGameRules->PlayerAllowsRiding(pOther, this))
		{
			bHostile = true;
			continue;
		}

		pBest = pOther;
		flBestDot = flDot;
	}

	if (!pBest)
	{
		if (bHostile)
			CLIENT_PRINTF(edict(), print_console, "Cannot ride hostile players\n");
		else
			CLIENT_PRINTF(edict(), print_console, "Nothing to ride\n");
		return;
	}

	// any argument but "right" asks for the left shoulder; "right" (or none) picks at random
	bool bLeft;
	if (CMD_ARGC() > 1 && strcmp(CMD_ARGV(1), "right") != 0)
		bLeft = true;
	else
		bLeft = RANDOM_LONG(0, 1) != 0;

	if (pBest->m_iShoulderRider[1] != 0 && bLeft)
	{
		bLeft = false;
	}
	else if (pBest->m_iShoulderRider[0] != 0)
	{
		if (!bLeft)
		{
			bLeft = true;
		}
		else if (pBest->m_iShoulderRider[0] != 0 && pBest->m_iShoulderRider[1] != 0)
		{
			ALERT(at_console, "Sanity check failed, both shoulders occupied (%d,%d), bailing\n", pBest->m_iShoulderRider[0], pBest->m_iShoulderRider[1]);
			return;
		}
	}

	m_flEnergy = m_flEnergy - 40.0;

	unsigned char* pRideInfo = PM_GetRideInfo(ENTINDEX(edict()));
	m_iRidingIndex = ENTINDEX(pBest->edict());
	pev->owner = pBest->edict();

	if (bLeft)
	{
		pBest->m_iShoulderRider[1] = ENTINDEX(edict());
		m_iRideShoulder = 1;
		*pRideInfo = (unsigned char)m_iRidingIndex | 0x80;
	}
	else
	{
		pBest->m_iShoulderRider[0] = ENTINDEX(edict());
		m_iRideShoulder = 0;
		*pRideInfo = (unsigned char)m_iRidingIndex;
	}

	pev->view_ofs = Vector(0, 0, 11);
	pev->solid = SOLID_NOT;
	m_fShieldOn = false;

	if (m_fJetpackOn)
	{
		m_fJetpackOn = false;
		ClearPhysFlags(1);
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(4);
		WRITE_BYTE(ENTINDEX(edict()));
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}

	char szName[128];
	char szMsg[256];

	strncpy(szName, STRING(pBest->pev->netname), 127);
	szName[127] = 0;
	const char* pszShoulder = m_iRideShoulder ? "left" : "right";
	// "Bob'" or "Ann's"
	const char* pszPossessive = (tolower(szName[strlen(szName) - 1]) == 's') ? "'" : "'s";
	sprintf(szMsg, "* You are now riding on %s%s %s shoulder\n", szName, pszPossessive, pszShoulder);
	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, edict());
	WRITE_BYTE(ENTINDEX(edict()));
	WRITE_STRING(szMsg);
	MESSAGE_END();

	strncpy(szName, STRING(pev->netname), 127);
	szName[127] = 0;
	pszShoulder = m_iRideShoulder ? "left" : "right";
	sprintf(szMsg, "* %s is now riding on your %s shoulder\n", szName, pszShoulder);
	CLIENT_PRINTF(pBest->edict(), print_chat, szMsg);
	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pBest->edict());
	WRITE_BYTE(ENTINDEX(pBest->edict()));
	WRITE_STRING(szMsg);
	MESSAGE_END();
}

// Off the shoulder. Unless bQuiet: tell both players, end the base byte +0x67/+0x66 states and
// shrinking, and put the rider down next to the ridden player (or wherever the last try was).
void CBasePlayer::StopRiding(bool bQuiet) // orig 0xb1f50
{
	char szRiddenName[128];
	char szMsg[256];
	strcpy(szRiddenName, "somebody");

	Vector vecOrigin = pev->origin;

	if (m_iRidingIndex != 0)
	{
		CBasePlayer* pRidden = (CBasePlayer*)UTIL_PlayerByIndex(m_iRidingIndex);
		if (pRidden)
		{
			if (m_iRideShoulder != 0)
				pRidden->m_iShoulderRider[1] = 0;
			else
				pRidden->m_iShoulderRider[0] = 0;
			vecOrigin = pRidden->Center();

			char szMyName[128];
			strncpy(szMyName, STRING(pev->netname), 127);
			szMyName[127] = 0;
			sprintf(szMsg, "* %s is no longer riding you\n", szMyName);
			MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pRidden->edict());
			WRITE_BYTE(ENTINDEX(pRidden->edict()));
			WRITE_STRING(szMsg);
			MESSAGE_END();

			strncpy(szRiddenName, STRING(pRidden->pev->netname), 127);
			szRiddenName[127] = 0;
		}
	}

	if (!bQuiet)
	{
		sprintf(szMsg, "* You are no longer riding %s\n", szRiddenName);
		MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, edict());
		WRITE_BYTE(ENTINDEX(edict()));
		WRITE_STRING(szMsg);
		MESSAGE_END();
	}

	*PM_GetRideInfo(ENTINDEX(edict())) = 0;
	m_iRidingIndex = 0;
	pev->owner = NULL;
	m_iRideShoulder = 0;
	pev->view_ofs = Vector(0, 0, 28);
	pev->solid = SOLID_SLIDEBOX;

	if (bQuiet)
		return;

	if (RC2_BASE_BYTE(this, 0x67) != 0)
	{
		RC2_BASE_BYTE(this, 0x67) = 0;
		ClearPhysFlags(8);
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfe);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(0x35);
		MESSAGE_END();
	}

	if (RC2_BASE_BYTE(this, 0x66) != 0)
	{
		RC2_BASE_BYTE(this, 0x66) = 0;
		RemovePhysFlag2Source(2);
		m_iRC2State4ac &= ~1;
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfe);
		WRITE_SHORT(ENTINDEX(pev->owner)); // VERIFY: pev->owner was cleared above, so this is ENTINDEX(NULL)
		WRITE_BYTE(0x2f);
		MESSAGE_END();
	}

	if (m_iShrunk != 0)
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "shr", "0");
		m_iShrunk = 0;
		m_iRC2State4ac &= ~0x10;
		if (m_pActiveItem)
			m_pActiveItem->Deploy();
	}

	// find room: 36 units along x, then y, then 42 along z, each way, a 1-unit hull test
	static const Vector s_vecTries[6] = {
		Vector(36, 0, 0), Vector(-36, 0, 0), Vector(0, 36, 0), Vector(0, -36, 0), Vector(0, 0, 42), Vector(0, 0, -42)};

	TraceResult tr;
	Vector vecTry;
	bool bFound = false;
	for (int i = 0; i < 6; i++)
	{
		vecTry = vecOrigin + s_vecTries[i];
		UTIL_TraceHull(vecTry, vecTry + Vector(0, 0, 1), dont_ignore_monsters, human_hull, edict(), &tr);
		if (tr.flFraction >= 1.0f && !tr.fAllSolid && !tr.fStartSolid)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		// no room: to a spawn spot, but the UTIL_SetOrigin below still uses the last try
		edict_t* pSpot = EntSelectSpawnPoint(this);
		entvars_t* pevSpot = pSpot ? VARS(pSpot) : NULL;
		pev->origin = pevSpot->origin + Vector(0, 0, 1); // VERIFY: as the original, no NULL check
		pev->v_angle = g_vecZero;
		pev->velocity = g_vecZero;
		pev->angles = pevSpot->angles;
		pev->punchangle = g_vecZero;
		pev->fixangle = 1;
	}
	else
	{
		pev->origin = vecTry;
	}

	UTIL_SetOrigin(pev, vecTry);
}

void PM_GetEntityBox(int iEntIndex, float* vecOrigin, float* vecVAngle, float* vecMins, float* vecMaxs) // orig 0xb2810
{
	edict_t* pent = INDEXENT(iEntIndex);
	if (!UTIL_IsValidEntity(pent))
		return;

	VectorCopy(pent->v.origin, vecOrigin);
	VectorCopy(pent->v.v_angle, vecVAngle);
	VectorCopy(pent->v.mins, vecMins);
	VectorCopy(pent->v.maxs, vecMaxs);
}

bool PlayerHasRider(edict_t* pPlayer, edict_t* pRider) // orig 0xb28b0
{
	if (!UTIL_IsValidEntity(pPlayer) || !UTIL_IsValidEntity(pRider))
		return false;

	CBasePlayer* pEntity = (CBasePlayer*)CBaseEntity::Instance(pPlayer);
	if (!pEntity || !pEntity->IsActivePlayer())
		return false;

	int iRider = ENTINDEX(pRider);
	if (iRider == 0)
		return false;

	return pEntity->m_iShoulderRider[0] == iRider || pEntity->m_iShoulderRider[1] == iRider;
}


// CHalfLifeMultiplay::PlayerAllowsRiding is in multiplay_gamerules.cpp (p5)
