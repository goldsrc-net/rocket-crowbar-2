/***
*
*	Rocket Crowbar 2 (client): the transforms on the local client. Each transform has its own
*	crosshair (borrowed from the crossbow's, egon's and shotgun's sprite lists) and FOV, the
*	assassin and headcrab a jump charge meter, the houndeye its locator; TformStat says what
*	the server's transform code changed. Also the client's player-movement hooks for the
*	transforms (pm_rc2.h) and the race track the RaceStat message sends for PM_RaceMove.
*
*	client.dll 0xc9e00-0xcba00, between train.cpp and tri.cpp. All names here are ours.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"
#include "triangleapi.h"
#include "pm_defs.h"
#include "rc2prefs.h"
#include "rc2/transform_hud.h"
#include "rc2/c4_needs.h" // g_iHighestEntIndex (.bss 0x10206918, entity.cpp)

#include <cmath>

client_sprite_t* GetSpriteList(client_sprite_t* pList, const char* psz, int iRes, int iCount); // ammo.cpp
int PM_GetPlayerTimer(int iEnt, int iSlot);														  // pm_rc2.h

extern Vector v_angles; // view.cpp

// From elsewhere (Ammo_RestoreCrosshair and EV_TraceVisible are c1's; input_stubs_V.cpp stands in for them):
extern void Ammo_RestoreCrosshair();					// client.dll 0x4a900: CHudAmmo's, sets the current weapon's crosshair again (package c1)
extern void CLTE_KillByEntity(int iEnt, int iType); // client.dll 0x52280 (rc2/clte.cpp): frees the client tents of a type on an entity
extern Vector CLTE_GetOrigin(int iEnt);				// client.dll 0x59be0 (rc2/clte.cpp): an entity's origin as drawn
// client.dll 0xee70 (ev_hldm.cpp, package c1): what is left of flRange after tracing from vecStart
// along vecDir towards pTarget; 0 when it doesn't get there.
extern float EV_TraceVisible(Vector vecStart, Vector vecEnd, Vector vecDir, float flRange, cl_entity_t* pTarget, pmtrace_t* ptr, int iDepth);

int g_bHoundeyeAsleep = 0; // .bss 0x10220c40
int g_iBatPerch = 0;	   // .bss 0x10220c4c

// .bss 0x10220ba0-0x10220c68
static HSPRITE s_hAssassinCross;		  // 0x10220c08: crossbow.txt's "zoom" (else "crosshair")
static Rect s_rcAssassinCross;			  // 0x10220ba0
static HSPRITE s_hEgonCross;			  // 0x10220bc0: egon.txt's "crosshair", for the houndeye, controller, bat and headcrab
static Rect s_rcControllerCross;		  // 0x10220bc8
static HSPRITE s_hControllerCross;		  // 0x10220bd8
static Rect s_rcHeadcrabCross;			  // 0x10220be0
static HSPRITE s_hHeadcrabCross;		  // 0x10220bf0
static Rect s_rcHoundeyeCross;			  // 0x10220bf8
static Rect s_rcBatCross;				  // 0x10220c20
static HSPRITE s_hBatCross;				  // 0x10220c38
static HSPRITE s_hShotgunAutoaim;		  // 0x10220bc4: shotgun.txt's "autoaim" (else "crosshair")
static Rect s_rcShotgunAutoaim;			  // 0x10220bb0
static HSPRITE s_hLocatorGlow;			  // 0x10220c30
static HSPRITE s_hMeterEmpty;			  // 0x10220c34
static HSPRITE s_hMeterFull;			  // 0x10220c1c
static bool s_bHoundeyeLocator;			  // 0x10220c45: TformStat bit 0
static bool s_bHoundeyeSleeping;		  // 0x10220c46: TformStat bit 1
float g_flHoundeyeSleepFade;			  // 0x10220c48 (transform_hud.h)
static int s_iBatPerchInfo;				  // 0x10220c50
static float s_vecBatPerchOffset[3];	  // 0x10220c10
static int s_bHeadcrabLeap;				  // 0x10220c54
static int s_iOldViewMode;				  // 0x10220c58
static float s_flMeterAlpha;			  // 0x10220c5c
static float* s_pRacePoints;			  // 0x10220c60: origin and radius per point
static int s_iRacePointCount;			  // 0x10220c64
static int s_iRacePoint;				  // 0x10220c68
static float s_flRaceTrackRadius = 6.0f;  // .data 0x10175518

// Every frame (entity.cpp): the crosshair and FOV of the transform, the houndeye's sleep fade,
// and everything back to normal when the transform ends.
// orig client 0xc9e00
void RC2_TempEntThink(float flFrameTime)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	const bool bAlive = gHUD.m_Health.m_iHealth > 0;
	HSPRITE hCross = 0;
	const Rect* prc = nullptr;
	bool bDone = false; // the crosshair is set: skip to the transform-ended part

	switch (g_iPlayerClass)
	{
	case 6: // the race
		g_flTransformFOV = 70;
		s_iOldViewMode = g_iViewMode;
		return;

	case 1: // assassin: its FOV is the zoom's (TformStat)
		prc = &s_rcAssassinCross;
		if (bAlive)
		{
			if (0 == s_hAssassinCross)
			{
				s_iOldViewMode = g_iViewMode;
				return;
			}
			SetCrosshair(s_hAssassinCross, s_rcAssassinCross, 255, 255, 255);
			bDone = true;
		}
		break;

	case 2: // houndeye
		if (!bAlive)
		{
			SetCrosshair(0, s_rcHoundeyeCross, 255, 255, 255);
			g_flTransformFOV = 0;
		}
		else
		{
			g_flTransformFOV = 105;
			if (0 != s_hEgonCross)
				SetCrosshair(s_hEgonCross, s_rcHoundeyeCross, 255, 255, 255);
		}

		if (0 == g_bLocalTransforming)
		{
			if (!s_bHoundeyeSleeping)
			{
				if (0 < g_flHoundeyeSleepFade)
					g_flHoundeyeSleepFade -= flFrameTime * 0.5f;
			}
			else if (g_flHoundeyeSleepFade < 1)
				g_flHoundeyeSleepFade += flFrameTime * 0.3f;
		}
		else if (0 < g_flHoundeyeSleepFade)
			g_flHoundeyeSleepFade -= flFrameTime * 1.2f;

		if (g_flHoundeyeSleepFade < 0)
			g_flHoundeyeSleepFade = 0;
		else if (1 < g_flHoundeyeSleepFade)
			g_flHoundeyeSleepFade = 1;
		bDone = true;
		break;

	case 3: // controller
	case 4: // bat
		prc = (g_iPlayerClass == 3) ? &s_rcControllerCross : &s_rcBatCross;
		hCross = (g_iPlayerClass == 3) ? s_hControllerCross : s_hBatCross;
		if (bAlive)
		{
			g_flTransformFOV = 105;
			if (0 == s_hEgonCross)
			{
				s_iOldViewMode = g_iViewMode;
				return;
			}
			SetCrosshair(hCross, *prc, 255, 255, 255);
			bDone = true;
		}
		break;

	case 5: // headcrab
		prc = &s_rcHeadcrabCross;
		if (bAlive)
		{
			g_flTransformFOV = 110;
			if (0 == s_hEgonCross)
			{
				s_iOldViewMode = g_iViewMode;
				return;
			}
			// VERIFY: as the original, the headcrab's own sprite in the bat's rectangle (the two
			// are the same copy of egon's)
			SetCrosshair(s_hHeadcrabCross, s_rcBatCross, 255, 255, 255);
			bDone = true;
		}
		break;

	default:
		bDone = true;
		break;
	}

	if (!bDone)
	{
		// a dead transform: no crosshair, the normal FOV
		SetCrosshair(0, *prc, 255, 255, 255);
		g_flTransformFOV = 0;
	}

	if (g_iPlayerClass == 0)
	{
		g_flTransformFOV = 0;
		s_bHoundeyeLocator = false;
		s_bHoundeyeSleeping = false;
		g_flHoundeyeSleepFade = 0;
		g_bHoundeyeAsleep = 0;
		g_iBatPerch = 0;
		s_iBatPerchInfo = 0;
		if (0 != s_bHeadcrabLeap && pLocal)
			CLTE_KillByEntity(pLocal->index, 0x44);
		s_bHeadcrabLeap = 0;

		if (0 != s_iOldViewMode && 0 == g_iViewMode)
		{
			Ammo_RestoreCrosshair();
			s_iOldViewMode = g_iViewMode;
			return;
		}
		if (3 != s_iOldViewMode && 3 == g_iViewMode)
			SetCrosshair(s_hShotgunAutoaim, s_rcShotgunAutoaim, 255, 255, 255);
	}

	s_iOldViewMode = g_iViewMode;
}

// The jump charge meter, at the left edge, centred: flFraction of it full, pulsing when full;
// it fades in and out.
// orig client 0xca4c0
static void TransformHud_DrawMeter(float flFraction, float flTime, float flTimeDelta) // name ours
{
	if (0 < flFraction)
	{
		if (1 < flFraction)
			flFraction = 1;
		s_flMeterAlpha = flTimeDelta + flTimeDelta + s_flMeterAlpha;
		if (s_flMeterAlpha < 0)
			s_flMeterAlpha = 0;
		else if (1 < s_flMeterAlpha)
			s_flMeterAlpha = 1;
	}
	else
	{
		flFraction = 0;
		float a = s_flMeterAlpha - flTimeDelta * (4.0f / 3.0f);
		if (a < 0)
			a = 0;
		else if (1 < a)
			a = 1;
		s_flMeterAlpha = a;
		if (a <= 0)
			return;
	}

	float flBright;
	if (1 <= flFraction)
		flBright = cos(flTime * 8.377580409572781) * 0.2 + s_flMeterAlpha * 0.75;
	else
		flBright = cos(flTime * 4.1887902047863905) * 0.1 + s_flMeterAlpha * 0.7;

	if (flBright < 0)
		flBright = 0;
	else if (1 < flBright)
		flBright = 1;

	if (flBright <= 0)
		return;

	const int iWidth = SPR_Width(s_hMeterEmpty, 0);
	const int iHeight = SPR_Height(s_hMeterEmpty, 0);
	int y = ScreenHeight / 2 - iHeight / 2;
	Rect rc;

	if (flFraction < 1)
	{
		rc.left = 0;
		rc.right = iWidth;
		rc.top = 0;
		rc.bottom = (int)((1 - flFraction) * iHeight);
		if (0 < rc.bottom)
		{
			const int c = (int)(flBright * 255);
			SPR_Set(s_hMeterEmpty, c, c, c);
			SPR_DrawAdditive(0, 8, y, &rc);
		}
	}

	if (0 < flFraction)
	{
		const int iTop = (int)((1 - flFraction) * iHeight);
		y += iTop;
		rc.left = 0;
		rc.right = iWidth;
		rc.top = iTop;
		rc.bottom = iHeight;
		if (0 < iHeight - iTop)
		{
			const int c = (int)(flBright * 255);
			SPR_Set(s_hMeterFull, c, c, c);
			SPR_DrawAdditive(0, 8, y, &rc);
		}
	}
}

// From CHud::Redraw: the assassin's and headcrab's jump charge, the houndeye's locator (a glow on
// each player in sight whose state is "2", brighter the closer and the less in the way).
// orig client 0xca800
void RC2_HudRedraw(float flTime, float flTimeDelta)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	float flFull;

	switch (g_iPlayerClass)
	{
	case 1:
		if (!pLocal)
			return;
		flFull = RC2Pref(RC2P_TransformAssassin_JumpFullChargeTime).f;
		break;
	case 5:
		if (!pLocal)
			return;
		flFull = RC2Pref(RC2P_TransformHeadcrab_JumpFullChargeTime).f;
		break;
	case 2:
	{
		if (!s_bHoundeyeLocator || 0 == s_hLocatorGlow)
			return;

		if (pLocal)
		{
			// the cone: the FOV (45 when it is the normal one) plus 5 degrees
			const float flCosLimit = cos((((g_flTransformFOV == 0) ? 45.0 : g_flTransformFOV) + 5.0) * (M_PI / 180.0));
			const float flRange = RC2Pref(RC2P_TransformHoundeye_LocatorMaxPunch).f;

			Vector vecOrg = CLTE_GetOrigin(pLocal->index), vecOfs;
			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecOfs);
			const Vector vecEye = vecOrg + vecOfs;

			Vector vecForward;
			gEngfuncs.pfnAngleVectors(v_angles, vecForward, nullptr, nullptr);

			gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
			gEngfuncs.pEventAPI->EV_PushPMStates();
			gEngfuncs.pEventAPI->EV_SetSolidPlayers(pLocal->index - 1);
			gEngfuncs.pEventAPI->EV_SetTraceHull(2);

			for (int i = 1; i < g_iHighestEntIndex; i++)
			{
				cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(i);
				if (!pEnt || pEnt->index == pLocal->index)
					continue;
				if (pEnt->curstate.messagenum < pLocal->curstate.messagenum)
					continue;
				if (pEnt->curstate.team != 2) // VERIFY: what RC2 keeps in team (entity_state +0xa4) for this
					continue;

				const Vector vecCenter = pEnt->curstate.origin + (pEnt->curstate.mins + pEnt->curstate.maxs) * 0.5f;
				Vector vecTarget = vecCenter;
				vecTarget.z = (pEnt->curstate.maxs.z + pEnt->curstate.mins.z) * 0.25f + vecCenter.z;

				Vector vecDir = vecTarget - vecEye;
				const float flDist = vecDir.Length();
				if (flDist == 0)
					continue;
				vecDir = vecDir / flDist;
				if (DotProduct(vecDir, vecForward) < flCosLimit)
					continue;

				pmtrace_t tr;
				const float flLeft = EV_TraceVisible(vecEye, vecTarget, vecDir, flRange, pEnt, &tr, 16);
				if (flLeft <= 0)
					continue;

				float flPower = flLeft / flRange;
				if (flPower < 0)
					flPower = 0;
				else if (1 < flPower)
					flPower = 1;
				float flFar = flDist * (1.0f / 768.0f);
				if (flFar < 0)
					flFar = 0;
				else if (1 < flFar)
					flFar = 1;
				const float flAlpha = (1 - flFar) * 0.33 + flPower * 0.67;

				Vector vecScreen;
				gEngfuncs.pTriAPI->WorldToScreen(vecTarget, vecScreen);
				const float x = (float)(ScreenWidth / 2) + (float)(ScreenWidth / 2) * vecScreen.x - 32;
				const float y = (float)(ScreenHeight / 2) + (float)(-(ScreenHeight / 2)) * vecScreen.y - 32;

				const int c = (int)(flAlpha * 40);
				SPR_Set(s_hLocatorGlow, c, (int)(flAlpha * 255), c);
				SPR_DrawAdditive(0, (int)x, (int)y, nullptr);
			}
		}
		gEngfuncs.pEventAPI->EV_PopPMStates();
		return;
	}
	default:
		return;
	}

	const int iCharge = PM_GetPlayerTimer(pLocal->index, 2);
	if (iCharge < 1)
	{
		TransformHud_DrawMeter(0, flTime, flTimeDelta);
		return;
	}
	TransformHud_DrawMeter((float)iCharge / (flFull * 1000), flTime, flTimeDelta);
}

// What the server's transform code says (dlls/rc2/tassassin.cpp, thoundeye.cpp, tbat.cpp).
// orig client 0xcb0a0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_TformStat(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	switch (g_iPlayerClass)
	{
	case 1: // the assassin's zoom
		if ((READ_BYTE() & 1) != 0)
			g_flTransformFOV = RC2Pref(RC2P_TransformAssassin_ZoomFOV).f;
		else
			g_flTransformFOV = 0;
		return 1;

	case 2: // the houndeye: bit 0 the locator, bit 1 asleep
	{
		const int iStatus = READ_BYTE();
		s_bHoundeyeLocator = (iStatus & 1) != 0;
		if ((iStatus & 2) != 0)
		{
			s_bHoundeyeSleeping = true;
			g_bHoundeyeAsleep = 1;
		}
		else
		{
			s_bHoundeyeSleeping = false;
			g_bHoundeyeAsleep = 0;
		}
		return 1;
	}

	case 4: // the bat: perched sends what it hangs from and where
		g_iBatPerch = READ_BYTE();
		if (g_iBatPerch == 2)
		{
			s_iBatPerchInfo = READ_SHORT();
			s_vecBatPerchOffset[0] = READ_COORD();
			s_vecBatPerchOffset[1] = READ_COORD();
			s_vecBatPerchOffset[2] = READ_COORD();
		}
		break;
	}

	return 1;
}

static void TransformHud_LoadCrosshair(const char* pszList, const char* pszFirst, int iRes, HSPRITE& hspr, Rect& rc) // name ours: inlined
{
	char sz[128];
	int iCount;
	client_sprite_t* pList = SPR_GetList(pszList, &iCount);
	hspr = 0;
	if (!pList)
		return;

	client_sprite_t* p = pszFirst ? GetSpriteList(pList, pszFirst, iRes, iCount) : nullptr;
	if (!p)
		p = GetSpriteList(pList, "crosshair", iRes, iCount);
	if (!p)
		return;

	sprintf(sz, "sprites/%s.spr", p->szSprite);
	hspr = SPR_Load(sz);
	rc = p->rc;
}

// orig client 0xcb1c0
void TransformHud_VidInit()
{
	const int iRes = (ScreenWidth < 640) ? 320 : 640;

	TransformHud_LoadCrosshair("sprites/weapon_crossbow.txt", "zoom", iRes, s_hAssassinCross, s_rcAssassinCross);

	s_hEgonCross = 0;
	s_hControllerCross = 0;
	s_hBatCross = 0;
	s_hHeadcrabCross = 0;
	Rect rcEgon;
	TransformHud_LoadCrosshair("sprites/weapon_egon.txt", nullptr, iRes, s_hEgonCross, rcEgon);
	if (0 != s_hEgonCross)
	{
		s_rcControllerCross = rcEgon;
		s_hControllerCross = s_hEgonCross;
		s_rcHeadcrabCross = rcEgon;
		s_hHeadcrabCross = s_hEgonCross;
		s_rcHoundeyeCross = rcEgon;
		s_rcBatCross = rcEgon;
		s_hBatCross = s_hEgonCross;
	}

	TransformHud_LoadCrosshair("sprites/weapon_shotgun.txt", "autoaim", iRes, s_hShotgunAutoaim, s_rcShotgunAutoaim);

	s_hLocatorGlow = SPR_Load("sprites/glow01.spr");
	s_hMeterEmpty = SPR_Load("sprites/jm_empty.spr");
	s_hMeterFull = SPR_Load("sprites/jm_full.spr");
}

// The client's movement hooks (pm_rc2.h): the local player's transform state, nothing for others.

// orig client 0xcb5f0
int PM_GetPlayerMode(int iEnt)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return 0;
	return (pLocal->index == iEnt) ? g_iPlayerClass : 0;
}

// orig client 0xcb630
int PM_GetBatPerch(int iEnt, int* piAttachInfo, float* vecOffset)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pLocal->index != iEnt)
		return 0;
	if (piAttachInfo)
		*piAttachInfo = s_iBatPerchInfo;
	if (vecOffset)
	{
		vecOffset[0] = s_vecBatPerchOffset[0];
		vecOffset[1] = s_vecBatPerchOffset[1];
		vecOffset[2] = s_vecBatPerchOffset[2];
	}
	return g_iBatPerch;
}

// orig client 0xcb6a0
void PM_SetHeadcrabLeap(int iEnt, int bLeaping)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pLocal->index == iEnt)
		s_bHeadcrabLeap = bLeaping;
}

// orig client 0xcb6d0
int PM_GetHeadcrabLeap(int iEnt)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return 0;
	return (pLocal->index == iEnt) ? s_bHeadcrabLeap : 0;
}

// The race track (dlls/rc2/racecontroller.cpp sends it): byte 1 a header (the point count and
// the track radius), byte 2 one point (its index, origin and radius).
// orig client 0xcb710
int __MsgFunc_RaceStat(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	const int iType = READ_BYTE();
	if (iType == 1)
	{
		if (0 != s_iRacePointCount && s_pRacePoints)
			delete[] s_pRacePoints; // VERIFY: as the original, an empty track's buffer is never freed
		s_iRacePointCount = READ_SHORT();
		s_flRaceTrackRadius = READ_FLOAT();
		s_pRacePoints = new float[s_iRacePointCount * 4];
		s_iRacePoint = 0;
	}
	else if (iType == 2)
	{
		const int i = READ_SHORT();
		if (0 <= i && i < s_iRacePointCount && s_pRacePoints)
		{
			s_pRacePoints[i * 4 + 0] = READ_FLOAT();
			s_pRacePoints[i * 4 + 1] = READ_FLOAT();
			s_pRacePoints[i * 4 + 2] = READ_FLOAT();
			s_pRacePoints[i * 4 + 3] = READ_FLOAT();
			return 1;
		}
	}
	return 1;
}

// orig client 0xcb830
void PM_GetRacePoint(int iEnt, float* vecOrigin, float* flRadius)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pLocal->index != iEnt || s_iRacePoint < 0 || s_iRacePointCount <= s_iRacePoint)
		return;
	if (vecOrigin)
	{
		vecOrigin[0] = s_pRacePoints[s_iRacePoint * 4 + 0];
		vecOrigin[1] = s_pRacePoints[s_iRacePoint * 4 + 1];
		vecOrigin[2] = s_pRacePoints[s_iRacePoint * 4 + 2];
	}
	if (flRadius)
		*flRadius = s_pRacePoints[s_iRacePoint * 4 + 3];
}

// orig client 0xcb8f0
void PM_GetRacePointOffset(int iEnt, int iOffset, float* vecOrigin, float* flRadius)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pLocal->index != iEnt || 0 == s_iRacePointCount)
		return;

	int i = iOffset + s_iRacePoint;
	while (true)
	{
		while (i < 0)
			i += s_iRacePointCount;
		if (i < s_iRacePointCount)
			break;
		i -= s_iRacePointCount;
	}

	if (vecOrigin)
	{
		vecOrigin[0] = s_pRacePoints[i * 4 + 0];
		vecOrigin[1] = s_pRacePoints[i * 4 + 1];
		vecOrigin[2] = s_pRacePoints[i * 4 + 2];
	}
	if (flRadius)
		*flRadius = s_pRacePoints[i * 4 + 3];
}

// orig client 0xcb9b0
void PM_NextRacePoint(int iEnt)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pLocal->index == iEnt)
	{
		s_iRacePoint++;
		if (s_iRacePoint < 0 || s_iRacePointCount <= s_iRacePoint)
			s_iRacePoint = 0;
	}
}

// orig client 0xcba00
void PM_GetRaceTrackRadius(int iEnt, float* flRadius)
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pLocal->index == iEnt && flRadius)
		*flRadius = s_flRaceTrackRadius;
}
