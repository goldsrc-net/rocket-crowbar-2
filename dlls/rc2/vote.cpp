/***
*
*	Rocket Crowbar 2: the players' votes, to change the map ("votemap") or to kick a player
*	("votekick"), cast with "vote yes" or "vote no" (orig 0xf7ed0-0xf89d0, the run after SDK
*	2.3's voice_gamemgr.cpp). Every name here is ours.
*
*	A vote needs ceil(51%) (map) or ceil(75%) (kick) of the other eligible players (not HLTV
*	proxies). It ends when enough say yes, when too few are left undecided, or after 60 seconds,
*	when the votes cast decide. A player can call one every 120 seconds.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "maintainent.h"
#include "vote.h"

#include <cmath>

enum
{
	VOTE_NONE = 0,
	VOTE_CHANGELEVEL = 1,
	VOTE_KICK = 2,
};

static int g_iVoteType = VOTE_NONE;		 // orig 0x101af640
static EHANDLE g_hVoteTarget;			 // orig 0x101af648: the player a kick vote is about
static EHANDLE g_hVoteCaller;			 // orig 0x101af650
static float g_flNextVoteUpdate;		 // orig 0x101af658: VoteUpdate goes out twice a second
static int g_iVotedBits;				 // orig 0x101af65c: a bit per player who may not vote (any more)
static int g_iVoteYesBits;				 // orig 0x101af660
static int g_iVoteNoBits;				 // orig 0x101af664
static float g_flVoteEnd;				 // orig 0x101af668
static float g_flVoteFraction = 0.51f;	 // orig .data 0x10181024
static int g_iVotesNeeded = 1;			 // orig .data 0x10181028
static float g_flChangeLevelTime = -1.0; // orig .data 0x1018102c: when a won map vote changes level
static char g_szVoteMap[256];			 // orig 0x101af440
static char g_szChangeLevelMap[256];	 // orig 0x101af540

static int PlayerBit(int iIndex)
{
	return 1 << (iIndex - 1);
}

static bool IsVoter(CBaseEntity* pPlayer)
{
	return (pPlayer->pev->flags & FL_PROXY) == 0;
}

void ResetVote() // orig 0xf7ed0
{
	g_iVoteType = VOTE_NONE;
	g_iVoteNoBits = 0;
	g_iVoteYesBits = 0;
	g_iVotedBits = 0;
	g_flVoteEnd = 0;
	g_iVotesNeeded = 0;
	g_flNextVoteUpdate = gpGlobals->time;
	g_hVoteCaller = NULL;
	g_hVoteTarget = NULL;
}

// ends a running vote and clears the clients' vote display. Name p1's.
void StopVote() // orig 0xf7f20
{
	if (g_iVoteType == VOTE_NONE)
		return;

	ResetVote();
	MESSAGE_BEGIN(MSG_ALL, gmsgVoteUpdate);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	MESSAGE_END();
}

static void VoteSucceeded(int iYes, int iNo) // orig 0xf7f70
{
	char szCommand[256];
	char szName[256];
	char szText[512];

	szCommand[0] = '\0';

	if (g_iVoteType == VOTE_CHANGELEVEL)
	{
		if (!IS_MAP_VALID(g_szVoteMap))
		{
			StopVote();
			return;
		}

		g_flChangeLevelTime = gpGlobals->time + 5.0;
		strcpy(g_szChangeLevelMap, g_szVoteMap);
		sprintf(szText, "* Vote to changelevel %s, has succeeded; %i vote%s for changelevel. Changelevel in %d seconds\n",
			g_szVoteMap, iYes, iYes == 1 ? "" : "s", (int)(g_flChangeLevelTime - gpGlobals->time + 0.5));
	}
	else if (g_iVoteType == VOTE_KICK)
	{
		if (!g_hVoteTarget)
		{
			StopVote();
			return;
		}

		// Original bug kept: the entity index where banid wants the user id
		sprintf(szCommand, "banid %i #%u kick\n", 10, ENTINDEX(g_hVoteTarget->edict()));
		strncpy(szName, STRING(g_hVoteTarget->pev->netname), 255);
		szName[255] = '\0';
		sprintf(szText, "* Vote to kick \"%s\", has succeeded; %i for kick\n", szName, iYes);
	}
	// VERIFY: with no vote running the original prints its uninitialized buffer; never reached

	UTIL_ClientPrintAll(HUD_PRINTTALK, szText);
	if ('\0' != szCommand[0])
		SERVER_COMMAND(szCommand);

	StopVote();
}

static void VoteFailed(int iYes, int iNo) // orig 0xf8110
{
	char szName[256];
	char szText[512];

	if (g_iVoteType == VOTE_CHANGELEVEL)
	{
		sprintf(szText, "* Vote to changelevel %s, has failed; %i vote%s for changelevel\n", g_szVoteMap, iYes, iYes == 1 ? "" : "s");
	}
	else if (g_iVoteType == VOTE_KICK)
	{
		if (!g_hVoteTarget)
		{
			StopVote();
			return;
		}

		strncpy(szName, STRING(g_hVoteTarget->pev->netname), 255);
		szName[255] = '\0';
		sprintf(szText, "* Vote to kick \"%s\", has failed; %i vote%s for kick\n", szName, iYes, iYes == 1 ? "" : "s");
	}
	else
	{
		StopVote();
		return;
	}

	UTIL_ClientPrintAll(HUD_PRINTTALK, szText);
	StopVote();
}

// time is up: the votes cast decide
static void VoteTimedOut(int iYes, int iNo) // orig 0xf81f0
{
	char szText[512];

	sprintf(szText, "* Vote timeout: %i for, %i against; arbitrating based on votes cast\n", iYes, iNo);
	UTIL_ClientPrintAll(HUD_PRINTTALK, szText);

	if (iYes > 0 && (int)ceil((iYes + iNo) * g_flVoteFraction) <= iYes)
		VoteSucceeded(iYes, iNo);
	else
		VoteFailed(iYes, iNo);
}

// every frame (and after each vote): the count, the result, the clients' display, and the
// level change a won map vote leads to
void VoteThink() // orig 0xf8280
{
	if (g_flChangeLevelTime != -1.0 && gpGlobals->time > g_flChangeLevelTime)
	{
		g_flChangeLevelTime = -1.0;
		if (IS_MAP_VALID(g_szChangeLevelMap))
		{
			ResetVote();
			MESSAGE_BEGIN(MSG_ALL, gmsgVoteUpdate);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			MESSAGE_END();
			CHANGE_LEVEL(g_szChangeLevelMap, NULL);
		}
		return;
	}

	if (g_iVoteType == VOTE_NONE)
		return;

	if (g_iVoteType == VOTE_KICK && !g_hVoteTarget)
	{
		StopVote();
		return;
	}

	int iYes = 0;
	int iNo = 0;
	int iUndecided = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer || !IsVoter(pPlayer))
			continue;

		if ((g_iVoteYesBits & PlayerBit(i)) != 0)
			iYes++;
		else if ((g_iVoteNoBits & PlayerBit(i)) != 0)
			iNo++;
		else
			iUndecided++;
	}

	if (iYes >= g_iVotesNeeded)
	{
		VoteSucceeded(iYes, iNo);
		return;
	}

	// VERIFY: the original fails the vote once the undecided alone (the caller among them)
	// couldn't reach the votes needed, whatever the yes votes so far
	if (iUndecided < g_iVotesNeeded)
	{
		VoteFailed(iYes, iNo);
		return;
	}

	if (g_flVoteEnd < gpGlobals->time)
	{
		VoteTimedOut(iYes, iNo);
		return;
	}

	if (g_flNextVoteUpdate > gpGlobals->time)
		return;
	g_flNextVoteUpdate = gpGlobals->time + 0.5;

	int iTimeLeft = (int)(g_flVoteEnd - gpGlobals->time);
	if (iTimeLeft > 255)
		iTimeLeft = 255;
	else if (iTimeLeft < 0)
		iTimeLeft = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer || !UTIL_IsValidEntity(pPlayer->edict()) || !IsVoter(pPlayer))
			continue;
		if ((g_iVotedBits & PlayerBit(i)) == 0)
			continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgVoteUpdate, NULL, pPlayer->edict());
		WRITE_BYTE(iYes);
		WRITE_BYTE(iNo);
		WRITE_BYTE(iTimeLeft);
		MESSAGE_END();
	}
}

static bool IsVoteClient(edict_t* pEdict)
{
	return pEdict && 0 != OFFSET(pEdict) && pEdict->pvPrivateData && 0 == pEdict->free;
}

// "vote yes|no"
void ClientVote(edict_t* pEdict) // orig 0xf8520
{
	const char* pszVote = CMD_ARGV(1);

	if (!IsVoteClient(pEdict) || !g_pGameRules->IsMultiplayer())
		return;

	if (g_iVoteType == VOTE_NONE)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Not vote in progress\n");
		return;
	}

	const int iBit = PlayerBit(ENTINDEX(pEdict));
	if ((g_iVotedBits & iBit) != 0)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "You have already voted\n");
		return;
	}

	g_iVotedBits |= iBit;
	if (0 == stricmp(pszVote, "yes"))
		g_iVoteYesBits |= iBit;
	else
		g_iVoteNoBits |= iBit;

	VoteThink();
}

// the others who may vote: valid, not proxies, not in g_iVotedBits
static int CountOtherVoters()
{
	int iCount = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer && UTIL_IsValidEntity(pPlayer->edict()) && IsVoter(pPlayer) && (g_iVotedBits & PlayerBit(i)) == 0)
			iCount++;
	}
	return iCount;
}

// VoteBegin to every player: whether they may vote, and what about
static void SendVoteBegin(const char* pszText)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer || !IsVoter(pPlayer))
			continue;

		const bool fCanVote = (g_iVotedBits & PlayerBit(i)) == 0;
		MESSAGE_BEGIN(MSG_ONE, gmsgVoteBegin, NULL, pPlayer->edict());
		WRITE_BYTE(fCanVote ? 1 : 0);
		WRITE_STRING(pszText);
		MESSAGE_END();
	}
}

// "votemap <map>"
void ClientVoteMap(edict_t* pEdict) // orig 0xf8620
{
	const char* pszMap = CMD_ARGV(1);
	char szName[256];
	char szText[512];

	if (!g_pGameRules->IsMultiplayer() || !IsVoteClient(pEdict))
		return;
	CBasePlayer* pCaller = (CBasePlayer*)CBaseEntity::Instance(pEdict);
	if (!pCaller)
		return;

	if (g_iVoteType != VOTE_NONE)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Cannot call vote while another is in progress\n");
		return;
	}

	if (pCaller->m_flNextVoteTime > gpGlobals->time)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, UTIL_VarArgs("Cannot call vote for %i seconds\n", (int)(pCaller->m_flNextVoteTime - gpGlobals->time)));
		return;
	}

	g_iVotedBits = PlayerBit(ENTINDEX(pEdict));
	const int iCount = CountOtherVoters();
	if (iCount < 1)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Cannot call vote when alone\n");
		return;
	}

	strncpy(g_szVoteMap, pszMap, 255);
	g_szVoteMap[255] = '\0';
	if (!IS_MAP_VALID(g_szVoteMap))
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, UTIL_VarArgs("Map %s not found on server\n", g_szVoteMap));
		return;
	}

	g_iVoteType = VOTE_CHANGELEVEL;
	g_iVoteNoBits = 0;
	g_iVoteYesBits = 0;
	g_flVoteEnd = gpGlobals->time + 60.0;
	g_flNextVoteUpdate = gpGlobals->time;
	g_hVoteCaller = pCaller;
	g_flVoteFraction = 0.51f;
	g_iVotesNeeded = (int)ceil(iCount * 0.51f);

	sprintf(szText, "changelevel to %s", g_szVoteMap);
	SendVoteBegin(szText);

	strncpy(szName, STRING(g_hVoteCaller->pev->netname), 255);
	szName[255] = '\0';
	sprintf(szText, "* Vote to changelevel to %s initiated by %s, need %i of %i votes to succeed\n", g_szVoteMap, szName, g_iVotesNeeded, iCount);
	UTIL_ClientPrintAll(HUD_PRINTTALK, szText);

	pCaller->m_flNextVoteTime = gpGlobals->time + 120.0;
}

// "votekick <player # | player name>"
void ClientVoteKick(edict_t* pEdict) // orig 0xf89d0
{
	char szTarget[256];
	char szName[256];
	char szText[512];

	if (!g_pGameRules->IsMultiplayer() || !IsVoteClient(pEdict))
		return;

	if (CMD_ARGC() < 2)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Ussage: votekick <player # | player name>\n");
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Valid player numbers:\n");
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (!pPlayer)
				continue;
			strncpy(szName, STRING(pPlayer->pev->netname), 255);
			szName[255] = '\0';
			ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, UTIL_VarArgs("  %i - %s\n", i, szName));
		}
		return;
	}

	strncpy(szTarget, CMD_ARGV(1), 255);
	szTarget[255] = '\0';
	strncpy(szName, STRING(VARS(pEdict)->netname), 255);
	szName[255] = '\0';

	CBasePlayer* pCaller = (CBasePlayer*)CBaseEntity::Instance(pEdict);
	CBaseEntity* pTarget = Admin_FindPlayer(szTarget);
	if (!pCaller)
		return;

	if (g_iVoteType != VOTE_NONE)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Cannot call vote while another is in progress\n");
		return;
	}

	if (pCaller->m_flNextVoteTime > gpGlobals->time)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, UTIL_VarArgs("Cannot call vote for %i seconds\n", (int)(pCaller->m_flNextVoteTime - gpGlobals->time)));
		return;
	}

	if (!pTarget)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, UTIL_VarArgs("Cannot find player %s\n", szTarget));
		return;
	}

	// Original bug kept: the second bit set is the caller's again (the player to be kicked was
	// likely meant), so that player may vote. VERIFY: the first ENTINDEX's argument
	g_iVotedBits = PlayerBit(ENTINDEX(pEdict));
	g_iVotedBits |= PlayerBit(ENTINDEX(pCaller->edict()));
	const int iCount = CountOtherVoters();
	if (iCount < 1)
	{
		ClientPrint(VARS(pEdict), HUD_PRINTCONSOLE, "Cannot call vote when alone\n");
		return;
	}

	strncpy(szTarget, STRING(pTarget->pev->netname), 255);
	szTarget[255] = '\0';

	g_iVoteType = VOTE_KICK;
	g_iVoteNoBits = 0;
	g_iVoteYesBits = 0;
	g_flVoteEnd = gpGlobals->time + 60.0;
	g_flNextVoteUpdate = gpGlobals->time;
	g_hVoteCaller = pCaller;
	g_hVoteTarget = pTarget;
	g_flVoteFraction = 0.75f;
	g_iVotesNeeded = (int)ceil(iCount * 0.75f);

	sprintf(szText, "kick %s", szTarget);
	SendVoteBegin(szText);

	sprintf(szText, "* Vote to kick %s initiated by %s, need %i of %i votes to succeed\n", szTarget, szName, g_iVotesNeeded, iCount);
	UTIL_ClientPrintAll(HUD_PRINTTALK, szText);

	pCaller->m_flNextVoteTime = gpGlobals->time + 120.0;
}
