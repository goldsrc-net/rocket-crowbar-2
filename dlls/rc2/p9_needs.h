/***
*
*	Rocket Crowbar 2, work package p9: what p9's files use from outside the package, until the
*	merge gives each its real home. The temporary definitions of the ones that need one to link
*	are in p9_stubs.cpp, each marked TEMP STUB. reference/dossiers/p9.md lists each.
*
****/

#pragma once

class CBaseEntity;

// User message "WeaponSync" (orig .bss 0x1018f4c0), registered with size -1 in player.cpp's
// LinkUserMessages (p5). The weapons' WeaponFrame sends their state with it.
extern int gmsgWeaponSync;
// User message "CLTE" (orig .bss 0x1018f4b8), RC2's client temp effects; same place.
extern int gmsgCLTE;
// User messages "VoteBegin" (orig 0x1018f4d4: byte can vote, string what) and "VoteUpdate"
// (0x1018f4d8: byte yes, byte no, byte seconds left); same place.
extern int gmsgVoteBegin;
extern int gmsgVoteUpdate;

// p1's (orig 0x25e0, static in its maintainent.cpp): a player by index, else the first whose
// name contains pszName.
class CBasePlayer;
CBasePlayer* Admin_FindPlayer(const char* pszName);

// The player shield (orig 0xc45b0, p7's "new: scripted .. shotgun"; p7 names it ShieldDeflect):
// the damage left of flDamage after the shield of pEntity's player takes what its energy allows.
float ShieldDeflect(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecHitPos, Vector vecDir, Vector* pvecDeflectPos, Vector* pvecDeflectDir, int iEffect);

// tripmine.cpp (p8): the player who placed a tripmine, the EHANDLE RC2 added to CTripmineGrenade
// at orig +0x304 (bouncy_, punch_ and target_tripmine have it too). CTripmineGrenade is declared
// in tripmine.cpp, so p9 can't read it directly. Name ours.
CBaseEntity* TripmineGrenade_GetPlacer(CBaseEntity* pMine);

