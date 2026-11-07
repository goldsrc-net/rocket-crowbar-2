//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_SpectatorPanel.h"
#include "hltv.h"

#include "pm_shared.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "parsemsg.h"
#include "entity_types.h"

// these are included for the math functions
#include "com_model.h"
#include "demo_api.h"
#include "event_api.h"
#include "studio_util.h"
#include "screenfade.h"


#pragma warning(disable : 4244)

extern bool iJumpSpectator;
extern float vJumpOrigin[3];
extern float vJumpAngles[3];


extern void V_GetInEyePos(int entity, float* origin, float* angles);
extern void V_ResetChaseCam();
extern void V_GetChasePos(int target, float* cl_angles, float* origin, float* angles);
extern float* GetClientColor(int clientIndex);

extern Vector v_origin;	   // last view origin
extern Vector v_angles;	   // last view angle
extern Vector v_cl_angles; // last client/mouse angle
extern Vector v_sim_org;   // last sim origin

#if 0 
const char *GetSpectatorLabel ( int iMode )
{
	switch ( iMode )
	{
		case OBS_CHASE_LOCKED:
			return "#OBS_CHASE_LOCKED";

		case OBS_CHASE_FREE:
			return "#OBS_CHASE_FREE";

		case OBS_ROAMING:
			return "#OBS_ROAMING";
		
		case OBS_IN_EYE:
			return "#OBS_IN_EYE";

		case OBS_MAP_FREE:
			return "#OBS_MAP_FREE";

		case OBS_MAP_CHASE:
			return "#OBS_MAP_CHASE";

		case OBS_NONE:
		default:
			return "#OBS_NONE";
	}
}

#endif

// orig client 0xa62d0 (not in files-client.tsv: Ghidra missed it). RC2's "specmode": HLTV only,
// sets the main mode and turns the inset off
void SpectatorMode()
{
	if (0 == gEngfuncs.IsSpectateOnly())
	{
		gEngfuncs.Con_Printf("specmode only in HLTV mode available.\n");
		return;
	}

	if (gEngfuncs.Cmd_Argc() <= 1)
	{
		gEngfuncs.Con_Printf("usage:  specmode < modenumber >\n");
		return;
	}

	gHUD.m_Spectator.SetModes(atoi(gEngfuncs.Cmd_Argv(1)), 0);
}

void SpectatorSpray()
{
	Vector forward;
	char string[128];

	if (0 == gEngfuncs.IsSpectateOnly())
		return;

	AngleVectors(v_angles, forward, NULL, NULL);
	VectorScale(forward, 128, forward);
	VectorAdd(forward, v_origin, forward);
	pmtrace_t* trace = gEngfuncs.PM_TraceLine(v_origin, forward, PM_TRACELINE_PHYSENTSONLY, 2, -1);
	if (trace->fraction != 1.0)
	{
		sprintf(string, "drc_spray %.2f %.2f %.2f %i",
			trace->endpos[0], trace->endpos[1], trace->endpos[2], trace->ent);
		gEngfuncs.pfnServerCmd(string);
	}
}
void SpectatorHelp()
{
	if (gViewPort)
	{
		gViewPort->ShowVGUIMenu(MENU_SPECHELP);
	}
	else
	{
		char* text = CHudTextMessage::BufferedLocaliseTextString("#Spec_Help_Text");

		if (text)
		{
			while ('\0' != *text)
			{
				if (*text != 13)
					gEngfuncs.Con_Printf("%c", *text);
				text++;
			}
		}
	}
}

void SpectatorMenu()
{
	if (gEngfuncs.Cmd_Argc() <= 1)
	{
		gEngfuncs.Con_Printf("usage:  spec_menu <0|1>\n");
		return;
	}

	gViewPort->m_pSpectatorPanel->ShowMenu(atoi(gEngfuncs.Cmd_Argv(1)) != 0);
}

void ToggleScores()
{
	if (gViewPort)
	{
		if (gViewPort->IsScoreBoardVisible())
		{
			gViewPort->HideScoreBoard();
		}
		else
		{
			gViewPort->ShowScoreBoard();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
// orig client 0xa6350. RC2: its own modes, "cl_drawnames" and "specmode" only. The
// halflife-updated cvars the rest of this tree's spectator code reads (spec_pip and the others)
// are still registered so that code doesn't read through NULL; RC2 has none of them.
bool CHudSpectator::Init()
{
	gHUD.AddHudElem(this);

	m_iFlags |= HUD_ACTIVE;
	m_flNextObserverInput = 0.0f;
	m_iTrackedPlayer = 0;
	m_zoomDelta = 0.0f;
	m_moveDelta = 0.0f;
	m_FOV = 90.0f;
	m_chatEnabled = true;
	iJumpSpectator = false;

	memset(&m_OverviewData, 0, sizeof(m_OverviewData));
	memset(&m_OverviewEntities, 0, sizeof(m_OverviewEntities));
	m_iMainMode = 0;
	m_iInsetMode = 0;

	m_drawnames = gEngfuncs.pfnRegisterVariable("cl_drawnames", "1", FCVAR_ARCHIVE);
	gEngfuncs.pfnAddCommand("specmode", SpectatorMode);

	// halflife-updated's, VERIFY: not RC2's (see above)
	m_drawcone = gEngfuncs.pfnRegisterVariable("spec_drawcone", "1", 0);
	m_drawstatus = gEngfuncs.pfnRegisterVariable("spec_drawstatus", "1", 0);
	m_autoDirector = gEngfuncs.pfnRegisterVariable("spec_autodirector", "1", 0);
	m_pip = gEngfuncs.pfnRegisterVariable("spec_pip", "1", 0);

	return true;
}


//-----------------------------------------------------------------------------
// UTIL_StringToVector originally from ..\dlls\util.cpp, slightly changed
//-----------------------------------------------------------------------------

void UTIL_StringToVector(float* pVector, const char* pString)
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy(tempString, pString, sizeof(tempString));
	tempString[sizeof(tempString) - 1] = '\0';
	pstr = pfront = tempString;

	for (j = 0; j < 3; j++)
	{
		pVector[j] = atof(pfront);

		while ('\0' != *pstr && *pstr != ' ')
			pstr++;
		if ('\0' == *pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	if (j < 2)
	{
		for (j = j + 1; j < 3; j++)
			pVector[j] = 0;
	}
}

// orig client 0xa64e0 (not in files-client.tsv: Ghidra missed it). VERIFY: an older shape than
// SDK 2.3's; halflife-updated's body kept
bool UTIL_FindEntityInMap(const char* name, float* origin, float* angle)
{
	int n;
	bool found = false;
	char keyname[256];
	char token[1024];

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(0); // get world model

	if (!pEnt)
		return false;

	if (!pEnt->model)
		return false;

	char* data = pEnt->model->entities;

	while (data)
	{
		data = gEngfuncs.COM_ParseFile(data, token);

		if ((token[0] == '}') || (token[0] == 0))
			break;

		if (!data)
		{
			gEngfuncs.Con_DPrintf("UTIL_FindEntityInMap: EOF without closing brace\n");
			return false;
		}

		if (token[0] != '{')
		{
			gEngfuncs.Con_DPrintf("UTIL_FindEntityInMap: expected {\n");
			return false;
		}

		// we parse the first { now parse entities properties

		while (true)
		{
			// parse key
			data = gEngfuncs.COM_ParseFile(data, token);
			if (token[0] == '}')
				break; // finish parsing this entity

			if (!data)
			{
				gEngfuncs.Con_DPrintf("UTIL_FindEntityInMap: EOF without closing brace\n");
				return false;
			}

			strcpy(keyname, token);

			// another hack to fix keynames with trailing spaces
			n = strlen(keyname);
			while (0 != n && keyname[n - 1] == ' ')
			{
				keyname[n - 1] = 0;
				n--;
			}

			// parse value
			data = gEngfuncs.COM_ParseFile(data, token);
			if (!data)
			{
				gEngfuncs.Con_DPrintf("UTIL_FindEntityInMap: EOF without closing brace\n");
				return false;
			}

			if (token[0] == '}')
			{
				gEngfuncs.Con_DPrintf("UTIL_FindEntityInMap: closing brace without data");
				return false;
			}

			if (0 == strcmp(keyname, "classname"))
			{
				if (0 == strcmp(token, name))
				{
					found = true; // thats our entity
				}
			}

			if (0 == strcmp(keyname, "angle"))
			{
				float y = atof(token);

				if (y >= 0)
				{
					angle[0] = 0.0f;
					angle[1] = y;
				}
				else if ((int)y == -1)
				{
					angle[0] = -90.0f;
					angle[1] = 0.0f;
				}
				else
				{
					angle[0] = 90.0f;
					angle[1] = 0.0f;
				}

				angle[2] = 0.0f;
			}

			if (0 == strcmp(keyname, "angles"))
			{
				UTIL_StringToVector(angle, token);
			}

			if (0 == strcmp(keyname, "origin"))
			{
				UTIL_StringToVector(origin, token);
			}
		} // while (1)

		if (found)
			return true;
	}

	return false; // we search all entities, but didn't found the correct
}

//-----------------------------------------------------------------------------
// SetSpectatorStartPosition():
// Get valid map position and 'beam' spectator to this position
//-----------------------------------------------------------------------------

void CHudSpectator::SetSpectatorStartPosition()
{
	// search for info_player start
	if (UTIL_FindEntityInMap("trigger_camera", m_cameraOrigin, m_cameraAngles))
		iJumpSpectator = true;

	else if (UTIL_FindEntityInMap("info_player_start", m_cameraOrigin, m_cameraAngles))
		iJumpSpectator = true;

	else if (UTIL_FindEntityInMap("info_player_deathmatch", m_cameraOrigin, m_cameraAngles))
		iJumpSpectator = true;

	else if (UTIL_FindEntityInMap("info_player_coop", m_cameraOrigin, m_cameraAngles))
		iJumpSpectator = true;
	else
	{
		// jump to 0,0,0 if no better position was found
		VectorCopy(vec3_origin, m_cameraOrigin);
		VectorCopy(vec3_origin, m_cameraAngles);
	}

	VectorCopy(m_cameraOrigin, vJumpOrigin);
	VectorCopy(m_cameraAngles, vJumpAngles);

	iJumpSpectator = true; // jump anyway
}


void CHudSpectator::SetCameraView(Vector pos, Vector angle, float fov)
{
	m_FOV = fov;
	VectorCopy(pos, vJumpOrigin);
	VectorCopy(angle, vJumpAngles);
	gEngfuncs.SetViewAngles(vJumpAngles);
	iJumpSpectator = true; // jump anyway
}

void CHudSpectator::AddWaypoint(float time, Vector pos, Vector angle, float fov, int flags)
{
	//TODO: this flags check is incorrect, fix it. Comment contains original code before bool fix.
	if (/*!flags == 0*/ flags == 0 && time == 0.0f)
	{
		// switch instantly to this camera view
		SetCameraView(pos, angle, fov);
		return;
	}

	if (m_NumWayPoints >= MAX_CAM_WAYPOINTS)
	{
		gEngfuncs.Con_Printf("Too many camera waypoints!\n");
		return;
	}

	VectorCopy(angle, m_CamPath[m_NumWayPoints].angle);
	VectorCopy(pos, m_CamPath[m_NumWayPoints].position);
	m_CamPath[m_NumWayPoints].flags = flags;
	m_CamPath[m_NumWayPoints].fov = fov;
	m_CamPath[m_NumWayPoints].time = time;

	gEngfuncs.Con_DPrintf("Added waypoint %i\n", m_NumWayPoints);

	m_NumWayPoints++;
}

void CHudSpectator::SetWayInterpolation(cameraWayPoint_t* prev, cameraWayPoint_t* start, cameraWayPoint_t* end, cameraWayPoint_t* next)
{
	m_WayInterpolation.SetViewAngles(start->angle, end->angle);

	m_WayInterpolation.SetFOVs(start->fov, end->fov);

	m_WayInterpolation.SetSmoothing((start->flags & DRC_FLAG_SLOWSTART) != 0,
		(start->flags & DRC_FLAG_SLOWEND) != 0);

	if (prev && next)
	{
		m_WayInterpolation.SetWaypoints(&prev->position, start->position, end->position, &next->position);
	}
	else if (prev)
	{
		m_WayInterpolation.SetWaypoints(&prev->position, start->position, end->position, NULL);
	}
	else if (next)
	{
		m_WayInterpolation.SetWaypoints(NULL, start->position, end->position, &next->position);
	}
	else
	{
		m_WayInterpolation.SetWaypoints(NULL, start->position, end->position, NULL);
	}
}

bool CHudSpectator::GetDirectorCamera(Vector& position, Vector& angle)
{
	float now = gHUD.m_flTime;
	float fov = 90.0f;

	if (0 != m_ChaseEntity)
	{
		cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_ChaseEntity);

		if (ent)
		{
			Vector vt = ent->curstate.origin;

			if (m_ChaseEntity <= gEngfuncs.GetMaxClients())
			{
				if (ent->curstate.solid == SOLID_NOT)
				{
					vt = vt + VEC_DEAD_VIEW;
				}
				else if (ent->curstate.usehull == 1)
				{
					vt = vt + VEC_DUCK_VIEW;
				}
				else
				{
					vt = vt + VEC_VIEW;
				}
			}

			vt = vt - position;
			VectorAngles(vt, angle);
			angle[0] = -angle[0];
			return true;
		}
		else
		{
			return false;
		}
	}

	if (!m_IsInterpolating)
		return false;

	if (m_WayPoint < 0 || m_WayPoint >= (m_NumWayPoints - 1))
		return false;

	cameraWayPoint_t* wp1 = &m_CamPath[m_WayPoint];
	cameraWayPoint_t* wp2 = &m_CamPath[m_WayPoint + 1];

	if (now < wp1->time)
		return false;

	while (now > wp2->time)
	{
		// go to next waypoint, if possible
		m_WayPoint++;

		if (m_WayPoint >= (m_NumWayPoints - 1))
		{
			m_IsInterpolating = false;
			return false; // there is no following waypoint
		}

		wp1 = wp2;
		wp2 = &m_CamPath[m_WayPoint + 1];

		if (m_WayPoint > 0)
		{
			// we have a predecessor

			if (m_WayPoint < (m_NumWayPoints - 1))
			{
				// we have also a successor
				SetWayInterpolation(&m_CamPath[m_WayPoint - 1], wp1, wp2, &m_CamPath[m_WayPoint + 2]);
			}
			else
			{
				SetWayInterpolation(&m_CamPath[m_WayPoint - 1], wp1, wp2, NULL);
			}
		}
		else if (m_WayPoint < (m_NumWayPoints - 1))
		{
			// we only have a successor
			SetWayInterpolation(NULL, wp1, wp2, &m_CamPath[m_WayPoint + 2]);
		}
		else
		{
			// we have only two waypoints
			SetWayInterpolation(NULL, wp1, wp2, NULL);
		}
	}

	if (wp2->time <= wp1->time)
		return false;

	float fraction = (now - wp1->time) / (wp2->time - wp1->time);

	if (fraction < 0.0f)
		fraction = 0.0f;
	else if (fraction > 1.0f)
		fraction = 1.0f;

	m_WayInterpolation.Interpolate(fraction, position, angle, &fov);

	// gEngfuncs.Con_Printf("Interpolate time: %.2f, fraction %.2f, point : %.2f,%.2f,%.2f\n", now, fraction, position[0], position[1], position[2] );

	SetCameraView(position, angle, fov);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Loads new icons
//-----------------------------------------------------------------------------
// orig client 0xa67b0. Not byte-identical to SDK 2.3's, but it reads the same
bool CHudSpectator::VidInit()
{
	m_hsprPlayer = SPR_Load("sprites/iplayer.spr");
	m_hsprPlayerBlue = SPR_Load("sprites/iplayerblue.spr");
	m_hsprPlayerRed = SPR_Load("sprites/iplayerred.spr");
	m_hsprPlayerDead = SPR_Load("sprites/iplayerdead.spr");
	m_hsprUnkownMap = SPR_Load("sprites/tile.spr");
	m_hsprBeam = SPR_Load("sprites/laserbeam.spr");
	m_hsprCamera = SPR_Load("sprites/camera.spr");
	m_hCrosshair = SPR_Load("sprites/crosshairs.spr");

	m_lastPrimaryObject = m_lastSecondaryObject = 0;
	m_flNextObserverInput = 0.0f;
	m_lastHudMessage = 0;
	m_iSpectatorNumber = 0;
	iJumpSpectator = false;
	g_iUser1 = g_iUser2 = 0;

	return true;
}

float CHudSpectator::GetFOV()
{
	return m_FOV;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : flTime -
//			intermission -
//-----------------------------------------------------------------------------
// orig client 0xa6880 (not in files-client.tsv: Ghidra missed it). RC2: HLTV only, by its own
// modes; map zoom and move unless roaming; names in map mode in plain team colours
bool CHudSpectator::Draw(float flTime)
{
	char string[256];

	if (gEngfuncs.IsSpectateOnly() != 1)
		return true;

	// if user pressed zoom, aplly changes
	if ((m_zoomDelta != 0.0f) && (m_iMainMode != MAIN_MODE_ROAMING))
	{
		m_mapZoom += m_zoomDelta;

		if (m_mapZoom > 3.0f)
			m_mapZoom = 3.0f;

		if (m_mapZoom < 0.5f)
			m_mapZoom = 0.5f;
	}

	// if user moves in map mode, change map origin
	if ((m_moveDelta != 0.0f) && (m_iMainMode != MAIN_MODE_ROAMING))
	{
		Vector right;
		gEngfuncs.pfnAngleVectors(m_mapAngles, NULL, right, NULL);
		VectorNormalize(right);
		VectorScale(right, m_moveDelta, right);

		VectorAdd(m_mapOrigin, right, m_mapOrigin)
	}

	// Only draw the icon names only if map mode is in Main Mode
	if (m_iMainMode != MAIN_MODE_MAP)
		return true;

	if (0 == m_drawnames->value)
		return true;

	// make sure we have player info
	gViewPort->GetAllPlayersInfo();

	// loop through all the players and draw additional infos to their sprites on the map
	for (int i = 0; i < MAX_PLAYERS_HUD; i++)
	{
		if (m_vPlayerPos[i][2] < 0) // marked as invisible ?
			continue;

		// check if name would be in inset window
		if (m_iInsetMode != INSET_MODE_OFF)
		{
			if (m_vPlayerPos[i][0] > (int)(XRES(m_OverviewData.insetWindowX) + 0.5f) &&
				m_vPlayerPos[i][1] > (int)(YRES(m_OverviewData.insetWindowY) + 0.5f) &&
				m_vPlayerPos[i][0] <= (int)(XRES(m_OverviewData.insetWindowX + m_OverviewData.insetWindowWidth) + 0.5f) &&
				m_vPlayerPos[i][1] <= (int)(YRES(m_OverviewData.insetWindowY + m_OverviewData.insetWindowHeight) + 0.5f))
				continue;
		}

		float r, g, b;
		switch (g_PlayerExtraInfo[i + 1].teamnumber)
		{
		case 1:
			r = 0.4f, g = 0.4f, b = 1.0f;
			break;
		case 2:
			r = 1.0f, g = 0.4f, b = 0.4f;
			break;
		default:
			r = g = b = 0.8f;
			break;
		}

		// draw the players name and health underneath
		sprintf(string, "%s", g_PlayerInfoList[i + 1].name);

		int lx = strlen(string) * 3; // 3 is avg. character length :)

		gEngfuncs.pfnDrawSetTextColor(r, g, b);
		DrawConsoleString((int)(m_vPlayerPos[i][0] - lx), (int)m_vPlayerPos[i][1], string);
	}

	return true;
}


// orig client 0xa6cc0. RC2: only the start (a fake InitHUD and ResetHUD; the team is cleared, the
// playerclass, RC2's transform, isn't) and the event (the last objects, followed by the eye)
void CHudSpectator::DirectorMessage(int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int cmd = READ_BYTE();

	switch (cmd)
	{
	case DRC_CMD_START:
		g_iTeamNumber = 0;

		// fake a InitHUD & ResetHUD message
		gHUD.MsgFunc_InitHUD(NULL, 0, NULL);
		gHUD.MsgFunc_ResetHUD(NULL, 0, NULL);
		break;

	case DRC_CMD_EVENT:
		m_lastPrimaryObject = READ_SHORT();
		m_lastSecondaryObject = READ_SHORT();
		g_iUser2 = m_lastPrimaryObject;
		g_iUser3 = m_lastSecondaryObject;
		break;

	default:
		gEngfuncs.Con_DPrintf("CHudSpectator::DirectorMessage: unknown command %i.\n", cmd);
	}
}

// orig client 0xa6d90. RC2: the client keeps the target itself (m_iTrackedPlayer), says whom it
// tracks, and never asks the server
void CHudSpectator::FindNextPlayer(bool bReverse)
{
	int iStart = 0 != m_iTrackedPlayer ? m_iTrackedPlayer : 1;
	m_iTrackedPlayer = 0;

	// make sure we have player info
	gViewPort->GetAllPlayersInfo();

	int iCurrent = iStart;
	int iDir = bReverse ? -1 : 1;

	while (true)
	{
		iCurrent += iDir;

		// Loop through the clients
		if (iCurrent > MAX_PLAYERS_HUD)
			iCurrent = 1;
		else if (iCurrent < 1)
			iCurrent = MAX_PLAYERS_HUD;

		if (IsActivePlayer(gEngfuncs.GetEntityByIndex(iCurrent)))
		{
			m_iTrackedPlayer = iCurrent;
			break;
		}

		if (iCurrent == iStart)
			break;
	}

	if (0 == m_iTrackedPlayer)
	{
		gEngfuncs.Con_Printf("No observer targets.\n");
		return;
	}

	if (m_iMainMode != MAIN_MODE_ROAMING)
	{
		g_iUser3 = 0;
		g_iUser2 = m_iTrackedPlayer;
	}

	gEngfuncs.Con_Printf("Now Tracking %s\n", g_PlayerInfoList[m_iTrackedPlayer].name);
}


void CHudSpectator::FindPlayer(const char* name)
{
	// MOD AUTHORS: Modify the logic of this function if you want to restrict the observer to watching
	//				only a subset of the players. e.g. Make it check the target's team.

	// if we are NOT in HLTV mode, spectator targets are set on server
	if (0 == gEngfuncs.IsSpectateOnly())
	{
		char cmdstring[32];
		// forward command to server
		sprintf(cmdstring, "follow %s", name);
		gEngfuncs.pfnServerCmd(cmdstring);
		return;
	}

	g_iUser2 = 0;

	// make sure we have player info
	gViewPort->GetAllPlayersInfo();

	cl_entity_t* pEnt = NULL;

	for (int i = 1; i < MAX_PLAYERS_HUD; i++)
	{

		pEnt = gEngfuncs.GetEntityByIndex(i);

		if (!IsActivePlayer(pEnt))
			continue;

		if (!stricmp(g_PlayerInfoList[pEnt->index].name, name))
		{
			g_iUser2 = i;
			break;
		}
	}

	// Did we find a target?
	if (0 == g_iUser2)
	{
		gEngfuncs.Con_DPrintf("No observer targets.\n");
		// take save camera position
		VectorCopy(m_cameraOrigin, vJumpOrigin);
		VectorCopy(m_cameraAngles, vJumpAngles);
	}
	else
	{
		// use new entity position for roaming
		VectorCopy(pEnt->origin, vJumpOrigin);
		VectorCopy(pEnt->angles, vJumpAngles);
	}

	iJumpSpectator = true;
	gViewPort->MsgFunc_ResetFade(NULL, 0, NULL);
}

// orig client 0xa6ea0. RC2: HLTV only, 0.2 s between presses: jump steps the main mode (2-5),
// duck the inset (0-3), attack2 (or 0x40, back) the target, forward/back zoom, the strafes move
void CHudSpectator::HandleButtonsDown(int ButtonPressed)
{
	double time = gEngfuncs.GetClientTime();

	int newMainMode = m_iMainMode;
	int newInsetMode = m_iInsetMode;

	if (0 == g_iUser1)
		return; // dont do anything if not in spectator mode

	// Slow down mouse clicks.
	if (m_flNextObserverInput > time)
		return;

	if (gEngfuncs.IsSpectateOnly() != 1)
		return;

	if ((ButtonPressed & IN_JUMP) != 0)
	{
		newMainMode++;
		if (newMainMode > MAIN_MODE_MAP)
			newMainMode = MAIN_MODE_CHASE;
	}

	if ((ButtonPressed & IN_DUCK) != 0)
	{
		newInsetMode++;
		if (newInsetMode > INSET_MODE_MAP)
			newInsetMode = INSET_MODE_OFF;
	}

	// VERIFY: RC2 tests 0x840 (IN_ATTACK2 | IN_CANCEL) and goes back on 0x40
	if ((ButtonPressed & (IN_ATTACK2 | IN_CANCEL)) != 0)
	{
		g_iUser1 = OBS_CHASE_FREE;
		FindNextPlayer((ButtonPressed & IN_CANCEL) != 0);

		if (m_iMainMode == MAIN_MODE_ROAMING && 0 != m_iTrackedPlayer)
		{
			cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iTrackedPlayer);
			VectorCopy(pEnt->origin, vJumpOrigin);
			VectorCopy(pEnt->angles, vJumpAngles);
			gEngfuncs.SetViewAngles(vJumpAngles);
			iJumpSpectator = true;
		}
	}

	if ((ButtonPressed & IN_FORWARD) != 0)
		m_zoomDelta = 0.01f;

	if ((ButtonPressed & IN_BACK) != 0)
		m_zoomDelta = -0.01f;

	if ((ButtonPressed & IN_MOVELEFT) != 0)
		m_moveDelta = -12.0f;

	if ((ButtonPressed & IN_MOVERIGHT) != 0)
		m_moveDelta = 12.0f;

	if (newMainMode != m_iMainMode || newInsetMode != m_iInsetMode)
		SetModes(newMainMode, newInsetMode);

	m_flNextObserverInput = time + 0.2;
}

// orig client 0xa7070. RC2: HLTV only
void CHudSpectator::HandleButtonsUp(int ButtonPressed)
{
	if (gEngfuncs.IsSpectateOnly() != 1)
		return;

	if ((ButtonPressed & (IN_FORWARD | IN_BACK)) != 0)
		m_zoomDelta = 0.0f;

	if ((ButtonPressed & (IN_MOVELEFT | IN_MOVERIGHT)) != 0)
		m_moveDelta = 0.0f;
}

// orig client 0xa70b0. RC2: its own main modes 1-5 and insets 0-3 (the engine's observer mode
// follows: in eye for chase, in eye and map, roaming for roaming), the crosshair in roaming and
// in eye; the map inset only outside map mode, the others only in it
void CHudSpectator::SetModes(int iNewMainMode, int iNewInsetMode)
{
	char string[32];

	if (iNewMainMode < MAIN_MODE_1 || iNewMainMode > MAIN_MODE_MAP)
	{
		gEngfuncs.Con_Printf("Invalid spectator mode.\n");
		return;
	}

	if (iNewInsetMode != m_iInsetMode)
	{
		m_iInsetMode = iNewInsetMode;
		sprintf(string, "%c#Spec_Mode_Inset%d", HUD_PRINTCENTER, iNewInsetMode);
		gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, strlen(string) + 1, string);
	}

	if (iNewMainMode != m_iMainMode)
	{
		switch (iNewMainMode)
		{
		case MAIN_MODE_CHASE:
		case MAIN_MODE_INEYE:
			g_iUser1 = OBS_IN_EYE;
			break;

		case MAIN_MODE_ROAMING:
			g_iUser1 = OBS_ROAMING;

			if (m_iMainMode == MAIN_MODE_CHASE)
			{
				VectorCopy(v_origin, vJumpOrigin);
				VectorCopy(v_angles, vJumpAngles);
				gEngfuncs.SetViewAngles(vJumpAngles);
				iJumpSpectator = true;
			}

			if (m_iMainMode == MAIN_MODE_INEYE)
			{
				V_GetInEyePos(m_iTrackedPlayer, vJumpOrigin, vJumpAngles);
				gEngfuncs.SetViewAngles(vJumpAngles);
				iJumpSpectator = true;
			}

			m_iTrackedPlayer = 0;
			g_iUser2 = 0;
			g_iUser3 = 0;
			break;

		case MAIN_MODE_MAP:
			g_iUser1 = OBS_IN_EYE;
			m_mapZoom = m_OverviewData.zoom;
			m_mapOrigin = m_OverviewData.origin;
			break;
		}

		m_iMainMode = iNewMainMode;

		sprintf(string, "%c#Spec_Mode%d", HUD_PRINTCENTER, iNewMainMode);
		gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, strlen(string) + 1, string);

		if (g_iUser1 == OBS_IN_EYE && 0 == m_iTrackedPlayer)
		{
			if (IsActivePlayer(gEngfuncs.GetEntityByIndex(m_lastPrimaryObject)))
			{
				g_iUser2 = m_lastPrimaryObject;
				m_iTrackedPlayer = g_iUser2;
				g_iUser3 = m_lastSecondaryObject;
			}
		}

		if (m_iMainMode == MAIN_MODE_ROAMING || m_iMainMode == MAIN_MODE_INEYE)
		{
			m_crosshairRect.left = 24;
			m_crosshairRect.top = 0;
			m_crosshairRect.right = 48;
			m_crosshairRect.bottom = 24;
			SetCrosshair(m_hCrosshair, m_crosshairRect, 255, 255, 255);
		}
		else
		{
			memset(&m_crosshairRect, 0, sizeof(m_crosshairRect));
			SetCrosshair(0, m_crosshairRect, 0, 0, 0);
		}
	}

	if (m_iMainMode != MAIN_MODE_MAP && (m_iInsetMode == INSET_MODE_CHASE || m_iInsetMode == INSET_MODE_INEYE))
		SetModes(m_iMainMode, INSET_MODE_MAP);

	if (m_iMainMode == MAIN_MODE_MAP && m_iInsetMode == INSET_MODE_MAP)
		SetModes(MAIN_MODE_MAP, INSET_MODE_OFF);

	gViewPort->UpdateSpectatorPanel(); // RC2: no check for the viewport
}

bool CHudSpectator::IsActivePlayer(cl_entity_t* ent)
{
	return (nullptr != ent &&
			0 != ent->player &&
			ent->curstate.solid != SOLID_NOT &&
			ent != gEngfuncs.GetLocalPlayer() &&
			g_PlayerInfoList[ent->index].name != NULL);
}


// orig client 0xa74d0 (not in files-client.tsv: Ghidra missed it). VERIFY: close to SDK 2.3's;
// halflife-updated's body kept, not diffed line by line
bool CHudSpectator::ParseOverviewFile()
{
	char filename[255];
	char levelname[255];
	char token[1024];
	float height;

	char* pfile = NULL;

	memset(&m_OverviewData, 0, sizeof(m_OverviewData));

	// fill in standrd values
	m_OverviewData.insetWindowX = 4; // upper left corner
	m_OverviewData.insetWindowY = 4;
	m_OverviewData.insetWindowHeight = 180;
	m_OverviewData.insetWindowWidth = 240;
	m_OverviewData.origin[0] = 0.0f;
	m_OverviewData.origin[1] = 0.0f;
	m_OverviewData.origin[2] = 0.0f;
	m_OverviewData.zoom = 1.0f;
	m_OverviewData.layers = 0;
	m_OverviewData.layersHeights[0] = 0.0f;
	strcpy(m_OverviewData.map, gEngfuncs.pfnGetLevelName());

	if (strlen(m_OverviewData.map) == 0)
		return false; // not active yet

	strcpy(levelname, m_OverviewData.map + 5);
	levelname[strlen(levelname) - 4] = 0;

	sprintf(filename, "overviews/%s.txt", levelname);

	pfile = (char*)gEngfuncs.COM_LoadFile(filename, 5, NULL);

	if (!pfile)
	{
		gEngfuncs.Con_DPrintf("Couldn't open file %s. Using default values for overiew mode.\n", filename);
		return false;
	}


	while (true)
	{
		pfile = gEngfuncs.COM_ParseFile(pfile, token);

		if (!pfile)
			break;

		if (!stricmp(token, "global"))
		{
			// parse the global data
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			if (stricmp(token, "{"))
			{
				gEngfuncs.Con_Printf("Error parsing overview file %s. (expected { )\n", filename);
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile(pfile, token);

			while (stricmp(token, "}"))
			{
				if (!stricmp(token, "zoom"))
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.zoom = atof(token);
				}
				else if (!stricmp(token, "origin"))
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.origin[0] = atof(token);
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.origin[1] = atof(token);
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.origin[2] = atof(token);
				}
				else if (!stricmp(token, "rotated"))
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.rotated = atoi(token) != 0;
				}
				else if (!stricmp(token, "inset"))
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.insetWindowX = atof(token);
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.insetWindowY = atof(token);
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.insetWindowWidth = atof(token);
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					m_OverviewData.insetWindowHeight = atof(token);
				}
				else
				{
					gEngfuncs.Con_Printf("Error parsing overview file %s. (%s unkown)\n", filename, token);
					return false;
				}

				pfile = gEngfuncs.COM_ParseFile(pfile, token); // parse next token
			}
		}
		else if (!stricmp(token, "layer"))
		{
			// parse a layer data

			if (m_OverviewData.layers == OVERVIEW_MAX_LAYERS)
			{
				gEngfuncs.Con_Printf("Error parsing overview file %s. ( too many layers )\n", filename);
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile(pfile, token);


			if (stricmp(token, "{"))
			{
				gEngfuncs.Con_Printf("Error parsing overview file %s. (expected { )\n", filename);
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile(pfile, token);

			while (stricmp(token, "}"))
			{
				if (!stricmp(token, "image"))
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					strcpy(m_OverviewData.layersImages[m_OverviewData.layers], token);
				}
				else if (!stricmp(token, "height"))
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token);
					height = atof(token);
					m_OverviewData.layersHeights[m_OverviewData.layers] = height;
				}
				else
				{
					gEngfuncs.Con_Printf("Error parsing overview file %s. (%s unkown)\n", filename, token);
					return false;
				}

				pfile = gEngfuncs.COM_ParseFile(pfile, token); // parse next token
			}

			m_OverviewData.layers++;
		}
	}

	gEngfuncs.COM_FreeFile(pfile);

	m_mapZoom = m_OverviewData.zoom;
	m_mapOrigin = m_OverviewData.origin;

	return true;
}

// orig client 0xa7b60. Not byte-identical to SDK 2.3's, but it reads the same
void CHudSpectator::LoadMapSprites()
{
	// right now only support for one map layer
	if (m_OverviewData.layers > 0)
	{
		m_MapSprite = gEngfuncs.LoadMapSprite(m_OverviewData.layersImages[0]);
	}
	else
		m_MapSprite = NULL; // the standard "unkown map" sprite will be used instead
}

// orig client 0xa7ba0. VERIFY: SDK 2.3's instruction for instruction except that the view pitch is
// the spectator's own m_mapAngles; halflife-updated's body kept (it reads v_angles)
void CHudSpectator::DrawOverviewLayer()
{
	float screenaspect, xs, ys, xStep, yStep, x, y, z;
	int ix, iy, i, xTiles, yTiles, frame;

	bool hasMapImage = nullptr != m_MapSprite;
	model_t* dummySprite = (struct model_s*)gEngfuncs.GetSpritePointer(m_hsprUnkownMap);

	if (hasMapImage)
	{
		i = m_MapSprite->numframes / (4 * 3);
		i = sqrt((float)i);
		xTiles = i * 4;
		yTiles = i * 3;
	}
	else
	{
		xTiles = 8;
		yTiles = 6;
	}


	screenaspect = 4.0f / 3.0f;


	xs = m_OverviewData.origin[0];
	ys = m_OverviewData.origin[1];
	z = (90.0f - v_angles[0]) / 90.0f;
	z *= m_OverviewData.layersHeights[0]; // gOverviewData.z_min - 32;

	// i = r_overviewTexture + ( layer*OVERVIEW_X_TILES*OVERVIEW_Y_TILES );

	gEngfuncs.pTriAPI->RenderMode(kRenderTransTexture);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	gEngfuncs.pTriAPI->Color4f(1.0, 1.0, 1.0, 1.0);

	frame = 0;


	// rotated view ?
	if (m_OverviewData.rotated)
	{
		xStep = (2 * 4096.0f / m_OverviewData.zoom) / xTiles;
		yStep = -(2 * 4096.0f / (m_OverviewData.zoom * screenaspect)) / yTiles;

		y = ys + (4096.0f / (m_OverviewData.zoom * screenaspect));

		for (iy = 0; iy < yTiles; iy++)
		{
			x = xs - (4096.0f / (m_OverviewData.zoom));

			for (ix = 0; ix < xTiles; ix++)
			{
				if (hasMapImage)
					gEngfuncs.pTriAPI->SpriteTexture(m_MapSprite, frame);
				else
					gEngfuncs.pTriAPI->SpriteTexture(dummySprite, 0);

				gEngfuncs.pTriAPI->Begin(TRI_QUADS);
				gEngfuncs.pTriAPI->TexCoord2f(0, 0);
				gEngfuncs.pTriAPI->Vertex3f(x, y, z);

				gEngfuncs.pTriAPI->TexCoord2f(1, 0);
				gEngfuncs.pTriAPI->Vertex3f(x + xStep, y, z);

				gEngfuncs.pTriAPI->TexCoord2f(1, 1);
				gEngfuncs.pTriAPI->Vertex3f(x + xStep, y + yStep, z);

				gEngfuncs.pTriAPI->TexCoord2f(0, 1);
				gEngfuncs.pTriAPI->Vertex3f(x, y + yStep, z);
				gEngfuncs.pTriAPI->End();

				frame++;
				x += xStep;
			}

			y += yStep;
		}
	}
	else
	{
		xStep = -(2 * 4096.0f / m_OverviewData.zoom) / xTiles;
		yStep = -(2 * 4096.0f / (m_OverviewData.zoom * screenaspect)) / yTiles;


		x = xs + (4096.0f / (m_OverviewData.zoom * screenaspect));



		for (ix = 0; ix < yTiles; ix++)
		{

			y = ys + (4096.0f / (m_OverviewData.zoom));

			for (iy = 0; iy < xTiles; iy++)
			{
				if (hasMapImage)
					gEngfuncs.pTriAPI->SpriteTexture(m_MapSprite, frame);
				else
					gEngfuncs.pTriAPI->SpriteTexture(dummySprite, 0);

				gEngfuncs.pTriAPI->Begin(TRI_QUADS);
				gEngfuncs.pTriAPI->TexCoord2f(0, 0);
				gEngfuncs.pTriAPI->Vertex3f(x, y, z);

				gEngfuncs.pTriAPI->TexCoord2f(0, 1);
				gEngfuncs.pTriAPI->Vertex3f(x + xStep, y, z);

				gEngfuncs.pTriAPI->TexCoord2f(1, 1);
				gEngfuncs.pTriAPI->Vertex3f(x + xStep, y + yStep, z);

				gEngfuncs.pTriAPI->TexCoord2f(1, 0);
				gEngfuncs.pTriAPI->Vertex3f(x, y + yStep, z);
				gEngfuncs.pTriAPI->End();

				frame++;

				y += yStep;
			}

			x += xStep;
		}
	}
}

// orig client 0xa81b0. VERIFY: close to SDK 2.3's (0.81 of the instructions); halflife-updated's
// body kept, not diffed line by line
void CHudSpectator::DrawOverviewEntities()
{
	int i, ir, ig, ib;
	struct model_s* hSpriteModel;
	Vector origin, angles, point, forward, right, left, up, world, screen, offset;
	float x, y, z, r, g, b, sizeScale = 4.0f;
	cl_entity_t* ent;
	float rmatrix[3][4]; // transformation matrix

	float zScale = (90.0f - v_angles[0]) / 90.0f;


	z = m_OverviewData.layersHeights[0] * zScale;
	// get yellow/brown HUD color
	UnpackRGB(ir, ig, ib, RGB_YELLOWISH);
	r = (float)ir / 255.0f;
	g = (float)ig / 255.0f;
	b = (float)ib / 255.0f;

	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	for (i = 0; i < MAX_PLAYERS_HUD; i++)
		m_vPlayerPos[i][2] = -1; // mark as invisible

	// draw all players
	for (i = 0; i < MAX_OVERVIEW_ENTITIES; i++)
	{
		if (0 == m_OverviewEntities[i].hSprite)
			continue;

		hSpriteModel = (struct model_s*)gEngfuncs.GetSpritePointer(m_OverviewEntities[i].hSprite);
		ent = m_OverviewEntities[i].entity;

		gEngfuncs.pTriAPI->SpriteTexture(hSpriteModel, 0);
		gEngfuncs.pTriAPI->RenderMode(kRenderTransTexture);

		// see R_DrawSpriteModel
		// draws players sprite

		AngleVectors(ent->angles, right, up, NULL);

		VectorCopy(ent->origin, origin);

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);

		gEngfuncs.pTriAPI->Color4f(1.0, 1.0, 1.0, 1.0);

		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		VectorMA(origin, 16.0f * sizeScale, up, point);
		VectorMA(point, 16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv(point);

		gEngfuncs.pTriAPI->TexCoord2f(0, 0);

		VectorMA(origin, 16.0f * sizeScale, up, point);
		VectorMA(point, -16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv(point);

		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		VectorMA(origin, -16.0f * sizeScale, up, point);
		VectorMA(point, -16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv(point);

		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		VectorMA(origin, -16.0f * sizeScale, up, point);
		VectorMA(point, 16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv(point);

		gEngfuncs.pTriAPI->End();


		if (0 == ent->player)
			continue;
		// draw line under player icons
		origin[2] *= zScale;

		gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);

		hSpriteModel = (struct model_s*)gEngfuncs.GetSpritePointer(m_hsprBeam);
		gEngfuncs.pTriAPI->SpriteTexture(hSpriteModel, 0);

		gEngfuncs.pTriAPI->Color4f(r, g, b, 0.3);

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] + 4, origin[1] + 4, origin[2] - zScale);
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] - 4, origin[1] - 4, origin[2] - zScale);
		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] - 4, origin[1] - 4, z);
		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] + 4, origin[1] + 4, z);
		gEngfuncs.pTriAPI->End();

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] - 4, origin[1] + 4, origin[2] - zScale);
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] + 4, origin[1] - 4, origin[2] - zScale);
		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] + 4, origin[1] - 4, z);
		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		gEngfuncs.pTriAPI->Vertex3f(origin[0] - 4, origin[1] + 4, z);
		gEngfuncs.pTriAPI->End();

		// calculate screen position for name and infromation in hud::draw()
		if (0 != gEngfuncs.pTriAPI->WorldToScreen(origin, screen))
			continue; // object is behind viewer

		screen[0] = XPROJECT(screen[0]);
		screen[1] = YPROJECT(screen[1]);
		screen[2] = 0.0f;

		// calculate some offset under the icon
		origin[0] += 32.0f;
		origin[1] += 32.0f;

		gEngfuncs.pTriAPI->WorldToScreen(origin, offset);

		offset[0] = XPROJECT(offset[0]);
		offset[1] = YPROJECT(offset[1]);
		offset[2] = 0.0f;

		VectorSubtract(offset, screen, offset);

		int playerNum = ent->index - 1;

		m_vPlayerPos[playerNum][0] = screen[0];
		m_vPlayerPos[playerNum][1] = screen[1] + Length(offset);
		m_vPlayerPos[playerNum][2] = 1; // mark player as visible
	}

	if (0 == m_pip->value || 0 == m_drawcone->value)
		return;

	// get current camera position and angle

	if (m_pip->value == INSET_IN_EYE || g_iUser1 == OBS_IN_EYE)
	{
		V_GetInEyePos(g_iUser2, origin, angles);
	}
	else if (m_pip->value == INSET_CHASE_FREE || g_iUser1 == OBS_CHASE_FREE)
	{
		V_GetChasePos(g_iUser2, v_cl_angles, origin, angles);
	}
	else if (g_iUser1 == OBS_ROAMING)
	{
		VectorCopy(v_sim_org, origin);
		VectorCopy(v_cl_angles, angles);
	}
	else
		V_GetChasePos(g_iUser2, NULL, origin, angles);


	// draw camera sprite

	x = origin[0];
	y = origin[1];
	z = origin[2];

	angles[0] = 0; // always show horizontal camera sprite

	hSpriteModel = (struct model_s*)gEngfuncs.GetSpritePointer(m_hsprCamera);
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->SpriteTexture(hSpriteModel, 0);


	gEngfuncs.pTriAPI->Color4f(r, g, b, 1.0);

	AngleVectors(angles, forward, NULL, NULL);
	VectorScale(forward, 512.0f, forward);

	offset[0] = 0.0f;
	offset[1] = 45.0f;
	offset[2] = 0.0f;

	AngleMatrix(offset, rmatrix);
	VectorTransform(forward, rmatrix, right);

	offset[1] = -45.0f;
	AngleMatrix(offset, rmatrix);
	VectorTransform(forward, rmatrix, left);

	gEngfuncs.pTriAPI->Begin(TRI_TRIANGLES);
	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	gEngfuncs.pTriAPI->Vertex3f(x + right[0], y + right[1], (z + right[2]) * zScale);

	gEngfuncs.pTriAPI->TexCoord2f(0, 1);
	gEngfuncs.pTriAPI->Vertex3f(x, y, z * zScale);

	gEngfuncs.pTriAPI->TexCoord2f(1, 1);
	gEngfuncs.pTriAPI->Vertex3f(x + left[0], y + left[1], (z + left[2]) * zScale);
	gEngfuncs.pTriAPI->End();
}



// orig client 0xa8bd0. RC2: HLTV only, by its own modes
void CHudSpectator::DrawOverview()
{
	if (gEngfuncs.IsSpectateOnly() != 1)
		return;

	// Only draw the overview if Map Mode is selected for this view
	if (m_iDrawCycle == 0 && m_iMainMode != MAIN_MODE_MAP)
		return;

	if (m_iDrawCycle == 1 && m_iInsetMode != INSET_MODE_MAP)
		return;

	DrawOverviewLayer();
	DrawOverviewEntities();
	CheckOverviewEntities();
}

// orig client 0xa80d0. SDK 2.3's GetMapPosition, from the spectator's own view angles
void CHudSpectator::GetMapPosition(float* returnvec)
{
	Vector forward;
	Vector pos;

	pos[0] = m_mapOrigin[0];
	pos[1] = m_mapOrigin[1];
	pos[2] = (90.0f - m_mapAngles[0]) / 90.0f * m_mapOrigin[2];

	gEngfuncs.pfnAngleVectors(m_mapAngles, forward, NULL, NULL);
	VectorNormalize(forward);
	VectorScale(forward, -(4096.0f / m_mapZoom), forward);

	returnvec[0] = forward[0] + pos[0];
	returnvec[1] = forward[1] + pos[1];
	returnvec[2] = forward[2] + pos[2];
}
// orig client 0xa8c20. Not byte-identical to SDK 2.3's, but it reads the same
void CHudSpectator::CheckOverviewEntities()
{
	double time = gEngfuncs.GetClientTime();

	// removes old entities from list
	for (int i = 0; i < MAX_OVERVIEW_ENTITIES; i++)
	{
		// remove entity from list if it is too old
		if (m_OverviewEntities[i].killTime < time)
		{
			memset(&m_OverviewEntities[i], 0, sizeof(overviewEntity_t));
		}
	}
}

// orig client 0xa8c70. Not byte-identical to SDK 2.3's, but it reads the same
bool CHudSpectator::AddOverviewEntity(int type, struct cl_entity_s* ent, const char* modelname)
{
	HSPRITE hSprite = 0;
	double duration = -1.0f; // duration -1 means show it only this frame;

	if (!ent)
		return false;

	if (type == ET_PLAYER)
	{
		if (ent->curstate.solid != SOLID_NOT)
		{
			switch (g_PlayerExtraInfo[ent->index].teamnumber)
			{
			// blue and red teams are swapped in CS and TFC
			case 1:
				hSprite = m_hsprPlayerBlue;
				break;
			case 2:
				hSprite = m_hsprPlayerRed;
				break;
			default:
				hSprite = m_hsprPlayer;
				break;
			}
		}
		else
			return false; // it's an spectator
	}
	else if (type == ET_NORMAL)
	{
		return false;
	}
	else
		return false;

	return AddOverviewEntityToList(hSprite, ent, gEngfuncs.GetClientTime() + duration);
}

// orig client 0xa8d10. Not byte-identical to SDK 2.3's, but it reads the same
void CHudSpectator::DeathMessage(int victim)
{
	// find out where the victim is
	cl_entity_t* pl = gEngfuncs.GetEntityByIndex(victim);

	if (pl && 0 != pl->player)
		AddOverviewEntityToList(m_hsprPlayerDead, pl, gEngfuncs.GetClientTime() + 2.0f);
}

// orig client 0xa8d80. Not byte-identical to SDK 2.3's, but it reads the same
bool CHudSpectator::AddOverviewEntityToList(HSPRITE sprite, cl_entity_t* ent, double killTime)
{
	for (int i = 0; i < MAX_OVERVIEW_ENTITIES; i++)
	{
		// find empty entity slot
		if (m_OverviewEntities[i].entity == NULL)
		{
			m_OverviewEntities[i].entity = ent;
			m_OverviewEntities[i].hSprite = sprite;
			m_OverviewEntities[i].killTime = killTime;
			return true;
		}
	}

	return false; // maximum overview entities reached
}
void CHudSpectator::CheckSettings()
{
	// disallow same inset mode as main mode:

	m_pip->value = (int)m_pip->value;

	if ((g_iUser1 < OBS_MAP_FREE) && (m_pip->value == INSET_CHASE_FREE || m_pip->value == INSET_IN_EYE))
	{
		// otherwise both would show in World picures
		m_pip->value = INSET_MAP_FREE;
	}

	if ((g_iUser1 >= OBS_MAP_FREE) && (m_pip->value >= INSET_MAP_FREE))
	{
		// both would show map views
		m_pip->value = INSET_CHASE_FREE;
	}

	// disble in intermission screen
	if (gHUD.m_iIntermission)
		m_pip->value = INSET_OFF;

	// check chat mode
	if (!m_chatEnabled) // c4: RC2's saytext has no hud_saytext
	{
		// hud_saytext changed
		m_chatEnabled = true; // c4: RC2's saytext has no hud_saytext

		if (0 != gEngfuncs.IsSpectateOnly())
		{
			// tell proxy our new chat mode
			char chatcmd[32];
			sprintf(chatcmd, "ignoremsg %i", m_chatEnabled ? 0 : 1);
			gEngfuncs.pfnServerCmd(chatcmd);
		}
	}

	// HL/TFC has no oberserver corsshair, so set it client side
	if ((g_iUser1 == OBS_IN_EYE) || (g_iUser1 == OBS_ROAMING))
	{
		m_crosshairRect.left = 24;
		m_crosshairRect.top = 0;
		m_crosshairRect.right = 48;
		m_crosshairRect.bottom = 24;

		SetCrosshair(m_hCrosshair, m_crosshairRect, 255, 255, 255);
	}
	else
	{
		memset(&m_crosshairRect, 0, sizeof(m_crosshairRect));
		SetCrosshair(0, m_crosshairRect, 0, 0, 0);
	}



	// if we are a real player on server don't allow inset window
	// in First Person mode since this is our resticted forcecamera mode 2
	// team number 3 = SPECTATOR see player.h

	if (((g_iTeamNumber == 1) || (g_iTeamNumber == 2)) && (g_iUser1 == OBS_IN_EYE))
		m_pip->value = INSET_OFF;

	// draw small border around inset view, adjust upper black bar
	gViewPort->m_pSpectatorPanel->EnableInsetView(m_pip->value != INSET_OFF);
}

int CHudSpectator::ToggleInset(bool allowOff)
{
	int newInsetMode = (int)m_pip->value + 1;

	if (g_iUser1 < OBS_MAP_FREE)
	{
		if (newInsetMode > INSET_MAP_CHASE)
		{
			if (allowOff)
				newInsetMode = INSET_OFF;
			else
				newInsetMode = INSET_MAP_FREE;
		}

		if (newInsetMode == INSET_CHASE_FREE)
			newInsetMode = INSET_MAP_FREE;
	}
	else
	{
		if (newInsetMode > INSET_IN_EYE)
		{
			if (allowOff)
				newInsetMode = INSET_OFF;
			else
				newInsetMode = INSET_CHASE_FREE;
		}
	}

	return newInsetMode;
}
void CHudSpectator::Reset()
{
	// Reset HUD
	if (0 != strcmp(m_OverviewData.map, gEngfuncs.pfnGetLevelName()))
	{
		// update level overview if level changed
		ParseOverviewFile();
		LoadMapSprites();
	}

	memset(&m_OverviewEntities, 0, sizeof(m_OverviewEntities));

	m_FOV = 90.0f;

	m_IsInterpolating = false;

	m_ChaseEntity = 0;

	SetSpectatorStartPosition();
}

void CHudSpectator::InitHUDData()
{
	m_lastPrimaryObject = m_lastSecondaryObject = 0;
	m_flNextObserverInput = 0.0f;
	m_lastHudMessage = 0;
	m_iSpectatorNumber = 0;
	iJumpSpectator = false;
	g_iUser1 = g_iUser2 = 0;

	memset(&m_OverviewData, 0, sizeof(m_OverviewData));
	memset(&m_OverviewEntities, 0, sizeof(m_OverviewEntities));

	if (0 != gEngfuncs.IsSpectateOnly() || 0 != gEngfuncs.pDemoAPI->IsPlayingback())
		m_autoDirector->value = 1.0f;
	else
		m_autoDirector->value = 0.0f;

	Reset();

	SetModes(OBS_CHASE_LOCKED, INSET_OFF);

	g_iUser2 = 0; // fake not target until first camera command

	// reset HUD FOV
	gHUD.m_iFOV = CVAR_GET_FLOAT("default_fov");
}
