// Rocket Crowbar 2: the name of the player under the crosshair, faded in and out on the
// viewport's player name label. HUD_AddEntity records each player's centre (PlayerID_AddEntity);
// HUD_Frame picks, ten times a second, the visible player nearest the view direction (within a
// cone of cos 0.968, or flat when close and level) and holds the name for a quarter second.
// client.dll keeps no file names: this run sits between vgui_grid and vgui_int. Names ours.

#include "hud.h"
#include "cl_util.h"
#include "event_api.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "entity_types.h"

#include "vgui_TeamFortressViewport.h"

#define PLAYERID_MAX 32

static bool g_bPlayerIDListFresh;			   // 0x102223c0: set by the first player added since the last pick
static bool g_bPlayerIDEnabled;				   // 0x102223c4
static float g_flPlayerIDTraceTime;			   // 0x102223c8: until the next pick
static float g_flPlayerIDHoldTime;			   // 0x102223cc: until the name starts to fade
static float g_flPlayerIDAlpha = 255;		   // 0x10175c2c: VGUI alpha, 255 is invisible
static int g_iPlayerIDEnts[PLAYERID_MAX];	   // 0x10222340: entity indexes, -1 for none
static Vector g_vecPlayerIDCentre[PLAYERID_MAX]; // 0x102221c0

extern float* GetClientColor(int clientIndex);
extern Vector v_angles; // view.cpp
// the player's ride info from PlrStat: the low 7 bits the player he rides, bit 7 the left
// shoulder (rc2/pm_client.cpp, client 0xaf980)
unsigned char* PM_GetRideInfo(int iEnt);

// orig client 0xd8980. Called from HUD_Frame with the frame time.
void PlayerID_Frame(float flFrameTime)
{
	if (!gViewPort || !gViewPort->m_pPlayerIDLabel)
		return;

	if (flFrameTime <= 0)
		return;

	if (!g_bPlayerIDEnabled)
		return;

	if (gEngfuncs.GetMaxClients() <= 1)
		return;

	if (gEngfuncs.GetClientTime() <= 0)
		return;

	g_flPlayerIDHoldTime -= flFrameTime;
	g_flPlayerIDTraceTime -= flFrameTime;

	if (g_flPlayerIDHoldTime > 0)
	{
		if (g_flPlayerIDAlpha > 0)
			g_flPlayerIDAlpha -= flFrameTime * 510;
		if (g_flPlayerIDAlpha < 0)
			g_flPlayerIDAlpha = 0;
	}
	else
	{
		if (g_flPlayerIDAlpha < 255)
			g_flPlayerIDAlpha += flFrameTime * 255;
		if (g_flPlayerIDAlpha > 255)
			g_flPlayerIDAlpha = 255;
	}

	float* pColor = gViewPort->m_pPlayerIDTeamColor;
	if (!pColor)
	{
		gViewPort->m_pPlayerIDLabel->setFgColor(gViewPort->m_iPlayerIDColor[0], gViewPort->m_iPlayerIDColor[1], gViewPort->m_iPlayerIDColor[2], (int)g_flPlayerIDAlpha);
	}
	else
	{
		gViewPort->m_pPlayerIDLabel->setFgColor((int)(pColor[0] * 255.0), (int)(pColor[1] * 255.0), (int)(pColor[2] * 255.0), (int)g_flPlayerIDAlpha);
	}

	gViewPort->m_pPlayerIDLabel->setVisible(g_flPlayerIDAlpha < 255.0);

	if (g_flPlayerIDTraceTime > 0)
		return;

	if (!g_bPlayerIDListFresh || 0 != g_iUser1)
		return;

	g_bPlayerIDListFresh = false;
	g_flPlayerIDTraceTime = 0.1f;

	float flBestDot = 0.968f;
	int iBest = -1;

	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	if (!pPlayer || 0 != g_iUser1)
		return;

	// VERIFY: gHUD+0xb8 in client.dll, which CL_CreateMove and IN_MouseMove also test for > 0;
	// read as the health
	if (gHUD.m_Health.m_iHealth <= 0)
		return;

	int iLocalAttached = *PM_GetRideInfo(pPlayer->index) & 0x7f;

	Vector vecAngles = v_angles;
	Vector vecEye;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecEye);
	vecEye = vecEye + pPlayer->origin;

	Vector vecForward;
	AngleVectors(vecAngles, vecForward, NULL, NULL);

	Vector vecFlat(vecForward[0], vecForward[1], 0);
	VectorNormalize(vecFlat);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(pPlayer->index - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	for (int i = 0; i < PLAYERID_MAX; i++)
	{
		int iEntity = g_iPlayerIDEnts[i];
		if (iEntity <= 0)
			continue;

		if (0 != gEngfuncs.pEventAPI->EV_IsLocal(iEntity - 1))
			continue;

		// skip what the player is attached to if that's the local player, and what the local
		// player is attached to
		int iAttached = *PM_GetRideInfo(iEntity) & ~0x80;
		if (0 != iAttached && 0 != gEngfuncs.pEventAPI->EV_IsLocal(iAttached - 1))
			continue;

		if (0 != iLocalAttached && iLocalAttached == iEntity)
			continue;

		cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(iEntity);
		if (!pEntity)
			continue;

		// hardly visible
		if (pEntity->curstate.rendermode != kRenderNormal && pEntity->curstate.renderamt < 127)
			continue;

		Vector vecCentre = g_vecPlayerIDCentre[i];
		Vector vecDir = vecCentre - vecEye;
		float flDot;

		if (VectorNormalize(vecDir) < 120 && fabs(vecEye[2] - vecCentre[2]) < 32 && fabs(vecForward[2]) < 0.8)
		{
			// close and level: compare flat
			vecDir[2] = 0;
			VectorNormalize(vecDir);
			flDot = vecDir[2] * vecFlat[2] + vecDir[1] * vecFlat[1] + vecDir[0] * vecFlat[0];
		}
		else
		{
			flDot = vecDir[2] * vecForward[2] + vecDir[1] * vecForward[1] + vecDir[0] * vecForward[0];
		}

		if (flDot < flBestDot)
			continue;

		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecEye, vecCentre, PM_STUDIO_BOX | PM_GLASS_IGNORE, -1, &tr);
		if (tr.fraction < 1.0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) != iEntity)
			continue;

		iBest = iEntity;
		flBestDot = flDot;
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (iBest <= 0)
		return;

	g_flPlayerIDHoldTime = 0.25f;

	if (iBest > MAX_PLAYERS || !g_PlayerInfoList[iBest].name)
		return;

	// the name goes through setText's format, as in the original
	gViewPort->m_pPlayerIDLabel->setText(g_PlayerInfoList[iBest].name);
	gViewPort->m_pPlayerIDTeamColor = GetClientColor(iBest);
}

// orig client 0xd9060. Called from HUD_AddEntity: the first entity since the last pick clears
// the list; each player's centre is kept.
void PlayerID_AddEntity(int type, cl_entity_t* ent)
{
	if (!g_bPlayerIDEnabled)
		return;

	if (!g_bPlayerIDListFresh)
	{
		g_bPlayerIDListFresh = true;
		for (int i = 0; i < PLAYERID_MAX; i++)
			g_iPlayerIDEnts[i] = -1;
	}

	if (type != ET_PLAYER)
		return;

	int i = 0;
	while (g_iPlayerIDEnts[i] != -1 && g_iPlayerIDEnts[i] != ent->index)
	{
		if (++i >= PLAYERID_MAX)
			return;
	}

	g_iPlayerIDEnts[i] = ent->index;

	Vector vecSize = ent->curstate.maxs + ent->curstate.mins;
	VectorMA(ent->origin, 0.5, vecSize, g_vecPlayerIDCentre[i]);
}

// orig client 0xd9150. Called from HUD_VidInit.
void PlayerID_Enable()
{
	g_bPlayerIDListFresh = false;
	g_bPlayerIDEnabled = true;
}

// orig client 0xd9180
void PlayerID_Disable()
{
	g_bPlayerIDListFresh = true;
	g_bPlayerIDEnabled = false;
}

