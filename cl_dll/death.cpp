/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// death notice
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE(m_DeathNotice, DeathMsg); // orig client 0x9b8d0 (not in files-client.tsv: Ghidra missed it)

// dlls/rc2/killmessages.cpp, compiled into the client too (cl_dll/rc2/killmessages_client.cpp)
const char* KillMsg_Get(int iMsg, const char* pszVictim, const char* pszKiller, const char* pszWeapon, int iGender, int bSelf);
const char* KillMsg_His(int iGender);

struct DeathNoticeItem
{
	char szKiller[MAX_PLAYER_NAME_LENGTH * 2];
	char szVictim[MAX_PLAYER_NAME_LENGTH * 2];
	int iId; // the index number of the associated sprite
	bool iSuicide;
	bool iTeamKill;
	bool iNonPlayerKill;
	float flDisplayTime;
	float* KillerColor;
	float* VictimColor;
};

#define MAX_DEATHNOTICES 4
static int DEATHNOTICE_DISPLAY_TIME = 6;

#define DEATHNOTICE_TOP 64

DeathNoticeItem rgDeathNoticeList[MAX_DEATHNOTICES + 1];

float g_ColorBlue[3] = {0.6, 0.8, 1.0};
float g_ColorRed[3] = {1.0, 0.25, 0.25};
float g_ColorGreen[3] = {0.6, 1.0, 0.6};
float g_ColorYellow[3] = {1.0, 0.7, 0.0};
float g_ColorGrey[3] = {0.8, 0.8, 0.8};

// orig client 0x9b900. Not byte-identical to SDK 2.3's (g_PlayerExtraInfo differs), but it reads the same
float* GetClientColor(int clientIndex)
{
	switch (g_PlayerExtraInfo[clientIndex].teamnumber)
	{
	case 1:
		return g_ColorBlue;
	case 2:
		return g_ColorRed;
	case 3:
		return g_ColorYellow;
	case 4:
		return g_ColorGreen;
	case 0:
		return g_ColorYellow;

	default:
		return g_ColorGrey;
	}

	return NULL;
}

bool CHudDeathNotice::Init()
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(DeathMsg);

	CVAR_CREATE("hud_deathnotice_time", "6", FCVAR_ARCHIVE);

	return true;
}


void CHudDeathNotice::InitHUDData()
{
	memset(rgDeathNoticeList, 0, sizeof(rgDeathNoticeList));
}


bool CHudDeathNotice::VidInit()
{
	m_HUD_d_skull = gHUD.GetSpriteIndex("d_skull");

	return true;
}

// orig client 0x9ba30. RC2: SDK 2.3's layout moved to the top right corner (rows 20 apart from
// y 20, 20 in from the right) and the victim's name 10 further from the sprite; halflife-updated's
// row gap from the sprite and font height and its text offset are not RC2's
bool CHudDeathNotice::Draw(float flTime)
{
	int x, y, r, g, b;

	for (int i = 0; i < MAX_DEATHNOTICES; i++)
	{
		if (rgDeathNoticeList[i].iId == 0)
			break; // we've gone through them all

		if (rgDeathNoticeList[i].flDisplayTime < flTime)
		{ // display time has expired
			// remove the current item from the list
			memmove(&rgDeathNoticeList[i], &rgDeathNoticeList[i + 1], sizeof(DeathNoticeItem) * (MAX_DEATHNOTICES - i));
			i--; // continue on the next item;  stop the counter getting incremented
			continue;
		}

		rgDeathNoticeList[i].flDisplayTime = V_min(rgDeathNoticeList[i].flDisplayTime, gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME);

		// Only draw if the viewport will let me
		if (gViewPort && gViewPort->AllowedToPrintText())
		{
			// Draw the death notice
			y = 20 + (20 * i);

			int id = (rgDeathNoticeList[i].iId == -1) ? m_HUD_d_skull : rgDeathNoticeList[i].iId;
			x = ScreenWidth - ConsoleStringLen(rgDeathNoticeList[i].szVictim) - (gHUD.GetSpriteRect(id).right - gHUD.GetSpriteRect(id).left) - 20;

			if (!rgDeathNoticeList[i].iSuicide)
			{
				x -= (5 + ConsoleStringLen(rgDeathNoticeList[i].szKiller));

				// Draw killers name
				if (rgDeathNoticeList[i].KillerColor)
					gEngfuncs.pfnDrawSetTextColor(rgDeathNoticeList[i].KillerColor[0], rgDeathNoticeList[i].KillerColor[1], rgDeathNoticeList[i].KillerColor[2]);
				x = 5 + DrawConsoleString(x, y, rgDeathNoticeList[i].szKiller);
			}

			r = 255;
			g = 80;
			b = 0;
			if (rgDeathNoticeList[i].iTeamKill)
			{
				r = 10;
				g = 240;
				b = 10; // display it in sickly green
			}

			// Draw death weapon
			SPR_Set(gHUD.GetSprite(id), r, g, b);
			SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(id));

			x += 10 + (gHUD.GetSpriteRect(id).right - gHUD.GetSpriteRect(id).left);

			// Draw victims name (if it was a player that was killed)
			if (!rgDeathNoticeList[i].iNonPlayerKill)
			{
				if (rgDeathNoticeList[i].VictimColor)
					gEngfuncs.pfnDrawSetTextColor(rgDeathNoticeList[i].VictimColor[0], rgDeathNoticeList[i].VictimColor[1], rgDeathNoticeList[i].VictimColor[2]);
				x = DrawConsoleString(x, y, rgDeathNoticeList[i].szVictim);
			}
		}
	}

	return true;
}

// This message handler may be better off elsewhere
// orig client 0x9bd70. RC2: the message also carries RC2's death message number and the victim's
// gender (multiplay_gamerules.cpp DeathNotice), and the console gets RC2's line for it
// (KillMsg_Get) in place of "killed ... with ...", printed with developer on
bool CHudDeathNotice::MsgFunc_DeathMsg(const char* pszName, int iSize, void* pbuf)
{
	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ(pbuf, iSize);

	int killer = READ_BYTE();
	int victim = READ_BYTE();
	int iDeathMessage = READ_SHORT(); // name ours
	int iGender = READ_BYTE();		  // name ours

	char killedwith[32];
	strcpy(killedwith, "d_");
	strncat(killedwith, READ_STRING(), 32);

	if (gViewPort)
		gViewPort->DeathMsg(killer, victim);

	gHUD.m_Spectator.DeathMessage(victim);
	int i;
	for (i = 0; i < MAX_DEATHNOTICES; i++)
	{
		if (rgDeathNoticeList[i].iId == 0)
			break;
	}
	if (i == MAX_DEATHNOTICES)
	{ // move the rest of the list forward to make room for this item
		memmove(rgDeathNoticeList, rgDeathNoticeList + 1, sizeof(DeathNoticeItem) * MAX_DEATHNOTICES);
		i = MAX_DEATHNOTICES - 1;
	}

	if (gViewPort)
		gViewPort->GetAllPlayersInfo();

	// Get the Killer's name
	const char* killer_name = g_PlayerInfoList[killer].name;
	if (!killer_name)
	{
		killer_name = "";
		rgDeathNoticeList[i].szKiller[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].KillerColor = GetClientColor(killer);
		strncpy(rgDeathNoticeList[i].szKiller, killer_name, MAX_PLAYER_NAME_LENGTH);
		rgDeathNoticeList[i].szKiller[MAX_PLAYER_NAME_LENGTH - 1] = 0;
	}

	// Get the Victim's name
	const char* victim_name = NULL;
	// If victim is -1, the killer killed a specific, non-player object (like a sentrygun)
	if (((char)victim) != -1)
		victim_name = g_PlayerInfoList[victim].name;
	if (!victim_name)
	{
		victim_name = "";
		rgDeathNoticeList[i].szVictim[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].VictimColor = GetClientColor(victim);
		strncpy(rgDeathNoticeList[i].szVictim, victim_name, MAX_PLAYER_NAME_LENGTH);
		rgDeathNoticeList[i].szVictim[MAX_PLAYER_NAME_LENGTH - 1] = 0;
	}

	// Is it a non-player object kill?
	if (((char)victim) == -1)
	{
		rgDeathNoticeList[i].iNonPlayerKill = true;

		// Store the object's name in the Victim slot (skip the d_ bit)
		strcpy(rgDeathNoticeList[i].szVictim, killedwith + 2);
	}
	else
	{
		if (killer == victim || killer == 0)
			rgDeathNoticeList[i].iSuicide = true;

		if (0 == strcmp(killedwith, "d_teammate"))
			rgDeathNoticeList[i].iTeamKill = true;
	}

	// Find the sprite in the list
	int spr = gHUD.GetSpriteIndex(killedwith);

	rgDeathNoticeList[i].iId = spr;

	DEATHNOTICE_DISPLAY_TIME = CVAR_GET_FLOAT("hud_deathnotice_time");
	rgDeathNoticeList[i].flDisplayTime = gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME;

	if (rgDeathNoticeList[i].iNonPlayerKill)
	{
		ConsolePrint(rgDeathNoticeList[i].szKiller);
		ConsolePrint(" killed a ");
		ConsolePrint(rgDeathNoticeList[i].szVictim);
		ConsolePrint("\n");
		return true;
	}

	// record the death notice in the console, with developer on while it prints
	const int iDeveloper = (int)CVAR_GET_FLOAT("developer"); // name ours
	if (iDeveloper == 0)
		gEngfuncs.Cvar_SetValue("developer", 1);

	if (rgDeathNoticeList[i].iTeamKill)
	{
		ConsolePrint(rgDeathNoticeList[i].szKiller);
		ConsolePrint(" killed ");
		ConsolePrint(KillMsg_His(iGender));
		ConsolePrint(" teammate ");
		ConsolePrint(rgDeathNoticeList[i].szVictim);
	}
	else
	{
		ConsolePrint(KillMsg_Get(iDeathMessage, rgDeathNoticeList[i].szVictim, rgDeathNoticeList[i].szKiller, killedwith + 2, iGender, rgDeathNoticeList[i].iSuicide ? 1 : 0));
	}

	ConsolePrint("\n");

	if (iDeveloper == 0)
		gEngfuncs.Cvar_SetValue("developer", 0);

	return true;
}
