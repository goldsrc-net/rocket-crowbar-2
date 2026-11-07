/***
*
*	Rocket Crowbar 2: the start of rc2.dll's .text, before SDK 2.3's aflock.cpp (orig
*	0x1000-0x5440). One or more files of RC2's own whose names are lost: the in-game admin
*	commands, the LCS entity scripting that maps/lcs/*.lcs and the admin commands run on
*	entities, the admin camera and CMaintainEnt (maintainer_ent), which re-runs an LCS script
*	on an entity every frame. Named after its one class, as dlls/rc2/README.md asks.
*
*	Every name here is ours (marked at first use) unless it is a classname, a string or an
*	exported method. reference/dossiers/p1.md has the function map.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "trains.h"
#include "client.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// --- from other packages (declared here; see the dossier's cross-file needs) ---

// util (p9), orig 0xf5a20 / 0xf5a80 / 0xf5bf0: RC 1.9's classname precache list. The last one
// (ours) returns the list's own copy of the classname, or the argument when it isn't listed.
extern void AddToPrecachedList(const char* pszClassname);
extern bool IsClassnamePrecachedNoInvalid(const char* pszClassname);
extern const char* GetPrecachedClassname(const char* pszClassname); // name ours

// The LCS expression parser (p4, the run between maprules and monstermaker), names ours:
// orig 0x73fb0 splits a script into whitespace separated tokens (brackets, braces, parens and
// quotes group) and calls pfnToken(index, token) for each; orig 0x71c60 / 0x72a90 evaluate a
// token's value as a float / a vector.
typedef void (*LCSTOKENFUNC)(int iArg, char* pszToken);
extern void LCS_ParseTokens(char* pszScript, LCSTOKENFUNC pfnToken);
extern float LCS_EvalFloat(char* pszToken);
extern Vector LCS_EvalVector(char* pszToken);
// orig 0x6db00 (p4): the "lcs" command, its subcommand at argument iFirstArg
extern void LCS_Command(int iFirstArg, edict_t* pClient); // name ours
// orig 0xf7f20 (p9): ends a running vote
extern void StopVote(); // name ours

// user messages RC2 added (registered with the others, in p8's player.cpp or wherever
// LinkUserMessages lands)
extern int gmsgViewMode; // "ViewMode", 1 byte
extern int gmsgCLTE;	 // "CLTE", variable

// --- admin rights ---

// orig 0x1000. True for the listen server's host (entity 1 on a non-dedicated server).
bool Admin_IsListenHost(edict_t* pEntity) // name ours
{
	if (!pEntity)
		return false;
	if (0 == OFFSET(pEntity) || !pEntity->pvPrivateData || 0 != pEntity->free)
		return false;
	if (IS_DEDICATED_SERVER())
		return false;
	return ENTINDEX(pEntity) == 1;
}

// orig 0x1050. The admin level adminprim.cfg gives this player's WON id: lines of
// "<wonid> <level>" ('#' comments), the highest level that matches. The listen host is 2.
int Admin_GetLevel(edict_t* pEntity) // name ours
{
	if (Admin_IsListenHost(pEntity))
		return 2;
	if (!pEntity)
		return 0;

	unsigned int wonid = g_engfuncs.pfnGetPlayerWONId(pEntity);
	if (0 == wonid)
		return 0;

	FILE* pFile = fopen("adminprim.cfg", "r");
	if (!pFile)
		return 0;

	int iLevel = 0;
	char szLine[256];
	// VERIFY: the original's two sscanf targets are uninitialised stack slots, so a line that
	// doesn't parse reuses the last line's values; ours start at 0
	unsigned int lineWonid = 0;
	int lineLevel = 0;
	while (fgets(szLine, 255, pFile))
	{
		szLine[255] = '\0';
		if (strlen(szLine) > 1 && szLine[0] != '#')
		{
			sscanf(szLine, "%u %i", &lineWonid, &lineLevel);
			if (lineWonid == wonid && iLevel < lineLevel)
				iLevel = lineLevel;
		}
	}
	fclose(pFile);
	return iLevel;
}

// orig 0x1160. True when the player's WON id is listed in sv_tempadmins (separated by
// " \t\n\r,;").
static bool Admin_IsTempAdmin(edict_t* pEntity) // name ours
{
	int wonid = g_engfuncs.pfnGetPlayerWONId(pEntity);
	char szList[128];
	// the original strcpy's the whole cvar into its 128-byte buffer (a stack overrun when the
	// cvar is longer); ours truncates (dossier: original bugs)
	strncpy(szList, CVAR_GET_STRING("sv_tempadmins"), sizeof(szList) - 1);
	szList[127] = '\0';

	for (char* pszTok = strtok(szList, " \t\n\r,;"); pszTok; pszTok = strtok(NULL, " \t\n\r,;"))
	{
		if (atoi(pszTok) == wonid)
			return true;
	}
	return false;
}

// orig 0x1200. True when pszToken begins with pszKey, ignoring case.
static bool LCS_TokenIs(const char* pszToken, const char* pszKey) // name ours
{
	const size_t len = strlen(pszKey);
	if (strlen(pszToken) < len)
		return false;
	for (size_t i = 0; i < len; i++)
	{
		if (tolower(pszToken[i]) != tolower(pszKey[i]))
			return false;
	}
	return true;
}

// --- what the admin commands do to one entity (the callbacks of Admin_ForEachEntity) ---

// orig 0x1270: use (toggle), the world as activator and caller
static void Admin_Use(CBaseEntity* pEntity) // name ours
{
	CBaseEntity* pWorld = CBaseEntity::Instance(INDEXENT(0));
	pEntity->Use(pWorld, pWorld, USE_TOGGLE, 0);
}

static USE_TYPE g_AdminUseType = USE_TOGGLE; // name ours; orig 0x136070, set by useset

// orig 0x12e0: useset's use, of the type it was given
static void Admin_UseSet(CBaseEntity* pEntity) // name ours
{
	CBaseEntity* pWorld = CBaseEntity::Instance(INDEXENT(0));
	pEntity->Use(pWorld, pWorld, g_AdminUseType, 0);
}

// orig 0x1350: kill. Not clients, not brush entities, only what IsCleanupKillable allows (the
// monsters); the world is the attacker.
static void Admin_Kill(CBaseEntity* pEntity) // name ours
{
	if ((pEntity->pev->flags & FL_CLIENT) != 0)
		return;
	if (pEntity->IsBSPModel())
		return;
	if (!pEntity->IsCleanupKillable())
		return;
	pEntity->Killed(VARS(INDEXENT(0)), GIB_NORMAL);
}

// orig 0x13b0: mkally. A monster stops being provoked and forgets players as enemies.
static void Admin_MakeAllied(CBaseEntity* pEntity) // name ours
{
	if ((pEntity->pev->flags & FL_MONSTER) == 0)
		return;
	CBaseMonster* pMonster = static_cast<CBaseMonster*>(pEntity);
	pMonster->m_fAdminAllied = true;
	pMonster->m_afMemory &= ~bits_MEMORY_PROVOKED;
	if (pMonster->m_hEnemy != NULL && pMonster->m_hEnemy->IsPlayer())
		pMonster->m_hEnemy = NULL;
	for (int i = 0; i < MAX_OLD_ENEMIES; i++)
	{
		if (pMonster->m_hOldEnemy[i] != NULL && pMonster->m_hOldEnemy[i]->IsPlayer())
			pMonster->m_hOldEnemy[i] = NULL;
	}
}

// orig 0x1450: mkhostile
static void Admin_MakeHostile(CBaseEntity* pEntity) // name ours
{
	if ((pEntity->pev->flags & FL_MONSTER) == 0)
		return;
	CBaseMonster* pMonster = static_cast<CBaseMonster*>(pEntity);
	pMonster->m_fAdminAllied = false;
	pMonster->m_afMemory |= bits_MEMORY_PROVOKED;
}

// orig 0x1480: remove (anything but players)
static void Admin_Remove(CBaseEntity* pEntity) // name ours
{
	if (pEntity->IsPlayer())
		return;
	// VERIFY: the original compares the entity's index with maxClients, so a non-player in a
	// client slot isn't removed either
	if (ENTINDEX(pEntity->edict()) > gpGlobals->maxClients)
		UTIL_Remove(pEntity);
}

// orig 0x14c0 (unlisted: no function at it in the decompile): fixtrain. Lets players drive a
// train again (clears its no-control and forward-only flags) and re-enables a path_track that
// disables trains.
static void Admin_FixTrain(CBaseEntity* pEntity) // name ours
{
	const char* pszClassname = STRING(pEntity->pev->classname);
	if (!strcmp(pszClassname, "func_tracktrain"))
		pEntity->pev->spawnflags &= ~(SF_TRACKTRAIN_NOCONTROL | SF_TRACKTRAIN_FORWARDONLY);
	else if (!strcmp(pszClassname, "path_track"))
		pEntity->pev->spawnflags &= ~SF_PATH_DISABLE_TRAIN;
}

// --- LCS entity scripting ---

// LCS state, all ours: the entity a script works on, the first token it reads, the indices
// "owner" and the script's selection use, and whether the entity hasn't spawned yet.
static edict_t* g_pentLCS = NULL;		 // orig 0x18be78
static int g_iLCSFirstArg = 1;			 // orig 0x136074
int g_iLCSPlayerScriptFirstArg = 2;		 // orig 0x136078: the map's player script, set by the LCS loader
static int g_iLCSCallerIndex = 0;		 // orig 0x149c68
static int g_iLCSSelectedIndex = 0;		 // orig 0x149c6c
static bool g_fLCSPreSpawn = false;		 // orig 0x18be7c
char g_szLCSPlayerScript[0x200];		 // orig 0x18be80: run on every player (CBasePlayer); the loader fills it, CWorld clears it
static char g_szLCSKeyName[256];		 // orig 0x183c70
static char g_szLCSKeyValue[256];		 // orig 0x183d70
static char g_szLCSStrings[512][64];	 // orig 0x183e70: strings LCS sets as targetname/target
static int g_iLCSStringCount = 0;		 // orig 0x18be70
static int g_iLCSStringNext = 0;		 // orig 0x18be74

// orig 0x1580. A persistent copy of the string for MAKE_STRING: an existing one if the pool
// already holds it, else the next of 512 64-byte slots.
static char* LCS_AllocString(const char* psz) // name ours
{
	for (int i = 0; i < g_iLCSStringCount; i++)
	{
		if (!strcmp(psz, g_szLCSStrings[i]))
			return g_szLCSStrings[i];
	}

	if (g_iLCSStringNext < 512)
	{
		if (g_iLCSStringCount < 512)
			g_iLCSStringCount++;
		// the original strcpy's unbounded into 64 bytes (dossier: original bugs)
		strcpy(g_szLCSStrings[g_iLCSStringNext], psz);
		return g_szLCSStrings[g_iLCSStringNext++];
	}

	// full: the original goes back to slot 0 but doesn't advance past it, so the next string
	// overwrites slot 0 again (dossier: original bugs)
	g_iLCSStringNext = 0;
	strcpy(g_szLCSStrings[0], psz);
	return g_szLCSStrings[0];
}

// orig 0x1670 (unlisted: no function at it in the decompile, read from the disassembly). One
// token of an entity script: a key (matched by prefix, case ignored, first match wins) and its
// value, which the parser evaluates out of the whole token. Tokens before g_iLCSFirstArg are
// skipped (the command words), and no client nor the world is changed.
static void LCS_EntityToken(int iArg, char* pszToken) // name ours
{
	if (!g_pentLCS || iArg < g_iLCSFirstArg)
		return;
	if (UTIL_IsValidEntity(g_pentLCS) && ENTINDEX(g_pentLCS) <= gpGlobals->maxClients)
		return;

	entvars_t* pev = &g_pentLCS->v;

	if (LCS_TokenIs(pszToken, "vel"))
		pev->velocity = LCS_EvalVector(pszToken);
	else if (LCS_TokenIs(pszToken, "avel"))
		pev->avelocity = LCS_EvalVector(pszToken);
	else if (LCS_TokenIs(pszToken, "ang"))
		pev->angles = LCS_EvalVector(pszToken);
	else if (LCS_TokenIs(pszToken, "v_a"))
		pev->v_angle = LCS_EvalVector(pszToken);
	else if (LCS_TokenIs(pszToken, "movedir"))
		pev->movedir = LCS_EvalVector(pszToken);
	else if (LCS_TokenIs(pszToken, "origin") || LCS_TokenIs(pszToken, "org") || LCS_TokenIs(pszToken, "pos"))
	{
		pev->origin = LCS_EvalVector(pszToken);
		if (!g_fLCSPreSpawn)
			UTIL_SetOrigin(pev, pev->origin);
	}
	else if (LCS_TokenIs(pszToken, "dmg_time") || LCS_TokenIs(pszToken, "dmgtime") || LCS_TokenIs(pszToken, "tdmg"))
		pev->dmgtime = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "dmg"))
		pev->dmg = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "air_finished"))
		pev->air_finished = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "friction"))
		pev->friction = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "health"))
		pev->health = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "armorvalue"))
		pev->armorvalue = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "max_health") || LCS_TokenIs(pszToken, "maxhealth"))
		pev->max_health = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "fixangle"))
		pev->fixangle = 1;
	else if (LCS_TokenIs(pszToken, "trans"))
		pev->rendermode = kRenderTransTexture;
	else if (LCS_TokenIs(pszToken, "notrans"))
		pev->rendermode = kRenderNormal;
	else if (LCS_TokenIs(pszToken, "ramt") || LCS_TokenIs(pszToken, "alpha"))
		pev->renderamt = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "rcolor"))
		pev->rendercolor = LCS_EvalVector(pszToken);
	else if (LCS_TokenIs(pszToken, "grav"))
		pev->gravity = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "nextthink"))
		pev->nextthink = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "glowsh"))
		pev->renderfx = kRenderFxGlowShell;
	else if (LCS_TokenIs(pszToken, "noglowsh"))
		pev->renderfx = kRenderFxNone;
	else if (LCS_TokenIs(pszToken, "ground"))
		pev->flags |= FL_ONGROUND;
	else if (LCS_TokenIs(pszToken, "noground"))
		pev->flags &= ~FL_ONGROUND;
	else if (LCS_TokenIs(pszToken, "takedamage"))
		pev->takedamage = DAMAGE_AIM;
	else if (LCS_TokenIs(pszToken, "takedamagenoaim"))
		pev->takedamage = DAMAGE_YES;
	else if (LCS_TokenIs(pszToken, "notakedamage"))
		pev->takedamage = DAMAGE_NO;
	else if (LCS_TokenIs(pszToken, "con"))
	{
		// con0..con3: the bone controller in the fourth character
		int i = std::min(std::max(pszToken[3] - '0', 0), 3);
		pev->controller[i] = static_cast<byte>(std::min(std::max(LCS_EvalFloat(pszToken), 0.0f), 255.0f));
	}
	else if (LCS_TokenIs(pszToken, "blend"))
	{
		// VERIFY: the original reads the index from the fourth character here too ("blend"'s
		// 'n'), so it is always blending[1]
		int i = std::min(std::max(pszToken[3] - '0', 0), 1);
		pev->blending[i] = static_cast<byte>(std::min(std::max(LCS_EvalFloat(pszToken), 0.0f), 255.0f));
	}
	else if (LCS_TokenIs(pszToken, "seq"))
	{
		pev->sequence = static_cast<int>(LCS_EvalFloat(pszToken));
		if ((pev->flags & (FL_CLIENT | FL_MONSTER)) == 0 && 0 != strcmp(STRING(pev->classname), "cycler"))
			return;
		CBaseEntity* pEntity = CBaseEntity::Instance(g_pentLCS);
		if (pEntity)
			static_cast<CBaseAnimating*>(pEntity)->ResetSequenceInfo();
	}
	else if (LCS_TokenIs(pszToken, "rsi"))
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(g_pentLCS);
		if (pEntity)
			static_cast<CBaseAnimating*>(pEntity)->ResetSequenceInfo();
	}
	else if (LCS_TokenIs(pszToken, "frate"))
		pev->framerate = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "scale"))
		pev->scale = LCS_EvalFloat(pszToken);
	else if (LCS_TokenIs(pszToken, "owner"))
	{
		// owner=<entindex>, or alone: whoever runs the script
		char* pszEq = strpbrk(pszToken, "=");
		edict_t* pentOwner = INDEXENT(pszEq ? atoi(pszEq + 1) : g_iLCSCallerIndex);
		if (UTIL_IsValidEntity(pentOwner))
			pev->owner = pentOwner;
	}
	else if (LCS_TokenIs(pszToken, "targetname"))
	{
		char* pszEq = strpbrk(pszToken, "=");
		pev->targetname = pszEq ? MAKE_STRING(LCS_AllocString(pszEq + 1)) : 0;
	}
	else if (LCS_TokenIs(pszToken, "target"))
	{
		char* pszEq = strpbrk(pszToken, "=");
		pev->target = pszEq ? MAKE_STRING(LCS_AllocString(pszEq + 1)) : 0;
	}
	else if (LCS_TokenIs(pszToken, "dodmg") || LCS_TokenIs(pszToken, "dotakedamage"))
	{
		// dodmg=<amount>: damage from the world
		char* pszEq = strpbrk(pszToken, "=");
		if (!pszEq)
			return;
		CBaseEntity* pEntity = CBaseEntity::Instance(g_pentLCS);
		if (!pEntity)
			return;
		entvars_t* pevWorld = VARS(INDEXENT(0));
		pEntity->TakeDamage(pevWorld, pevWorld, atof(pszEq + 1), 0);
	}
	else if (LCS_TokenIs(pszToken, "nextattack"))
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(g_pentLCS);
		if (!pEntity || !pEntity->IsPlayer())
			return;
		static_cast<CBasePlayer*>(pEntity)->m_flNextAttack = LCS_EvalFloat(pszToken);
	}
	else if (LCS_TokenIs(pszToken, "speed"))
	{
		pev->speed = LCS_EvalFloat(pszToken);
		if (0 != strcmp(STRING(pev->classname), "func_tracktrain"))
			return;
		CBaseEntity* pEntity = CBaseEntity::Instance(g_pentLCS);
		if (!pEntity)
			return;
		pEntity->pev->impulse = static_cast<int>(pEntity->pev->speed);
		static_cast<CFuncTrackTrain*>(pEntity)->m_speed = pEntity->pev->speed;
	}
	else if (LCS_TokenIs(pszToken, "select"))
	{
		// select=<entindex>: later tokens change that entity instead (not a client)
		int iIndex = static_cast<int>(LCS_EvalFloat(pszToken));
		if (iIndex <= gpGlobals->maxClients || iIndex <= 0 || iIndex >= gpGlobals->maxEntities)
			return;
		edict_t* pent = INDEXENT(iIndex);
		if (!pent || !UTIL_IsValidEntity(pent))
			return;
		g_pentLCS = pent;
		g_iLCSSelectedIndex = ENTINDEX(pent);
	}
	else if (LCS_TokenIs(pszToken, "keyvalue"))
	{
		// keyvalue=<key>,<value>: a keyvalue as the map would give it
		char* pszEq = strpbrk(pszToken, "=");
		if (!pszEq)
			return;
		KeyValueData kvd;
		kvd.fHandled = 0;
		kvd.szClassName = STRING(pev->classname);
		char* pszComma = strpbrk(pszToken, ",");
		if (!pszComma)
			return;
		int len = pszComma - pszEq - 1;
		strncpy(g_szLCSKeyName, pszEq + 1, std::min(len, 255));
		g_szLCSKeyName[std::min(len, 255)] = '\0';
		strncpy(g_szLCSKeyValue, pszComma + 1, 255);
		g_szLCSKeyValue[255] = '\0';
		kvd.szKeyName = g_szLCSKeyName;
		kvd.szValue = g_szLCSKeyValue;
		DispatchKeyValue(g_pentLCS, &kvd);
	}
}

// orig 0x2370 (unlisted, and nothing in rc2.dll calls it). One token of a message script:
// writes the token's value as the message field its key names.
static void LCS_MessageToken(int iArg, char* pszToken) // name ours
{
	if (iArg < g_iLCSFirstArg)
		return;

	if (LCS_TokenIs(pszToken, "vec"))
	{
		Vector vec = LCS_EvalVector(pszToken);
		WRITE_COORD(vec.x);
		WRITE_COORD(vec.y);
		WRITE_COORD(vec.z);
	}
	else if (LCS_TokenIs(pszToken, "dvec"))
	{
		// VERIFY: VC6 wrote these as x, z, y, and for a zero vector the third from a stack slot
		// it never set; ours writes x, z, y of the normalized vector ((0, 0, 1) for zero)
		Vector vec = LCS_EvalVector(pszToken).Normalize();
		WRITE_SHORT(static_cast<int>(vec.x * 32767));
		WRITE_SHORT(static_cast<int>(vec.z * 32767));
		WRITE_SHORT(static_cast<int>(vec.y * 32767));
	}
	else if (LCS_TokenIs(pszToken, "fl"))
	{
		// the float's bits as a long
		float fl = LCS_EvalFloat(pszToken);
		int iBits;
		memcpy(&iBits, &fl, sizeof(iBits));
		WRITE_LONG(iBits);
	}
	else if (LCS_TokenIs(pszToken, "int"))
		WRITE_LONG(static_cast<int>(LCS_EvalFloat(pszToken)));
	else if (LCS_TokenIs(pszToken, "short"))
		WRITE_SHORT(static_cast<int>(LCS_EvalFloat(pszToken)));
	else if (LCS_TokenIs(pszToken, "byte"))
		WRITE_BYTE(static_cast<int>(LCS_EvalFloat(pszToken)));
	else if (LCS_TokenIs(pszToken, "string"))
	{
		// the original writes from the '=' on, '=' included (dossier: original bugs)
		char* pszEq = strpbrk(pszToken, "=");
		if (pszEq)
			WRITE_STRING(pszEq);
	}
	else if (LCS_TokenIs(pszToken, "char"))
	{
		// the original writes the '=' itself (dossier: original bugs)
		char* pszEq = strpbrk(pszToken, "=");
		if (pszEq)
			WRITE_CHAR(*pszEq);
	}
}

// --- admin command helpers ---

// orig 0x25e0. A player by index, else the first whose name contains pszName.
static CBasePlayer* Admin_FindPlayer(const char* pszName) // name ours
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(atoi(pszName)));
	if (pPlayer)
		return pPlayer;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));
		if (pPlayer && strstr(STRING(pPlayer->pev->netname), pszName))
			return pPlayer;
	}
	return NULL;
}

// orig 0x2660. What the admin aims at (its GetEffectedEnt), else the admin's selection.
static CBaseEntity* Admin_GetAimedEntity(entvars_t* pevAdmin) // name ours
{
	UTIL_MakeVectors(pevAdmin->v_angle);
	const Vector vecSrc = pevAdmin->origin + pevAdmin->view_ofs;
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, pevAdmin->pContainingEntity, &tr);

	if (tr.pHit && UTIL_IsValidEntity(tr.pHit) && 0 != strcmp(STRING(tr.pHit->v.classname), "worldspawn"))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit)
			return pHit->GetEffectedEnt();
	}

	CBasePlayer* pAdmin = static_cast<CBasePlayer*>(CBaseEntity::Instance(pevAdmin->pContainingEntity));
	if (pAdmin && UTIL_IsValidEntity(pAdmin->m_pentDebug))
		return CBaseEntity::Instance(pAdmin->m_pentDebug);
	return NULL;
}

// orig 0x28a0 (unlisted, and nothing in rc2.dll calls it). The entity nearest the admin's aim
// within 45 degrees that it can see.
static CBaseEntity* Admin_FindBestAimedEntity(entvars_t* pevAdmin) // name ours
{
	float flBestDot = 0.707107f;
	CBaseEntity* pBest = NULL;

	UTIL_MakeVectors(pevAdmin->v_angle);
	const Vector vecSrc = pevAdmin->origin + pevAdmin->view_ofs;
	const Vector vecForward = gpGlobals->v_forward;

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent) || pent == pevAdmin->pContainingEntity)
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity)
			continue;

		const Vector vecDir = (pEntity->Center() - vecSrc).Normalize();
		const float flDot = DotProduct(vecDir, vecForward);
		if (flDot < flBestDot)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, pEntity->Center(), ignore_monsters, pevAdmin->pContainingEntity, &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
		{
			pBest = pEntity;
			flBestDot = flDot;
		}
	}
	return pBest;
}

typedef void (*ADMINENTFUNC)(CBaseEntity* pEntity);

// orig 0x2b10. Runs pfn on every entity of a classname ("all", empty or one character: every
// entity). Players only with fIncludePlayers.
static void Admin_ForEachEntity(entvars_t* pevAdmin, const char* pszClassname, ADMINENTFUNC pfn, bool fIncludePlayers) // name ours
{
	bool fAll = true;
	if (pszClassname && strlen(pszClassname) > 1 && 0 != stricmp(pszClassname, "all"))
		fAll = false;

	if (!fIncludePlayers && !strcmp(pszClassname, "player"))
		return;

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity)
			continue;
		if (pEntity->IsPlayer() && !fIncludePlayers)
			continue;
		if (fAll || !strcmp(pszClassname, STRING(pEntity->pev->classname)))
			pfn(pEntity);
	}
}

// orig 0x2c70 (unlisted, and nothing in rc2.dll calls it): an admin "create <x> <y> <classname>
// <script...>". Creates a precached classname and runs the command line on it as an LCS
// script, before and after it spawns.
static void Admin_Create(entvars_t* pevAdmin) // name ours
{
	const char* pszClassname = CMD_ARGV(3);
	if (!IsClassnamePrecachedNoInvalid(pszClassname))
	{
		ClientPrint(pevAdmin, HUD_PRINTCONSOLE, UTIL_VarArgs("\"%s\" not precached\n", pszClassname));
		return;
	}

	edict_t* pent = CREATE_NAMED_ENTITY(MAKE_STRING(GetPrecachedClassname(pszClassname)));
	if (!pent || 0 == OFFSET(pent))
	{
		ClientPrint(pevAdmin, HUD_PRINTCONSOLE, UTIL_VarArgs("Could not create \"%s\"\n", pszClassname));
		return;
	}

	CBaseEntity* pEntity = static_cast<CBaseEntity*>(pent->pvPrivateData);
	g_iLCSFirstArg = 3;
	g_pentLCS = pEntity->edict();
	g_iLCSCallerIndex = ENTINDEX(pevAdmin->pContainingEntity);
	g_iLCSSelectedIndex = ENTINDEX(pEntity->edict());
	g_fLCSPreSpawn = true;
	LCS_ParseTokens(const_cast<char*>(CMD_ARGS()), LCS_EntityToken);
	DispatchSpawn(pEntity->edict());

	g_iLCSFirstArg = 3;
	g_pentLCS = pEntity->edict();
	g_iLCSCallerIndex = ENTINDEX(pevAdmin->pContainingEntity);
	g_iLCSSelectedIndex = ENTINDEX(pEntity->edict());
	g_fLCSPreSpawn = false;
	LCS_ParseTokens(const_cast<char*>(CMD_ARGS()), LCS_EntityToken);
}

static void Admin_CameraSet(edict_t* pClient);
static void Admin_CameraOn(edict_t* pClient);
static void Admin_CameraOff(edict_t* pClient);
static void Admin_CameraPlayer(edict_t* pClient, edict_t* pTarget);

// orig 0x2de0. The client command "admin" (client.cpp's ClientCommand): ARGV(1) is the admin
// command. Admin level 1 gets the first set; 2 (super admin) also remove, lcs, stopvote,
// mkspawn, demote and promote. A command on a player needs a level above that player's,
// unless the admin is a super admin.
void Admin_Command(edict_t* pEntity) // name ours
{
	const char* pszCmd = CMD_ARGV(1);

	if (!pEntity || 0 == OFFSET(pEntity) || !pEntity->pvPrivateData || 0 != pEntity->free)
		return;

	CBasePlayer* pAdmin = static_cast<CBasePlayer*>(pEntity->pvPrivateData);
	entvars_t* pev = &pEntity->v;

	int iLevel = pAdmin->m_iAdminLevel;
	if (iLevel < 1)
		iLevel = Admin_IsTempAdmin(pEntity) ? 1 : 0;
	const bool fSuper = iLevel > 1;

	if (iLevel < 1)
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, "You need admin primisions to use this command\n");
		return;
	}

	if (CMD_ARGC() < 2)
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, "Available admin commands:\n");
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tlistclients\n\tgiveban\n\tsuicideban\n\tforcespawn\n\tpunch\n\tsetangle\n");
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tkill\n\tuse\n\tuseset\n\tmkally\n\tmkhostile\n\tfixtrain\n\tmove\n\tselectent\n");
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tmovecenterforward\n\tdumpentinfo\tcamera\n");
		if (fSuper)
			ClientPrint(pev, HUD_PRINTCONSOLE, "\tremove\n\tlcs\n\tmkspawn\n\tdemote\n\tpromote\n\tnukeclte\n");
		return;
	}

	// a command on another player: the target's level must be lower, or the admin super
	auto canTarget = [&](CBasePlayer* pTarget)
	{ return pTarget->m_iAdminLevel < iLevel || iLevel == 2; };

	// a command on the aimed entity, or with a classname on every entity of it
	auto onEntities = [&](ADMINENTFUNC pfn)
	{
		if (CMD_ARGC() < 3)
		{
			CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
			if (pTarget && !pTarget->IsPlayer())
				pfn(pTarget);
		}
		else
			Admin_ForEachEntity(pev, CMD_ARGV(2), pfn, false);
	};

	bool fDenied = false;

	if (!strcmp(pszCmd, "listclients"))
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, "Clients on server:\n");
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (pPlayer)
				ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("  %i %s\n", i, STRING(pPlayer->pev->netname)));
		}
		return;
	}
	else if (!strcmp(pszCmd, "giveban"))
	{
		if (CMD_ARGC() < 4)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: giveban playindex|playername ban(0/1)\n");
			return;
		}
		CBasePlayer* pTarget = Admin_FindPlayer(CMD_ARGV(2));
		if (!pTarget)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Player not found\n");
			return;
		}
		if (canTarget(pTarget))
		{
			pTarget->m_fGiveBan = atoi(CMD_ARGV(3)) != 0;
			return;
		}
	}
	else if (!strcmp(pszCmd, "suicideban"))
	{
		if (CMD_ARGC() < 4)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: suicideban playindex|playername ban(0/1)\n");
			return;
		}
		CBasePlayer* pTarget = Admin_FindPlayer(CMD_ARGV(2));
		if (!pTarget)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Player not found\n");
			return;
		}
		if (canTarget(pTarget))
		{
			pTarget->m_fSuicideBan = atoi(CMD_ARGV(3)) != 0;
			return;
		}
	}
	else if (!strcmp(pszCmd, "forcespawn"))
	{
		if (CMD_ARGC() < 3)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: forcespawn playindex|playername\n");
			return;
		}
		CBasePlayer* pTarget = Admin_FindPlayer(CMD_ARGV(2));
		if (!pTarget)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Player not found\n");
			return;
		}
		if (canTarget(pTarget))
		{
			pTarget->m_fForceSpawn = true;
			return;
		}
	}
	else if (!strcmp(pszCmd, "camera"))
	{
		if (CMD_ARGC() < 3)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: camera set/on/off/player\n");
			return;
		}
		if (0 != strcmp(CMD_ARGV(2), "player"))
		{
			if (!strcmp(CMD_ARGV(2), "set"))
				Admin_CameraSet(pev->pContainingEntity);
			else if (!strcmp(CMD_ARGV(2), "on"))
				Admin_CameraOn(pev->pContainingEntity);
			else if (!strcmp(CMD_ARGV(2), "off"))
				Admin_CameraOff(pev->pContainingEntity);
			else
				ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: camera set/on/off/player\n");
			return;
		}
		if (CMD_ARGC() < 4)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: camera player playindex|playername\n");
			return;
		}
		CBasePlayer* pTarget = Admin_FindPlayer(CMD_ARGV(3));
		if (!pTarget)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Player not found\n");
			return;
		}
		if (canTarget(pTarget))
		{
			Admin_CameraPlayer(pev->pContainingEntity, pTarget->edict());
			return;
		}
	}
	else if (!strcmp(pszCmd, "selectent"))
	{
		// selectent [entindex]: the aimed entity, or by index (anything else clears)
		int iIndex = 1;
		if (CMD_ARGC() < 3)
		{
			CBaseEntity* pAimed = Admin_GetAimedEntity(pev);
			if (pAimed)
				pAdmin->m_pentDebug = pAimed->edict();
		}
		else
		{
			iIndex = atoi(CMD_ARGV(2));
			if (iIndex < 1 || iIndex >= gpGlobals->maxEntities)
				pAdmin->m_pentDebug = NULL;
			else
				pAdmin->m_pentDebug = INDEXENT(iIndex);
		}

		if (!UTIL_IsValidEntity(pAdmin->m_pentDebug))
		{
			if (iIndex < 1 || iIndex >= gpGlobals->maxEntities)
				ClientPrint(pev, HUD_PRINTCONSOLE, "Selection cleared\n");
			else
				ClientPrint(pev, HUD_PRINTCONSOLE, "Nothing to select: Ussage: selectent [entindex|playerindex|-1 to clear]\n");
			pAdmin->m_pentDebug = NULL;
			pAdmin->m_bDebugCounts = 0;
			return;
		}

		const int iSelected = ENTINDEX(pAdmin->m_pentDebug);
		if (iSelected > 0 && iSelected <= gpGlobals->maxClients)
		{
			CBasePlayer* pSelected = static_cast<CBasePlayer*>(CBaseEntity::Instance(pAdmin->m_pentDebug));
			if (pSelected->IsPlayer())
			{
				ClientPrint(pev, HUD_PRINTCONSOLE, "Cannot select players\n");
				pAdmin->m_pentDebug = NULL;
				pAdmin->m_bDebugCounts = 0;
			}
			else if (!canTarget(pSelected))
			{
				// VERIFY: a client slot that isn't a player is checked by its admin level
				fDenied = true;
				pAdmin->m_pentDebug = NULL;
				pAdmin->m_bDebugCounts = 0;
			}
		}

		if (pAdmin->m_pentDebug)
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Selected: \"%s\" -- %d\n", STRING(pAdmin->m_pentDebug->v.classname), iSelected));
		if (!fDenied)
			return;
	}
	else if (!strcmp(pszCmd, "punch"))
	{
		if (CMD_ARGC() < 3)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: punch xangle [yangle] [zangle]\n");
			return;
		}
		CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
		if (!pTarget)
			return;
		Vector vecPunch;
		vecPunch.x = atof(CMD_ARGV(2));
		vecPunch.y = CMD_ARGC() > 3 ? atof(CMD_ARGV(3)) : g_vecZero.y;
		vecPunch.z = CMD_ARGC() > 4 ? atof(CMD_ARGV(4)) : g_vecZero.z;
		pTarget->pev->punchangle = pTarget->pev->punchangle + vecPunch;
		return;
	}
	else if (!strcmp(pszCmd, "setangle"))
	{
		if (CMD_ARGC() < 3)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: setangle xangle [yangle] [zangle]\n");
			return;
		}
		CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
		if (!pTarget || pTarget->IsBSPModel())
			return;
		Vector vecAngles;
		vecAngles.x = atof(CMD_ARGV(2));
		vecAngles.y = CMD_ARGC() > 3 ? atof(CMD_ARGV(3)) : g_vecZero.y;
		vecAngles.z = CMD_ARGC() > 4 ? atof(CMD_ARGV(4)) : g_vecZero.z;
		pTarget->pev->v_angle = g_vecZero;
		pTarget->pev->angles = vecAngles;
		pTarget->pev->fixangle = 1;
		return;
	}
	else if (!strcmp(pszCmd, "move"))
	{
		if (CMD_ARGC() < 5)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: move xoffset yoffset zoffset [0=offsets as absolute coords]\n");
			return;
		}
		CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
		if (!pTarget)
			return;
		if (pTarget->IsPlayer())
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Cannot move players\n");
			return;
		}
		Vector vecMove(atof(CMD_ARGV(2)), atof(CMD_ARGV(3)), atof(CMD_ARGV(4)));
		if (CMD_ARGC() > 5)
			UTIL_SetOrigin(pTarget->pev, vecMove);
		else
			UTIL_SetOrigin(pTarget->pev, pTarget->pev->origin + vecMove);
		return;
	}
	else if (!strcmp(pszCmd, "movecenterforward"))
	{
		// moves the aimed entity's center to a point in front of the admin (default 128 units)
		CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
		if (!pTarget)
			return;
		if (pTarget->IsPlayer())
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Cannot move players\n");
			return;
		}
		UTIL_MakeVectors(pev->v_angle);
		float flDist = 128;
		if (CMD_ARGC() > 2)
			flDist = atof(CMD_ARGV(2));
		const Vector vecTarget = pev->origin + pev->view_ofs + gpGlobals->v_forward * flDist;
		UTIL_SetOrigin(pTarget->pev, pTarget->pev->origin - pTarget->Center() + vecTarget);
		return;
	}
	else if (!strcmp(pszCmd, "dumpentinfo"))
	{
		CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
		if (!pTarget)
			return;
		char szInfo[512];
		const int iIndex = ENTINDEX(pTarget->edict());
		entvars_t* pevTarget = pTarget->pev;
		sprintf(szInfo, "%s -- %d\n\tOrigin: %f %f %f\n\tAngles: %f %f %f\n", STRING(pevTarget->classname), iIndex,
			pevTarget->origin.x, pevTarget->origin.y, pevTarget->origin.z,
			pevTarget->angles.x, pevTarget->angles.y, pevTarget->angles.z);
		ClientPrint(pev, HUD_PRINTCONSOLE, szInfo);
		return;
	}
	else if (!strcmp(pszCmd, "kill"))
	{
		onEntities(Admin_Kill);
		return;
	}
	else if (!strcmp(pszCmd, "use"))
	{
		onEntities(Admin_Use);
		return;
	}
	else if (!strcmp(pszCmd, "useset"))
	{
		if (CMD_ARGC() < 3)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: useset <on/off/set/toggle> [classname|all]\n");
			return;
		}
		g_AdminUseType = USE_TOGGLE;
		if (!strcmp(CMD_ARGV(2), "on"))
			g_AdminUseType = USE_ON;
		else if (!strcmp(CMD_ARGV(2), "off"))
			g_AdminUseType = USE_OFF;
		else
			g_AdminUseType = !strcmp(CMD_ARGV(2), "set") ? USE_SET : USE_TOGGLE;

		if (CMD_ARGC() > 3)
			Admin_ForEachEntity(pev, CMD_ARGV(3), Admin_UseSet, false);
		else
		{
			CBaseEntity* pTarget = Admin_GetAimedEntity(pev);
			if (pTarget && !pTarget->IsPlayer())
				Admin_UseSet(pTarget);
		}
		return;
	}
	else if (!strcmp(pszCmd, "mkally") || !strcmp(pszCmd, "mkallied") || !strcmp(pszCmd, "makeallied"))
	{
		onEntities(Admin_MakeAllied);
		return;
	}
	else if (!strcmp(pszCmd, "mkhostile") || !strcmp(pszCmd, "makehostile"))
	{
		onEntities(Admin_MakeHostile);
		return;
	}
	else if (!strcmp(pszCmd, "fixtrain") || !strcmp(pszCmd, "fixtrains"))
	{
		Admin_ForEachEntity(pev, "func_tracktrain", Admin_FixTrain, false);
		Admin_ForEachEntity(pev, "path_track", Admin_FixTrain, false);
		return;
	}
	else if (!strcmp(pszCmd, "remove"))
	{
		if (fSuper)
		{
			onEntities(Admin_Remove);
			return;
		}
	}
	else if (!strcmp(pszCmd, "lcs"))
	{
		if (fSuper)
		{
			LCS_Command(2, pEntity);
			return;
		}
	}
	else if (!strcmp(pszCmd, "stopvote"))
	{
		if (fSuper)
		{
			StopVote();
			return;
		}
	}
	else if (!strcmp(pszCmd, "mkspawn"))
	{
		if (fSuper)
		{
			CBaseEntity::Create("info_player_deathmatch", pAdmin->pev->origin, pAdmin->pev->v_angle, NULL);
			return;
		}
	}
	else if (!strcmp(pszCmd, "demote"))
	{
		if (fSuper)
		{
			CBasePlayer* pTarget = Admin_FindPlayer(CMD_ARGV(2));
			if (!pTarget)
			{
				ClientPrint(pev, HUD_PRINTCONSOLE, "Player not found\n");
				return;
			}
			if (pTarget->m_iAdminLevel < 2)
			{
				pTarget->m_iAdminLevel = 0;
				return;
			}
			ClientPrint(pev, HUD_PRINTCONSOLE, "Sorry, cannot demote another admin\n");
			return;
		}
	}
	else if (!strcmp(pszCmd, "promote"))
	{
		if (fSuper)
		{
			CBasePlayer* pTarget = Admin_FindPlayer(CMD_ARGV(2));
			if (!pTarget)
			{
				ClientPrint(pev, HUD_PRINTCONSOLE, "Player not found\n");
				return;
			}
			if (pTarget->m_iAdminLevel < 1)
			{
				pTarget->m_iAdminLevel = 1;
				return;
			}
			ClientPrint(pev, HUD_PRINTCONSOLE, "Sorry, cannot promote another admin\n");
			return;
		}
	}
	else if (!strcmp(pszCmd, "nukeclte"))
	{
		// any admin: tells every client to drop its client-side temp entities
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfc);
		MESSAGE_END();
		return;
	}
	else
	{
		char szCmd[128];
		strncpy(szCmd, pszCmd, 127);
		szCmd[127] = '\0';
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Unkown admin command \"%s\"\n", szCmd));
		return;
	}

	ClientPrint(pev, HUD_PRINTCONSOLE, "You need higher admin permissions to use this command\n");
}

// --- CMaintainEnt: re-runs an LCS script on an entity every frame ---

class CMaintainEnt : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT MaintainThink();

	char m_szScript[0x200]; // orig +0x70
	edict_t* m_pentTarget;	// orig +0x270, name ours
	edict_t* m_pentCaller;	// orig +0x274, name ours
};

LINK_ENTITY_TO_CLASS(maintainer_ent, CMaintainEnt); // orig 0x46f0

// orig 0x4d90 (unlisted: Ghidra has no function there; it is CMaintainEnt's vtable slot 0)
void CMaintainEnt::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->impulse = 0;
	pev->body = 2; // the script's first token
	SetThink(&CMaintainEnt::MaintainThink);
	pev->nextthink = gpGlobals->time;
}

// orig 0x4df0. Until either entity goes or impulse is set.
void CMaintainEnt::MaintainThink()
{
	if (UTIL_IsValidEntity(m_pentTarget) && UTIL_IsValidEntity(m_pentCaller) && pev->impulse == 0)
	{
		g_pentLCS = m_pentTarget;
		g_iLCSFirstArg = pev->body;
		g_iLCSCallerIndex = ENTINDEX(m_pentCaller);
		g_iLCSSelectedIndex = ENTINDEX(m_pentTarget);
		g_fLCSPreSpawn = false;
		LCS_ParseTokens(m_szScript, LCS_EntityToken);
		pev->nextthink = gpGlobals->time + 0.001;
		return;
	}

	SetThink(NULL);
	UTIL_Remove(this);
}

// orig 0x4eb0. Runs a script on an entity now, as itself.
void LCS_RunOnEntity(edict_t* pent, char* pszScript, int iFirstArg) // name ours
{
	g_iLCSFirstArg = iFirstArg;
	g_pentLCS = pent;
	g_iLCSCallerIndex = ENTINDEX(pent);
	g_iLCSSelectedIndex = ENTINDEX(pent);
	g_fLCSPreSpawn = false;
	LCS_ParseTokens(pszScript, LCS_EntityToken);
}

// orig 0x4f00. Creates an entity and runs a script on it before it spawns and after.
void LCS_CreateWithScript(const char* pszClassname, char* pszScript, int iFirstArg) // name ours
{
	AddToPrecachedList(pszClassname);
	edict_t* pent = CREATE_NAMED_ENTITY(MAKE_STRING(GetPrecachedClassname(pszClassname)));
	if (!pent || 0 == OFFSET(pent))
	{
		ALERT(at_console, UTIL_VarArgs("Could not create \"%s\"\n", pszClassname));
		return;
	}

	CBaseEntity* pEntity = static_cast<CBaseEntity*>(pent->pvPrivateData);
	if (!pEntity)
	{
		ALERT(at_console, UTIL_VarArgs("Could not create \"%s\"\n", pszClassname));
		return;
	}

	g_pentLCS = pEntity->edict();
	g_iLCSFirstArg = iFirstArg;
	g_iLCSCallerIndex = ENTINDEX(pEntity->edict());
	g_iLCSSelectedIndex = ENTINDEX(pEntity->edict());
	g_fLCSPreSpawn = true;
	LCS_ParseTokens(pszScript, LCS_EntityToken);
	DispatchSpawn(pEntity->edict());

	g_pentLCS = pEntity->edict();
	g_iLCSFirstArg = iFirstArg;
	g_iLCSCallerIndex = ENTINDEX(pEntity->edict());
	g_iLCSSelectedIndex = ENTINDEX(pEntity->edict());
	g_fLCSPreSpawn = false;
	LCS_ParseTokens(pszScript, LCS_EntityToken);
}

// orig 0x5040. Runs a script on an entity every frame from now on (a maintainer_ent).
void LCS_MaintainOnEntity(edict_t* pent, const char* pszScript, int iFirstArg) // name ours
{
	CMaintainEnt* pMaintain = static_cast<CMaintainEnt*>(CBaseEntity::Create("maintainer_ent", g_vecZero, g_vecZero, NULL));
	if (!pMaintain)
		return;
	pMaintain->m_pentTarget = pent;
	pMaintain->m_pentCaller = pent;
	strncpy(pMaintain->m_szScript, pszScript, 0x1ff);
	pMaintain->m_szScript[0x1ff] = '\0';
	pMaintain->pev->body = iFirstArg;
}

// orig 0x50a0. The map's player script, on a player (from CBasePlayer).
void LCS_RunPlayerScript(edict_t* pent) // name ours
{
	g_iLCSFirstArg = g_iLCSPlayerScriptFirstArg;
	g_pentLCS = pent;
	g_iLCSCallerIndex = ENTINDEX(pent);
	g_iLCSSelectedIndex = ENTINDEX(pent);
	g_fLCSPreSpawn = false;
	LCS_ParseTokens(g_szLCSPlayerScript, LCS_EntityToken);
}

// --- the admin camera: an invisible stand-in the admin can look through ---

// orig 0x50f0: camera set. Moves the admin's camera (or makes one) to the admin's eyes.
static void Admin_CameraSet(edict_t* pClient)
{
	CBaseEntity* pCamera = UTIL_FindEntityByClassname(NULL, "admin_camera");
	while (pCamera)
	{
		// the admin's own, or one whose owner has gone
		if (pCamera->pev->owner == pClient || !UTIL_IsValidEntity(pCamera->pev->owner))
			break;
		pCamera = UTIL_FindEntityByClassname(pCamera, "admin_camera");
	}

	if (!pCamera)
	{
		pCamera = GetClassPtr(static_cast<CBaseEntity*>(NULL));
		pCamera->Spawn();
		if (!pCamera)
			return;
	}

	pCamera->pev->owner = pClient;
	SET_MODEL(pCamera->edict(), "models/player.mdl");
	pCamera->pev->classname = MAKE_STRING("admin_camera");
	pCamera->pev->renderamt = 0;
	pCamera->pev->rendermode = kRenderTransColor;
	pCamera->m_iRC2Flags |= 1; // VERIFY: what bit 0 of the added member means
	entvars_t* pevClient = VARS(pClient);
	pCamera->pev->origin = pevClient->origin + pevClient->view_ofs;
	UTIL_SetOrigin(pCamera->pev, pCamera->pev->origin);
	pCamera->pev->v_angle = pevClient->v_angle;
	pCamera->pev->v_angle.x = -pCamera->pev->v_angle.x;
	pCamera->pev->angles = pCamera->pev->v_angle;
	ClientPrint(pevClient, HUD_PRINTCONSOLE, "Cammera set\n");
}

// orig 0x52f0: camera on
static void Admin_CameraOn(edict_t* pClient)
{
	for (CBaseEntity* pCamera = UTIL_FindEntityByClassname(NULL, "admin_camera"); pCamera; pCamera = UTIL_FindEntityByClassname(pCamera, "admin_camera"))
	{
		if (pCamera->pev->owner == pClient)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, NULL, pClient);
			WRITE_BYTE(0);
			MESSAGE_END();
			unsigned char* pvs;
			SetupVisibility(pCamera->edict(), pClient, &pvs, &pvs);
			SET_VIEW(pClient, pCamera->edict());
			return;
		}
	}
	ClientPrint(VARS(pClient), HUD_PRINTCONSOLE, "You don't have a cammera placed\n");
}

// orig 0x53b0: camera off
static void Admin_CameraOff(edict_t* pClient)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, NULL, pClient);
	WRITE_BYTE(0);
	MESSAGE_END();
	unsigned char* pvs;
	SetupVisibility(pClient, pClient, &pvs, &pvs);
	SET_VIEW(pClient, pClient);
}

// orig 0x5400: camera player <who>
static void Admin_CameraPlayer(edict_t* pClient, edict_t* pTarget)
{
	if (!pTarget)
		return;
	MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, NULL, pClient);
	WRITE_BYTE(0);
	MESSAGE_END();
	unsigned char* pvs;
	SetupVisibility(pTarget, pClient, &pvs, &pvs);
	SET_VIEW(pClient, pTarget);
}

// CBaseEntity's inline virtuals, emitted as COMDATs into this file's object in the original
// (they are cbase.h's, and their bodies are stock):
//   orig 0x46d0 SUB_CallUseToggle (stock)
//   orig 0x46e0 CBaseMonster::CallMonsterThink (stock)
//   orig 0x4760 Precache (stock)
//   orig 0x4770 KeyValue (stock)
//   orig 0x4780 ObjectCaps (stock)
//   orig 0x4790 Activate (stock)
//   orig 0x47a0 Classify (stock)
//   orig 0x47b0 DeathNotice (stock)
//   orig 0x47c0 BloodColor (stock)
//   orig 0x47d0 IsTriggered (stock)
//   orig 0x47e0 MyMonsterPointer (stock)
//   orig 0x47f0 MySquadMonsterPointer (stock)
//   orig 0x4800 GetToggleState (stock)
//   orig 0x4810 AddPoints (stock)
//   orig 0x4820 AddPointsToTeam (stock)
//   orig 0x4830 AddPlayerItem (stock)
//   orig 0x4840 RemovePlayerItem (stock)
//   orig 0x4860 GetDelay (stock)
//   orig 0x48c0 OverrideReset (stock)
//   orig 0x48d0 SetToggleState (stock)
//   orig 0x48e0 StartSneaking (stock)
//   orig 0x48f0 StopSneaking (stock)
//   orig 0x4900 OnControls (stock)
//   orig 0x4910 IsSneaking (stock)
//   orig 0x4920 IsAlive (stock)
//   orig 0x4a10 IsPlayer (stock)
//   orig 0x4a20 IsNetClient (stock)
//   orig 0x4a30 TeamID (stock)
//   orig 0x4a40 Think (stock)
//   orig 0x4ac0 Respawn (stock)
//   orig 0x4ad0 UpdateOwner (stock)
//   orig 0x4ae0 FBecomeProne (stock)
//   orig 0x4b60 EyePosition (stock)
//   orig 0x4ba0 EarPosition (stock)
//   orig 0x4be0 BodyTarget (stock)
//   orig 0x4c10 Illumination (stock)
//   orig 0x52e0 Spawn (stock)
//   orig 0x4850 GiveAmmo: stock body (return -1), but RC2's takes a fourth argument (ret 0x10);
//     cbase.h's signature is the SDK's (dossier: cross-file needs)
// The added virtuals' defaults at 0x4ab0-0x4d80 are in cbase.h and cbase.cpp, orig 0x4d50
// (IsCleanupKillable) among them.

// gmsgViewMode: the real one is reconstructed elsewhere now
// gmsgCLTE: the real one is reconstructed elsewhere now
// AddToPrecachedList: the real one is reconstructed elsewhere now
// IsClassnamePrecachedNoInvalid: the real one is reconstructed elsewhere now
// IsClassnameMapOnly: the real one is reconstructed elsewhere now
const char* GetPrecachedClassname(const char* pszClassname) { return pszClassname; }
void LCS_ParseTokens(char* pszScript, LCSTOKENFUNC pfnToken) {}
float LCS_EvalFloat(char* pszToken) { return 0; }
Vector LCS_EvalVector(char* pszToken) { return g_vecZero; }
// LCS_Command: the real one is reconstructed elsewhere now
// StopVote: the real one is reconstructed elsewhere now
