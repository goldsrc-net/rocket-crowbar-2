/***
*
*	Rocket Crowbar 2: rc2prefs.cpp. rc2.ini's values, and (to be reconstructed) its loader
*	and writer, the original's functions at 0xaea60-0xb28b0. See rc2prefs.h.
*
****/

#include "extdll.h"
#include "util.h"
#include "rc2prefs.h"

// orig .bss 0x191bc8: one 8-byte slot per key of g_rc2Prefs
rc2prefvalue_t g_rc2PrefValues[RC2P_COUNT];
