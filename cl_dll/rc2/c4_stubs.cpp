/***
*
*	TEMP STUB (c4): weak stand-ins for what c4's client files use from files not reconstructed
*	in this worktree (c4_needs.h says where each real one belongs). Weak, so a real definition
*	anywhere replaces it without a duplicate symbol. None carries an orig tag.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "c4_needs.h"

#define C4_WEAK __attribute__((weak))

// TEMP STUB (c4): real ones belong to c2 (CLTE, the detail level) and RC2's entity hiding
C4_WEAK unsigned char g_bEntityHidden[2048]; // MAX_EDICTS; VERIFY: the original's size
C4_WEAK int g_iHighestEntIndex;
C4_WEAK int g_iDetailLevel;
C4_WEAK void CLTE_Update(float frametime) {}
// TEMP STUB (c4): real one belongs in fork V's train .. util file
C4_WEAK void RC2_TempEntThink(float frametime) {}
C4_WEAK void RC2_HudRedraw(float flTime, float flTimeDelta) {}
// TEMP STUB (c4): real one belongs to c2 (0x4fe80)
C4_WEAK void RC2_InitHUD() {}
// TEMP STUB (c4): real one belongs to c5's debug panel
C4_WEAK void DebugPanel_AddEntity(int type, struct cl_entity_s* ent, const char* modelname) {}
// TEMP STUB (c4): real ones belong in vgui_TeamFortressViewport.cpp (c5)
C4_WEAK void TeamFortressViewport::ToggleServerBrowser() {}
C4_WEAK bool TeamFortressViewport::MsgFunc_AttrConf(const char* pszName, int iSize, void* pbuf) { return false; }
C4_WEAK bool TeamFortressViewport::MsgFunc_VoteBegin(const char* pszName, int iSize, void* pbuf) { return false; }
C4_WEAK bool TeamFortressViewport::MsgFunc_VoteUpdate(const char* pszName, int iSize, void* pbuf) { return false; }
C4_WEAK void TeamFortressViewport::SetDebugPanelVisible(bool bVisible) {}
C4_WEAK bool TeamFortressViewport::MsgFunc_DBGCounts(const char* pszName, int iSize, void* pbuf) { return false; }
C4_WEAK bool TeamFortressViewport::MsgFunc_DBGEntInfo(const char* pszName, int iSize, void* pbuf) { return false; }

// TEMP STUB (c4): real ones belong to c2 (0x4f4f0, 0x510b0) and c1 (0x3b900)
C4_WEAK int __MsgFunc_SetAttr(const char* pszName, int iSize, void* pbuf) { return 0; }
C4_WEAK int __MsgFunc_CLTE(const char* pszName, int iSize, void* pbuf) { return 0; }
C4_WEAK int __MsgFunc_WeaponSync(const char* pszName, int iSize, void* pbuf) { return 0; }
C4_WEAK void TeamFortressViewport::HideDebugPanels() {}
