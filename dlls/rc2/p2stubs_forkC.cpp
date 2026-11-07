/***
*
*	Work package p2's temporary stubs for the egon and shrinker (see dlls/rc2/p2stubs.cpp). None
*	carries an orig address; each says where the real one belongs. Delete each at merge.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

// TEMP STUB (p2): real one belongs in player.cpp (0x95860)
int RC2PlayerWaterWeapons(CBasePlayer* pPlayer)
{
	return 0;
}

// RC2ReadStateShort: the real one is reconstructed elsewhere now

// UTIL_PackNormal: the real one is reconstructed elsewhere now

// UTIL_NamedInflictor: the real one is reconstructed elsewhere now
