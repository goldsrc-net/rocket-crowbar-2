/***
*
*	Rocket Crowbar 2 (client): the client's side of the race move and the pullers (pm_rc2.h).
*	The server sends its players' tethers and the global pull points in PullerCtrl
*	(dlls/rc2/pullers.cpp); the client keeps a copy for its prediction of pm_shared. client.dll
*	0xbc680-0xbd490, right after pm_shared.cpp, as rc2.dll's dlls/rc2/pullers.cpp is. File name
*	and every name here are ours.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_rc2.h"

#include <cstring>

// pm_math.cpp's, as the original calls (cl_util.h makes it the engine's)
#undef AngleVectors

extern playermove_t* pmove;
extern Vector g_vecRaceViewAngles; // pm_shared.cpp

// the race track, from RaceStat (cl_dll/rc2/transform_hud.cpp, client 0xcb830-0xcba00)
void PM_GetRacePoint(int iEnt, float* vecOrigin, float* flRadius);
void PM_GetRacePointOffset(int iEnt, int iOffset, float* vecOrigin, float* flRadius);
void PM_NextRacePoint(int iEnt);
void PM_GetRaceTrackRadius(int iEnt, float* flRadius);

// ThisClient's player index (hud.cpp; client .data 0x1013d0b8, initially 1)
extern int g_iThisClient;

float g_flRaceSpeed; // client .bss 0x1020f6c4: PM_RaceMove's speed; CHudHealth::Draw shows it (times 16.665) in a race, name ours

// The local player's tethers, as PullerCtrl 1 and 2 set them
static float s_flTetherLength[64]; // client .bss 0x10210170
static float s_flTetherForce[64];  // client .bss 0x10210270: 0 = slot free
static int s_iTetherEnt[64];	   // client .bss 0x10210370: below 1, the offset is a world point
static Vector s_vecTetherOffset[64]; // client .bss 0x10210470
static bool s_bPullersValid;	   // client .bss 0x10210870

// The global pull points, PullerCtrl 4
static Vector s_vecPullPointOrg[64];   // client .bss 0x1020fe70
static float s_flPullPointRadius[64];  // client .bss 0x10210770
static float s_flPullPointForce[64];   // client .bss 0x1020fd60: the server's float bits

static Vector s_vecPullVec; // client .bss 0x1020fe60: PullerCtrl 3; zero for none

// As rc2.dll's PM_RaceMove (dlls/rc2/pullers.cpp) on the client's race track, and the view
// angles and speed are kept for the view (g_vecRaceViewAngles).
void PM_RaceMove() // orig client 0xbc680
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

	// as the server's: (1 - n) where (1 + n) would wrap any negative roll
	if (flRoll < 0)
		flRoll = (1 - (int)(flRoll * -0.0027777778f)) * 360 + flRoll;
	else if (flRoll >= 360)
		flRoll = flRoll - (int)(flRoll * 0.0027777778f) * 360;

	float flHeight; // VERIFY: left unset when this isn't the local player, as in the original
	PM_GetRaceTrackRadius(g_iPMPlayerEnt, &flHeight);

	float flDist = flSpeed * pmove->frametime;
	pmove->origin = pmove->origin - pmove->velocity;
	Vector vecStart = pmove->origin;

	Vector vecPoint;
	float flRadius;
	while (flDist > 0)
	{
		PM_GetRacePoint(g_iPMPlayerEnt, vecPoint, &flRadius);
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
		PM_NextRacePoint(g_iPMPlayerEnt);
	}

	PM_GetRacePoint(g_iPMPlayerEnt, vecPoint, &flRadius);
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
		PM_GetRacePointOffset(g_iPMPlayerEnt, -1, vecPrev, NULL);
		Vector vecSeg = vecPoint - vecPrev;
		PM_GetRacePointOffset(g_iPMPlayerEnt, 1, vecNext, NULL);
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

	g_vecRaceViewAngles = vecAngles;
	g_flRaceSpeed = flSpeed;
	pmove->flFallVelocity = flSpeed;
	pmove->waterjumptime = flRoll;
}

// Tether iSlot of player iPlayer; the client knows only its own.
bool PM_GetPlayerTether(int iPlayer, int iSlot, float* pvecOrigin, float* pflForce, float* pflLength) // orig client 0xbce50
{
	if (!s_bPullersValid || iSlot < 0 || iSlot >= 64)
		return false;
	if (iPlayer < 1 || iPlayer > gEngfuncs.GetMaxClients())
		return false;
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pLocal->index != iPlayer || s_flTetherForce[iSlot] == 0)
		return false;

	if (s_iTetherEnt[iSlot] < 1)
	{
		pvecOrigin[0] = s_vecTetherOffset[iSlot].x;
		pvecOrigin[1] = s_vecTetherOffset[iSlot].y;
		pvecOrigin[2] = s_vecTetherOffset[iSlot].z;
	}
	else
	{
		// the original calls a one-line wrapper of GetEntityByIndex (client 0x9e9a0)
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(s_iTetherEnt[iSlot]);
		if (!pEnt)
			return false;
		pvecOrigin[0] = s_vecTetherOffset[iSlot].x + pEnt->origin.x;
		pvecOrigin[1] = s_vecTetherOffset[iSlot].y + pEnt->origin.y;
		pvecOrigin[2] = s_vecTetherOffset[iSlot].z + pEnt->origin.z;
	}
	*pflForce = s_flTetherForce[iSlot];
	*pflLength = s_flTetherLength[iSlot];
	return true;
}

bool PM_GetPlayerPullVec(int iEnt, float* pvecPoint) // orig client 0xbcfb0
{
	if (iEnt != g_iThisClient)
		return false;
	if (s_vecPullVec.x == 0 && s_vecPullVec.y == 0 && s_vecPullVec.z == 0)
		return false;
	pvecPoint[0] = s_vecPullVec.x;
	pvecPoint[1] = s_vecPullVec.y;
	pvecPoint[2] = s_vecPullVec.z;
	return true;
}

// The server's clears the tractor; the client leaves that to the server.
void PM_ClearPlayerPull(int iEnt) // orig client 0xbd050
{
}

void PM_GetGlobalPuller(int iPoint, float* pvecOrigin, float* pflRadius, float* pflPlayerForce) // orig client 0xbd070
{
	if (iPoint < 0 || iPoint >= 64)
		return;
	pvecOrigin[0] = s_vecPullPointOrg[iPoint].x;
	pvecOrigin[1] = s_vecPullPointOrg[iPoint].y;
	pvecOrigin[2] = s_vecPullPointOrg[iPoint].z;
	*pflRadius = s_flPullPointRadius[iPoint];
	*pflPlayerForce = s_flPullPointForce[iPoint];
}

static void ResetTethers()
{
	for (int i = 0; i < 64; i++)
		s_iTetherEnt[i] = -1;
	for (int i = 0; i < 64; i++)
		s_flTetherLength[i] = 0;
	for (int i = 0; i < 64; i++)
		s_flTetherForce[i] = 0;
	for (int i = 0; i < 64; i++)
		s_vecTetherOffset[i] = g_vecZero;
}

// Everything back to none (client 0x4fe80 calls it, at the HUD's reset).
void PM_ResetPullers() // orig client 0xbd0e0
{
	ResetTethers();
	for (int i = 0; i < 64; i++)
		s_vecPullPointOrg[i] = g_vecZero;
	s_vecPullVec = g_vecZero;
	s_bPullersValid = true;
}

// PullerCtrl: 0 drop all tethers, 1 a tether, 2 drop one, 3 the pull vector, 4 a pull point.
// The original returns 0 for a slot out of range.
int __MsgFunc_PullerCtrl(const char* pszName, int iSize, void* pbuf) // orig client 0xbd1f0
{
	BEGIN_READ(pbuf, iSize);
	const int iType = READ_BYTE();

	if (iType == 0)
	{
		ResetTethers();
		s_bPullersValid = true;
		return 1;
	}

	if (iType == 1)
	{
		const int iSlot = READ_BYTE();
		if (iSlot < 0 || iSlot >= 64)
			return 0;
		s_iTetherEnt[iSlot] = READ_SHORT();
		s_vecTetherOffset[iSlot].x = READ_COORD();
		s_vecTetherOffset[iSlot].y = READ_COORD();
		s_vecTetherOffset[iSlot].z = READ_COORD();
		s_flTetherForce[iSlot] = READ_SHORT() * 0.01f;
		s_flTetherLength[iSlot] = READ_SHORT() * 0.01f;
		return 1;
	}

	if (iType == 2)
	{
		const int iSlot = READ_BYTE();
		if (iSlot < 0 || iSlot >= 64)
			return 0;
		s_iTetherEnt[iSlot] = -1;
		s_flTetherForce[iSlot] = 0;
		s_vecTetherOffset[iSlot] = g_vecZero;
		s_flTetherLength[iSlot] = 0;
		return 1;
	}

	if (iType == 3)
	{
		s_vecPullVec.x = READ_COORD();
		s_vecPullVec.y = READ_COORD();
		s_vecPullVec.z = READ_COORD();
		return 1;
	}

	if (iType != 4)
	{
		gEngfuncs.pfnConsolePrint("Bad PullerCtrl message!\n");
		return 1;
	}

	const int iPoint = READ_BYTE();
	if (iPoint < 0 || iPoint >= 64)
		return 0;
	s_vecPullPointOrg[iPoint].x = READ_COORD();
	s_vecPullPointOrg[iPoint].y = READ_COORD();
	s_vecPullPointOrg[iPoint].z = READ_COORD();
	// original bug: the server sends up to 0xff37, read back signed
	s_flPullPointRadius[iPoint] = (float)READ_SHORT();
	const int iForceBits = READ_LONG(); // the float's bits
	memcpy(&s_flPullPointForce[iPoint], &iForceBits, sizeof(float));
	return 1;
}
