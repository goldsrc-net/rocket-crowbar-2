/***
*
*	Rocket Crowbar 2: temporary definitions of what the client's studio renderer
*	(StudioModelRenderer.cpp) uses from other packages' files (cl_dll/rc2/studio_rc2.h).
*
****/

#include "hud.h"
#include "cl_util.h"
#include "rc2/studio_rc2.h"

// TEMP STUB (c4): real ones belong in the client-tent and entity-state code (client.dll
// 0x522f0, 0x59830-0x59be0, 0x4f4c0: c2), the client frame (0x4ff90: c2), the view
// (0xe9ee0: the package after c4). Weak, so the real ones win. Each answers "nothing set",
// so models draw as in SDK 2.3.
__attribute__((weak)) unsigned char g_rgbEntAftexShell[1024];
__attribute__((weak)) unsigned int g_iRC2FrameCount;
__attribute__((weak)) bool g_bViewChasing;

__attribute__((weak)) void RC2_SetEntityDrawOrigin(int iEntIndex, Vector vecOrigin) {}
__attribute__((weak)) bool RC2_GetEntityShellColor(int iEntIndex, float* vecColor, float* pflAmount) { return false; }
__attribute__((weak)) int RC2_GetPlayerStateBits(int iPlayer) { return 0; }
