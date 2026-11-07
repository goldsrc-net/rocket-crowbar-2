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

// TEMP STUB (p2): real one belongs in weapons.cpp (0xfb790)
short WeaponSync_ReadShort(void** ppBuf, void** ppSize)
{
	return 0;
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf5920)
int UTIL_PackNormal(Vector vecNormal)
{
	return 0;
}

// TEMP STUB (p2): real one belongs in util.cpp (0xf58a0)
entvars_t* UTIL_NamedInflictor(const char* pszName)
{
	return VARS(INDEXENT(0));
}
