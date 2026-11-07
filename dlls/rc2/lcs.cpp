/***
*
*	Rocket Crowbar 2: level customization scripts (see lcs.h) and lcs_spawner, the entity a
*	create line leaves behind to spawn (and with respawncreate, respawn) its entity.
*
*	In the original this code follows items.cpp in link order, before leech.cpp; the file
*	name is ours.
*
****/

#include <stdio.h>
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "p4_needs.h"
#include "lcs.h"

//=========================================================
// lcs_spawner
//=========================================================
class CLCSSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	void DeathNotice(entvars_t* pevChild) override;
	void EXPORT CreateThink();

	// Not saved, so the names are ours.
	CBaseEntity* m_pEntity;	  // orig +0x70: what it spawned, NULL until then and after it dies
	char m_szClassname[256]; // orig +0x74: what it spawns (pev->impulse 1: respawn it)
};

LINK_ENTITY_TO_CLASS(lcs_spawner, CLCSSpawner); // orig 0x6cd70

// orig 0x6cde0
void CLCSSpawner::Spawn()
{
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CLCSSpawner::CreateThink);
	pev->nextthink = gpGlobals->time + 0.25;
	m_pEntity = NULL;
	pev->impulse = 0;
}

// orig 0x6ce50
void CLCSSpawner::CreateThink()
{
	if (m_pEntity)
	{
		// already out: a one-shot spawner is done, a respawning one waits for DeathNotice
		if (0 == pev->impulse)
		{
			pev->flags |= FL_KILLME;
			return;
		}
		pev->nextthink = gpGlobals->time + 0.5;
		return;
	}

	AddToPrecachedList(m_szClassname);
	m_pEntity = CBaseEntity::Create(UTIL_PersistentClassname(m_szClassname), pev->origin, pev->angles, edict());
	if (m_pEntity)
	{
		m_pEntity->pev->spawnflags |= 0x80000200;
		if (0 == pev->impulse)
			m_pEntity->pev->owner = NULL;
	}

	if (0 == pev->impulse)
		pev->flags |= FL_KILLME;

	// the owner the line above cleared is set again here, so the spawned entity always
	// reports its death to the spawner (kept as the original has it)
	if (m_pEntity)
		m_pEntity->pev->owner = edict();

	pev->nextthink = gpGlobals->time + 0.5;
}

// orig 0x6cf50
void CLCSSpawner::DeathNotice(entvars_t* pevChild)
{
	m_pEntity = NULL;
	pev->nextthink = gpGlobals->time + 1.5;
}

//=========================================================
// script parsing
//=========================================================

// orig 0x6d390: the iToken'th whitespace-separated token of pszLine into pszOut, a "quoted"
// or 'quoted' or {braced} token without its delimiters. False (and pszOut "") when there are
// fewer tokens. Name ours.
static bool LCS_GetToken(const char* pszLine, char* pszOut, int iToken)
{
	const char* p = pszLine;
	char c = *p;
	for (int i = 0; c != '\0' && i < iToken; i++)
	{
		while (c != '\0' && (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
			c = *++p;
		c = *p;
		if (c == '\0')
			break;

		if (c == '"' || c == '\'' || c == '{')
		{
			const char cClose = (c == '{') ? '}' : c;
			char d = *++p;
			while (d != '\0' && d != cClose)
				d = *++p;
			// past the closing character; an unterminated token steps past the end of the
			// string too (kept: the caller's buffers have room)
			p++;
		}
		else
		{
			while (c != ' ' && c != '\t' && c != '\n' && c != '\r')
			{
				c = *++p;
				if (c == '\0')
					break;
			}
		}
		c = *p;
	}

	c = *p;
	while (c != '\0' && (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
		c = *++p;
	c = *p;

	if (c == '\0')
	{
		pszOut[0] = '\0';
		return false;
	}

	if (c == '"' || c == '\'' || c == '{')
	{
		const char cClose = (c == '{') ? '}' : c;
		char d = *++p;
		while (d != '\0' && d != cClose)
		{
			*pszOut++ = d;
			d = *++p;
		}
		*pszOut = '\0';
		return true;
	}

	while (c != ' ' && c != '\t' && c != '\n' && c != '\r')
	{
		*pszOut++ = c;
		c = *++p;
		if (c == '\0')
			break;
	}
	*pszOut = '\0';
	return true;
}

// orig 0x6d2d0: "x y z" as a vector; missing components are 0. Name ours.
static Vector LCS_ParseVector(const char* pszText)
{
	char szToken[256];
	Vector vec = g_vecZero;
	if (LCS_GetToken(pszText, szToken, 0))
		vec.x = atof(szToken);
	if (LCS_GetToken(pszText, szToken, 1))
		vec.y = atof(szToken);
	if (LCS_GetToken(pszText, szToken, 2))
		vec.z = atof(szToken);
	return vec;
}

// orig 0x6d250: a script's classname match: exact, or with a leading '*' a prefix match (so
// "monster_" alone matches no classname). Name ours.
static bool LCS_ClassnameMatches(const char* pszClassname, const char* pszPattern)
{
	if (pszPattern[0] != '*')
		return strcmp(pszClassname, pszPattern) == 0;
	return strncmp(pszClassname, pszPattern + 1, strlen(pszPattern + 1)) == 0;
}

typedef void (*LCSACTION)(CBaseEntity* pEntity);

// orig 0x6d170: pfnAction on every live entity (but the world) whose classname matches.
// Name ours.
static void LCS_ForEachNamed(LCSACTION pfnAction, const char* pszPattern)
{
	edict_t* pent = INDEXENT(1);
	if (!pent)
		return;
	for (int i = 1; i < gpGlobals->maxEntities; i++, pent++)
	{
		if (0 != UTIL_IsValidEdict(pent) && LCS_ClassnameMatches(STRING(pent->v.classname), pszPattern))
			pfnAction((CBaseEntity*)GET_PRIVATE(pent));
	}
}

// orig 0x6d220: pfnAction on entity iEntity when it is live. Name ours.
static void LCS_ForIndex(LCSACTION pfnAction, int iEntity)
{
	edict_t* pent = INDEXENT(iEntity);
	if (pent && 0 != UTIL_IsValidEdict(pent))
		pfnAction((CBaseEntity*)GET_PRIVATE(pent));
}

// orig 0x6d0a0 (removeall, removeent). Name ours.
static void LCS_Remove(CBaseEntity* pEntity)
{
	UTIL_Remove(pEntity);
}

// orig 0x6d0b0 (useall, useent): used by the world, USE_SET 1. Name ours.
static void LCS_Use(CBaseEntity* pEntity)
{
	CBaseEntity* pWorld = CBaseEntity::Instance(INDEXENT(0));
	pEntity->Use(pWorld, pWorld, USE_SET, 1);
}

// orig 0x6d110 (killall, killent): 100000 damage from the world. Name ours.
static void LCS_Kill(CBaseEntity* pEntity)
{
	entvars_t* pevWorld = VARS(INDEXENT(0));
	pEntity->TakeDamage(pevWorld, pevWorld, 100000, DMG_GENERIC);
}

// orig 0x6cf70: a create or respawncreate line: an lcs_spawner for the classname at the
// line's origin and angles. Name ours.
static void LCS_Create(const char* pszLine)
{
	char szToken[256];

	CLCSSpawner* pSpawner = (CLCSSpawner*)CBaseEntity::Create("lcs_spawner", g_vecZero, g_vecZero, NULL);
	if (!LCS_GetToken(pszLine, pSpawner->m_szClassname, 1))
		pSpawner->pev->flags |= FL_KILLME;

	pSpawner->pev->impulse = 0;
	if (LCS_GetToken(pszLine, szToken, 0) && stricmp(szToken, "respawncreate") == 0)
		pSpawner->pev->impulse = 1;

	if (LCS_GetToken(pszLine, szToken, 2))
	{
		pSpawner->pev->origin = LCS_ParseVector(szToken);
		UTIL_SetOrigin(pSpawner->pev, pSpawner->pev->origin);
	}

	if (LCS_GetToken(pszLine, szToken, 3))
		pSpawner->pev->angles = LCS_ParseVector(szToken);
}

// orig 0x6d4d0
void LCS_Load(int iPass)
{
	if (!g_pGameRules->IsMultiplayer())
		return;

	char szGameDir[1024];
	char szPath[512];
	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/maps/lcs/%s.lcs", szGameDir, STRING(gpGlobals->mapname));

	FILE* pFile = fopen(szPath, "r");
	if (!pFile)
		return;

	char szLine[512];
	char szCommand[256];
	char szArg[256];
	while (fgets(szLine, 0x1ff, pFile))
	{
		szLine[511] = '\0';
		int iLength = strlen(szLine);
		if (iLength <= 1)
			continue;

		if (szLine[iLength - 1] == '\n' || szLine[iLength - 1] == '\r')
		{
			szLine[iLength - 1] = '\0';
			iLength--;
		}

		// meant to skip leading space, but the test asks for a character that is both space
		// and '#', so it never moves (kept); GetToken skips the space anyway
		int i = 0;
		if (iLength <= 0)
			continue;
		while (0 != isspace(szLine[i]) && szLine[i] == '#')
		{
			if (++i >= iLength)
				break;
		}
		if (i >= iLength || szLine[i] == '#')
			continue;

		if (!LCS_GetToken(szLine, szCommand, 0))
			continue;

		if (iPass == 0)
		{
			if ((stricmp(szCommand, "create") == 0 || stricmp(szCommand, "respawncreate") == 0 || stricmp(szCommand, "advcreate") == 0) && LCS_GetToken(szLine, szArg, 1))
				UTIL_PrecacheOther(szArg);
			continue;
		}

		if (stricmp(szCommand, "removeall") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				LCS_ForEachNamed(LCS_Remove, szArg);
		}
		else if (stricmp(szCommand, "killall") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				LCS_ForEachNamed(LCS_Kill, szArg);
		}
		else if (stricmp(szCommand, "useall") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				LCS_ForEachNamed(LCS_Use, szArg);
		}
		else if (stricmp(szCommand, "removeent") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				LCS_ForIndex(LCS_Remove, atoi(szArg));
		}
		else if (stricmp(szCommand, "killent") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				LCS_ForIndex(LCS_Kill, atoi(szArg));
		}
		else if (stricmp(szCommand, "useent") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				LCS_ForIndex(LCS_Use, atoi(szArg));
		}
		else if (stricmp(szCommand, "create") == 0 || stricmp(szCommand, "respawncreate") == 0)
		{
			LCS_Create(szLine);
		}
		else if (stricmp(szCommand, "advcreate") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
				AdvCreateEntity(szArg, szLine, 1);
		}
		else if (stricmp(szCommand, "edit") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
			{
				edict_t* pent = INDEXENT(atoi(szArg));
				if (0 != UTIL_IsValidEdict(pent))
					EditEntity(pent, szLine, 0);
			}
		}
		else if (stricmp(szCommand, "maintain") == 0)
		{
			if (LCS_GetToken(szLine, szArg, 1))
			{
				edict_t* pent = INDEXENT(atoi(szArg));
				if (0 != UTIL_IsValidEdict(pent))
					MaintainEdit(pent, szLine, 0);
			}
		}
		else if (stricmp(szCommand, "setspawncommand") == 0)
		{
			// LCS_Command writes these lines as "setspawncmd", which this never matches
			strncpy(g_szRC2SpawnCommand, szLine, 0x1ff);
			g_szRC2SpawnCommand[0x1ff] = '\0';
			g_iRC2SpawnCommandRun = 0;
		}
	}

	fclose(pFile);
}

// orig 0x6da80: append pszLine to the current map's script. Name ours.
static void LCS_AppendLine(const char* pszLine)
{
	char szGameDir[1024];
	char szPath[512];
	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/maps/lcs/%s.lcs", szGameDir, STRING(gpGlobals->mapname));

	FILE* pFile = fopen(szPath, "a+");
	if (pFile)
	{
		fprintf(pFile, "%s\n", pszLine);
		fclose(pFile);
	}
}

// The CMD_ARGV(iFirst..) arguments onto pszLine, each followed by a space. Name ours.
static void LCS_AppendArgs(char* pszLine, int iFirst)
{
	for (int i = iFirst; i < CMD_ARGC(); i++)
	{
		strcat(pszLine, CMD_ARGV(i));
		strcat(pszLine, " ");
	}
}

// orig 0x6db00
void LCS_Command(int iArg, edict_t* pEntity)
{
	entvars_t* pev = pEntity ? VARS(pEntity) : NULL;
	const char* pszCmd = iArg < CMD_ARGC() ? CMD_ARGV(iArg) : "";
	char szLine[512];
	const char* pszMessage;

	if (strcmp(pszCmd, "create") == 0 || strcmp(pszCmd, "respawncreate") == 0)
	{
		const bool bRespawn = strcmp(pszCmd, "respawncreate") == 0;
		if (CMD_ARGC() <= iArg + 1)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, bRespawn ? "Ussage: respawncreate classname [0]\n" : "Ussage: create classname [0]\n");
			return;
		}
		// a third argument keeps the pitch level
		const float flPitch = CMD_ARGC() > iArg + 2 ? 0 : -pev->v_angle.x;
		sprintf(szLine, bRespawn ? "respawncreate %s {%f %f %f} {%f %f %f}" : "create %s {%f %f %f} {%f %f %f}",
			CMD_ARGV(iArg + 1), pev->origin.x, pev->origin.y, pev->origin.z, flPitch, pev->v_angle.y, 0.0);
		pszMessage = UTIL_VarArgs("Created \"%s\"\n", CMD_ARGV(iArg + 1));
	}
	else if (strcmp(pszCmd, "advcreate") == 0)
	{
		if (CMD_ARGC() <= iArg + 1)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: create classname <alteration arguments>\n");
			return;
		}
		sprintf(szLine, "advcreate %s ", CMD_ARGV(iArg + 1));
		LCS_AppendArgs(szLine, iArg + 2);
		pszMessage = UTIL_VarArgs("Adv. created \"%s\"\n", CMD_ARGV(iArg + 1));
	}
	else if (strcmp(pszCmd, "setspawncmd") == 0)
	{
		if (CMD_ARGC() <= iArg + 1)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: setspawncmd <alteration arguments>\n");
			return;
		}
		sprintf(szLine, "setspawncmd  ");
		LCS_AppendArgs(szLine, iArg + 1);
		pszMessage = UTIL_VarArgs("Set spawn command\n");
	}
	else if (strcmp(pszCmd, "remove") == 0 || strcmp(pszCmd, "kill") == 0 || strcmp(pszCmd, "use") == 0)
	{
		// the aimed-at entity by index, or every entity of the named class
		const int iVerb = pszCmd[0] == 'r' ? 0 : (pszCmd[0] == 'k' ? 1 : 2);
		static const char* const s_pszEnt[] = {"removeent %i", "killent %i", "useent %i"};
		static const char* const s_pszAll[] = {"removeall %s", "killall %s", "useall %s"};
		static const char* const s_pszOne[] = {"Removing \"%s\"\n", "Killing \"%s\"\n", "Using \"%s\"\n"};
		static const char* const s_pszMany[] = {"Removing all \"%s\"\n", "Killing all \"%s\"\n", "Using all \"%s\"\n"};
		static const char* const s_pszUsage[] = {
			"Ussage: remove <classname if not aimed at a useable ent>\n",
			"Ussage: kill <classname if not aimed at a killable ent>\n",
			"Ussage: use <classname if not aimed at a useable ent>\n"};

		CBaseEntity* pAimed = GetAimedEntity(pev);
		if (pAimed)
		{
			sprintf(szLine, s_pszEnt[iVerb], ENTINDEX(pAimed->edict()));
			pszMessage = UTIL_VarArgs(s_pszOne[iVerb], STRING(pAimed->pev->classname));
		}
		else if (CMD_ARGC() > iArg + 1)
		{
			sprintf(szLine, s_pszAll[iVerb], CMD_ARGV(iArg + 1));
			pszMessage = UTIL_VarArgs(s_pszMany[iVerb], CMD_ARGV(iArg + 1));
		}
		else
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, s_pszUsage[iVerb]);
			return;
		}
	}
	else if (strcmp(pszCmd, "edit") == 0 || strcmp(pszCmd, "maintain") == 0)
	{
		const bool bMaintain = pszCmd[0] == 'm';
		if (CMD_ARGC() <= iArg + 1)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, bMaintain ? "Ussage: maintain <alteration arguments (need to be aimed at an ent)>\n" : "Ussage: edit <alteration arguments (need to be aimed at an ent)>\n");
			return;
		}
		CBaseEntity* pAimed = GetAimedEntity(pev);
		if (!pAimed)
			return;
		sprintf(szLine, bMaintain ? "maintain %i " : "edit %i ", ENTINDEX(pAimed->edict()));
		LCS_AppendArgs(szLine, iArg + 1);
		pszMessage = UTIL_VarArgs(bMaintain ? "Maintaing edit on \"%s\"\n" : "Editting \"%s\"\n", STRING(pAimed->pev->classname));
	}
	else
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, "LCS commands are:\n");
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tcreate\n\tadvcreate\n\trespawncreate\n\tsetspawncmd\n\tremove\n\tkill\n\tuse\n\tedit\n\tmaintain\n");
		return;
	}

	ClientPrint(pev, HUD_PRINTCONSOLE, pszMessage);
	LCS_AppendLine(szLine);
}
