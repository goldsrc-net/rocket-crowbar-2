/***
*
*	Rocket Crowbar 2 (client): the local player's transform state and what reads it, the
*	transform crosshairs, FOV and meters (transform_hud.cpp), the client's player-movement
*	hooks for the transforms (pm_rc2.h), and the race track the RaceStat message sends.
*	All names here are ours (client.dll keeps none).
*
****/

#pragma once

// RC2 keeps the local player's movement mode (pm_rc2.h PM_MODE_*) in its playerclass, so the
// stock g_iPlayerClass (.bss 0x10220c3c, set by HUD_ProcessPlayerState) is the transform.
extern int g_iPlayerClass;

// The houndeye's sleep: TformStat bit 1. While set, mouse look, key look and the sent view
// angles are frozen (input.cpp, inputw32.cpp). .bss 0x10220c40.
extern int g_bHoundeyeAsleep;

// The bat's state from TformStat: 0 flying, 1 walking, 2 perched (PM_GetBatPerch). Perched
// (upside down), the look controls turn the other way. .bss 0x10220c4c.
extern int g_iBatPerch;

// The houndeye's sleep fade, 0 to 1 (RC2_TempEntThink moves it): the screen goes black with it
// (view.cpp's V_UpdateScreenTint). .bss 0x10220c48.
extern float g_flHoundeyeSleepFade;

inline bool TransformViewInverted() // name ours: the original tests this inline everywhere
{
	return g_iPlayerClass == 4 && g_iBatPerch == 2;
}

// From elsewhere:
extern int g_iViewMode;				// hud.cpp .bss 0x1020bb38: the ViewMode message's byte (client.dll 0xa10b0)
extern float g_flTransformFOV;		// hud.cpp .bss 0x1020bb44: the FOV HUD_GetFOV gives in a transform (0xa1f30)
extern int g_iLocalPlayerFrozen;	// rc2/clte.cpp, .bss 0x101f69f8: CLTE 0x2f froze the local player (0x7bff0); locks the view
extern int g_bLocalTransforming;	// rc2/clte.cpp, .bss 0x101f6a00: CLTE 0x40 transforms the local player (0x8ebf0); locks the view

// The view angles input works from: in view mode 3 the last refdef's (view.cpp's v_angles,
// .bss 0x10227148), else the engine's.
void IN_GetInputViewAngles(float* angles); // name ours: inlined in the original

// transform_hud.cpp
void RC2_TempEntThink(float flFrameTime);			// 0xc9e00, from HUD_TempEntUpdate (entity.cpp)
void RC2_HudRedraw(float flTime, float flTimeDelta); // 0xca800, from CHud::Redraw (hud_redraw.cpp)
void TransformHud_VidInit();					   // 0xcb1c0
int __MsgFunc_TformStat(const char* pszName, int iSize, void* pbuf); // 0xcb0a0
int __MsgFunc_RaceStat(const char* pszName, int iSize, void* pbuf);	 // 0xcb710

// the race track (the client side of the race path pm_shared's PM_RaceMove follows)
void PM_GetRacePoint(int iEnt, float* vecOrigin, float* flRadius);					  // 0xcb830
void PM_GetRacePointOffset(int iEnt, int iOffset, float* vecOrigin, float* flRadius); // 0xcb8f0
void PM_NextRacePoint(int iEnt);													  // 0xcb9b0
void PM_GetRaceTrackRadius(int iEnt, float* flRadius);								  // 0xcba00
