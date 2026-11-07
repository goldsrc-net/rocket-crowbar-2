/***
*
*	Rocket Crowbar 2: what package c4's client files (death, entity, ev_common, health, hud,
*	hud_msg, hud_redraw, the spectator, menu, saytext, the status bar and the RC2 HUD elements)
*	use from other client files. Names ours unless SDK 2.3 has them; client.dll addresses.
*	Each is declared here under the name of its real definition (reference/dossiers/w3-reconcile.md);
*	what is still nobody's (package c1's) has a stand-in in c4_stubs.cpp.
*
****/

#pragma once

struct cl_entity_s;

// .bss 0x101f24c4 (rc2/clte.cpp), one byte per entity index: set while CLTE 0x2f holds the entity
// frozen. Its studio events are skipped (HUD_StudioEvent) and the renderer draws it uninterpolated
// with a white shell (fork S).
extern unsigned char g_bEntityFrozen[1024];

// .bss 0x10206918 (entity.cpp): one past the highest ET_NORMAL/ET_PLAYER entity index HUD_AddEntity saw.
extern int g_iHighestEntIndex;

// .data 0x101a9598 (cdll_int.cpp): the detail level from cl_detail (HUD_Frame copies it each
// frame): below 0 low, 0 normal, above 0 high.
extern int g_iClientDetail;

// 0x523f0 (rc2/clte.cpp): RC2's client temp entities, updated each frame before the engine's.
void CLTE_Update(float flFrametime);
// 0xc9e00 (rc2/transform_hud.cpp): the second per-frame update HUD_TempEntUpdate runs.
void RC2_TempEntThink(float flFrameTime);
// 0xca800 (rc2/transform_hud.cpp): RC2's own HUD drawing, after the elements (CHud::Redraw)
void RC2_HudRedraw(float flTime, float flTimeDelta);
// 0x4fe80 (cdll_int.cpp): RC2's state cleared by HUD_Init, HUD_VidInit, HUD_Reset and CHud::MsgFunc_InitHUD
void RC2_ResetClientState();
// 0xd9060 (rc2/vgui_PlayerID.cpp): records players' centres for the player name label.
void PlayerID_AddEntity(int type, struct cl_entity_s* ent);

// The message handlers CHud::Init hooks that live in other files (client.dll addresses):
int __MsgFunc_SetAttr(const char* pszName, int iSize, void* pbuf);	  // 0x4f4f0 (rc2/attributes.cpp)
int __MsgFunc_CLTE(const char* pszName, int iSize, void* pbuf);		  // 0x510b0 (rc2/clte.cpp)
int __MsgFunc_WeaponSync(const char* pszName, int iSize, void* pbuf); // 0x3b900 (c1)
int __MsgFunc_PullerCtrl(const char* pszName, int iSize, void* pbuf); // 0xbd1f0 (fork P: cl_dll/rc2/pullers_client.cpp)
int __MsgFunc_RCCFG(const char* pszName, int iSize, void* pbuf);	  // 0xbd5c0 (fork P: cl_dll/rc2/rc2prefs_client.cpp)
int __MsgFunc_PlrStat(const char* pszName, int iSize, void* pbuf);	  // 0xaf9b0 (fork P: cl_dll/rc2/pm_client.cpp)
int __MsgFunc_TformStat(const char* pszName, int iSize, void* pbuf);  // 0xcb0a0 (fork V: cl_dll/rc2/transform_hud.cpp)
int __MsgFunc_RaceStat(const char* pszName, int iSize, void* pbuf);	  // 0xcb710 (fork V: cl_dll/rc2/transform_hud.cpp)

// --- c4's own, used by other files

// entity.cpp (client 0x9e770): a client gib (RC2's client effects throw them)
struct tempent_s* CL_SpawnGib(Vector vecOrigin, Vector vecVelocity, int bHuman, int bSkull);
