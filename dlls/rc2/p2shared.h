/***
*
*	Rocket Crowbar 2: what work package p2's files (the crossbow, crowbar, crowbarang, forklift,
*	doors, effects, egon, explode, breakables and tanks) use from elsewhere, and the few things of
*	theirs other files call. Names marked ours have no original name (rc2.dll is stripped).
*	reference/dossiers/p2.md, "Cross-file needs", lists where each one belongs; the ones whose
*	file isn't reconstructed yet have a TEMP STUB in dlls/rc2/p2stubs.cpp.
*
****/

#pragma once

class CBaseEntity;
class CBasePlayer;

// --- RC2 globals from other files

extern bool g_runfuncs;		// orig .data 0x181434: 1 on the server (weapons.cpp sets it in ItemPostFrame)
extern bool g_fGivingItem;	// orig .data 0x18f3c8, name ours: an item is being handed over (impulse 101, crowbarang pickup); DefaultTouch removes a refused one
extern bool g_fItemTimeout; // orig .data 0x181430, name ours: 1 normally; 0 while a pickup is created, so FallInit doesn't arm its multiplayer timeout
extern int gmsgWeaponSync;	// "WeaponSync": weapon id, then its state bits
extern int gmsgCLTE;		// "CLTE": RC2's client temp entities, a type byte first

// gmsgCLTE types p2 sends (names ours)
#define RC2_CLTE_ROCKET_TRAIL 1			// short entity
#define RC2_CLTE_CROWBARANG_GLINT 0x15	// short entity
#define RC2_CLTE_CROWBARANG_EMBED 0x19	// short entity, short what it is in, 3 coords offset, 3 coords angles
#define RC2_CLTE_KILL 0xff				// short entity (UTIL_KillTrail)

// RC2's own buttons (CBasePlayer +0x708, read through RC2PlayerButtons), names ours
#define RC2_IN_CROWBARANG 0x40 // steers a thrown crowbarang

// GetRCCaps/GetRCCaps2 bits p2 tests (names ours)
#define RC2_CAP_SHOVE 0x1	 // slot 75: may be knocked about
#define RC2_CAP_NOHOME 0x20	 // slot 76: crowbarangs don't home on it

// --- RC2 functions from other files

// rc2.dll 0xf5900 (util): push *pflTime out to flBase + flDelay, never in
void UTIL_ExtendTime(float* pflTime, float flBase, float flDelay);
// rc2.dll 0xf5630 (util): a random direction inside a cone around vecForward, flSpread wide
Vector UTIL_RandomSpreadDir(Vector vecForward, Vector vecRight, Vector vecUp, float flSpread);
// rc2.dll 0xf5800 (util): vecDir mirrored off a plane
Vector UTIL_Reflect(Vector vecDir, Vector vecNormal);
// rc2.dll 0xf55b0 (util): TE_KILLBEAM and a CLTE kill for pent
void UTIL_KillTrail(edict_t* pent);
// rc2.dll 0xc45b0 (the run between scripted and shotgun): the damage left after the target's shield
// takes its share; pvecHit gets the point hit
float RC2ShieldDamage(CBaseEntity* pTarget, float flDamage, float flShieldFraction, Vector vecPos, Vector vecDir, Vector* pvecHit, float* pflOut, int iFlags);
// rc2.dll 0xb28b0: pEnt is one of the two entities pPlayer sees through (CBasePlayer +0x6a4, +0x6a8)
bool IsPlayersViewEnt(edict_t* pPlayer, edict_t* pEnt);
// rc2.dll 0x958b0 (player): RC2's button bits of player iIndex (CBasePlayer +0x708), 0 if none
int RC2PlayerButtons(int iIndex);
// rc2.dll 0x29aa0, CXBowTeather::DieThink (p1's crossbow bolts), which the crossbow calls directly
void XBowTeatherDie(CBaseEntity* pTeather);
// rc2.dll 0xfb780, 0xfb7b0 (weapons): read WeaponSync data; the server's return 0
unsigned char WeaponSync_ReadByte(void** ppBuf, void** ppSize);
float WeaponSync_ReadFloat(void** ppBuf, void** ppSize);

// --- p2's own, used by other files

// rc2.dll 0x31fe0, 0x32540 (dlls/rc2/killmessages.cpp): the death messages. Pick a line for the
// weapon or classname (bSelf: the self forms; iRandom 0..1000), then fill it in.
int KillMsg_Pick(const char* pszName, int bSelf, int iRandom);
const char* KillMsg_Get(int iMsg, const char* pszVictim, const char* pszKiller, const char* pszWeapon, int iGender, int bSelf);

// rc2.dll 0x40ab0 (explode.cpp): an explosion entity of class pszClassname at vecOrigin, blowing
// along vecDir
void ExplodeAt(Vector vecOrigin, Vector vecDir, float flDamage, const char* pszClassname, entvars_t* pevOwner, bool bFlag);
