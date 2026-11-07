/***
*
*	Rocket Crowbar 2: rc2prefs.cpp. rc2.ini's values, its loader and writers, the "rccfg"
*	console command and the sync of NETWORK_SYNC keys to clients: the original's functions at
*	0xaea60-0xb1180. See rc2prefs.h. Every function name here is ours (rc2.dll keeps none).
*
*	rc2.ini is "[Heading]" lines and "Key=Value" lines. The table (g_rc2Prefs) is fixed: a key
*	the table doesn't have is reported and ignored, and the writers write the table's keys,
*	grouped by heading in table order.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "game.h"
#include "UserMessages.h"
#include "rc2prefs.h"

#include <ctype.h>
#include <math.h>

// orig .bss 0x191bc8: one 8-byte slot per key of g_rc2Prefs
rc2prefvalue_t g_rc2PrefValues[RC2P_COUNT];

extern int gmsgRCCFG; // "RCCFG", RC2's own (orig .bss 0x18f4c4)

bool IsTempAdmin(edict_t* pEntity); // name ours: orig 0x1160, sv_tempadmins (TEMP STUB below)

static void RC2Prefs_ParseValue(int iIndex, const char* pszText, rc2prefvalue_t* pValue);
static void RC2Prefs_FormatValue(int iIndex, const rc2prefvalue_t* pValue, char* pszOut);
static bool RC2Prefs_ValuesEqual(int iIndex, const rc2prefvalue_t* pA, const rc2prefvalue_t* pB);
static void RC2Prefs_RegisterAllWeaponInfo();

// The writers and listings mark keys done in a bit array of ceil(count / 8) bytes.
static unsigned char* AllocDoneBits()
{
	unsigned char* pBits = (unsigned char*)malloc((size_t)ceil(RC2P_COUNT / 8.0));
	for (int i = 0; i < (int)ceil(RC2P_COUNT / 8.0); i++)
		pBits[i] = 0;
	return pBits;
}

static inline bool IsDone(const unsigned char* pBits, int i) { return (pBits[i / 8] & (1 << (i % 8))) != 0; }
static inline void SetDone(unsigned char* pBits, int i) { pBits[i / 8] |= 1 << (i % 8); }

// Reads an rc2.ini over the current values. Keys it doesn't name keep theirs.
static void RC2Prefs_ReadFile(const char* pszPath) // orig 0xaea60
{
	FILE* pFile = fopen(pszPath, "r");
	if (!pFile)
		return;

	char szHeading[256];
	char szLine[256];
	szHeading[0] = 0;
	bool bInHeading = false;

	do
	{
		// VERIFY: as the original, fgets' result isn't checked; at EOF the buffer keeps the
		// last line and the loop ends on feof
		fgets(szLine, 255, pFile);
		szLine[255] = 0;
		int iLen = strlen(szLine);
		if (iLen <= 2)
			continue;
		while (iLen > 2 && isspace((unsigned char)szLine[iLen - 1]))
			iLen--;
		if (iLen <= 2)
			continue;
		szLine[iLen] = 0;

		if (szLine[0] == '[' && szLine[iLen - 1] == ']')
		{
			strncpy(szHeading, szLine + 1, iLen - 2);
			szHeading[iLen - 2] = 0;
			bInHeading = true;
		}
		else if (bInHeading)
		{
			char* pszEquals = strstr(szLine, "=");
			if (!pszEquals)
			{
				ALERT(at_console, "In file \"%s\": line \"%s\" is invalid (ignoring)\n", pszPath, szLine);
				continue;
			}
			*pszEquals = 0;

			int i;
			for (i = 0; i < RC2P_COUNT; i++)
			{
				if (stricmp(g_rc2Prefs[i].szHeading, szHeading) == 0 && stricmp(szLine, g_rc2Prefs[i].szKey) == 0)
				{
					RC2Prefs_ParseValue(i, pszEquals + 1, &g_rc2PrefValues[i]);
					break;
				}
			}
			if (i == RC2P_COUNT)
				ALERT(at_console, "In file \"%s\": cannot match key [%s]%s=%s (ignoring)\n", pszPath, szHeading, szLine, pszEquals + 1);
		}
		else
		{
			ALERT(at_console, "In file \"%s\": line \"%s\" is outside all headings (ignoring)\n", pszPath, szLine);
		}
	} while (!feof(pFile));

	fclose(pFile);
}

// Writes every key, grouped by heading in table order.
static void RC2Prefs_WriteFile(const char* pszPath) // orig 0xaec40
{
	FILE* pFile = fopen(pszPath, "w");
	if (!pFile)
		return;

	unsigned char* pDone = AllocDoneBits();
	char szValue[32];

	for (int iHead = 0; iHead < RC2P_COUNT; iHead++)
	{
		if (IsDone(pDone, iHead))
			continue;

		fprintf(pFile, "[%s]\n", g_rc2Prefs[iHead].szHeading);
		for (int i = 0; i < RC2P_COUNT; i++)
		{
			if (!IsDone(pDone, i) && stricmp(g_rc2Prefs[i].szHeading, g_rc2Prefs[iHead].szHeading) == 0)
			{
				SetDone(pDone, i);
				RC2Prefs_FormatValue(i, &g_rc2PrefValues[i], szValue);
				fprintf(pFile, "%s=%s\n", g_rc2Prefs[i].szKey, szValue);
			}
		}
		fprintf(pFile, "\n");
		fflush(pFile);
	}

	free(pDone);
	fclose(pFile);
}

// As RC2Prefs_WriteFile, but meant to write only the keys that differ from their defaults.
// The original tests the heading's first key (iHead) for every key of the heading, so a
// heading is written whole when its first key differs and empty when it doesn't; kept.
static void RC2Prefs_WriteDiffFile(const char* pszPath) // orig 0xaee10
{
	FILE* pFile = fopen(pszPath, "w");
	if (!pFile)
		return;

	unsigned char* pDone = AllocDoneBits();
	char szValue[32];
	rc2prefvalue_t defaultValue;

	for (int iHead = 0; iHead < RC2P_COUNT; iHead++)
	{
		if (IsDone(pDone, iHead))
			continue;

		fprintf(pFile, "[%s]\n", g_rc2Prefs[iHead].szHeading);
		for (int i = 0; i < RC2P_COUNT; i++)
		{
			if (!IsDone(pDone, i) && stricmp(g_rc2Prefs[i].szHeading, g_rc2Prefs[iHead].szHeading) == 0)
			{
				SetDone(pDone, i);
				// original bug: iHead, not i
				RC2Prefs_ParseValue(iHead, g_rc2Prefs[iHead].szDefault, &defaultValue);
				if (!RC2Prefs_ValuesEqual(iHead, &defaultValue, &g_rc2PrefValues[iHead]))
				{
					RC2Prefs_FormatValue(i, &g_rc2PrefValues[i], szValue);
					fprintf(pFile, "%s=%s\n", g_rc2Prefs[i].szKey, szValue);
				}
			}
		}
		fprintf(pFile, "\n");
		fflush(pFile);
	}

	free(pDone);
	fclose(pFile);
}

// pszFile NULL: the rccfgfile cvar's. Paths are under the game directory.
void RC2Prefs_LoadConfig(const char* pszFile) // orig 0xaf030
{
	char szGameDir[512];
	char szPath[512];

	if (!pszFile)
		pszFile = CVAR_GET_STRING("rccfgfile");
	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/%s", szGameDir, pszFile);
	RC2Prefs_ReadFile(szPath);
}

void RC2Prefs_SaveConfig(const char* pszFile) // orig 0xaf090
{
	char szGameDir[512];
	char szPath[512];

	if (!pszFile)
		pszFile = CVAR_GET_STRING("rccfgfile");
	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/%s", szGameDir, pszFile);
	RC2Prefs_WriteFile(szPath);
}

void RC2Prefs_SaveDiffConfig(const char* pszFile) // orig 0xaf0f0
{
	char szGameDir[512];
	char szPath[512];

	if (!pszFile)
		pszFile = CVAR_GET_STRING("rccfgfile");
	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/%s", szGameDir, pszFile);
	RC2Prefs_WriteDiffFile(szPath);
}

// The value part of an RCCFG message, by the key's type.
static void RC2Prefs_WriteValue(int iIndex, const rc2prefvalue_t* pValue) // orig 0xaf150
{
	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
		WRITE_LONG(pValue->i); // the float's bits
		break;
	case RC2PT_SMALLFLOAT:
		WRITE_SHORT((short)(int)(pValue->f * 100.0));
		break;
	case RC2PT_DOUBLE:
		// original bug: the low half twice
		WRITE_LONG(((const int*)pValue)[0]);
		WRITE_LONG(((const int*)pValue)[0]);
		break;
	case RC2PT_INT:
		WRITE_LONG(pValue->i);
		break;
	case RC2PT_SHORT:
		WRITE_SHORT(pValue->s);
		break;
	case RC2PT_USHORT:
		WRITE_SHORT(pValue->s);
		break;
	case RC2PT_BYTE:
		WRITE_BYTE(pValue->uc);
		break;
	case RC2PT_BOOL:
		WRITE_BYTE(pValue->c != 0);
		break;
	}
}

// Sends one client every NETWORK_SYNC key whose value isn't its default.
void RC2Prefs_SendToClient(edict_t* pClient) // orig 0xaf270
{
	rc2prefvalue_t defaultValue;

	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;
		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &defaultValue);
		if (RC2Prefs_ValuesEqual(i, &defaultValue, &g_rc2PrefValues[i]))
			continue;
		MESSAGE_BEGIN(MSG_ONE, gmsgRCCFG, g_vecZero, pClient);
		WRITE_SHORT(i);
		RC2Prefs_WriteValue(i, &g_rc2PrefValues[i]);
		MESSAGE_END();
	}
}

// After key iIndex changed.
static void RC2Prefs_KeyChanged(int iIndex) // orig 0xaf300
{
	if ((g_rc2Prefs[iIndex].iFlags & RC2PF_REGISTER_ITEM_INFO) != 0)
		RC2Prefs_RegisterAllWeaponInfo();

	if ((g_rc2Prefs[iIndex].iFlags & RC2PF_NETWORK_SYNC) != 0)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgRCCFG);
		WRITE_SHORT(iIndex);
		RC2Prefs_WriteValue(iIndex, &g_rc2PrefValues[iIndex]);
		MESSAGE_END();
	}
}

void RC2Prefs_SendResetToClient(edict_t* pClient) // orig 0xaf370
{
	MESSAGE_BEGIN(MSG_ONE, gmsgRCCFG, g_vecZero, pClient);
	WRITE_SHORT(0x7fff);
	MESSAGE_END();
}

static void RC2Prefs_SendResetToAll() // orig 0xaf3a0
{
	RC2Prefs_RegisterAllWeaponInfo();
	MESSAGE_BEGIN(MSG_ALL, gmsgRCCFG);
	WRITE_SHORT(0x7fff);
	MESSAGE_END();
}

// A line to one client's console (TextMsg, HUD_PRINTCONSOLE), unreliable. Name ours; the
// original writes it out at each use.
static void ConsoleLine(edict_t* pEntity, const char* pszText)
{
	MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgTextMsg, NULL, pEntity);
	WRITE_BYTE(HUD_PRINTCONSOLE);
	WRITE_STRING(pszText);
	MESSAGE_END();
}

// "rccfg listheadings"
static void RC2Prefs_ListHeadings(edict_t* pEntity) // orig 0xaf3d0
{
	unsigned char* pDone = AllocDoneBits();
	char szLine[64];

	for (int iHead = 0; iHead < RC2P_COUNT; iHead++)
	{
		if (IsDone(pDone, iHead))
			continue;

		snprintf(szLine, sizeof(szLine), "\t%s\n", g_rc2Prefs[iHead].szHeading); // the original: sprintf, then [63] = 0
		szLine[63] = 0;
		ConsoleLine(pEntity, szLine);

		for (int i = 0; i < RC2P_COUNT; i++)
		{
			if (!IsDone(pDone, i) && stricmp(g_rc2Prefs[iHead].szHeading, g_rc2Prefs[i].szHeading) == 0)
				SetDone(pDone, i);
		}
	}

	free(pDone);
}

// "rccfg listkeys heading page": 20 keys a page.
static void RC2Prefs_ListKeys(edict_t* pEntity, char* pszHeading, int iPage) // orig 0xaf550
{
	unsigned char* pDone = AllocDoneBits();
	char szValue[64];
	char szLine[128];

	int iLen = strlen(pszHeading);
	if (pszHeading[0] == '[')
	{
		pszHeading++;
		iLen--;
	}
	if (pszHeading[iLen - 1] == ']')
		pszHeading[iLen - 1] = 0;

	if (iPage < 1)
		iPage = 1;
	const int iFirst = iPage * 20 - 20;
	int iCount = 0;

	for (int iHead = 0; iHead < RC2P_COUNT; iHead++)
	{
		if (IsDone(pDone, iHead))
			continue;

		if (stricmp(pszHeading, g_rc2Prefs[iHead].szHeading) != 0)
		{
			SetDone(pDone, iHead);
			continue;
		}

		for (int i = 0; i < RC2P_COUNT; i++)
		{
			if (IsDone(pDone, i) || stricmp(g_rc2Prefs[iHead].szHeading, g_rc2Prefs[i].szHeading) != 0)
				continue;
			SetDone(pDone, i);
			if (iFirst <= iCount && iCount < iPage * 20)
			{
				RC2Prefs_FormatValue(i, &g_rc2PrefValues[i], szValue);
				snprintf(szLine, sizeof(szLine), "\t%s=%s\n", g_rc2Prefs[i].szKey, szValue);
				szLine[127] = 0;
				ConsoleLine(pEntity, szLine);
			}
			iCount++;
		}
		break;
	}

	free(pDone);

	// an unknown heading gives "Page 1 of 0" for page 1, "Last page is 0" after
	if (iFirst <= iCount)
		snprintf(szLine, sizeof(szLine), "Page %i of %i\n", iPage, (int)ceil(iCount * 0.05f));
	else
		snprintf(szLine, sizeof(szLine), "Last page is %i\n", (int)ceil(iCount * 0.05f));
	szLine[127] = 0;
	ConsoleLine(pEntity, szLine);
}

static void AppendLine(char* pszLine, const char* pszText) // name ours
{
	strncat(pszLine, pszText, 255 - strlen(pszLine));
}

// "rccfg findkey partialkey [partialheading]": the keys starting with partialkey (and their
// heading with partialheading), with type and flags; stops after 22.
static void RC2Prefs_FindKey(edict_t* pEntity, const char* pszKey, char* pszHeading) // orig 0xaf890
{
	int iHeadLen = 0;
	if (pszHeading)
	{
		iHeadLen = strlen(pszHeading);
		if (pszHeading[0] == '[')
		{
			pszHeading++;
			iHeadLen--;
		}
		if (pszHeading[iHeadLen - 1] == ']')
		{
			pszHeading[iHeadLen - 1] = 0;
			iHeadLen--;
		}
	}

	const int iKeyLen = strlen(pszKey);
	int iShown = 0;
	char szValue[64];
	char szLine[256];

	for (int i = 0; i < RC2P_COUNT; i++)
	{
		const rc2pref_t& pref = g_rc2Prefs[i];
		if (strnicmp(pszKey, pref.szKey, iKeyLen) != 0)
			continue;
		if (pszHeading && strnicmp(pszHeading, pref.szHeading, iHeadLen) != 0)
			continue;

		RC2Prefs_FormatValue(i, &g_rc2PrefValues[i], szValue);
		snprintf(szLine, sizeof(szLine), "\t[%s]%s=%s\n\t", pref.szHeading, pref.szKey, szValue);
		szLine[255] = 0;
		AppendLine(szLine, "TYPE: ");
		switch (pref.iType)
		{
		case RC2PT_FLOAT: AppendLine(szLine, "FLOAT (-3E38...3E38)"); break;
		case RC2PT_SMALLFLOAT: AppendLine(szLine, "SMALL FLOAT (0...655.35)"); break;
		case RC2PT_DOUBLE: AppendLine(szLine, "DOUBLE (-1E308...1E38)"); break;
		case RC2PT_INT: AppendLine(szLine, "INT (-2E9...2E9)"); break;
		case RC2PT_SHORT: AppendLine(szLine, "SHORT (-32767...32767)"); break;
		case RC2PT_USHORT: AppendLine(szLine, "UNSIGNED SHORT (0...65535)"); break;
		case RC2PT_BYTE: AppendLine(szLine, "BYTE (0...255)"); break;
		case RC2PT_BOOL: AppendLine(szLine, "BOOL (T/F)"); break;
		}
		AppendLine(szLine, "  FLAGS: ");
		if (pref.iFlags == 0)
		{
			AppendLine(szLine, "NONE\n");
		}
		else
		{
			if (pref.iFlags & RC2PF_POS_ONLY)
				AppendLine(szLine, "POS_ONLY ");
			if (pref.iFlags & RC2PF_THREE_DIGITS)
				AppendLine(szLine, "THREE_DIGITS ");
			if (pref.iFlags & RC2PF_NOT_ZERO)
				AppendLine(szLine, "NOT_ZERO ");
			if (pref.iFlags & RC2PF_GREATER_THEN_ONE)
				AppendLine(szLine, "GREATER_THEN_ONE ");
			if (pref.iFlags & RC2PF_ZERO_TO_ONE)
				AppendLine(szLine, "ZERO_TO_ONE ");
			if (pref.iFlags & RC2PF_NEGATIVE_ONE_TO_ONE)
				AppendLine(szLine, "NEGATIVE_ONE_TO_ONE ");
			if (pref.iFlags & RC2PF_FOUR_DIGITS)
				AppendLine(szLine, "FOUR_DIGITS ");
			if (pref.iFlags & RC2PF_REGISTER_ITEM_INFO)
				AppendLine(szLine, "REGISTER_ITEM_INFO ");
			if (pref.iFlags & RC2PF_NETWORK_SYNC)
				AppendLine(szLine, "NETWORK_SYNC ");
			AppendLine(szLine, "\n");
		}
		szLine[255] = 0;
		ConsoleLine(pEntity, szLine);

		if (iShown++ > 20)
			return;
	}
}

// "rccfg setkey heading key value"
static void RC2Prefs_SetKey(edict_t* pEntity, char* pszHeading, const char* pszKey, const char* pszValue) // orig 0xafd70
{
	int iLen = strlen(pszHeading);
	if (pszHeading[0] == '[')
	{
		pszHeading++;
		iLen--;
	}
	if (pszHeading[iLen - 1] == ']')
		pszHeading[iLen - 1] = 0;

	int i;
	for (i = 0; i < RC2P_COUNT; i++)
	{
		if (stricmp(pszHeading, g_rc2Prefs[i].szHeading) == 0 && stricmp(pszKey, g_rc2Prefs[i].szKey) == 0)
			break;
	}
	if (i == RC2P_COUNT)
	{
		ClientPrint(pEntity ? VARS(pEntity) : NULL, HUD_PRINTCONSOLE, "Key not found\n");
		return;
	}

	char szValue[64];
	char szLine[256];
	RC2Prefs_ParseValue(i, pszValue, &g_rc2PrefValues[i]);
	RC2Prefs_KeyChanged(i);
	RC2Prefs_FormatValue(i, &g_rc2PrefValues[i], szValue);
	snprintf(szLine, sizeof(szLine), "RCCFG Key Changed: [%s]%s=%s\n", g_rc2Prefs[i].szHeading, g_rc2Prefs[i].szKey, szValue);
	szLine[255] = 0;
	UTIL_ClientPrintAll(HUD_PRINTNOTIFY, szLine);
}

void RC2Prefs_SendToAllClients() // orig 0xafeb0
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* pClient = INDEXENT(i);
		if (UTIL_IsValidEntity(pClient))
			RC2Prefs_SendToClient(pClient);
	}
}

// Re-reads a weapon's ItemInfo into CBasePlayerItem::ItemInfoArray from a throwaway copy, as
// UTIL_PrecacheOtherWeapon does (keys such as max ammo feed GetItemInfo).
static void RC2Prefs_RegisterWeaponInfo(const char* pszClassname) // orig 0xaff00
{
	edict_t* pent = CREATE_NAMED_ENTITY(MAKE_STRING(pszClassname));
	if (FNullEnt(pent))
	{
		ALERT(at_console, "NULL Ent in RCCFG_RegisterWeapon\n");
		return;
	}

	// the original calls GetItemInfo's slot on whatever the entity is, unchecked
	CBasePlayerItem* pEntity = (CBasePlayerItem*)CBaseEntity::Instance(VARS(pent));
	if (pEntity)
	{
		ItemInfo II;
		memset(&II, 0, sizeof(II));
		if (pEntity->GetItemInfo(&II))
			CBasePlayerItem::ItemInfoArray[II.iId] = II;
	}

	REMOVE_ENTITY(pent);
}

static void RC2Prefs_RegisterAllWeaponInfo() // orig 0xaffc0
{
	RC2Prefs_RegisterWeaponInfo("weapon_shotgun");
	RC2Prefs_RegisterWeaponInfo("weapon_crowbar");
	RC2Prefs_RegisterWeaponInfo("weapon_9mmhandgun");
	RC2Prefs_RegisterWeaponInfo("weapon_9mmAR");
	RC2Prefs_RegisterWeaponInfo("weapon_357");
	RC2Prefs_RegisterWeaponInfo("weapon_gauss");
	RC2Prefs_RegisterWeaponInfo("weapon_rpg");
	RC2Prefs_RegisterWeaponInfo("weapon_crossbow");
	RC2Prefs_RegisterWeaponInfo("weapon_egon");
	RC2Prefs_RegisterWeaponInfo("weapon_tripmine");
	RC2Prefs_RegisterWeaponInfo("weapon_satchel");
	RC2Prefs_RegisterWeaponInfo("weapon_handgrenade");
	RC2Prefs_RegisterWeaponInfo("weapon_snark");
	RC2Prefs_RegisterWeaponInfo("weapon_hornetgun");
}

// "rccfg <command> ...". Listing is open to all; setkey and reset need admin level 1 (or a
// temporary admin), save, savediff and load level 2. Commands are case-sensitive.
void RC2Prefs_ClientCommand(edict_t* pEntity) // orig 0xb0050
{
	const char* pszCommand = CMD_ARGV(1);

	if (!pEntity || OFFSET(pEntity) == 0 || !pEntity->pvPrivateData || pEntity->free)
		return;

	entvars_t* pev = VARS(pEntity);
	CBasePlayer* pPlayer = (CBasePlayer*)pEntity->pvPrivateData;
	bool bAdmin = pPlayer->m_iAdminLevel > 0;
	const bool bSuperAdmin = pPlayer->m_iAdminLevel > 1;
	if (!bAdmin)
		bAdmin = IsTempAdmin(pEntity);

	char szArg1[64];
	char szArg2[64];

	if (CMD_ARGC() < 2)
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, "Availabe configuration commands:\n");
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tlistheadings\n\tlistkeys\n\tfindkey\n");
		if (!bAdmin)
			return;
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tsetkey\n\treset\n");
		if (!bSuperAdmin)
			return;
		ClientPrint(pev, HUD_PRINTCONSOLE, "\tsave\n\tsavediff\n\tload\n");
		return;
	}

	if (strcmp(pszCommand, "listheadings") == 0)
	{
		RC2Prefs_ListHeadings(pEntity);
		return;
	}

	if (strcmp(pszCommand, "listkeys") == 0)
	{
		if (CMD_ARGC() < 4)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: listkeys heading pagenumber(1-N)\n");
			return;
		}
		strncpy(szArg1, CMD_ARGV(2), 63);
		szArg1[63] = 0;
		RC2Prefs_ListKeys(pEntity, szArg1, atoi(CMD_ARGV(3)));
		return;
	}

	if (strcmp(pszCommand, "findkey") == 0)
	{
		if (CMD_ARGC() < 3)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: findkey <partial key> [partial heading]\n");
			return;
		}
		strncpy(szArg1, CMD_ARGV(2), 63);
		szArg1[63] = 0;
		if (CMD_ARGC() < 4)
		{
			RC2Prefs_FindKey(pEntity, szArg1, NULL);
			return;
		}
		strncpy(szArg2, CMD_ARGV(3), 63);
		szArg2[63] = 0;
		RC2Prefs_FindKey(pEntity, szArg1, szArg2);
		return;
	}

	if (strcmp(pszCommand, "setkey") == 0)
	{
		if (bAdmin)
		{
			if (CMD_ARGC() > 4)
			{
				// the original hands over the engine's argv buffers and strips the heading's [] in place
				RC2Prefs_SetKey(pEntity, (char*)CMD_ARGV(2), CMD_ARGV(3), CMD_ARGV(4));
				return;
			}
			ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: setkey heading key value\n");
			return;
		}
	}
	else if (strcmp(pszCommand, "reset") == 0)
	{
		if (bAdmin)
		{
			UTIL_ClientPrintAll(HUD_PRINTNOTIFY, "RCCFG Restoring defaults\n");
			RC2Prefs_ResetToDefaults();
			RC2Prefs_SendResetToAll();
			return;
		}
	}
	else if (strcmp(pszCommand, "save") == 0)
	{
		if (bSuperAdmin)
		{
			if (CMD_ARGC() < 3)
			{
				RC2Prefs_SaveConfig(NULL);
				return;
			}
			strncpy(szArg1, CMD_ARGV(2), 63);
			szArg1[63] = 0;
			RC2Prefs_SaveConfig(szArg1);
			return;
		}
	}
	else if (strcmp(pszCommand, "savediff") == 0)
	{
		if (bSuperAdmin)
		{
			if (CMD_ARGC() < 3)
			{
				ClientPrint(pev, HUD_PRINTCONSOLE, "Ussage: savediff filename\n");
				return;
			}
			strncpy(szArg1, CMD_ARGV(2), 63);
			szArg1[63] = 0;
			RC2Prefs_SaveDiffConfig(szArg1);
			return;
		}
	}
	else if (strcmp(pszCommand, "load") == 0)
	{
		if (bSuperAdmin)
		{
			if (CMD_ARGC() < 3)
			{
				UTIL_ClientPrintAll(HUD_PRINTNOTIFY, "RCCFG Reloading config\n");
				RC2Prefs_LoadConfig(NULL);
				RC2Prefs_SendToAllClients();
				return;
			}
			strncpy(szArg1, CMD_ARGV(2), 63);
			szArg1[63] = 0;
			UTIL_ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("RCCFG Loading config from \"%s\"\n", szArg1));
			RC2Prefs_LoadConfig(szArg1);
			RC2Prefs_SendToAllClients();
			return;
		}
	}
	else
	{
		char szCommand[128];
		strncpy(szCommand, pszCommand, 127);
		szCommand[127] = 0;
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Unkown configuration command \"%s\"\n", szCommand));
		return;
	}

	ClientPrint(pev, HUD_PRINTCONSOLE, "You need higher admin permissions to use this command\n");
}

// The clamps a key's flags ask for, in the original's order, on a value of type T (the
// type the original compares in: signed short even for RC2PT_USHORT, unsigned char for
// BYTE and BOOL). For unsigned char the negative and above-255 tests never fire, as in the
// original's code.
template <typename T>
static void ApplyFlags(unsigned int iFlags, T& value)
{
	if ((iFlags & RC2PF_POS_ONLY) != 0 && value < 0)
		value = 0;
	if ((iFlags & RC2PF_THREE_DIGITS) != 0)
		value = (value < 0) ? 0 : ((value > 999) ? 999 : value);
	if ((iFlags & RC2PF_ZERO_TO_ONE) != 0)
		value = (value < 0) ? 0 : ((value > 1) ? 1 : value);
	if ((iFlags & RC2PF_NEGATIVE_ONE_TO_ONE) != 0)
		value = (value < -1) ? -1 : ((value > 1) ? 1 : value);
	if ((iFlags & RC2PF_FOUR_DIGITS) != 0)
		value = (value < 0) ? 0 : ((value > 9999) ? 9999 : value);
	if ((iFlags & RC2PF_GREATER_THEN_ONE) != 0 && value < 1)
		value = 1;
	if ((iFlags & RC2PF_NOT_ZERO) != 0 && value == 0)
		value = 1;
}

// Parses pszText as key iIndex's type and clamps it by the key's flags.
static void RC2Prefs_ParseValue(int iIndex, const char* pszText, rc2prefvalue_t* pValue) // orig 0xb06a0
{
	const unsigned int iFlags = g_rc2Prefs[iIndex].iFlags;

	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
		pValue->f = (float)atof(pszText);
		ApplyFlags(iFlags, pValue->f);
		break;
	case RC2PT_SMALLFLOAT:
	{
		double flValue = atof(pszText);
		if (flValue < 0.0)
			flValue = 0.0;
		else if (flValue > 655.35)
			flValue = 655.35;
		pValue->f = (float)flValue;
		ApplyFlags(iFlags, pValue->f);
		break;
	}
	case RC2PT_DOUBLE:
		pValue->d = atof(pszText);
		ApplyFlags(iFlags, pValue->d);
		break;
	case RC2PT_INT:
		pValue->i = atoi(pszText);
		ApplyFlags(iFlags, pValue->i);
		break;
	case RC2PT_SHORT:
	{
		int iValue = atoi(pszText);
		pValue->s = (short)((iValue < -0x7fff) ? -0x7fff : ((iValue > 0x7fff) ? 0x7fff : iValue));
		ApplyFlags(iFlags, pValue->s);
		break;
	}
	case RC2PT_USHORT:
	{
		int iValue = atoi(pszText);
		pValue->s = (short)((iValue < 0) ? 0 : ((iValue > 0xffff) ? 0xffff : iValue));
		ApplyFlags(iFlags, pValue->s); // signed, as the original: 32768 and up count as negative
		break;
	}
	case RC2PT_BYTE:
	{
		int iValue = atoi(pszText);
		pValue->uc = (unsigned char)((iValue < 0) ? 0 : ((iValue > 0xff) ? 0xff : iValue));
		ApplyFlags(iFlags, pValue->uc);
		break;
	}
	case RC2PT_BOOL:
		pValue->uc = (stricmp(pszText, "true") == 0 || stricmp(pszText, "yes") == 0 || atoi(pszText) != 0) ? 1 : 0;
		ApplyFlags(iFlags, pValue->uc);
		break;
	}
}

// Key iIndex's value as rc2.ini text.
static void RC2Prefs_FormatValue(int iIndex, const rc2prefvalue_t* pValue, char* pszOut) // orig 0xb1030
{
	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
	case RC2PT_SMALLFLOAT:
		sprintf(pszOut, "%f", (double)pValue->f);
		break;
	case RC2PT_DOUBLE:
		sprintf(pszOut, "%f", pValue->d);
		break;
	case RC2PT_INT:
		sprintf(pszOut, "%d", pValue->i);
		break;
	case RC2PT_SHORT:
		sprintf(pszOut, "%d", pValue->s);
		break;
	case RC2PT_USHORT:
		sprintf(pszOut, "%d", pValue->us);
		break;
	case RC2PT_BYTE:
		sprintf(pszOut, "%d", pValue->uc);
		break;
	case RC2PT_BOOL:
		sprintf(pszOut, "%s", pValue->c != 0 ? "true" : "false");
		break;
	}
}

// Every key back to its default (also what the world's precache does before loading).
void RC2Prefs_ResetToDefaults() // orig 0xb1150
{
	for (int i = 0; i < RC2P_COUNT; i++)
		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &g_rc2PrefValues[i]);
}

static bool RC2Prefs_ValuesEqual(int iIndex, const rc2prefvalue_t* pA, const rc2prefvalue_t* pB) // orig 0xb1180
{
	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
	case RC2PT_SMALLFLOAT:
		return pA->f == pB->f;
	case RC2PT_DOUBLE:
		return pA->d == pB->d;
	case RC2PT_INT:
		return pA->i == pB->i;
	case RC2PT_SHORT:
	case RC2PT_USHORT:
		return pA->s == pB->s;
	case RC2PT_BYTE:
		return pA->c == pB->c;
	case RC2PT_BOOL:
		return (pA->c != 0) == (pB->c != 0);
	}
	return false;
}


// The three below pack the NETWORK_SYNC keys into one buffer and back: a bit per NETWORK_SYNC
// key (set when its value isn't its default), then those values in table order, each in its
// type's size (a SMALLFLOAT as a short of hundredths). Nothing in rc2.dll calls them; the
// client gets the values one RCCFG message per key instead (RC2Prefs_SendToClient).
int RC2Prefs_PackedSize(int* piCount);
void RC2Prefs_PackValues(unsigned char** ppBuf, int* piSize);
bool RC2Prefs_UnpackValues(const unsigned char* pBuf, int iSize);

// The packed size; *piCount gets the number of NETWORK_SYNC keys.
int RC2Prefs_PackedSize(int* piCount) // orig 0xb1260, name ours
{
	int iCount = 0;
	int iSize = 0;
	rc2prefvalue_t defaultValue;

	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;
		iCount++;
		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &defaultValue);
		if (RC2Prefs_ValuesEqual(i, &g_rc2PrefValues[i], &defaultValue))
			continue;

		switch (g_rc2Prefs[i].iType)
		{
		case RC2PT_FLOAT:
		case RC2PT_INT:
			iSize += 4;
			break;
		case RC2PT_DOUBLE:
			iSize += 8;
			break;
		case RC2PT_SMALLFLOAT:
		case RC2PT_SHORT:
		case RC2PT_USHORT:
			iSize += 2;
			break;
		case RC2PT_BYTE:
		case RC2PT_BOOL:
			iSize += 1;
			break;
		}
	}

	if (piCount)
		*piCount = iCount;

	return (int)(ceil(iCount * 0.125) + iSize);
}

// A malloc'ed buffer in *ppBuf (NULL when no key differs) and its size in *piSize.
void RC2Prefs_PackValues(unsigned char** ppBuf, int* piSize) // orig 0xb1340, name ours
{
	int iCount;

	*ppBuf = NULL;
	iCount = 0;
	*piSize = 0;

	const int iSize = RC2Prefs_PackedSize(&iCount);
	if (iSize <= 0 || iCount <= 0)
		return;

	const int iBitBytes = (int)ceil(iCount * 0.125);
	if (iBitBytes == iSize)
		return;

	unsigned char* pBuf = (unsigned char*)malloc(iSize);
	if (!pBuf)
		return;
	memset(pBuf, 0, iSize);

	unsigned char* p = pBuf + iBitBytes;
	int iBit = 0;
	rc2prefvalue_t defaultValue;

	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if (p - pBuf >= iSize)
			break;

		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;

		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &defaultValue);
		if (RC2Prefs_ValuesEqual(i, &g_rc2PrefValues[i], &defaultValue))
		{
			iBit++;
			continue;
		}

		pBuf[iBit / 8] |= 1 << (iBit % 8);
		iBit++;

		const rc2prefvalue_t& value = g_rc2PrefValues[i];
		switch (g_rc2Prefs[i].iType)
		{
		case RC2PT_FLOAT:
		case RC2PT_INT:
			memcpy(p, &value.i, 4);
			p += 4;
			break;
		case RC2PT_SMALLFLOAT:
		{
			const short sValue = (short)(int)(value.f * 100.0);
			memcpy(p, &sValue, 2);
			p += 2;
			break;
		}
		case RC2PT_DOUBLE:
			memcpy(p, &value.d, 8);
			p += 8;
			break;
		case RC2PT_SHORT:
		case RC2PT_USHORT:
			memcpy(p, &value.s, 2);
			p += 2;
			break;
		case RC2PT_BYTE:
			*p++ = value.uc;
			break;
		case RC2PT_BOOL:
			*p++ = value.c != 0 ? 1 : 0;
			break;
		}
	}

	*ppBuf = pBuf;
	*piSize = iSize;
}

// Every key back to its default, then the packed values over them. False when the buffer is
// too short for the bits or ends before the last value.
bool RC2Prefs_UnpackValues(const unsigned char* pBuf, int iSize) // orig 0xb1520, name ours
{
	RC2Prefs_ResetToDefaults();

	int iCount = 0;
	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) != 0)
			iCount++;
	}

	const int iBitBytes = (int)ceil(iCount * 0.125);
	if (iCount <= 0 || iSize < iBitBytes)
		return false;
	if (iSize == iBitBytes)
		return true;

	const unsigned char* p = pBuf + iBitBytes;
	int iBit = 0;

	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if (p - pBuf >= iSize)
			return false;

		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;

		if ((pBuf[iBit / 8] & (1 << (iBit % 8))) == 0)
		{
			iBit++;
			continue;
		}
		iBit++;

		rc2prefvalue_t& value = g_rc2PrefValues[i];
		switch (g_rc2Prefs[i].iType)
		{
		case RC2PT_FLOAT:
		case RC2PT_INT:
			memcpy(&value.i, p, 4);
			p += 4;
			break;
		case RC2PT_SMALLFLOAT:
		{
			unsigned short usValue;
			memcpy(&usValue, p, 2);
			// Original bug, kept: multiplies by 100 again where it should divide
			value.f = (float)usValue * 100.0f;
			p += 2;
			break;
		}
		case RC2PT_DOUBLE:
			memcpy(&value.d, p, 8);
			p += 8;
			break;
		case RC2PT_SHORT:
		case RC2PT_USHORT:
			memcpy(&value.s, p, 2);
			p += 2;
			break;
		case RC2PT_BYTE:
			value.uc = *p++;
			break;
		case RC2PT_BOOL:
			value.uc = *p++ != 0 ? 1 : 0;
			break;
		}
	}

	return true;
}

// gmsgRCCFG: UserMessages.h (p5)

// TEMP STUB (p6): real one belongs in the run before aflock (orig 0x1160): whether the
// player's WON id is listed in the sv_tempadmins cvar
bool IsTempAdmin(edict_t* pEntity)
{
	return false;
}
