/***
*
*	TEMP STUB (c4): a weak stand-in for what c4's client files use from package c1's range
*	(client.dll below 0x4ec50, still being reconstructed). Weak, so the real definition replaces
*	it without a duplicate symbol. It carries no orig tag.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "c4_needs.h"

#define C4_WEAK __attribute__((weak))

// TEMP STUB (c4): real one belongs to c1 (0x3b900, the weapons' "WeaponSync" message)
C4_WEAK int __MsgFunc_WeaponSync(const char* pszName, int iSize, void* pbuf) { return 0; }
