/***
*
*	Rocket Crowbar 2: pullers.cpp (file name ours). See pullers.h. Original functions
*	0xa9170-0xaac20 of rc2.dll, the run right after pm_shared (CBaseEntity::ApplyExternalForce,
*	0xa9ba0, is with the other CBaseEntity code in cbase.cpp).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "p6stubs.h"
#include "rc2/pullers.h"

extern playermove_t* pmove;

Vector g_vecPullPointOrg[64];
float g_flPullPointMonsterForce[64];
float g_flPullPointPlayerForce[64];
float g_flPullPointRadius[64];

// The racing move (pmove->movetype set by the race code): the player rides the race path at a
// speed the forward/back keys change, rolled by the strafe keys. RC2 keeps the speed in
// pmove->flFallVelocity and the roll in pmove->waterjumptime, and uses pmove->velocity as the
// sideways bulge of the path's curve, taken off at the start and put back at the end.
void PM_RaceMove() // orig 0xa9170, name ours
{
	float flSpeed = pmove->flFallVelocity;
	float flRoll = pmove->waterjumptime;

	pmove->flags &= ~FL_DUCKING;
	pmove->usehull = 2;
	pmove->onground = -1;

	if (pmove->cmd.forwardmove > 0)
		flSpeed = pmove->frametime * 32 + flSpeed;
	else if (pmove->cmd.forwardmove < 0)
		flSpeed = flSpeed - pmove->frametime * 16;

	if (pmove->cmd.sidemove > 0)
		flRoll = pmove->frametime * 180 + flRoll;
	else if (pmove->cmd.sidemove < 0)
		flRoll = flRoll - pmove->frametime * 180;

	if (flSpeed < 30)
		flSpeed = 30;
	else if (flSpeed > 600)
		flSpeed = 600;

	// the original's wrap: (1 - n) where (1 + n) would bring any negative roll into 0..360, so
	// a roll below -360 stays negative (it only moves 180 a second, so it doesn't happen)
	if (flRoll < 0)
		flRoll = (1 - (int)(flRoll * -0.0027777778f)) * 360 + flRoll;
	else if (flRoll >= 360)
		flRoll = flRoll - (int)(flRoll * 0.0027777778f) * 360;

	float flHeight; // VERIFY: left unset when the player isn't racing, as in the original
	RacePath_GetHeight(g_iPMPlayerEnt, &flHeight);

	float flDist = flSpeed * pmove->frametime;
	pmove->origin = pmove->origin - pmove->velocity;
	Vector vecStart = pmove->origin;

	Vector vecPoint;
	float flRadius;
	while (flDist > 0)
	{
		RacePath_GetCurrent(g_iPMPlayerEnt, &vecPoint, &flRadius);
		Vector vecTo = vecPoint - pmove->origin;
		float flLen = Length(vecTo);
		if (flLen == 0)
		{
			flDist = flDist - flDist * 0.1;
		}
		else
		{
			float flStep = flDist;
			if (flLen < flDist)
				flStep = flLen;
			VectorMA(pmove->origin, flStep / flLen, vecTo, pmove->origin);
			flDist -= flStep;
			if (flDist <= 0)
				break;
		}
		RacePath_Advance(g_iPMPlayerEnt);
	}

	RacePath_GetCurrent(g_iPMPlayerEnt, &vecPoint, &flRadius);
	if (flRadius <= 0)
	{
		pmove->velocity = g_vecZero;
	}
	else
	{
		// the bulge: the circle of radius flRadius through the previous point and this one
		Vector vecToPoint = vecPoint - pmove->origin;
		float flToPoint = Length(vecToPoint);
		Vector vecPrev, vecNext;
		RacePath_GetRelative(g_iPMPlayerEnt, -1, &vecPrev, NULL);
		Vector vecSeg = vecPoint - vecPrev;
		RacePath_GetRelative(g_iPMPlayerEnt, 1, &vecNext, NULL);
		Vector vecSegNext = vecNext - vecPoint;
		Vector vecNormal, vecOut;
		CrossProduct(vecSeg, vecSegNext, vecNormal);
		CrossProduct(vecSeg, vecNormal, vecOut);
		VectorNormalize(vecOut);
		float flSegLen = Length(vecSeg);
		float flAlong = flSegLen - flToPoint;
		if (flAlong < 0)
			flAlong = 0;
		if (flSegLen < flAlong)
			flAlong = flSegLen;
		flAlong = flSegLen * 0.5 - flAlong;
		VectorScale(vecOut, sqrt(flRadius * flRadius - flAlong * flAlong) - sqrt(flRadius * flRadius - flSegLen * flSegLen * 0.25), pmove->velocity);
	}

	Vector vecMoved = pmove->origin - vecStart;
	Vector vecAngles;
	VectorAngles(vecMoved, vecAngles);
	vecAngles[2] = flRoll;
	AngleVectors(vecAngles, &pmove->forward, &pmove->right, &pmove->up);
	VectorMA(pmove->velocity, -flHeight, pmove->up, pmove->velocity);
	pmove->origin = pmove->velocity + pmove->origin;
	pmove->flFallVelocity = flSpeed;
	pmove->waterjumptime = flRoll;
}

// Tether iSlot of player iPlayer: where it pulls to, its force and slack.
bool PM_GetPlayerTether(int iPlayer, int iSlot, float* pvecOrigin, float* pflForce, float* pflLength) // orig 0xa9710, name ours
{
	if (iSlot < 0 || iSlot >= 64)
		return false;

	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return false;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent); // VERIFY: not checked to be a player, as in the original
	if (!pPlayer || pPlayer->m_flTetherForce[iSlot] == 0)
		return false;

	if (pPlayer->m_iTetherEnt[iSlot] < 1)
	{
		pvecOrigin[0] = pPlayer->m_vecTetherOffset[iSlot].x;
		pvecOrigin[1] = pPlayer->m_vecTetherOffset[iSlot].y;
		pvecOrigin[2] = pPlayer->m_vecTetherOffset[iSlot].z;
	}
	else
	{
		if (!UTIL_IsValidEntity(INDEXENT(pPlayer->m_iTetherEnt[iSlot])))
			return false;
		entvars_t* pevEnt = VARS(INDEXENT(pPlayer->m_iTetherEnt[iSlot]));
		pvecOrigin[0] = pPlayer->m_vecTetherOffset[iSlot].x + pevEnt->origin.x;
		pvecOrigin[1] = pPlayer->m_vecTetherOffset[iSlot].y + pevEnt->origin.y;
		pvecOrigin[2] = pPlayer->m_vecTetherOffset[iSlot].z + pevEnt->origin.z;
	}
	*pflForce = pPlayer->m_flTetherForce[iSlot];
	*pflLength = pPlayer->m_flTetherLength[iSlot];
	return true;
}

// The point entity iEnt is tractored to (the egon's tractor beam): CBaseEntity's
// m_vecRC2Base58, while its byte +0x64 is set.
bool PM_GetPlayerPullVec(int iEnt, float* pvecPoint) // orig 0xa9840, name ours
{
	edict_t* pent = INDEXENT(iEnt);
	if (!UTIL_IsValidEntity(pent))
		return false;
	CBaseEntity* pEnt = CBaseEntity::Instance(pent);
	if (!pEnt)
		return false;
	if (pEnt->m_ucRC2Base64[0] == 0)
		return false;
	pvecPoint[0] = pEnt->m_vecRC2Base58.x;
	pvecPoint[1] = pEnt->m_vecRC2Base58.y;
	pvecPoint[2] = pEnt->m_vecRC2Base58.z;
	return true;
}

void PM_ClearPlayerPull(int iEnt) // orig 0xa98b0, name ours
{
	edict_t* pent = INDEXENT(iEnt);
	if (!UTIL_IsValidEntity(pent))
		return;
	CBaseEntity* pEnt = CBaseEntity::Instance(pent);
	if (!pEnt)
		return;
	pEnt->m_ucRC2Base64[0] = 0;
	pEnt->m_ucRC2Base64[1] = 0; // VERIFY: what the second byte is
}

void PM_GetGlobalPuller(int iPoint, float* pvecOrigin, float* pflRadius, float* pflPlayerForce) // orig 0xa98f0, name ours
{
	if (iPoint < 0 || iPoint >= 64)
		return;
	pvecOrigin[0] = g_vecPullPointOrg[iPoint].x;
	pvecOrigin[1] = g_vecPullPointOrg[iPoint].y;
	pvecOrigin[2] = g_vecPullPointOrg[iPoint].z;
	*pflRadius = g_flPullPointRadius[iPoint];
	*pflPlayerForce = g_flPullPointPlayerForce[iPoint];
}

// The force every entity's GetPullOrg puts on pEnt: each pulls along the line to its point,
// (distance - slack) * force, scaled down by pEnt's GetForceScale, at most 10000.
Vector RC2_GetPullForce(CBaseEntity* pEnt) // orig 0xa9940, name ours
{
	if (!pEnt)
		return g_vecZero;

	Vector vecForce = g_vecZero;
	float flScale = pEnt->GetForceScale();
	if (flScale == 0)
		flScale = 1.0;
	float flInvScale = 1.0 / flScale;

	// from entity 1, and the last one (maxEntities - 1) is included: i < maxEntities
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pPuller = CBaseEntity::Instance(pent);
		if (!pPuller)
			continue;

		Vector vecOrigin;
		float flForce, flLength;
		if (!pPuller->GetPullOrg(pEnt, vecOrigin, flForce, flLength) || flForce == 0)
			continue;

		Vector vecTo = vecOrigin - pEnt->Center();
		float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);
		if (flLength < flDist && flDist != 0)
		{
			float flPull = (flDist - flLength) * flForce * flInvScale;
			if (flPull > 10000)
				flPull = 10000;
			vecForce = Vector(vecTo.x / flDist * flPull + vecForce.x, vecTo.y / flDist * flPull + vecForce.y, flPull * (vecTo.z / flDist) + vecForce.z);
		}
	}
	return vecForce;
}

// PullerCtrl 1: tether iSlot of this client now pulls to entity iEnt + vecOffset.
static void SendTetherAdd(int iPlayer, int iSlot, int iEnt, const Vector& vecOffset, float flForce, float flLength) // orig 0xaa7d0, name ours
{
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return;
	MESSAGE_BEGIN(MSG_ONE, gmsgPullerCtrl, NULL, pent);
	WRITE_BYTE(1);
	WRITE_BYTE(iSlot);
	WRITE_SHORT(iEnt);
	WRITE_COORD(vecOffset.x);
	WRITE_COORD(vecOffset.y);
	WRITE_COORD(vecOffset.z);
	WRITE_SHORT((int)(flForce * 100.0));
	WRITE_SHORT((int)(flLength * 100.0));
	MESSAGE_END();
}

// orig 0xaa880 is Vector::operator== (mathlib.h's inline), emitted out of line in this file

// PullerCtrl 0: drop all of this client's tethers.
static void SendTetherReset(int iPlayer) // orig 0xaa8c0, name ours
{
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return;
	MESSAGE_BEGIN(MSG_ONE, gmsgPullerCtrl, NULL, pent);
	WRITE_BYTE(0);
	MESSAGE_END();
}

// PullerCtrl 2: drop tether iSlot.
static void SendTetherRemove(int iPlayer, int iSlot) // orig 0xaa900, name ours
{
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return;
	MESSAGE_BEGIN(MSG_ONE, gmsgPullerCtrl, NULL, pent);
	WRITE_BYTE(2);
	WRITE_BYTE(iSlot);
	MESSAGE_END();
}

int AddPullPoint(Vector vecOrigin, float flRadius, float flPlayerForce, float flMonsterForce) // orig 0xaa950, name ours
{
	int i;
	for (i = 0; i < 64; i++)
	{
		if (g_vecPullPointOrg[i] == g_vecZero)
			break;
	}
	if (i >= 64)
		return -1;

	g_flPullPointRadius[i] = flRadius;
	g_flPullPointPlayerForce[i] = flPlayerForce;
	g_vecPullPointOrg[i] = vecOrigin;
	g_flPullPointMonsterForce[i] = flMonsterForce;
	return i;
}

// orig 0xaa9e0 is Vector::operator/(float) (mathlib.h's inline), emitted out of line in this file

// Only the origin is cleared; the forces and radius stay until the slot is reused.
void RemovePullPoint(int iPoint) // orig 0xaaa20, name ours
{
	if (iPoint < 0 || iPoint >= 64)
		return;
	g_vecPullPointOrg[iPoint] = g_vecZero;
}

void CBasePlayer::ResetTethers() // orig 0xaaa60, name ours
{
	for (int i = 0; i < 64; i++)
	{
		m_flTetherForce[i] = 0;
		m_flTetherLength[i] = 0;
		m_vecTetherOffset[i] = g_vecZero;
		m_iTetherEnt[i] = -1;
	}
	for (int i = 0; i < 64; i++)
		m_vecSentPullPoint[i] = g_vecZero;

	SendTetherReset(ENTINDEX(edict()));
}

// Takes the first free slot; -1 when all 64 are in use.
int CBasePlayer::AddTether(int iEnt, Vector vecOffset, float flForce, float flLength) // orig 0xaab00, name ours
{
	for (int i = 0; i < 64; i++)
	{
		if (m_flTetherForce[i] != 0)
			continue;

		m_flTetherForce[i] = flForce;
		m_flTetherLength[i] = flLength;
		m_vecTetherOffset[i] = vecOffset;
		m_iTetherEnt[i] = iEnt;
		SendTetherAdd(ENTINDEX(edict()), i, iEnt, vecOffset, flForce, flLength);
		return i;
	}
	return -1;
}

void CBasePlayer::RemoveTether(int iSlot) // orig 0xaabb0, name ours
{
	if (iSlot < 0 || iSlot >= 64)
		return;
	m_flTetherForce[iSlot] = 0;
	m_flTetherLength[iSlot] = 0;
	m_vecTetherOffset[iSlot] = g_vecZero;
	m_iTetherEnt[iSlot] = -1;
	SendTetherRemove(ENTINDEX(edict()), iSlot);
}

// PullerCtrl 4, for every global pull point whose origin changed since the last send.
// VERIFY: no caller found (likely called through a pointer or from the player's per-frame code)
void CBasePlayer::SendPullPoints() // orig 0xaac20, name ours
{
	for (int i = 0; i < 64; i++)
	{
		if (m_vecSentPullPoint[i] == g_vecPullPointOrg[i])
			continue;

		m_vecSentPullPoint[i] = g_vecPullPointOrg[i];
		MESSAGE_BEGIN(MSG_ONE, gmsgPullerCtrl, NULL, edict());
		WRITE_BYTE(4);
		WRITE_BYTE(i);
		WRITE_COORD(g_vecPullPointOrg[i].x);
		WRITE_COORD(g_vecPullPointOrg[i].y);
		WRITE_COORD(g_vecPullPointOrg[i].z);
		int iRadius = (int)g_flPullPointRadius[i];
		if (iRadius < 0)
			iRadius = 0;
		else if (iRadius > 0xff37)
			iRadius = 0xff37;
		WRITE_SHORT(iRadius);
		// the float's bits, not its value
		WRITE_LONG(*reinterpret_cast<int*>(&g_flPullPointPlayerForce[i]));
		MESSAGE_END();
	}
}
