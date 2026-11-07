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
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include <SDL2/SDL_messagebox.h>

#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#include "interface.h"
//#include "vgui_schememanager.h"

#include "pm_shared.h"

#include <string.h>
#include "vgui_int.h"

#include "Platform.h"
#include "Exports.h"

#include "tri.h"
#include "vgui_TeamFortressViewport.h"
#include "filesystem_utils.h"

cl_enginefunc_t gEngfuncs;

// RC2: cl_detail as an int, refreshed every HUD_Frame (orig .bss 0x101a9598, right after
// gEngfuncs). 0 without the cvar. name ours
int g_iClientDetail = 0;

// gHUD's construction and destruction are compiler-generated: orig client 0x4fcf0 (the dynamic
// initializer, calling orig client 0x4fd10, which runs the constructor, and orig client 0x4fd30,
// which registers orig client 0x4fd50 with atexit to run ~CHud), orig client 0x50090 (CHud::CHud
// itself: m_pHudList and m_iSpriteCount zeroed, the HUD elements constructed). RC2's CHud holds,
// in this order, Ammo, Health, Geiger, Battery, Energy, EIcons, Train, Flash, Message, StatusBar,
// DeathNotice, SayText, Menu, AmmoSecondary, TextMessage, StatusIcons and Spectator (moved last;
// its 64 Vectors at +0x427 dwords are the constructor's vector iterator). The elements' inline
// CHudBase destructors and their scalar deleting destructors, in that order:
// orig client 0x501f0, orig client 0x50210, orig client 0x50230, orig client 0x50250,
// orig client 0x50270, orig client 0x50290, orig client 0x502b0, orig client 0x502d0,
// orig client 0x502f0, orig client 0x50310, orig client 0x50330, orig client 0x50350,
// orig client 0x50370, orig client 0x50390, orig client 0x503b0, orig client 0x503d0,
// orig client 0x50720 (the destructors), orig client 0x503f0, orig client 0x50420,
// orig client 0x50450, orig client 0x50480, orig client 0x504b0, orig client 0x504e0,
// orig client 0x50510, orig client 0x50540, orig client 0x50570, orig client 0x505a0,
// orig client 0x505d0, orig client 0x50600, orig client 0x50630, orig client 0x50660,
// orig client 0x50690, orig client 0x506c0, orig client 0x506f0 (the deleting destructors).
// VERIFY: the Energy and EIcons elements and the member order are hud.h's (another package's).
CHud gHUD;
TeamFortressViewport* gViewPort = NULL;


#include "particleman.h"
IParticleMan* g_pParticleMan = nullptr;

void CL_LoadParticleMan();
void CL_UnloadParticleMan();

void InitInput();
void EV_HookEvents();
void IN_Commands();

// RC2 (client.dll's own code elsewhere; names ours)
void CLTE_Reset();				// cl_dll/rc2/clte.cpp
void RC2_ResetEntityTables();	// orig client 0xbd0e0: three 64-entry tables at .bss 0x10210170-0x10210474
void RC2_ResetViewportFlags();	// orig client 0xd9150: .bss 0x102223c0 = 0, 0x102223c4 = 1
void RC2_ResetPendingMenu();	// orig client 0xcfe90: .bss 0x101755e0 = -1, 0x10221ab0 = 0
void RC2_LoadCrosshairs();		// orig client 0xcb1c0: the crosshair sprites from sprites/weapon_crossbow.txt
void RC2Prefs_ResetToDefaults(); // orig client 0xbe3c0: the client's copy of rc2.ini back to its defaults
void RC2_ViewportFrame(float flTime); // orig client 0xd8980
void RC2_RunPendingMenu();		// orig client 0xcfe10
extern cvar_t* cl_detail;		// "cl_detail", registered by CHud::Init (orig .bss 0x1020bb30)
extern int g_iMaxPlayerEnt;		// 1 + the highest player entity HUD_AddEntity has seen (orig .bss 0x10206918)
extern unsigned int g_iClientFrame; // cl_dll/rc2/clte.cpp

// orig client 0x4fe80. RC2: what HUD_Init, HUD_VidInit and HUD_Reset all clear. name ours
static void RC2_ResetClientState()
{
	CLTE_Reset();
	RC2_ResetEntityTables();
	g_iMaxPlayerEnt = 0;
	RC2_ResetViewportFlags();
	RC2_ResetPendingMenu();
}

/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
// SDK 2.3's (orig client 0x4fd70) assigned the hull to its local pointers and so returned 1 for
// hulls 0-2 with mins and maxs untouched; halflife-updated's fix kept
int DLLEXPORT HUD_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	return static_cast<int>(PM_GetHullBounds(hullnumber, mins, maxs));
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
// orig client 0x4fda0 (stock)
int DLLEXPORT HUD_ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	//	RecClConnectionlessPacket(net_from, args, response_buffer, response_buffer_size);

	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

// orig client 0x4fdc0 (stock)
void DLLEXPORT HUD_PlayerMoveInit(struct playermove_s* ppmove)
{
	//	RecClClientMoveInit(ppmove);

	PM_Init(ppmove);
}

// orig client 0x4fde0 (stock)
char DLLEXPORT HUD_PlayerMoveTexture(char* name)
{
	//	RecClClientTextureType(name);

	return PM_FindTextureType(name);
}

void DLLEXPORT HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
	//	RecClClientMove(ppmove, server);

	PM_Move(ppmove, server);
}

static bool CL_InitClient()
{
	EV_HookEvents();
	CL_LoadParticleMan();

	if (!FileSystem_LoadFileSystem())
	{
		return false;
	}

	if (UTIL_IsValveGameDirectory())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error",
			"This mod has detected that it is being run from a Valve game directory which is not supported\n"
			"Run this mod from its intended location\n\nThe game will now shut down", nullptr);
		return false;
	}

	// get tracker interface, if any
	return true;
}

int DLLEXPORT Initialize(cl_enginefunc_t* pEnginefuncs, int iVersion)
{
	gEngfuncs = *pEnginefuncs;

	//	RecClInitialize(pEnginefuncs, iVersion);

	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	memcpy(&gEngfuncs, pEnginefuncs, sizeof(cl_enginefunc_t));

	const bool result = CL_InitClient();

	if (!result)
	{
		gEngfuncs.Con_DPrintf("Error initializing client\n");
		gEngfuncs.pfnClientCmd("quit\n");
		return 0;
	}

	return 1;
}


/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

// orig client 0x4feb0. RC2: its own state cleared and the crosshairs loaded
int DLLEXPORT HUD_VidInit()
{
	//	RecClHudVidInit();
	gHUD.VidInit();

	VGui_Startup();

	RC2_ResetClientState();
	RC2_LoadCrosshairs();

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all 
the hud variables.
==========================
*/

// orig client 0x4fee0. RC2: its own state cleared and rc2.ini back to its defaults
void DLLEXPORT HUD_Init()
{
	//	RecClHudInit();
	InitInput();
	gHUD.Init();
	Scheme_Init();
	RC2_ResetClientState();
	RC2Prefs_ResetToDefaults();
}


/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int DLLEXPORT HUD_Redraw(float time, int intermission)
{
	//	RecClHudRedraw(time, intermission);

	gHUD.Redraw(time, 0 != intermission);

	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int DLLEXPORT HUD_UpdateClientData(client_data_t* pcldata, float flTime)
{
	//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();

	return static_cast<int>(gHUD.UpdateClientData(pcldata, flTime));
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

// orig client 0x4ff70. RC2: its own state cleared too
void DLLEXPORT HUD_Reset()
{
	//	RecClHudReset();

	gHUD.VidInit();
	RC2_ResetClientState();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

// orig client 0x4ff90. RC2 adds its viewport and menu work, refreshes cl_detail and counts
// frames for the client effects' origin cache. SDK 2.3's ServersThink(time) came first; halflife-
// updated has no server browser
void DLLEXPORT HUD_Frame(double time)
{
	//	RecClHudFrame(time);

	GetClientVoiceMgr()->Frame(time);
	RC2_ViewportFrame((float)time);
	RC2_RunPendingMenu();

	if (cl_detail)
		g_iClientDetail = (int)cl_detail->value;
	else
		g_iClientDetail = 0;
	g_iClientFrame++;
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	////	RecClVoiceStatus(entindex, bTalking);

	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, 0 != bTalking);
}

/*
==========================
HUD_DirectorMessage

Called when a director event message was received
==========================
*/

// orig client 0x50060 (stock)
void DLLEXPORT HUD_DirectorMessage(int iSize, void* pbuf)
{
	//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage(iSize, pbuf);
}

void CL_UnloadParticleMan()
{
	g_pParticleMan = nullptr;
}

void CL_LoadParticleMan()
{
	//Now implemented in the client library.
	auto particleManFactory = Sys_GetFactoryThis();

	g_pParticleMan = (IParticleMan*)particleManFactory(PARTICLEMAN_INTERFACE, nullptr);

	if (g_pParticleMan)
	{
		g_pParticleMan->SetUp(&gEngfuncs);
	}
}

extern "C" void DLLEXPORT F(void* pv)
{
	cldll_func_t* pcldll_func = (cldll_func_t*)pv;

	cldll_func_t cldll_func =
		{
			Initialize,
			HUD_Init,
			HUD_VidInit,
			HUD_Redraw,
			HUD_UpdateClientData,
			HUD_Reset,
			HUD_PlayerMove,
			HUD_PlayerMoveInit,
			HUD_PlayerMoveTexture,
			IN_ActivateMouse,
			IN_DeactivateMouse,
			IN_MouseEvent,
			IN_ClearStates,
			IN_Accumulate,
			CL_CreateMove,
			CL_IsThirdPerson,
			CL_CameraOffset,
			KB_Find,
			CAM_Think,
			V_CalcRefdef,
			HUD_AddEntity,
			HUD_CreateEntities,
			HUD_DrawNormalTriangles,
			HUD_DrawTransparentTriangles,
			HUD_StudioEvent,
			HUD_PostRunCmd,
			HUD_Shutdown,
			HUD_TxferLocalOverrides,
			HUD_ProcessPlayerState,
			HUD_TxferPredictionData,
			Demo_ReadBuffer,
			HUD_ConnectionlessPacket,
			HUD_GetHullBounds,
			HUD_Frame,
			HUD_Key_Event,
			HUD_TempEntUpdate,
			HUD_GetUserEntity,
			HUD_VoiceStatus,
			HUD_DirectorMessage,
			HUD_GetStudioModelInterface,
			HUD_ChatInputPosition,
		};

	*pcldll_func = cldll_func;
}

#include "cl_dll/IGameClientExports.h"

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	const char* GetServerHostName() override
	{
		/*if (gViewPortInterface)
		{
			return gViewPortInterface->GetServerName();
		}*/
		return "";
	}

	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted(int playerIndex) override
	{
		if (GetClientVoiceMgr())
			return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
		return false;
	}

	void MutePlayerGameVoice(int playerIndex) override
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	void UnmutePlayerGameVoice(int playerIndex) override
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION);

// TEMP STUB (c2): real ones belong in input.cpp/hud.cpp/entity.cpp and RC2's own client files
// at the addresses given with their declarations above (other client packages)
void RC2_ResetEntityTables() {}
void RC2_ResetViewportFlags() {}
void RC2_ResetPendingMenu() {}
void RC2_LoadCrosshairs() {}
void RC2Prefs_ResetToDefaults() {}
void RC2_ViewportFrame(float flTime) {}
void RC2_RunPendingMenu() {}
cvar_t* cl_detail = nullptr;
int g_iMaxPlayerEnt = 0;
