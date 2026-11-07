/***
*
*	Rocket Crowbar 2, work package p4: what p4's files use from other packages' files.
*
*	Every name here is ours (rc2.dll keeps none for these), chosen from what the function
*	does as read from p4's call sites and the function's own body; the owning package may
*	have named it differently, and the names are reconciled at merge. Each has a temporary
*	definition in p4_stubs.cpp until its real one lands. reference/dossiers/p4.md lists them.
*
****/

#pragma once

class CBaseEntity;
class CBasePlayer;

// orig 0xc45b0 lives in p7 ("new: scripted .. shotgun"). A player's shield taking a hit: when
// pEntity->GetEffectedEnt() is an active player with the shield up (player +0x778), draws
// flDamage * flEnergyScale * [Shield] BaseEnergyScale from its energy (player +0x758), sends the
// shield-hit message, drops the shield below [Shield] MinRunEnergy, and returns the damage the
// shield didn't stop (flDamage when there is no shield). vecSrc is where the hit comes from,
// vecDir its direction (g_vecZero: from vecSrc to the player); *pvecHit gets the point on the
// shield, *pvecNormal the shield's normal there; iSize (0-15) goes into the message. Name ours.
float ShieldAbsorbDamage(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecSrc, Vector vecDir, Vector* pvecHit, Vector* pvecNormal, int iSize);

// orig 0xf5800 lives in p9 (util): vecDir mirrored about the plane with normal vecNormal.
// Name ours.
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal);

// RC2's "CLTE" user message (client temp entities: a type byte, then that type's data),
// registered with the rest in LinkUserMessages (orig gmsg at 0x18f4b8). p4 sends types 25
// (freeze grenade stuck on something) and 49 (freeze grenade trail). Name ours, after the
// message's own.
extern int gmsgCLTE;

// orig 0xf2fe0 (p9, util): pent is a live entity other than the world: not free, not
// FL_KILLME, has private data, and for a player slot one whose bit is set in the original's
// client mask at 0x1af2a8. Name ours.
int UTIL_IsValidEdict(edict_t* pent);

// orig 0xf5920 (p9, util): a direction packed into an int, as event iparams carry it (each
// component's magnitude in 9 bits plus a sign bit 0x200; x at bit 0, y at 10, z at 20). Name ours.
int UTIL_PackDirection(Vector vecDir);

// orig 0xf4c70, 0xf4d40, 0xf4d90, 0xf4dd0 (p9, util): a stack (128 deep) of per-entity bit
// sets, so an area effect hits each entity once: push a clear set, mark an entity index,
// ask whether one is marked, pop. Names ours.
void UTIL_PushEntityMask();
void UTIL_PopEntityMask();
void UTIL_MarkEntity(int iEntity);
bool UTIL_IsEntityMarked(int iEntity);

// orig 0xf5900 (p9, util): *pflTime = max(*pflTime, flBase + flDelay). Name ours.
void UTIL_ExtendTime(float* pflTime, float flBase, float flDelay);

// orig 0xf58a0 (p9, util): the entvars of a shared stand-in entity whose classname is set to
// pszName, used as the inflictor so a kill reads as that weapon (the world's when the
// stand-in doesn't exist). Name ours.
entvars_t* UTIL_NamedInflictor(const char* pszName);

// orig .data 0x181434 (p3, weapons.cpp): set to 1 at the top of CBasePlayerWeapon::
// ItemPostFrame; the RC2 weapons play their events only when it is set. Name ours.
extern int g_iRC2WeaponFrame;

// orig 0x95890 (p5, player): the value of attribute iAttr of the player with entity index
// iPlayer (player +0x6e0 + 4 * iAttr; 1.0 when there is no such player). The battery reads 6
// and 7. Name ours.
float RC2_PlayerAttribute(int iAttr, int iPlayer);

// orig 0xfa100 (p9, "game_shared_voice_gamemgr .. weapons"): with mp_random set in multiplayer,
// replaces pItem with a random item or weapon (mp_random 1 or 3 pick from one list, others from
// another) at the same place and spawnflags, and removes pItem. Name ours.
void ReplaceWithRandomItem(CBaseEntity* pItem);

// orig 0xf5a20 (p9, util): RC2's list of every classname a factory or script asks for (RC
// 1.9's AddToPrecachedList; every LINK_ENTITY_TO_CLASS factory calls it in RC2).
void AddToPrecachedList(const char* pszClassname);

// orig 0xf5bf0 (p9, util): the list's own copy of pszClassname (128-byte entries), a string
// that outlives the caller's buffer, for CBaseEntity::Create. Name ours.
const char* UTIL_PersistentClassname(const char* pszClassname);

// orig 0x2660 (p1, "start .. aflock"): the entity the player pevPlayer is aiming at for the
// LCS commands (an 8192-unit trace from the eyes, worldspawn excluded, through
// GetEffectedEnt), NULL when none. Name ours. VERIFY: the player +0x78c branch at its end
CBaseEntity* GetAimedEntity(entvars_t* pevPlayer);

// orig 0x4f00, 0x4eb0, 0x5040 (p1): RC2's entity alteration language, applied by the LCS
// lines advcreate (create pszClassname and alter it), edit (alter pent once) and maintain (a
// maintainer_ent that keeps altering pent). pszArgs is the whole script line; iFlag as the
// LCS loader passes it (1 for advcreate, 0 otherwise). Names ours.
void AdvCreateEntity(const char* pszClassname, const char* pszArgs, int iFlag);
void EditEntity(edict_t* pent, const char* pszArgs, int iFlag);
void MaintainEdit(edict_t* pent, const char* pszArgs, int iFlag);

// orig .data 0x18be80 and 0x136078 (p1): the spawn command an LCS setspawncommand line sets
// (512 chars), and the flag cleared with it. Names ours.
extern char g_szRC2SpawnCommand[512];
extern int g_iRC2SpawnCommandRun;

// orig 0xf33b0 (p9, util): the next entity after pStart within flRadius of vecCenter, with its
// distance in *pflDist and the point measured to in *pvecSpot. Name ours. VERIFY: how the
// point is chosen (nearest point of the box or the center)
CBaseEntity* UTIL_FindEntityInSphereDist(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecSpot);

// RC2's "WeaponSync" user message (orig gmsg at 0x18f4c0): a weapon id byte, then that weapon's
// sync state (the MP5: its three bolt counts as shorts). Name ours, after the message's.
extern int gmsgWeaponSync;

// orig 0x95860 (p5, player): the player's attribute flags (0x958b0) have bit 0x200, which lets
// the MP5 fire under water. Name ours.
bool RC2_PlayerFiresUnderwater(CBasePlayer* pPlayer);

// orig 0xfb790 (p3, weapons.cpp): reads a short of synced weapon state, advancing *ppData and
// *piSize; the server's copy returns 0. Its siblings read a byte (0xfb780) and a float (0xfb7b0).
// Name ours. VERIFY: argument types
short ReadSyncShort(void** ppData, int* piSize);

// orig .data 0x149c68 and 0x149c6c (p1): the player and entity an alteration is for, what a p
// or e without a number means in an alteration expression (EditEntity and AdvCreateEntity set
// both to the altered entity's index). Names ours.
extern int g_iRC2AlterPlayer;
extern int g_iRC2AlterEntity;
