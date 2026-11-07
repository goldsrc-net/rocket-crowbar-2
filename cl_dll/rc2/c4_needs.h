/***
*
*	Rocket Crowbar 2: what package c4's client files (death, entity, ev_common, health, hud,
*	hud_msg, hud_redraw, the spectator, menu, saytext, the status bar and the RC2 HUD elements)
*	use from other client files. Names ours unless SDK 2.3 has them; client.dll addresses.
*	The ones whose file isn't reconstructed in this worktree have weak TEMP STUBs in
*	c4_stubs.cpp, so the real definitions win at the merge (reference/dossiers/c4.md).
*
****/

#pragma once

struct cl_entity_s;

// .data 0x101f24c4, one byte per entity index: the entity is held by one of RC2's client temp
// entities (c2's CLTE). Its studio events are skipped (HUD_StudioEvent) and the renderer draws it
// uninterpolated with a white shell (fork S). VERIFY: name ours, "hidden" fits only the events.
extern unsigned char g_bEntityHidden[];

// .bss 0x10206918: one past the highest ET_NORMAL/ET_PLAYER entity index HUD_AddEntity saw.
extern int g_iHighestEntIndex;

// .data 0x101a9598: the detail level from cl_detail (c2's 0x4ff90 copies it each frame):
// below 0 low, 0 normal, above 0 high.
extern int g_iDetailLevel;

// 0x523f0 (c2): RC2's client temp entities, updated each frame before the engine's.
void CLTE_Update(float frametime);
// 0xc9e00 (fork V of c4, "train .. util"): the second per-frame update HUD_TempEntUpdate runs.
void RC2_TempEntThink(float frametime);
// 0xca800 (fork V of c4, "train .. util"): RC2's own HUD drawing, after the elements (CHud::Redraw)
void RC2_HudRedraw(float flTime, float flTimeDelta);
// 0x4fe80 (c2): RC2's part of a new game's HUD reset (CHud::MsgFunc_InitHUD)
void RC2_InitHUD();
// 0xd9060 (c5, the debug panel): records players' centres while the debug panel shows.
void DebugPanel_AddEntity(int type, struct cl_entity_s* ent, const char* modelname);

// The message handlers CHud::Init hooks that live in other files (client.dll addresses):
int __MsgFunc_SetAttr(const char* pszName, int iSize, void* pbuf);	  // 0x4f4f0 (c2)
int __MsgFunc_CLTE(const char* pszName, int iSize, void* pbuf);		  // 0x510b0 (c2)
int __MsgFunc_WeaponSync(const char* pszName, int iSize, void* pbuf); // 0x3b900 (c1)
int __MsgFunc_PullerCtrl(const char* pszName, int iSize, void* pbuf); // 0xbd1f0 (fork P: cl_dll/rc2/pullers_client.cpp)
int __MsgFunc_RCCFG(const char* pszName, int iSize, void* pbuf);	  // 0xbd5c0 (fork P: cl_dll/rc2/rc2prefs_client.cpp)
int __MsgFunc_PlrStat(const char* pszName, int iSize, void* pbuf);	  // 0xaf9b0 (fork P: cl_dll/rc2/pm_client.cpp)
int __MsgFunc_TformStat(const char* pszName, int iSize, void* pbuf);  // 0xcb0a0 (fork V: cl_dll/rc2/transform_hud.cpp)
int __MsgFunc_RaceStat(const char* pszName, int iSize, void* pbuf);	  // 0xcb710 (fork V: cl_dll/rc2/transform_hud.cpp)

// --- c4's own, used by other files

// entity.cpp (client 0x9e770): a client gib (RC2's client effects throw them)
struct tempent_s* CL_SpawnGib(Vector vecOrigin, Vector vecVelocity, int bHuman, int bSkull);
