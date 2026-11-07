/***
*
*	Rocket Crowbar 2, work package p7: what p7's files use from outside the package, until the
*	merge gives each its real home. Temporary definitions of the ones that link are in
*	p7_stubs.cpp. reference/dossiers/p7.md lists each.
*
****/

#pragma once

class CBaseEntity;

// User messages (registered in player.cpp's LinkUserMessages, p5). Names from the original's
// REG_USER_MSG strings.
extern int gmsgCLTE;	   // "CLTE": client temp effects. byte type, then per type
extern int gmsgWeaponSync; // "WeaponSync"

// UTIL_ExtendTime: the real one is reconstructed elsewhere now

// p7, dlls/rc2/shield.cpp (0xc45b0): the player shield (RC2's [Shield] keys). Returns the damage
// left after the shield of pEntity's player absorbs what its energy allows. Name ours.
float ShieldDeflect(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecHitPos, Vector vecDir, Vector* pvecDeflectPos, Vector* pvecDeflectDir, int iEffect);

// util.cpp (p9), all names ours. Bodies in p7_stubs.cpp are temporary.
// 0xf4c70/0xf4d40: push and pop a set of marked entity indices (a stack of up to 128 bitsets
// of maxEntities bits); 0xf4dd0 tests and 0xf4d90 sets a bit in the top one. Blasts use it so
// each entity is hit once.
void UTIL_PushEntityMarks();
void UTIL_PopEntityMarks();
bool UTIL_IsEntityMarked(int iEntIndex);
void UTIL_MarkEntity(int iEntIndex);
// 0xf33b0: the next valid entity after pStart (by index) whose bounds come within flRadius
// of vecCenter; *pflDist gets the distance to the nearest point, *pvecNearest that point.
CBaseEntity* UTIL_FindEntityNearestInSphere(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecNearest);
// 0xf5160: true when a blast from vecSrc reaches pentTarget at vecEnd: traces, passing
// through non-BSP monsters (recursing, iMaxSteps deep) and punching through world brushes in
// 8-unit steps while *pflPunch lasts (what is left is written back).
bool UTIL_BlastReaches(Vector vecSrc, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iMaxSteps);
// 0xf5920: a direction packed into 30 bits (10 per axis, 0x200 the sign) for an event iparam.
int UTIL_PackDirection(Vector vecDir);
// 0xf5800: vecDir reflected about vecNormal.
Vector UTIL_Reflect(Vector vecDir, Vector vecNormal);

// Added by p7's sound/snark/shrink slice (names ours unless said).
extern int gmsgViewMode; // "ViewMode" (player.cpp, p5): byte mode; the snark cam sends 3 and 0
// 0xf3900 (util.cpp, p9): RC2's radius damage, with a falloff exponent. VERIFY: pentIgnore and
// iFlags are read from the call sites only (snark projectile passes its own edict and 0).
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, edict_t* pentIgnore, Vector vecSrc, float flRadius, float flDamage, float flFalloffExp, int bitsDamageType, int iFlags);
// 0xf58a0 (util.cpp, p9): the inflictor to pass TakeDamage for a named kind of death ("squash");
// keeps the name for the death notice, returns an entity's pev (the world's when none is set).
entvars_t* UTIL_NamedInflictor(const char* pszName);
// 0xf7780 (p9): true when pent stands (groundentity) on one of the 16 entities the player keeps at +0x7a8.
bool UTIL_PlayerPartIsGround(CBaseEntity* pPlayer, edict_t* pent);

// 0x10181434 (weapons.cpp, p3): set to 1 at the top of CBasePlayerWeapon::ItemPostFrame; the
// shotgun only plays events and clears its charge state while it is set (the client's
// g_runfuncs). Name ours.
extern int g_iWeaponRunFuncs;
