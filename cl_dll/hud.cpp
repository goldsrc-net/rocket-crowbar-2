/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// hud.cpp
//
// implementation of CHud class
//

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

#include "demo.h"
#include "demo_api.h"
#include "vgui_ScorePanel.h"
#include "rc2/hud_servers.h"
#include "rc2/c4_needs.h"

hud_player_info_t g_PlayerInfoList[MAX_PLAYERS_HUD + 1];	// player info from the engine
extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS_HUD + 1]; // additional player info sent directly to the client dll

// c4: client.dll's hud.obj also holds compiler-written code from the VGUI headers RC2's hud.cpp
// pulls in (nothing to write): vgui::Slider2's and vgui::ScrollBar2's implicit copy constructors
// and assignment operators (orig client 0xa34c0, orig client 0xa37e0, orig client 0xa3b40,
// orig client 0xa3dc0), and vgui::IntChangeSignal's inline constructor, destructor and empty
// handler (orig client 0xa3460, orig client 0xa3480, orig client 0xa34a0).

// c4: Rocket Crowbar 2's client globals (hud.h; names ours)
int g_iThisClient = 1;		   // .data 0x1013d0b8
int g_iViewMode;			   // .bss 0x1020bb38
int g_iScoreMode;			   // .bss 0x1020bb3c
float g_flTransformFOV;		   // .bss 0x1020bb44, VERIFY: set by fork V's transform HUD
int g_iVGUIDebugCount;		   // .bss 0x10222134
cvar_t* cl_gibcount = nullptr; // .bss 0x1020b71c
cvar_t* cl_giblife = nullptr;
cvar_t* cl_detail = nullptr;
cvar_t* cl_forkliftlights = nullptr;
cvar_t* cl_visiblejetpack = nullptr;
cvar_t* cl_debuggren = nullptr;

// The constructor, destructor and scalar deleting destructor the compiler writes for this class
// and its global object: orig client 0xa0ac0, orig client 0xa0aa0 (IVoiceStatusHelper's),
// orig client 0xa0bf0, and the static initializer and atexit hook orig client 0xa0a40,
// orig client 0xa0a60. RC2's methods are SDK 2.3's.
class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	// orig client 0xa0ae0 (not in files-client.tsv: Ghidra missed it)
	void GetPlayerTextColor(int entindex, int color[3]) override
	{
		color[0] = color[1] = color[2] = 255;

		if (entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo) / sizeof(g_PlayerExtraInfo[0]))
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if (iTeam < 0)
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	void UpdateCursorState() override
	{
		gViewPort->UpdateCursorState();
	}

	int GetAckIconHeight() override
	{
		return ScreenHeight - gHUD.m_iFontHeight * 3 - 6;
	}

	bool CanShowSpeakerLabels() override
	{
		if (gViewPort && gViewPort->m_pScoreBoard)
			return !gViewPort->m_pScoreBoard->isVisible();
		else
			return false;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t* GetSpriteList(client_sprite_t* pList, const char* psz, int iRes, int iCount);

extern float IN_GetMouseSensitivity();
cvar_t* cl_lw = NULL;
cvar_t* cl_rollangle = nullptr;
cvar_t* cl_rollspeed = nullptr;
cvar_t* cl_bobtilt = nullptr;
cvar_t* r_decals = nullptr;

void ShutdownInput();

//DECLARE_MESSAGE(m_Logo, Logo)
// orig client 0xa0c20 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_Logo(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_Logo(pszName, iSize, pbuf));
}

//DECLARE_MESSAGE(m_Logo, Logo)
// orig client 0xa0c50
int __MsgFunc_ResetHUD(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf));
}

// orig client 0xa0c80 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_InitHUD(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_InitHUD(pszName, iSize, pbuf);
	return 1;
}

// orig client 0xa10b0. RC2: its own "ViewMode", a byte the client keeps (g_iViewMode); SDK 2.3's
// CHud::MsgFunc_ViewMode isn't hooked
int __MsgFunc_ViewMode(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	g_iViewMode = READ_BYTE();
	return 1;
}

// orig client 0xa1070: the local player's entity index
int __MsgFunc_ThisClient(const char* pszName, int iSize, void* pbuf) // name ours
{
	BEGIN_READ(pbuf, iSize);
	g_iThisClient = READ_BYTE();
	return 1;
}

// orig client 0xa10f0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_ScoreMode(const char* pszName, int iSize, void* pbuf) // name ours
{
	BEGIN_READ(pbuf, iSize);
	g_iScoreMode = READ_BYTE();
	return 1;
}

// orig client 0xa1130 (not in files-client.tsv: Ghidra missed it): shows or hides the VGUI
// debug panel
int __MsgFunc_VGUIDebugOn(const char* pszName, int iSize, void* pbuf) // name ours
{
	BEGIN_READ(pbuf, iSize);
	const bool bOn = 0 != READ_BYTE(); // name ours

	if (gViewPort)
		gViewPort->SetDebugPanelVisible(bOn);

	if (!bOn)
		g_iVGUIDebugCount = 0;

	return 1;
}

// orig client 0xa11a0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_DBGCounts(const char* pszName, int iSize, void* pbuf) // name ours
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_DBGCounts(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa11e0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_DBGEntInfo(const char* pszName, int iSize, void* pbuf) // name ours
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_DBGEntInfo(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa1220 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_VoteBegin(const char* pszName, int iSize, void* pbuf) // name ours
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_VoteBegin(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa1260 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_VoteUpdate(const char* pszName, int iSize, void* pbuf) // name ours
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_VoteUpdate(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa1030
int __MsgFunc_AttrConf(const char* pszName, int iSize, void* pbuf) // name ours
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_AttrConf(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0cc0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_SetFOV(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_SetFOV(pszName, iSize, pbuf));
}

// orig client 0xa0cf0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_Concuss(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_Concuss(pszName, iSize, pbuf));
}

int __MsgFunc_Weapons(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_Weapons(pszName, iSize, pbuf));
}

// orig client 0xa0d20
int __MsgFunc_GameMode(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_GameMode(pszName, iSize, pbuf));
}

// TFFree Command Menu
// orig client 0xa0d50
void __CmdFunc_OpenCommandMenu()
{
	if (gViewPort)
	{
		gViewPort->ShowCommandMenu(gViewPort->m_StandardMenu);
	}
}

// TFC "special" command
// orig client 0xa0d70. RC2 keeps it but no longer hooks "special"
void __CmdFunc_InputPlayerSpecial()
{
	if (gViewPort)
	{
		gViewPort->InputPlayerSpecial();
	}
}

// orig client 0xa0d90 (not in files-client.tsv: Ghidra missed it)
void __CmdFunc_CloseCommandMenu()
{
	if (gViewPort)
	{
		gViewPort->InputSignalHideCommandMenu();
	}
}

// orig client 0xa0db0 (not in files-client.tsv: Ghidra missed it)
void __CmdFunc_ForceCloseCommandMenu()
{
	if (gViewPort)
	{
		gViewPort->HideCommandMenu();
	}
}

// orig client 0xa0dd0 (not in files-client.tsv: Ghidra missed it). SDK 2.3's, which
// halflife-updated dropped with the server browser
void __CmdFunc_ToggleServerBrowser()
{
	if (gViewPort)
	{
		gViewPort->ToggleServerBrowser();
	}
}

// TFFree Command Menu Message Handlers
// orig client 0xa0df0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamNames(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0e30 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_VGUIMenu(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0e70
int __MsgFunc_MOTD(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_MOTD(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0eb0
int __MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ServerName(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0ef0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_ScoreInfo(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ScoreInfo(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0f30 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_TeamScore(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamScore(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0f70 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_TeamInfo(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamInfo(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0fb0
int __MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_Spectator(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_SpecFade(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ResetFade(pszName, iSize, pbuf));
	return 0;
}

// orig client 0xa0ff0 (not in files-client.tsv: Ghidra missed it)
int __MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_AllowSpec(pszName, iSize, pbuf));
	return 0;
}

// This is called every time the DLL is loaded
// orig client 0xa12a0. RC2: its own messages (and its handlers' homes) and cvars, the energy meter
// and status icons, "togglebrowser" and ServersInit back from SDK 2.3; no TFC messages, no
// "special". halflife-updated's Weapons, SpecFade and ResetFade hooks and its cvars stay.
void CHud::Init()
{
	HOOK_MESSAGE(Logo);
	HOOK_MESSAGE(ResetHUD);
	HOOK_MESSAGE(GameMode);
	HOOK_MESSAGE(InitHUD);
	HOOK_MESSAGE(SetFOV);
	HOOK_MESSAGE(Concuss);
	HOOK_MESSAGE(Weapons); // halflife-updated's

	// TFFree CommandMenu
	HOOK_COMMAND("+commandmenu", OpenCommandMenu);
	HOOK_COMMAND("-commandmenu", CloseCommandMenu);
	HOOK_COMMAND("ForceCloseCommandMenu", ForceCloseCommandMenu);
	HOOK_COMMAND("togglebrowser", ToggleServerBrowser);

	HOOK_MESSAGE(TeamNames);
	HOOK_MESSAGE(MOTD);
	HOOK_MESSAGE(ServerName);
	HOOK_MESSAGE(ScoreInfo);
	HOOK_MESSAGE(TeamScore);
	HOOK_MESSAGE(TeamInfo);

	HOOK_MESSAGE(Spectator);
	HOOK_MESSAGE(AllowSpec);

	HOOK_MESSAGE(SpecFade);	 // halflife-updated's
	HOOK_MESSAGE(ResetFade); // halflife-updated's

	// RC2's messages
	HOOK_MESSAGE(AttrConf);
	HOOK_MESSAGE(SetAttr);
	HOOK_MESSAGE(ThisClient);
	HOOK_MESSAGE(ViewMode);
	HOOK_MESSAGE(ScoreMode);
	HOOK_MESSAGE(VGUIDebugOn);
	HOOK_MESSAGE(DBGCounts);
	HOOK_MESSAGE(DBGEntInfo);
	HOOK_MESSAGE(VoteBegin);
	HOOK_MESSAGE(VoteUpdate);
	HOOK_MESSAGE(CLTE);
	HOOK_MESSAGE(PullerCtrl);
	HOOK_MESSAGE(WeaponSync);
	HOOK_MESSAGE(RCCFG);
	HOOK_MESSAGE(TformStat);
	HOOK_MESSAGE(PlrStat);
	HOOK_MESSAGE(RaceStat);

	// VGUI Menus
	HOOK_MESSAGE(VGUIMenu);

	CVAR_CREATE("hud_takesshots", "0", FCVAR_ARCHIVE); // controls whether or not to automatically take screenshots at the end of a round
	cl_gibcount = CVAR_CREATE("cl_gibcount", "8", FCVAR_ARCHIVE);
	cl_giblife = CVAR_CREATE("cl_giblife", "15", FCVAR_ARCHIVE);
	cl_detail = CVAR_CREATE("cl_detail", "0", FCVAR_ARCHIVE);
	cl_forkliftlights = CVAR_CREATE("cl_forkliftlights", "0", FCVAR_ARCHIVE);
	cl_visiblejetpack = CVAR_CREATE("cl_visiblejetpack", "1", FCVAR_ARCHIVE);
	cl_debuggren = CVAR_CREATE("cl_debuggren", "0", FCVAR_ARCHIVE);

	m_iLogo = 0;
	m_iFOV = 0;

	// RC2 creates these two without FCVAR_ARCHIVE (as SDK 2.3); halflife-updated archives them
	CVAR_CREATE("zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO); // halflife-updated's
	default_fov = CVAR_CREATE("default_fov", "90", FCVAR_ARCHIVE);
	m_pCvarStealMouse = CVAR_CREATE("hud_capturemouse", "1", FCVAR_ARCHIVE);
	m_pCvarDraw = CVAR_CREATE("hud_draw", "1", FCVAR_ARCHIVE); // halflife-updated's
	cl_lw = gEngfuncs.pfnGetCvarPointer("cl_lw");
	cl_rollangle = CVAR_CREATE("cl_rollangle", "2.0", FCVAR_ARCHIVE); // halflife-updated's
	cl_rollspeed = CVAR_CREATE("cl_rollspeed", "200", FCVAR_ARCHIVE);
	cl_bobtilt = CVAR_CREATE("cl_bobtilt", "0", FCVAR_ARCHIVE);
	r_decals = gEngfuncs.pfnGetCvarPointer("r_decals");

	// RC2: r_drawentities is forced to 1 and made single-player only, so it can't hide models
	cvar_t* pDrawEntities = gEngfuncs.pfnGetCvarPointer("r_drawentities"); // name ours
	if (pDrawEntities)
	{
		pDrawEntities->value = 1.0f;
		pDrawEntities->flags |= FCVAR_SPONLY;
	}

	m_pSpriteList = NULL;

	// Clear any old HUD list
	if (m_pHudList)
	{
		HUDLIST* pList;
		while (m_pHudList)
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free(pList);
		}
		m_pHudList = NULL;
	}

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Ammo.Init();
	m_Health.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Energy.Init();
	m_EIcons.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper, (vgui::Panel**)&gViewPort);
	m_Spectator.Init();
	m_SayText.Init();
	m_Menu.Init();

	ServersInit();

	MsgFunc_ResetHUD(0, 0, NULL);

#ifdef STEAM_RICH_PRESENCE
	gEngfuncs.pfnClientCmd("richpresence_gamemode\n"); // reset
	gEngfuncs.pfnClientCmd("richpresence_update\n");
#endif
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
// orig client 0xa1890 (SDK 2.3's, with ServersShutdown)
CHud::~CHud()
{
	delete[] m_rghSprites;
	delete[] m_rgrcRects;
	delete[] m_rgszSpriteNames;

	if (m_pHudList)
	{
		HUDLIST* pList;
		while (m_pHudList)
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free(pList);
		}
		m_pHudList = NULL;
	}

	ServersShutdown(); // RC2 (SDK 2.3)
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
// orig client 0xa19b0. Not byte-identical to SDK 2.3's, but it reads the same
int CHud::GetSpriteIndex(const char* SpriteName)
{
	// look through the loaded sprite name list for SpriteName
	for (int i = 0; i < m_iSpriteCount; i++)
	{
		if (strncmp(SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH) == 0)
			return i;
	}

	return -1; // invalid sprite
}

// orig client 0xa1a20. RC2: SDK 2.3's resolutions only (640 from 640 wide, else 320: RC2's hud.txt
// has no 1280/2560 sprites), and the energy meter and status icons
void CHud::VidInit()
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// ----------
	// Load Sprites
	// ---------
	//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;

	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if (!m_pSpriteList)
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t* p = m_pSpriteList;
			int j;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new Rect[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy(&m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH);

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t* p = m_pSpriteList;
		int index = 0;
		for (int j = 0; j < m_iSpriteCountAllRes; j++)
		{
			if (p->iRes == m_iRes)
			{
				char sz[256];
				sprintf(sz, "sprites/%s.spr", p->szSprite);
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex("number_0");

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Energy.VidInit();
	m_EIcons.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	GetClientVoiceMgr()->VidInit();
	m_Spectator.VidInit();
}

bool CHud::MsgFunc_Logo(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// update Train data
	m_iLogo = READ_BYTE();

	return true;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase(const char* in, char* out)
{
	int len, start, end;

	len = strlen(in);

	// scan backward for '.'
	end = len - 1;
	while (0 != end && in[end] != '.' && in[end] != '/' && in[end] != '\\')
		end--;

	if (in[end] != '.') // no '.', copy to end
		end = len - 1;
	else
		end--; // Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len - 1;
	while (start >= 0 && in[start] != '/' && in[start] != '\\')
		start--;

	if (in[start] != '/' && in[start] != '\\')
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy(out, &in[start], len);
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
bool HUD_IsGame(const char* game)
{
	const char* gamedir;
	char gd[1024];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if (gamedir && '\0' != gamedir[0])
	{
		COM_FileBase(gamedir, gd);
		if (!stricmp(gd, game))
			return true;
	}
	return false;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
// orig client 0xa1f30. RC2: a transformed player (playerclass set) sees g_flTransformFOV
float HUD_GetFOV()
{
	if (0 != g_iPlayerClass)
		return g_flTransformFOV;

	if (0 != gEngfuncs.pDemoAPI->IsRecording())
	{
		// Write it
		int i = 0;
		unsigned char buf[100];

		// Active
		*(float*)&buf[i] = g_lastFOV;
		i += sizeof(float);

		Demo_WriteBuffer(TYPE_ZOOM, i, buf);
	}

	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

bool CHud::MsgFunc_SetFOV(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT("default_fov");

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	//But it doesn't restore correctly so this still needs to be used
	/*
	if ( cl_lw && cl_lw->value )
		return 1;
		*/

	g_lastFOV = newfov;

	if (newfov == 0)
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if (m_iFOV == def_fov)
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = IN_GetMouseSensitivity() * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return true;
}


void CHud::AddHudElem(CHudBase* phudelem)
{
	HUDLIST *pdl, *ptemp;

	//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST*)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

// orig client 0xa2110
float CHud::GetSensitivity()
{
	return m_flMouseSensitivity;
}
