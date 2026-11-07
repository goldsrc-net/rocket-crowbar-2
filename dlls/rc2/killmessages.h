/***
*
*	Rocket Crowbar 2: the death messages (dlls/rc2/killmessages.cpp). Names ours.
*
****/

#pragma once

// A line for the weapon or classname pszName (bSelf: the self forms; iRandom 0..1000 picks
// among the weighted lines, aliases followed); 0, the generic line, when there is none
int KillMsg_Pick(const char* pszName, int bSelf, int iRandom);
// Line iMsg filled in, in a static buffer
const char* KillMsg_Get(int iMsg, const char* pszVictim, const char* pszKiller, const char* pszWeapon, int iGender, int bSelf);
