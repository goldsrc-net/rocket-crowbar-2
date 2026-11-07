/***
*
*	Rocket Crowbar 2: what the transform files (tassassin, tbat, tcontroller, theadcrab,
*	thoundeye, transform) use from code other packages reconstruct, under our names with the
*	original's addresses. The ones nothing defines yet have a TEMP STUB in
*	p8_stubs_transform.cpp. reference/dossiers/p8.md, "Cross-file needs".
*
****/

#pragma once

#include "p8_needs.h"

class CBaseEntity;
class CBasePlayer;

// Events, precached in CBasePlayer::Precache (player.cpp), at 0x18f3f4-0x18f3fe
extern unsigned short g_usTAssassinGun;
extern unsigned short g_usTAssassinKick;
extern unsigned short g_usTHoundeyeBeam;
extern unsigned short g_usTHoundeyeSonic;
extern unsigned short g_usTControllerThrow;
extern unsigned short g_usTBatBite;

// User messages (player.cpp's LinkUserMessages)
extern int gmsgCLTE;	  // "CLTE", at 0x18f4b8: RC2's client temp entities (a type byte; 0xfe kills one)
extern int gmsgTformStat; // "TformStat", at 0x18f4c8: one byte of transform state for the HUD

// at 0xf58a0 (util): the entvars of a stand-in inflictor whose classname is set to pszName (for
// death notices); the world's when there is none. Name ours.
entvars_t* UTIL_NamedInflictor(const char* pszName);

// at 0xf33b0 (util): the next entity after pStart whose bounds come within flRadius of
// vecCenter, with the distance and the closest point. Name ours.
CBaseEntity* UTIL_FindEntityNearSphere(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecClosest);

// at 0xf5920 (util): a unit vector packed into an int for an event parameter. Name ours.
int UTIL_PackNormal(Vector vecNormal);

// at 0xf4c70, 0xf4dd0, 0xf4d90, 0xf4d40 (util): a stack of per-entity "already hit" bit sets,
// so a blast hits each entity once. Names ours.
void UTIL_PushEntFlags();
bool UTIL_TestEntFlag(int iEntIndex);
void UTIL_SetEntFlag(int iEntIndex);
void UTIL_PopEntFlags();

// at 0xf5160 (util): a trace from vecStart to vecEnd (ignoring monsters) that, when it stops in
// solid, steps through along vecDir in 8-unit steps while *pflPunch lasts and carries on from
// where it comes out (recursing up to iDepth times; a non-brush entity hit is traced past).
// True when it reaches the end or pentTarget. Leaves the last trace in *ptr. Name ours.
int UTIL_PunchTrace(Vector vecStart, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iDepth);

// g_pGameRules' virtual at byte offset 0xe8 (slot 58), which the assassin's kick asks before it
// knocks pVictim about. Not an SDK 2.3 slot; the gamerules package names and declares it. Ours.
bool RC2_GameRulesCanKnock(CBasePlayer* pAttacker, CBaseEntity* pVictim);

// Transform helpers other files call (defined in our files)
int TBat_GetRoost(int iPlayer, int* piEntIndex, Vector* pvecOffset);
void TBat_Unroost(int iPlayer);
void THeadcrab_SetLeaping(int iPlayer, int iLeaping);
int THeadcrab_IsLeaping(int iPlayer);
int THeadcrab_LeapHit(int iPlayer, int iVictim, float flSpeed);
int Transform_DeathDirection(CBasePlayer* pPlayer);
int Transform_OfPlayer(int iPlayer);
void Transform_Precache();
int Transform_Classify(int iTransform);
