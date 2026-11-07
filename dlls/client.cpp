/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include <algorithm>
#include <string>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "filesystem_utils.h"
#include "cbase.h"
#include "com_model.h"
#include "saverestore.h"
#include "player.h"
#include "rc2/virtualhull.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "UserMessages.h"
#include "rc2prefs.h"
#include "rc2/maintainent.h"
#include "rc2/vote.h"
#include "rc2/transform.h"
#include <cmath>

// Rocket Crowbar 2 (RC 1.9's name): set while a client command creates entities, so
// CBaseEntity::Create refuses classnames nothing has precached. orig 0x18f3f0
bool g_bUnsafeCreate = false;

DLL_GLOBAL unsigned int g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);

void LinkUserMessages();

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

	//	pev->frame		= $deatha11;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = DEAD_DEAD;
	pev->nextthink = -1;
}


// SDK 2.3's g_flWeaponCheat, which halflife-updated replaced with g_psv_cheats: RC2 reads
// sv_cheats into it every frame (StartFrame) and its give, givefor and fov test it. orig 0x1afd34
extern float g_flWeaponCheat; // world.cpp (SDK 2.3's place)

/*
===========
ClientConnect

called when a player connects to a server
============
*/
// orig 0x1a8f0. RC2: clears what the client's entity drew and marks the client connected.
qboolean ClientConnect(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	UTIL_KillEntityEffects(pEntity);
	g_iRC2ClientBits |= 1u << ((ENTINDEX(pEntity) - 1) & 31);
	return static_cast<qboolean>(g_pGameRules->ClientConnected(pEntity, pszName, pszAddress, szRejectReason));

	// a client connecting during an intermission can cause problems
	//	if (intermission_running)
	//		ExitIntermission ();
}


/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
// orig 0x1a940 (unlisted: no function at it in the decompile). RC2: marks the client gone (even
// at game over), kills what it owned, lets go of riders, and removes its roach charges.
// halflife-updated's tank release isn't in RC2 (SDK 2.3 had none either).
void ClientDisconnect(edict_t* pEntity)
{
	g_iRC2ClientBits &= ~(1u << ((ENTINDEX(pEntity) - 1) & 31));

	if (g_fGameOver)
		return;

	UTIL_KillEntityEffects(pEntity);

	char text[256];
	sprintf(text, "- %s has left the game\n", STRING(pEntity->v.netname));
	MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	CSound* pSound;
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(pEntity));
	{
		// since this client isn't around to think anymore, reset their sound.
		if (pSound)
		{
			pSound->Reset();
		}
	}

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(pEntity));
	if (pPlayer)
	{
		for (int i = 0; i < 16; i++)
		{
			CBaseEntity* pOwned = pPlayer->m_rgpVirtualHulls[i];
			if (pOwned)
			{
				pOwned->pev->solid = SOLID_NOT;
				UTIL_SetSize(pOwned->pev, g_vecZero, g_vecZero);
				pOwned->pev->flags |= FL_KILLME;
				pPlayer->m_rgpVirtualHulls[i] = NULL;
			}
		}

		if (pPlayer->m_iShoulderRider[0])
		{
			CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(pPlayer->m_iShoulderRider[0]));
			if (pRider)
				pRider->StopRiding(false);
		}
		if (pPlayer->m_iShoulderRider[1])
		{
			CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(pPlayer->m_iShoulderRider[1]));
			if (pRider)
				pRider->StopRiding(false);
		}
		if (pPlayer->m_iRidingIndex)
			pPlayer->StopRiding(true);

		if (pPlayer->m_pCam)
		{
			UTIL_Remove(pPlayer->m_pCam);
			pPlayer->m_pCam = NULL;
		}
		if (pPlayer->m_pCam2)
		{
			UTIL_Remove(pPlayer->m_pCam2);
			pPlayer->m_pCam2 = NULL;
		}
	}

	// the roach charges it threw and hasn't set off
	for (CBaseEntity* pCharge = UTIL_FindEntityByClassname(NULL, "roach_charge"); pCharge; pCharge = UTIL_FindEntityByClassname(pCharge, "roach_charge"))
	{
		if (pCharge->pev->impulse == 0 && pCharge->pev->dmg_inflictor == pEntity)
			pCharge->SUB_Remove();
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.deadflag = DEAD_DEAD;
	pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
	pEntity->v.solid = SOLID_NOT;	   // nonsolid
	UTIL_SetOrigin(&pEntity->v, pEntity->v.origin);

	g_pGameRules->ClientDisconnected(pEntity);
}


// called by ClientKill and DeadThink
// orig 0x1ab80 (stock)
void respawn(entvars_t* pev, bool fCopyCorpse)
{
	if (0 != gpGlobals->coop || 0 != gpGlobals->deathmatch)
	{
		if (fCopyCorpse)
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}

		// respawn player
		GetClassPtr((CBasePlayer*)pev)->Spawn();
	}
	else
	{ // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
// orig 0x1ad90 (unlisted). RC2: not while dead, suicide-banned or in the race forklift, and 5
// seconds apart (SDK 2.3: 1).
void ClientKill(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;

	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance(pev->pContainingEntity);

	if (pl->m_fNextSuicideTime > gpGlobals->time)
		return; // prevent suiciding too ofter

	if (pl->pev->deadflag != DEAD_NO || pl->m_fSuicideBan || pl->m_iTransformType == 6)
		return;

	pl->m_fNextSuicideTime = gpGlobals->time + 5;

	// have the player kill themself
	pev->health = 0;
	pl->Killed(pev, GIB_NEVER);

	//	pev->modelindex = g_ulModelIndexPlayer;
	//	pev->frags -= 2;		// extra penalty
	//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
// orig 0x1ae20 (unlisted; stock). halflife-updated's spectator reset kept (RC2 has none).
void ClientPutInServer(edict_t* pEntity)
{
	CBasePlayer* pPlayer;

	entvars_t* pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer*)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn();

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;

	pPlayer->pev->iuser1 = 0; // disable any spec modes
	pPlayer->pev->iuser2 = 0;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;



#if defined(_MSC_VER) || defined(WIN32)
typedef wchar_t uchar16;
typedef unsigned int uchar32;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

//-----------------------------------------------------------------------------
// Purpose: determine if a uchar32 represents a valid Unicode code point
//-----------------------------------------------------------------------------
bool Q_IsValidUChar32(uchar32 uVal)
{
	// Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
	// values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
	return (uVal < 0x110000u) && ((uVal - 0x00D800u) > 0x7FFu) && ((uVal & 0xFFFFu) < 0xFFFEu) && ((uVal - 0x00FDD0u) > 0x1Fu);
}


// Decode one character from a UTF-8 encoded string. Treats 6-byte CESU-8 sequences
// as a single character, as if they were a correctly-encoded 4-byte UTF-8 sequence.
int Q_UTF8ToUChar32(const char* pUTF8_, uchar32& uValueOut, bool& bErrorOut)
{
	const uint8* pUTF8 = (const uint8*)pUTF8_;

	int nBytes = 1;
	uint32 uValue = pUTF8[0];
	uint32 uMinValue = 0;

	// 0....... single byte
	if (uValue < 0x80)
		goto decodeFinishedNoCheck;

	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ((uValue - 0xC0u) > 0x37u || (pUTF8[1] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0xC0 << 6) + pUTF8[1] - 0x80;
	nBytes = 2;
	uMinValue = 0x80;

	// 110..... two-byte lead byte
	if ((uValue & (0x20 << 6)) == 0)
		goto decodeFinished;

	// Expecting at least a three-byte sequence
	if ((pUTF8[2] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0x20 << 12) + pUTF8[2] - 0x80;
	nBytes = 3;
	uMinValue = 0x800;

	// 1110.... three-byte lead byte
	if ((uValue & (0x10 << 12)) == 0)
		goto decodeFinishedMaybeCESU8;

	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ((pUTF8[3] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0x10 << 18) + pUTF8[3] - 0x80;
	nBytes = 4;
	uMinValue = 0x10000;

	// 11110... four-byte lead byte. fall through to finished.

decodeFinished:
	if (uValue >= uMinValue && Q_IsValidUChar32(uValue))
	{
	decodeFinishedNoCheck:
		uValueOut = uValue;
		bErrorOut = false;
		return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;

decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ((uValue - 0xD800u) < 0x400u && pUTF8[3] == 0xED && (uint8)(pUTF8[4] - 0xB0) < 0x10 && (pUTF8[5] & 0xC0) == 0x80)
	{
		uValue = 0x10000 + ((uValue - 0xD800u) << 10) + ((uint8)(pUTF8[4] - 0xB0) << 6) + pUTF8[5] - 0x80;
		nBytes = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}



//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-8 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate(const char* pUTF8)
{
	bool bError = false;
	while ('\0' != *pUTF8)
	{
		uchar32 uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32(pUTF8, uVal, bError);
		if (bError || nCharSize == 6)
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
// orig 0x1aec0. RC2's is SDK 2.0's: no chat flood limit, no voice muting, no spectator chat;
// every client by index, and the log line (before the message is built) carries the WON id.
// halflife-updated's UTF-8 check is kept.
void Host_Say(edict_t* pEntity, bool teamonly)
{
	CBasePlayer* client;
	int j;
	char* p;
	char text[128];
	char szTemp[256];
	const char* cpSay = "say";
	const char* cpSayTeam = "say_team";
	const char* pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if (CMD_ARGC() == 0)
		return;

	if (!stricmp(pcmd, cpSay) || !stricmp(pcmd, cpSayTeam))
	{
		if (CMD_ARGC() >= 2)
		{
			p = (char*)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else // Raw text, need to prepend argv[0]
	{
		if (CMD_ARGC() >= 2)
		{
			snprintf(szTemp, sizeof(szTemp), "%s %s", (char*)pcmd, (char*)CMD_ARGS());
		}
		else
		{
			// Just a one word command, use the first word...sigh
			snprintf(szTemp, sizeof(szTemp), "%s", (char*)pcmd);
		}
		p = szTemp;
	}

	// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	// make sure the text has content
	char* pc;
	for (pc = p; pc != NULL && *pc != 0; pc++)
	{
		if (0 != isprint((unsigned char)*pc) && 0 == isspace((unsigned char)*pc))
		{
			pc = NULL; // we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if (pc != NULL)
		return; // no character found, so say nothing

	// halflife-updated: drop text the client can't decode
	if (!Q_UnicodeValidate(p))
		return;

	UTIL_LogPrintf("\"%s<%i><%u>\" said: \"%s\"%s\n",
		STRING(pEntity->v.netname),
		GETPLAYERUSERID(pEntity),
		g_engfuncs.pfnGetPlayerWONId(pEntity),
		p,
		teamonly ? " TEAM" : "");

	// turn on color set 2  (color on,  no sound)
	if (teamonly)
		sprintf(text, "%c(TEAM) %s: ", 2, STRING(pEntity->v.netname));
	else
		sprintf(text, "%c%s: ", 2, STRING(pEntity->v.netname));

	j = sizeof(text) - 2 - strlen(text); // -2 for /n and null terminator
	if ((int)strlen(p) > j)
		p[j] = 0;

	strcat(text, p);
	strcat(text, "\n");

	// loop through all players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		client = (CBasePlayer*)UTIL_PlayerByIndex(i);
		if (!client || !client->pev)
			continue;

		if (client->edict() == pEntity)
			continue;

		if (!(client->IsNetClient())) // Not a client ? (should never be true)
			continue;

		if (teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE)
			continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, client->edict());
		WRITE_BYTE(ENTINDEX(pEntity));
		WRITE_STRING(text);
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pEntity->v.pContainingEntity);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint(text);
}

// Rocket Crowbar 1.9's (there orig 0xfdbf0), RC2's at orig 0x1b200: "say_private <name or index>
// <text>", a message to one player (and an echo to the sender). RC2 also logs it.
void SayPrivate(edict_t* pEntity)
{
	if (CMD_ARGC() < 3)
		return;

	CBaseEntity* pTarget = NULL;

	// an all-digits first argument is a client index
	const char* pc;
	for (pc = CMD_ARGV(1); *pc != 0; pc++)
	{
		if (0 == isdigit((unsigned char)*pc))
			break;
	}

	if (*pc == 0 && CMD_ARGV(1)[0] != 0)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(atoi(CMD_ARGV(1)));
		if (pPlayer && pPlayer->IsNetClient() && pPlayer->edict() != pEntity)
			pTarget = pPlayer;
	}

	if (!pTarget)
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (!pPlayer || !pPlayer->pev)
				continue;
			edict_t* pEdict = pPlayer->edict();
			if (!pEdict || !pEdict->pvPrivateData || 0 != pEdict->free || pEdict == pEntity)
				continue;
			if (!pPlayer->IsNetClient())
				continue;
			if (!stricmp(STRING(pPlayer->pev->netname), CMD_ARGV(1)))
			{
				pTarget = pPlayer;
				break;
			}
		}
	}

	if (!pTarget || !pTarget->IsNetClient())
		return;

	// Original bug (crash), guarded: the original can overrun its stack here: it strncat's each word (up to 192
	// chars at a time) into its 256-byte buffer, then cuts it at 191. Ours builds it in a
	// std::string and cuts it the same way. The plain text for the log is cut at 255.
	char prefix[256];
	snprintf(prefix, sizeof(prefix), "%c(PRIVATE <%s>) %s: ", 2, STRING(pTarget->pev->netname), STRING(pEntity->v.netname));
	std::string strText = prefix;
	std::string strPlain;
	for (int i = 2; i < CMD_ARGC(); i++)
	{
		strText.append(CMD_ARGV(i), 0, 192);
		strPlain.append(CMD_ARGV(i), 0, 255);
		if (i + 1 < CMD_ARGC())
		{
			strText += " ";
			strPlain += " ";
		}
		else
			strText += "\n";
	}
	if (strText.size() > 191)
		strText.resize(191);
	if (strPlain.size() > 255)
		strPlain.resize(255);
	const char* text = strText.c_str();

	edict_t* pTargetEdict = pTarget->edict();
	UTIL_LogPrintf("\"%s<%i><%u>\" said: \"%s\" PRIVATE to \"%s<%i><%u>\"\n",
		STRING(pEntity->v.netname), GETPLAYERUSERID(pEntity), g_engfuncs.pfnGetPlayerWONId(pEntity),
		strPlain.c_str(),
		STRING(pTarget->pev->netname), GETPLAYERUSERID(pTargetEdict), g_engfuncs.pfnGetPlayerWONId(pTargetEdict));

	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pTargetEdict);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pEntity->v.pContainingEntity);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	g_engfuncs.pfnServerPrint(text);
}

// --- RC2's player attributes (setattr) ---
// Ten values bought with points (jump, speed, attack, defend, health, extra health, armor,
// extra armor, energy regeneration, damage reflection), ability bits, weapon bits and a
// transform; rc2.ini's [Attributes] prices them. All names ours.

// orig 0x1d070. Points for attribute i, held to [0, max] (jump .. reflection) or [1, max] (speed,
// energy regeneration).
int Attr_ClampPoints(int i, int iPoints)
{
	int iMax;
	switch (i)
	{
	case 0: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxJumpPoints).c; break;
	case 1:
		iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxSpeedPoints).c;
		if (iPoints > iMax)
			return iMax;
		return iPoints < 1 ? 1 : iPoints;
	case 2: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxAttackPoints).c; break;
	case 3: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxDefendPoints).c; break;
	case 4: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxHealthPoints).c; break;
	case 5: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxExtraHealthPoints).c; break;
	case 6: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxArmorPoints).c; break;
	case 7: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxExtraArmorPoints).c; break;
	case 8:
		iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxEnergyRegenPoints).c;
		if (iPoints > iMax)
			return iMax;
		return iPoints < 1 ? 1 : iPoints;
	case 9: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxDamageReflectionPoints).c; break;
	default: return iPoints;
	}
	if (iPoints > iMax)
		return iMax;
	return iPoints < 0 ? 0 : iPoints;
}

// orig 0x1cf60. The value iPoints buy of attribute i.
float Attr_PointsToValue(int i, int iPoints)
{
	switch (i)
	{
	case 0: return iPoints / RC2Pref(RC2P_Attributes_JumpCost).f;
	case 1: return iPoints / RC2Pref(RC2P_Attributes_SpeedCost).f;
	case 2: return exp(iPoints * RC2Pref(RC2P_Attributes_AttackConstant).f) / RC2Pref(RC2P_Attributes_AttackScaler).f;
	case 3: return exp(-(iPoints * RC2Pref(RC2P_Attributes_DefendConstant).f)) * RC2Pref(RC2P_Attributes_DefendScaler).f;
	case 4: return iPoints / RC2Pref(RC2P_Attributes_HealthCost).f + RC2Pref(RC2P_Attributes_MinHealth).f;
	case 5: return iPoints / RC2Pref(RC2P_Attributes_ExtraHealthCost).f;
	case 6: return iPoints / RC2Pref(RC2P_Attributes_ArmorCost).f;
	case 7: return iPoints / RC2Pref(RC2P_Attributes_ExtraArmorCost).f;
	case 8: return iPoints / RC2Pref(RC2P_Attributes_EnergyRegenCost).f;
	case 9: return iPoints / RC2Pref(RC2P_Attributes_DamageReflectionCost).f;
	}
	return 0;
}

// orig 0x1cd50. The points a value of attribute i costs (the inverse, rounded, never below 0).
int Attr_ValueToPoints(int i, float flValue)
{
	double flPoints;
	switch (i)
	{
	case 0: flPoints = RC2Pref(RC2P_Attributes_JumpCost).f * flValue; break;
	case 1: flPoints = RC2Pref(RC2P_Attributes_SpeedCost).f * flValue; break;
	case 2: flPoints = log(RC2Pref(RC2P_Attributes_AttackScaler).f * flValue) / RC2Pref(RC2P_Attributes_AttackConstant).f; break;
	case 3: flPoints = log(flValue / RC2Pref(RC2P_Attributes_DefendScaler).f) / -RC2Pref(RC2P_Attributes_DefendConstant).f; break;
	case 4: flPoints = (flValue - RC2Pref(RC2P_Attributes_MinHealth).f) * RC2Pref(RC2P_Attributes_HealthCost).f; break;
	case 5: flPoints = RC2Pref(RC2P_Attributes_ExtraHealthCost).f * flValue; break;
	case 6: flPoints = RC2Pref(RC2P_Attributes_ArmorCost).f * flValue; break;
	case 7: flPoints = RC2Pref(RC2P_Attributes_ExtraArmorCost).f * flValue; break;
	case 8: flPoints = RC2Pref(RC2P_Attributes_EnergyRegenCost).f * flValue; break;
	case 9: flPoints = RC2Pref(RC2P_Attributes_DamageReflectionCost).f * flValue; break;
	default: return 0;
	}
	if (!(flPoints > 0.0))
		flPoints = 0.0;
	return static_cast<int>(flPoints + 0.5);
}

// orig 0x1d170. The price of one ability bit.
int Attr_AbilityCost(unsigned int iBit)
{
	switch (iBit)
	{
	case 0x1: return (unsigned short)RC2Pref(RC2P_Attributes_WallhopPointCost).s;
	case 0x2: return (unsigned short)RC2Pref(RC2P_Attributes_AqualungPointCost).s;
	case 0x4: return (unsigned short)RC2Pref(RC2P_Attributes_JetboostPointCost).s;
	case 0x8: return (unsigned short)RC2Pref(RC2P_Attributes_JetpackPointCost).s;
	case 0x10: return (unsigned short)RC2Pref(RC2P_Attributes_EnergyDrawPointCost).s;
	case 0x20: return (unsigned short)RC2Pref(RC2P_Attributes_CreateHealthPointCost).s;
	case 0x40: return (unsigned short)RC2Pref(RC2P_Attributes_CrowbarControlPointCost).s;
	case 0x80: return (unsigned short)RC2Pref(RC2P_Attributes_ShieldPointCost).s;
	case 0x100: return (unsigned short)RC2Pref(RC2P_Attributes_LongJumpPointCost).s;
	case 0x200: return (unsigned short)RC2Pref(RC2P_Attributes_SubmersibleWeaponsPointCost).s;
	}
	return 0;
}

// orig 0x1d140: the ten ability bits' prices
int Attr_AbilitiesCost(unsigned int iBits)
{
	int iCost = 0;
	for (int i = 0; i < 10; i++)
	{
		if ((iBits & (1u << i)) != 0)
			iCost += Attr_AbilityCost(1u << i);
	}
	return iCost;
}

// orig 0x1d250. The price of one weapon bit.
int Attr_WeaponCost(unsigned int iBit)
{
	switch (iBit)
	{
	case 0x1: return (unsigned short)RC2Pref(RC2P_Attributes_GlockPointCost).s;
	case 0x2: return (unsigned short)RC2Pref(RC2P_Attributes_PythonPointCost).s;
	case 0x4: return (unsigned short)RC2Pref(RC2P_Attributes_MP5PointCost).s;
	case 0x8: return (unsigned short)RC2Pref(RC2P_Attributes_ShotgunPointCost).s;
	case 0x10: return (unsigned short)RC2Pref(RC2P_Attributes_CrossbowPointCost).s;
	case 0x20: return (unsigned short)RC2Pref(RC2P_Attributes_RPGPointCost).s;
	case 0x40: return (unsigned short)RC2Pref(RC2P_Attributes_EgonPointCost).s;
	case 0x80: return (unsigned short)RC2Pref(RC2P_Attributes_GaussPointCost).s;
	case 0x100: return (unsigned short)RC2Pref(RC2P_Attributes_HornetgunPointCost).s;
	}
	return 0;
}

// orig 0x1d220: the nine weapon bits' prices
int Attr_WeaponsCost(unsigned int iBits)
{
	int iCost = 0;
	for (int i = 0; i < 9; i++)
	{
		if ((iBits & (1u << i)) != 0)
			iCost += Attr_WeaponCost(1u << i);
	}
	return iCost;
}

// orig 0x1d2f0. The price of a transform (0-4; anything else is free).
int Attr_TransformCost(int iTransform)
{
	switch (iTransform)
	{
	case 0: return (unsigned short)RC2Pref(RC2P_Attributes_TransformAssassinPointCost).s;
	case 1: return (unsigned short)RC2Pref(RC2P_Attributes_TransformHoundeyePointCost).s;
	case 2: return (unsigned short)RC2Pref(RC2P_Attributes_TransformControllerPointCost).s;
	case 3: return (unsigned short)RC2Pref(RC2P_Attributes_TransformBatPointCost).s;
	case 4: return (unsigned short)RC2Pref(RC2P_Attributes_TransformHeadcrabPointCost).s;
	}
	return 0;
}

// orig 0x1d360. Attribute i's value when the points run out: 1 for jump, speed, attack, defend
// and energy regeneration, 100 health, nothing extra.
float Attr_DefaultValue(int i)
{
	switch (i)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 8:
		return 1.0;
	case 4:
		return 100.0;
	}
	return 0.0;
}

// "give" and "givefor": the same gates (orig 0x1b550's)
static bool CanGive(CBasePlayer* pPlayer, bool fActive)
{
	if (g_flWeaponCheat == 0.0)
		return false;
	if (!fActive && pPlayer->m_iAdminLevel < 2)
		return false;
	if (sv_allowgive.value == 0.0)
		return false;
	if (pPlayer->m_fGiveBan || pPlayer->m_bFrozen)
		return false;
	return pPlayer->m_flNextImpulse <= gpGlobals->time;
}

// a CLTE message about the player: type, entity index
static void SendPlayerCLTE(edict_t* pEntity, int iType)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(iType);
	WRITE_BYTE(ENTINDEX(pEntity));
	MESSAGE_END();
}

/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern float g_flWeaponCheat;

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
// orig 0x1b550 (unlisted: no function at it in the decompile, read from the disassembly). RC2's
// set; halflife-updated's fullupdate, specmode, closemenus and follownext aren't in it.
void ClientCommand(edict_t* pEntity)
{
	const char* pcmd = CMD_ARGV(0);

	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;

	entvars_t* pev = &pEntity->v;

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(pEntity));
	const bool fActive = pPlayer->IsActivePlayer();

	if (!strcmp(pcmd, "say"))
	{
		Host_Say(pEntity, false);
	}
	else if (!strcmp(pcmd, "say_team"))
	{
		Host_Say(pEntity, true);
	}
	else if (!strcmp(pcmd, "sayprivate") || !strcmp(pcmd, "say_private"))
	{
		SayPrivate(pEntity);
	}
	else if (!strcmp(pcmd, "give"))
	{
		// give <classname>: weapons, ammo and items go to the player, anything else stays where
		// it spawned (at the player's feet)
		if (!CanGive(pPlayer, fActive))
			return;
		if (CMD_ARGC() < 2 || !IsClassnamePrecachedNoInvalid(CMD_ARGV(1)))
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Entity not precached! (create aborted)\n");
			return;
		}

		pPlayer->m_flNextImpulse = gpGlobals->time + 0.1;
		g_bUnsafeCreate = true;
		g_fRC2Randomize = false;
		CBaseEntity* pItem = CBaseEntity::Create(GetPrecachedName(CMD_ARGV(1)), pev->origin, g_vecZero, NULL);
		g_fRC2Randomize = true;
		g_bUnsafeCreate = false;
		if (!pItem)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Cannot create entity\n");
			return;
		}

		pItem->pev->spawnflags |= SF_NORESPAWN | (1 << 29); // VERIFY: what RC2's bit 29 does
		edict_t* pentItem = pItem->pev->pContainingEntity;

		if (strstr(CMD_ARGV(1), "weapon_") || strstr(CMD_ARGV(1), "ammo_"))
		{
			// VERIFY: game rules slot 30, CanHavePlayerItem if RC2 inserted one virtual before it
			// (its PlayerRelationship is slot 51, SDK 2.3's 50)
			if (g_pGameRules->CanHavePlayerItem(static_cast<CBasePlayer*>(CBaseEntity::Instance(pEntity)), static_cast<CBasePlayerItem*>(pItem)) && pPlayer->m_iTransformType == 0 && !pPlayer->m_bTransforming)
			{
				pItem->Touch(CBaseEntity::Instance(pEntity));
				return;
			}
			pItem->pev->flags |= FL_KILLME;
			return;
		}
		if (!strstr(CMD_ARGV(1), "item"))
			return;

		pItem->Touch(CBaseEntity::Instance(pEntity));
		if (UTIL_IsValidEntity(pentItem))
			pItem->pev->flags |= FL_KILLME;
	}
	else if (!strcmp(pcmd, "givefor"))
	{
		// givefor <classname> [distance]: in front of the player (default 128 units), facing its
		// way
		if (!CanGive(pPlayer, fActive))
			return;
		if (CMD_ARGC() < 2 || !IsClassnamePrecachedNoInvalid(CMD_ARGV(1)))
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Entity not precached! (create aborted)\n");
			return;
		}

		pPlayer->m_flNextImpulse = gpGlobals->time + 0.1;
		UTIL_MakeVectors(pev->v_angle);
		g_bUnsafeCreate = true;
		g_fRC2Randomize = false;
		const float flDist = CMD_ARGC() >= 3 ? atof(CMD_ARGV(2)) : 128.0;
		const Vector vecAngles(0, pev->v_angle.y, 0);
		CBaseEntity* pItem = CBaseEntity::Create(GetPrecachedName(CMD_ARGV(1)), pev->origin + gpGlobals->v_forward * flDist, vecAngles, NULL);
		g_fRC2Randomize = true;
		g_bUnsafeCreate = false;
		if (!pItem)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Cannot create entity\n");
			return;
		}
		pItem->pev->spawnflags |= SF_NORESPAWN;
	}
	else if (!strcmp(pcmd, "drop"))
	{
		// player is dropping an item.
		if (pPlayer->m_flNextImpulse > gpGlobals->time)
			return;
		pPlayer->DropPlayerItem((char*)CMD_ARGV(1));
		pPlayer->m_flNextImpulse = gpGlobals->time + 0.1;
	}
	else if (!strcmp(pcmd, "fov"))
	{
		if (0 != g_flWeaponCheat && CMD_ARGC() > 1)
		{
			pPlayer->m_iFOV = atoi(CMD_ARGV(1));
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("\"fov\" is \"%d\"\n", (int)GetClassPtr((CBasePlayer*)pev)->m_iFOV));
		}
	}
	else if (!strcmp(pcmd, "use"))
	{
		pPlayer->SelectItem((char*)CMD_ARGV(1));
	}
	else if (!strcmp(pcmd, "jetpack"))
	{
		// toggles the jetpack (ability bit 8)
		if ((PM_GetAbilityFlags(ENTINDEX(pPlayer->edict())) & 0x8) == 0 || pPlayer->pev->deadflag != DEAD_NO || pPlayer->m_iTransformType != 0)
		{
			CLIENT_PRINTF(pEntity, print_console, "You don't have a jetpack\n");
			return;
		}
		if (pPlayer->m_fCreatingHealth || pPlayer->m_fDrawingEnergy || pPlayer->m_bTransforming || pPlayer->m_iRidingIndex || pPlayer->m_bFrozen)
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't activate your jetpack right now\n");
			return;
		}
		if (pPlayer->m_fJetpackOn)
		{
			pPlayer->m_fJetpackOn = false;
			pPlayer->ClearPhysFlags(1);
			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(4);
			WRITE_BYTE(ENTINDEX(pEntity));
			WRITE_BYTE(0);
			MESSAGE_END();
			return;
		}
		if (pPlayer->m_flEnergy < RC2Pref(RC2P_Attributes_JetpackTurnOnCost).f)
		{
			CLIENT_PRINTF(pEntity, print_console, "Not enough energy to start jetpack\n");
			return;
		}
		pPlayer->m_flEnergy -= RC2Pref(RC2P_Attributes_JetpackTurnOnCost).f;
		UTIL_ExtendTime(&pPlayer->m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_Attributes_JetpackTurnOnRechargeDelay).f);
		pPlayer->m_fJetpackOn = true;
		pPlayer->SetPhysFlags(1);
		pev->velocity.z += RC2Pref(RC2P_Attributes_JetpackTurnZVelBoost).f;
		SendPlayerCLTE(pEntity, 3);
	}
	else if (!strcmp(pcmd, "energydraw") || !strcmp(pcmd, "drawenergy"))
	{
		// standing still to draw energy (ability bit 0x10)
		if ((PM_GetAbilityFlags(ENTINDEX(pPlayer->edict())) & 0x10) == 0 || pPlayer->pev->deadflag != DEAD_NO)
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't draw energy\n");
			return;
		}
		if (pPlayer->m_flEnergyDrawDone < gpGlobals->time && !pPlayer->m_fDrawingEnergy && (pev->flags & FL_ONGROUND) != 0 && 0 == pPlayer->m_iShrunk)
		{
			pPlayer->AddPhysFlag2Source(1);
			pPlayer->m_fDrawingEnergy = true;
			pPlayer->m_flEnergyDrawDone = RC2Pref(RC2P_Attributes_EnergyDrawTimeTaken).f + gpGlobals->time;
			SendPlayerCLTE(pEntity, 5);
			return;
		}
		CLIENT_PRINTF(pEntity, print_console, "You can't draw energy right now\n");
	}
	else if (!strcmp(pcmd, "createhealth") || !strcmp(pcmd, "makehealth") || !strcmp(pcmd, "energy2health"))
	{
		// energy into health (ability bit 0x20)
		if ((PM_GetAbilityFlags(ENTINDEX(pPlayer->edict())) & 0x20) == 0 || pPlayer->pev->deadflag != DEAD_NO)
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't create health\n");
			return;
		}
		if (pPlayer->m_flCreateHealthDone < gpGlobals->time && !pPlayer->m_fCreatingHealth && 0 == pPlayer->m_iShrunk)
		{
			pPlayer->m_fCreatingHealth = true;
			pPlayer->m_flCreateHealthDone = RC2Pref(RC2P_Attributes_CreateHealthTimeTaken).f + gpGlobals->time;
			pPlayer->m_flHealthCreated = 0;
			SendPlayerCLTE(pEntity, 9);
			return;
		}
		CLIENT_PRINTF(pEntity, print_console, "You can't create health right now\n");
	}
	else if (!strcmp(pcmd, "shield"))
	{
		// toggles the shield (ability bit 0x80)
		if ((PM_GetAbilityFlags(ENTINDEX(pPlayer->edict())) & 0x80) == 0 || pPlayer->pev->deadflag != DEAD_NO || 0 != pPlayer->m_iShrunk || pPlayer->m_iTransformType != 0 || pPlayer->m_bTransforming || pPlayer->m_iRidingIndex)
		{
			CLIENT_PRINTF(pEntity, print_console, "You don't have a shield\n");
			return;
		}
		if (pPlayer->m_fShieldOn)
		{
			pPlayer->m_fShieldOn = false;
			return;
		}
		if (RC2Pref(RC2P_Shield_ActivationEnergy).f + RC2Pref(RC2P_Shield_MinRunEnergy).f <= pPlayer->m_flEnergy)
		{
			pPlayer->m_flEnergy -= RC2Pref(RC2P_Shield_ActivationEnergy).f;
			pPlayer->m_fShieldOn = true;
			return;
		}
		CLIENT_PRINTF(pEntity, print_console, "You don't have enough energy to activate your shield\n");
	}
	else if (!strcmp(pcmd, "transform"))
	{
		// into the transform the attributes chose, or back when already that
		if (0 == static_cast<int>(mp_allowtransform.value))
		{
			CLIENT_PRINTF(pEntity, print_console, "Server has disallowed transforming\n");
			return;
		}
		if (pPlayer->m_iChosenTransform < 0 || pPlayer->m_iChosenTransform >= 5)
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't transform\n");
			return;
		}
		if (pPlayer->pev->deadflag != DEAD_NO || 0 != pPlayer->m_iShrunk || pPlayer->m_bFrozen || pPlayer->m_flEnergy < RC2Pref(RC2P_Attributes_TransformEnergyCost).f || pPlayer->m_bTransforming || 0 != pPlayer->m_iMoveLock)
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't transform right now\n");
			return;
		}
		if (pPlayer->m_iRidingIndex)
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't transform while riding\n");
			return;
		}
		if (pPlayer->m_iShoulderRider[0] || pPlayer->m_iShoulderRider[1])
		{
			CLIENT_PRINTF(pEntity, print_console, "You can't transform while being ridden\n");
			return;
		}
		if (pPlayer->m_iTransformType == 6)
		{
			CLIENT_PRINTF(pEntity, print_console, "You don't want to transform right now...\n");
			return;
		}

		int iTransform;
		switch (pPlayer->m_iChosenTransform)
		{
		case 0: iTransform = 1; break;
		case 1: iTransform = 2; break;
		case 2: iTransform = 3; break;
		case 3: iTransform = 4; break;
		case 4: iTransform = 5; break;
		default:
			// unreachable after the range check (the original's line 727)
			ALERT(at_console, "BUGBUG: Unsupported transform/wierdness (%s:%i)\n", "C:\\SIERRA\\Half-Life\\RC2dev\\dlls\\client.cpp", 727);
			return;
		}

		if (pPlayer->m_iTransformType == iTransform)
			pPlayer->StartUntransform();
		else
			pPlayer->StartTransform();
	}
	else if (!strcmp(pcmd, "rideplayer"))
	{
		pPlayer->StartRiding();
	}
	else if (strstr(pcmd, "weapon_") == pcmd && (pPlayer->m_iHideHUD & 0x10) == 0)
	{
		// VERIFY: RC2 tests bit 0x10 of the player's +0x4ac, m_iHideHUD's offset in the
		// RC2-layout reference (p6 reads +0x4ac as RC2 state bits, 0x10 with shrinking). With it
		// set, a weapon_ command goes on down the list (to the game rules, else unknown).
		if (pPlayer->m_bFrozen)
			return;
		pPlayer->SelectItem(pcmd);
	}
	else if (!strcmp(pcmd, "lastinv"))
	{
		if (pPlayer->m_bFrozen)
			return;
		pPlayer->SelectLastItem();
	}
	else if (!strcmp(pcmd, "attrconfig"))
	{
		// opens the client's attribute menu
		MESSAGE_BEGIN(MSG_ONE, gmsgAttrConf, NULL, pev->pContainingEntity);
		MESSAGE_END();
	}
	else if (!strcmp(pcmd, "spectate") || !strcmp(pcmd, "observe"))
	{
		if (pPlayer->m_fSuicideBan)
			return;
		pPlayer->GoToSpectatorView();
	}
	else if (!strcmp(pcmd, "vote"))
	{
		if (CMD_ARGC() >= 2)
			ClientVote(pEntity);
		else
			CLIENT_PRINTF(pEntity, print_console, "Ussage: vote <yes or no>\n");
	}
	else if (!strcmp(pcmd, "votemap"))
	{
		if (g_fGameOver)
			return;
		if (CMD_ARGC() >= 2)
			ClientVoteMap(pEntity);
		else
			CLIENT_PRINTF(pEntity, print_console, "Ussage: votemap\n");
	}
	else if (!strcmp(pcmd, "votekick"))
	{
		if (g_fGameOver)
			return;
		ClientVoteKick(pEntity);
	}
	else if (!strcmp(pcmd, "setattr"))
	{
		// setattr <10 attribute points> <ability bits> <weapon bits> <transform>, as the client's
		// attribute menu sends it
		if (CMD_ARGC() != 14)
		{
			ClientPrint(pev, HUD_PRINTTALK, "Please use the \"attrconfig\" command to change attributes\n");
			return;
		}

		int iTotal = 0;
		int i;
		for (i = 0; i < 10; i++)
		{
			const int iPoints = Attr_ClampPoints(i, abs(atoi(CMD_ARGV(i + 1))));
			const float flValue = Attr_PointsToValue(i, iPoints);
			pPlayer->m_flAttrPending[i] = flValue;
			iTotal += Attr_ValueToPoints(i, flValue);
		}
		sscanf(CMD_ARGV(i + 1), "%u", &pPlayer->m_iAbilitiesPending);
		iTotal += Attr_AbilitiesCost(pPlayer->m_iAbilitiesPending);
		sscanf(CMD_ARGV(i + 2), "%u", &pPlayer->m_iWeaponsPending);
		iTotal += Attr_WeaponsCost(pPlayer->m_iWeaponsPending);
		sscanf(CMD_ARGV(i + 3), "%d", &pPlayer->m_iTransformPending);
		if (pPlayer->m_iTransformPending < 0 || pPlayer->m_iTransformPending >= 5)
			pPlayer->m_iTransformPending = -1;
		iTotal += Attr_TransformCost(pPlayer->m_iTransformPending);

		if (iTotal > (unsigned short)RC2Pref(RC2P_Attributes_TotalPoints).s)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "Too many attribute points used, using defaults\n");
			for (i = 0; i < 10; i++)
				pPlayer->m_flAttrPending[i] = Attr_DefaultValue(i);
			pPlayer->m_iAbilitiesPending = 0;
			pPlayer->m_iWeaponsPending = 1;
			pPlayer->m_iTransformPending = -1;
		}

		if (g_pGameRules->IsMultiplayer())
		{
			// the player's next spawn takes them
			if (pev->deadflag != DEAD_NO)
				return;
			ClientPrint(pev, HUD_PRINTTALK, "* Changes will take effect on next spawn\n");
			return;
		}

		// single player: now, once
		if (pPlayer->m_fAttributesSet)
		{
			ClientPrint(pev, HUD_PRINTTALK, "* Can only change your attributes once\n");
			return;
		}

		bool fChanged = false;
		for (i = 0; i < 10; i++)
		{
			if (pPlayer->m_flAttrPending[i] != pPlayer->m_flAttributes[i])
			{
				pPlayer->m_flAttributes[i] = pPlayer->m_flAttrPending[i];
				fChanged = true;
			}
		}
		if (pPlayer->m_iAbilitiesPending != pPlayer->m_iAbilities)
		{
			pPlayer->m_iAbilities = pPlayer->m_iAbilitiesPending;
			fChanged = true;
		}
		pPlayer->m_iAttrWeapons = pPlayer->m_iWeaponsPending;
		pPlayer->m_iChosenTransform = pPlayer->m_iTransformPending;
		if (!fChanged)
			return;

		MESSAGE_BEGIN(MSG_ALL, gmsgSetAttr);
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		for (i = 0; i < 10; i++)
			WRITE_BYTE(Attr_ValueToPoints(i, pPlayer->m_flAttributes[i]));
		WRITE_LONG(pPlayer->m_iAbilities);
		MESSAGE_END();
		pPlayer->m_fAttributesSet = true;
	}
	else if (!strcmp(pcmd, "rcadmin"))
	{
		Admin_Command(pEntity);
	}
	else if (!strcmp(pcmd, "rccfg"))
	{
		RC2Prefs_ClientCommand(pEntity);
	}
	else if (g_pGameRules->ClientCommand(GetClassPtr((CBasePlayer*)pev), pcmd))
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy(command, pcmd, 127);
		command[127] = '\0';
		// halflife-updated: First parse the name and remove any %'s (RC2 doesn't)
		for (char* pApersand = command; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// tell the user they entered an unknown command
		ClientPrint(&pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs("Unknown command: %s\n", command));
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
// orig 0x1d3b0 (unlisted). RC2: the rename goes out in single player too, and the log line
// carries the WON id instead of the auth id.
void ClientUserInfoChanged(edict_t* pEntity, char* infobuffer)
{
	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if (!FStringNull(pEntity->v.netname) && STRING(pEntity->v.netname)[0] != 0 && !FStrEq(STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name")))
	{
		char sName[256];
		char* pName = g_engfuncs.pfnInfoKeyValue(infobuffer, "name");
		strncpy(sName, pName, sizeof(sName) - 1);
		sName[sizeof(sName) - 1] = '\0';

		// First parse the name and remove any %'s
		for (char* pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pEntity), infobuffer, "name", sName);

		{
			char text[256];
			sprintf(text, "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
			MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
			WRITE_BYTE(ENTINDEX(pEntity));
			WRITE_STRING(text);
			MESSAGE_END();
		}

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%u><%s>\" changed name to \"%s\"\n",
				STRING(pEntity->v.netname),
				GETPLAYERUSERID(pEntity),
				g_engfuncs.pfnGetPlayerWONId(pEntity),
				g_engfuncs.pfnInfoKeyValue(infobuffer, "model"),
				g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%u><%i>\" changed name to \"%s\"\n",
				STRING(pEntity->v.netname),
				GETPLAYERUSERID(pEntity),
				g_engfuncs.pfnGetPlayerWONId(pEntity),
				GETPLAYERUSERID(pEntity),
				g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		}
	}

	g_pGameRules->ClientUserInfoChanged(GetClassPtr((CBasePlayer*)&pEntity->v), infobuffer);
}

static int g_serveractive = 0;

// orig 0x1d650 (unlisted). RC2: clears a player member and ends any vote.
void ServerDeactivate()
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate
	if (g_serveractive != 1)
	{
		return;
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));
		if (pPlayer)
			pPlayer->m_iClientId = 0;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//
	StopVote();
	ResetVote();
}

// orig 0x1d6b0. RC2 keeps an older SDK's client test: it skips the entities below clientMax
// (the world included) and activates the one at clientMax (dossier: original bugs). The vote
// is reset after the user messages are linked.
void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	int i;
	CBaseEntity* pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	// Clients have not been initialized yet
	for (i = 0; i < edictCount; i++)
	{
		if (0 != pEdictList[i].free)
			continue;

		// Clients aren't necessarily initialized until ClientPutInServer()
		if (i < clientMax || !pEdictList[i].pvPrivateData)
			continue;

		pClass = CBaseEntity::Instance(&pEdictList[i]);
		// Activate this entity if it's got a class & isn't dormant
		if (pClass && (pClass->pev->flags & FL_DORMANT) == 0)
		{
			pClass->Activate();
		}
		else
		{
			ALERT(at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname));
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
	ResetVote();
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink();
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink();
}



// orig 0x1d790
void ParmsNewLevel()
{
}


void ParmsChangeLevel()
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA* pSaveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;

	if (pSaveData)
		pSaveData->connectionCount = BuildChangeList(pSaveData->levelList, MAX_LEVEL_CONNECTIONS);
}

static std::vector<std::string> g_MapsToLoad;

static void LoadNextMap()
{
	const std::string mapName = std::move(g_MapsToLoad.back());
	g_MapsToLoad.pop_back();

	pmove->Con_Printf("Loading map \"%s\" automatically (%d left)\n",
		mapName.c_str(), static_cast<int>(g_MapsToLoad.size() + 1));

	if (g_MapsToLoad.empty())
	{
		pmove->Con_Printf("Loading last map\n");
		g_MapsToLoad.shrink_to_fit();
	}

	SERVER_COMMAND(UTIL_VarArgs("map \"%s\"\n", mapName.c_str()));
}

static void LoadAllMaps()
{
	if (!g_MapsToLoad.empty())
	{
		pmove->Con_Printf("Already loading all maps (%d remaining)\nUse sv_stop_loading_all_maps to stop\n",
			static_cast<int>(g_MapsToLoad.size()));
		return;
	}

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;

	const char* fileName = g_pFileSystem->FindFirst("maps/*.bsp", &handle);

	if (fileName != nullptr)
	{
		do
		{
			std::string mapName = fileName;
			mapName.resize(mapName.size() - 4);

			if (std::find_if(g_MapsToLoad.begin(), g_MapsToLoad.end(), [=](const auto& candidate)
					{ return 0 == stricmp(candidate.c_str(), mapName.c_str()); }) == g_MapsToLoad.end())
			{
				g_MapsToLoad.push_back(std::move(mapName));
			}
		} while ((fileName = g_pFileSystem->FindNext(handle)) != nullptr);

		g_pFileSystem->FindClose(handle);

		// Sort in reverse order so the first map in alphabetic order is loaded first.
		std::sort(g_MapsToLoad.begin(), g_MapsToLoad.end(), [](const auto& lhs, const auto& rhs)
			{ return rhs < lhs; });
	}

	if (!g_MapsToLoad.empty())
	{
		if (CMD_ARGC() == 2)
		{
			const char* firstMapToLoad = CMD_ARGV(1);

			// Clear out all maps that would have been loaded before this one.
			if (auto it = std::find(g_MapsToLoad.begin(), g_MapsToLoad.end(), firstMapToLoad);
				it != g_MapsToLoad.end())
			{
				const std::size_t numberOfMapsToSkip = g_MapsToLoad.size() - (it - g_MapsToLoad.begin());

				g_MapsToLoad.erase(it + 1, g_MapsToLoad.end());

				pmove->Con_Printf("Skipping %d maps to start with \"%s\"\n",
					static_cast<int>(numberOfMapsToSkip), g_MapsToLoad.back().c_str());
			}
			else
			{
				pmove->Con_Printf("Unknown map \"%s\", starting from beginning\n", firstMapToLoad);
			}
		}

		pmove->Con_Printf("Loading %d maps one at a time to generate files\n", static_cast<int>(g_MapsToLoad.size()));

		// Load the first map right now.
		LoadNextMap();
	}
	else
	{
		pmove->Con_Printf("No maps to load\n");
	}
}

void InitMapLoadingUtils()
{
	g_engfuncs.pfnAddServerCommand("sv_load_all_maps", &LoadAllMaps);
	// Escape hatch in case the command is executed in error.
	g_engfuncs.pfnAddServerCommand("sv_stop_loading_all_maps", []()
		{ g_MapsToLoad.clear(); });
}

static bool g_LastAllowBunnyHoppingState = false;

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
// orig 0x1d7d0 (unlisted). RC2: reads sv_cheats into g_flWeaponCheat each frame, then runs
// the virtual hulls and the vote. halflife-updated's bunny hopping key and map loader are kept.
void StartFrame()
{
	if (g_pGameRules)
		g_pGameRules->Think();

	if (g_fGameOver)
		return;

	g_flWeaponCheat = CVAR_GET_FLOAT("sv_cheats");
	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;

	RemoveOrphanVirtualHulls();
	VoteThink();

	const bool allowBunnyHopping = sv_allowbunnyhopping.value != 0;

	if (allowBunnyHopping != g_LastAllowBunnyHoppingState)
	{
		g_LastAllowBunnyHoppingState = allowBunnyHopping;

		for (int i = 1; i <= gpGlobals->maxClients; ++i)
		{
			auto player = UTIL_PlayerByIndex(i);

			if (!player)
			{
				continue;
			}

			g_engfuncs.pfnSetPhysicsKeyValue(player->edict(), "bj", UTIL_dtos1(allowBunnyHopping ? 1 : 0));
		}
	}

	// If we're loading all maps then change maps after 3 seconds (time starts at 1)
	// to give the game time to generate files.
	if (!g_MapsToLoad.empty() && gpGlobals->time > 4)
	{
		LoadNextMap();
	}
}


// orig 0x1d820. RC2's list: SDK 2.3's (without halflife-updated's vehicle sound and grunt),
// then sprites and sounds of its own (the jetpack, energy draw, making health), the five
// transforms' (p8), and while sv_pcstuff is set, eight monsters (the human grunt twice).
void ClientPrecache()
{
	PRECACHE_SOUND("player/sprayer.wav");
	PRECACHE_SOUND("player/pl_fallpain2.wav");
	PRECACHE_SOUND("player/pl_fallpain3.wav");
	PRECACHE_SOUND("player/pl_step1.wav");
	PRECACHE_SOUND("player/pl_step2.wav");
	PRECACHE_SOUND("player/pl_step3.wav");
	PRECACHE_SOUND("player/pl_step4.wav");
	PRECACHE_SOUND("common/npc_step1.wav");
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");
	PRECACHE_SOUND("player/pl_metal1.wav");
	PRECACHE_SOUND("player/pl_metal2.wav");
	PRECACHE_SOUND("player/pl_metal3.wav");
	PRECACHE_SOUND("player/pl_metal4.wav");
	PRECACHE_SOUND("player/pl_dirt1.wav");
	PRECACHE_SOUND("player/pl_dirt2.wav");
	PRECACHE_SOUND("player/pl_dirt3.wav");
	PRECACHE_SOUND("player/pl_dirt4.wav");
	PRECACHE_SOUND("player/pl_duct1.wav");
	PRECACHE_SOUND("player/pl_duct2.wav");
	PRECACHE_SOUND("player/pl_duct3.wav");
	PRECACHE_SOUND("player/pl_duct4.wav");
	PRECACHE_SOUND("player/pl_grate1.wav");
	PRECACHE_SOUND("player/pl_grate2.wav");
	PRECACHE_SOUND("player/pl_grate3.wav");
	PRECACHE_SOUND("player/pl_grate4.wav");
	PRECACHE_SOUND("player/pl_slosh1.wav");
	PRECACHE_SOUND("player/pl_slosh2.wav");
	PRECACHE_SOUND("player/pl_slosh3.wav");
	PRECACHE_SOUND("player/pl_slosh4.wav");
	PRECACHE_SOUND("player/pl_tile1.wav");
	PRECACHE_SOUND("player/pl_tile2.wav");
	PRECACHE_SOUND("player/pl_tile3.wav");
	PRECACHE_SOUND("player/pl_tile4.wav");
	PRECACHE_SOUND("player/pl_tile5.wav");
	PRECACHE_SOUND("player/pl_swim1.wav");
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");
	PRECACHE_SOUND("player/pl_ladder1.wav");
	PRECACHE_SOUND("player/pl_ladder2.wav");
	PRECACHE_SOUND("player/pl_ladder3.wav");
	PRECACHE_SOUND("player/pl_ladder4.wav");
	PRECACHE_SOUND("player/pl_wade1.wav");
	PRECACHE_SOUND("player/pl_wade2.wav");
	PRECACHE_SOUND("player/pl_wade3.wav");
	PRECACHE_SOUND("player/pl_wade4.wav");
	PRECACHE_SOUND("debris/wood1.wav");
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");
	PRECACHE_SOUND("plats/train_use1.wav");
	PRECACHE_SOUND("buttons/spark5.wav");
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");
	PRECACHE_SOUND("items/flashlight1.wav");
	PRECACHE_SOUND("items/flashlight1.wav");
	PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("player/pl_pain2.wav");
	PRECACHE_SOUND("player/pl_pain4.wav");
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");
	PRECACHE_MODEL("models/player.mdl");
	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");
	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");
	PRECACHE_MODEL("sprites/sqflare.spr");
	PRECACHE_MODEL("sprites/bigflare1.spr");
	PRECACHE_MODEL("sprites/gradsmoke.spr");
	PRECACHE_MODEL("sprites/gradsmoke2.spr");
	PRECACHE_MODEL("sprites/gradsmoke3.spr");
	PRECACHE_MODEL("sprites/gradpulse.spr");
	PRECACHE_MODEL("sprites/laserbeam.spr");
	PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/jetboost.wav");
	PRECACHE_MODEL("sprites/xfireball3.spr");
	PRECACHE_MODEL("sprites/stmbal1.spr");
	PRECACHE_MODEL("models/jetpack.mdl");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_SOUND("ambience/industrial4.wav");
	PRECACHE_MODEL("sprites/gradbeam.spr");
	PRECACHE_MODEL("sprites/b-tele1.spr");
	PRECACHE_SOUND("weapons/energydraw.wav");
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_SOUND("weapons/mkhealth.wav");
	PRECACHE_MODEL("sprites/noise1.spr");
	Transform_Precache();
	if (sv_pcstuff.value != 0.0)
	{
		UTIL_PrecacheOther("monster_human_grunt");
		UTIL_PrecacheOther("monster_scientist");
		UTIL_PrecacheOther("monster_barney");
		UTIL_PrecacheOther("monster_alien_slave");
		UTIL_PrecacheOther("monster_ichthyosaur");
		UTIL_PrecacheOther("monster_human_grunt");
		UTIL_PrecacheOther("monster_headcrab");
		UTIL_PrecacheOther("monster_gargantua");
	}
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
// orig 0x1dce0 (unlisted): RC2's is a constant
const char* GetGameDescription()
{
	return "Rocket Crowbar 2";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
// orig 0x1dcf0 (unlisted; stock)
void Sys_Error(const char* error_string)
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization(edict_t* pEntity, customization_t* pCust)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
// orig 0x1dd70 (unlisted; stock)
void SpectatorConnect(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect();
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
// orig 0x1dd90 (unlisted; stock)
void SpectatorDisconnect(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect();
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
// orig 0x1ddb0 (unlisted; stock)
void SpectatorThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink();
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility(edict_t* pViewEntity, edict_t* pClient, unsigned char** pvs, unsigned char** pas)
{
	Vector org;
	edict_t* pView = pClient;

	// Find the client's PVS
	if (pViewEntity)
	{
		pView = pViewEntity;
	}

	if ((pClient->v.flags & FL_PROXY) != 0)
	{
		*pvs = NULL; // the spectator proxy sees
		*pas = NULL; // and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ((pView->v.flags & FL_DUCKING) != 0)
	{
		org = org + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	*pvs = ENGINE_SET_PVS((float*)&org);
	*pas = ENGINE_SET_PAS((float*)&org);
}

#include "entity_state.h"

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
// orig 0x1ded0 (unlisted: no function at it in the decompile, read from the disassembly).
// RC2: the entity's m_iRC2Flags and the host's state can force it into the update past the
// visibility tests ("forced"), and change what it shows. Names ours:
//   bit 1  forced to its owner
//   bit 2  sent even without a model or drawn (EF_NODRAW), but without its model fields
//   bit 4  to its owner: non-solid, and owned by it
//   bit 8  forced to everyone
//   bit 16 forced to the player it is attached to (dmg_inflictor), and drawn invisible
// A host with bit 32 is sent every DAMAGE_AIM entity, but those it can't see only as a
// position and box. A player and those riding it or ridden by it, the host's admin selection
// and (host member +0x788) everything are forced too. halflife-updated's 2048-entity guard is
// kept; its m_EFlags, FL_FLY interpolation and flesh-sound flag aren't in RC2.
int AddToFullPack(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet)
{
	// Entities with an index greater than this will corrupt the client's heap because
	// the index is sent with only 11 bits of precision (2^11 == 2048).
	// So we don't send them, just like having too many entities would result
	// in the entity not being sent.
	if (e >= MAX_EDICTS)
	{
		return 0;
	}

	int i;

	bool fForce = false;	   // name ours
	bool fNoModel = false;	   // bit 2
	bool fOwnedByHost = false; // bit 4, or the host's view
	bool fPositionOnly = false;
	bool fInvisible = false; // bit 16

	if (UTIL_IsValidEntity(ent))
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(ent);
		CBaseEntity* pHost = CBaseEntity::Instance(host);
		if (pEntity)
		{
			const int iFlags = pEntity->m_iRC2Flags;
			if ((iFlags & 1) != 0 && pEntity->pev->owner == host)
				fForce = true;
			if ((iFlags & 2) != 0)
				fNoModel = true;
			if ((iFlags & 4) != 0 && pEntity->pev->owner == host)
				fOwnedByHost = true;
			if ((iFlags & 8) != 0)
				fForce = true;
			if ((iFlags & 0x10) != 0 && pEntity->pev->dmg_inflictor == host)
			{
				fForce = true;
				fInvisible = true;
			}

			if (pHost && pHost->IsPlayer())
			{
				CBasePlayer* pHostPlayer = static_cast<CBasePlayer*>(pHost);
				if (0 != pHostPlayer->m_iRC2Player788)
					fForce = true;
				if (0 != pHostPlayer->m_bDebugCounts && pHostPlayer->m_pentDebug && UTIL_IsValidEntity(pHostPlayer->m_pentDebug) && pHostPlayer->m_pentDebug == ent)
					fForce = true;
				if (pEntity->IsViewOfPlayer(pHost))
					fOwnedByHost = true;
				if ((pHost->m_iRC2Flags & 0x20) != 0 && pEntity->pev->takedamage == DAMAGE_AIM && pEntity->edict() != pHost->edict())
				{
					fForce = true;
					if (!ENGINE_CHECK_VISIBILITY((const struct edict_s*)ent, pSet))
						fPositionOnly = true;
				}

				const int iEnt = ENTINDEX(ent);
				if (0 != iEnt)
				{
					if (pHostPlayer->m_iRidingIndex == iEnt)
						fForce = true;
					if (pHostPlayer->m_iShoulderRider[0] == iEnt || pHostPlayer->m_iShoulderRider[1] == iEnt)
						fForce = true;
					if (pEntity->IsPlayer())
					{
						CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pEntity);
						const int iHost = ENTINDEX(host);
						if (pPlayer->m_iRidingIndex == iHost)
							fForce = true;
						if (pPlayer->m_iShoulderRider[0] == iHost || pPlayer->m_iShoulderRider[1] == iHost)
							fForce = true;
					}
				}
			}
		}
	}

	if (UTIL_IsValidEntity(host))
	{
		// VERIFY: what +0x688 and +0x7a4 are; with iuser1 2 and iuser2 the target this looks
		// like RC2's own spectator chase
		CBasePlayer* pHostPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(host));
		if (pHostPlayer && 0 != pHostPlayer->m_bNotJoined && pHostPlayer->m_bSpecChase == 1 && pHostPlayer->pev->iuser1 == 2 && pHostPlayer->pev->iuser2 > 0 && pHostPlayer->pev->iuser2 == ENTINDEX(ent))
			fForce = true;
	}

	if ((host->v.flags & FL_PROXY) == 0 && !fForce)
	{
		// don't send if flagged for NODRAW and it's not the host getting the message
		// (RC2 tests effects == EF_NODRAW, not the bit)
		if (ent->v.effects == EF_NODRAW && ent != host && !fNoModel)
			return 0;

		// Ignore ents without valid / visible models
		if ((0 == ent->v.modelindex || !STRING(ent->v.model)) && !fNoModel)
			return 0;

		// Don't send spectators to other players
		if ((ent->v.flags & FL_SPECTATOR) != 0 && (ent != host))
		{
			return 0;
		}

		// Ignore if not the host and not touching a PVS/PAS leaf
		// If pSet is NULL, then the test will always succeed and the entity will be added to the update
		if (ent != host)
		{
			if (!ENGINE_CHECK_VISIBILITY((const struct edict_s*)ent, pSet) && (host->v.flags & FL_PROXY) == 0)
			{
				return 0;
			}
		}

		// Don't send entity to local client if the client says it's predicting the entity itself.
		if ((ent->v.flags & FL_SKIPLOCALHOST) != 0)
		{
			if ((hostflags & 1) != 0 && (ent->v.owner == host))
				return 0;
		}
	}

	if (0 != host->v.groupinfo)
	{
		UTIL_SetGroupTrace(host->v.groupinfo, GROUP_OP_AND);

		// Should always be set, of course
		if (0 != ent->v.groupinfo)
		{
			if (g_groupop == GROUP_OP_AND)
			{
				if ((ent->v.groupinfo & host->v.groupinfo) == 0)
					return 0;
			}
			else if (g_groupop == GROUP_OP_NAND)
			{
				if ((ent->v.groupinfo & host->v.groupinfo) != 0)
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset(state, 0, sizeof(*state));

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number = e;
	state->entityType = ENTITY_NORMAL;

	// Flag custom entities.
	if ((ent->v.flags & FL_CUSTOMENTITY) != 0)
	{
		state->entityType = ENTITY_BEAM;
	}

	//
	// Copy state data
	//

	// Round animtime to nearest millisecond
	if (!fPositionOnly)
		state->animtime = (int)(1000.0 * ent->v.animtime) / 1000.0;

	memcpy(state->origin, ent->v.origin, 3 * sizeof(float));
	if (!fPositionOnly)
		memcpy(state->angles, ent->v.angles, 3 * sizeof(float));
	memcpy(state->mins, ent->v.mins, 3 * sizeof(float));
	memcpy(state->maxs, ent->v.maxs, 3 * sizeof(float));

	if (!fPositionOnly)
	{
		memcpy(state->startpos, ent->v.startpos, 3 * sizeof(float));
		memcpy(state->endpos, ent->v.endpos, 3 * sizeof(float));

		state->impacttime = ent->v.impacttime;
		state->starttime = ent->v.starttime;
	}

	if (!fNoModel && !fPositionOnly)
	{
		state->modelindex = ent->v.modelindex;
		state->frame = ent->v.frame;
		state->skin = ent->v.skin;
	}
	if (!fPositionOnly)
		state->effects = ent->v.effects;

	// RC2 sends takedamage as the team
	state->team = static_cast<int>(ent->v.takedamage);

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if (0 == player &&
		0 != ent->v.animtime &&
		ent->v.velocity[0] == 0 &&
		ent->v.velocity[1] == 0 &&
		ent->v.velocity[2] == 0)
	{
		state->eflags |= EFLAG_SLERP;
	}

	if (!fNoModel && !fPositionOnly)
	{
		state->scale = ent->v.scale;
		state->colormap = ent->v.colormap;

		state->movetype = ent->v.movetype;
		state->sequence = ent->v.sequence;
		state->framerate = ent->v.framerate;
		state->body = ent->v.body;

		for (i = 0; i < 4; i++)
		{
			state->controller[i] = ent->v.controller[i];
		}

		for (i = 0; i < 2; i++)
		{
			state->blending[i] = ent->v.blending[i];
		}

		state->rendermode = ent->v.rendermode;
		state->renderamt = ent->v.renderamt;
		state->renderfx = ent->v.renderfx;
		state->rendercolor.r = ent->v.rendercolor.x;
		state->rendercolor.g = ent->v.rendercolor.y;
		state->rendercolor.b = ent->v.rendercolor.z;
	}

	if (!fPositionOnly)
	{
		state->aiment = 0;
		if (ent->v.aiment)
		{
			state->aiment = ENTINDEX(ent->v.aiment);
		}

		state->owner = 0;
		if (ent->v.owner)
		{
			int owner = ENTINDEX(ent->v.owner);

			// Only care if owned by a player
			if (owner >= 1 && owner <= gpGlobals->maxClients)
			{
				state->owner = owner;
			}
		}
	}

	if (fOwnedByHost)
	{
		state->solid = SOLID_NOT;
		state->owner = ENTINDEX(host);
	}
	else if (!fPositionOnly)
		state->solid = ent->v.solid;

	if (fInvisible)
	{
		state->rendermode = kRenderTransColor;
		state->renderamt = 0;
		state->rendercolor.r = 0;
		state->rendercolor.g = 0;
		state->rendercolor.b = 0;
		state->renderfx = kRenderFxNone;
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	if (0 == player)
	{
		state->playerclass = ent->v.playerclass;
		return 1;
	}

	// Special stuff for players only
	if (fPositionOnly)
		return 1;

	memcpy(state->basevelocity, ent->v.basevelocity, 3 * sizeof(float));

	state->weaponmodel = MODEL_INDEX(STRING(ent->v.weaponmodel));
	state->gaitsequence = ent->v.gaitsequence;
	state->spectator = ent->v.flags & FL_SPECTATOR;
	state->friction = ent->v.friction;
	// VERIFY: RC2 then sets spectator from state->iuser1, which is still 0 here (memset), so
	// it always sends 0
	state->spectator = state->iuser1 > 0;

	state->gravity = ent->v.gravity;
	state->usehull = (ent->v.flags & FL_DUCKING) != 0 ? 1 : 0;
	state->health = ent->v.health;
	state->iuser1 = ent->v.iuser1;
	state->iuser2 = ent->v.iuser2;
	state->iuser3 = ent->v.iuser3;
	state->playerclass = 0;

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(ent));
	if (pPlayer)
	{
		// the transform as the class, and its hull
		state->playerclass = pPlayer->m_iTransformType;
		switch (pPlayer->m_iTransformType)
		{
		case 6:
			state->usehull = 2;
			break;
		case 1:
		case 3:
			state->usehull = 0;
			break;
		case 2:
		case 4:
		case 5:
			state->usehull = 1;
			break;
		}
		if (0 != pPlayer->m_iShrunk)
			state->usehull = 2;
		if (0 != pPlayer->m_iRidingIndex)
		{
			// a rider is small, and its hull the point one
			state->usehull = 2;
			state->scale = 0.3f;
		}
	}

	return 1;
}

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
// orig 0x1e740. RC2: a player's baseline has no scale and zeroes iuser1, spectator,
// playerclass and usehull.
void CreateBaseline(int player, int eindex, struct entity_state_s* baseline, struct edict_s* entity, int playermodelindex, Vector* player_mins, Vector* player_maxs)
{
	baseline->origin = entity->v.origin;
	baseline->angles = entity->v.angles;
	baseline->frame = entity->v.frame;
	baseline->skin = (short)entity->v.skin;

	// render information
	baseline->rendermode = (byte)entity->v.rendermode;
	baseline->renderamt = (byte)entity->v.renderamt;
	baseline->rendercolor.r = (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g = (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b = (byte)entity->v.rendercolor.z;
	baseline->renderfx = (byte)entity->v.renderfx;

	if (0 != player)
	{
		baseline->mins = *player_mins;
		baseline->maxs = *player_maxs;

		baseline->iuser1 = 0;
		baseline->spectator = 0;
		baseline->playerclass = 0;
		baseline->usehull = 0;

		baseline->colormap = eindex;
		baseline->modelindex = playermodelindex;
		baseline->friction = 1.0;
		baseline->movetype = MOVETYPE_WALK;

		baseline->solid = SOLID_SLIDEBOX;
		baseline->framerate = 1.0;
		baseline->gravity = 1.0;
	}
	else
	{
		baseline->mins = entity->v.mins;
		baseline->maxs = entity->v.maxs;

		baseline->colormap = 0;
		baseline->modelindex = entity->v.modelindex; //SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype = entity->v.movetype;

		baseline->scale = entity->v.scale;
		baseline->solid = entity->v.solid;
		baseline->framerate = entity->v.framerate;
		baseline->gravity = entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int field;
} entity_field_alias_t;

#define FIELD_ORIGIN0 0
#define FIELD_ORIGIN1 1
#define FIELD_ORIGIN2 2
#define FIELD_ANGLES0 3
#define FIELD_ANGLES1 4
#define FIELD_ANGLES2 5

static entity_field_alias_t entity_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
		{"angles[0]", 0},
		{"angles[1]", 0},
		{"angles[2]", 0},
};

void Entity_FieldInit(struct delta_s* pFields)
{
	entity_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN0].name);
	entity_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN1].name);
	entity_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN2].name);
	entity_field_alias[FIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES0].name);
	entity_field_alias[FIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES1].name);
	entity_field_alias[FIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES2].name);
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
// orig 0x1e960 (unlisted; stock)
void Entity_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	static bool initialized = false;

	if (!initialized)
	{
		Entity_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	const bool localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->impacttime != 0) && (t->starttime != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);

		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) &&
		(t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

static entity_field_alias_t player_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
};

void Player_FieldInit(struct delta_s* pFields)
{
	player_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN0].name);
	player_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN1].name);
	player_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN2].name);
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
// orig 0x1eb00 (unlisted; stock)
void Player_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	static bool initialized = false;

	if (!initialized)
	{
		Player_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	const bool localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) &&
		(t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

#define CUSTOMFIELD_ORIGIN0 0
#define CUSTOMFIELD_ORIGIN1 1
#define CUSTOMFIELD_ORIGIN2 2
#define CUSTOMFIELD_ANGLES0 3
#define CUSTOMFIELD_ANGLES1 4
#define CUSTOMFIELD_ANGLES2 5
#define CUSTOMFIELD_SKIN 6
#define CUSTOMFIELD_SEQUENCE 7
#define CUSTOMFIELD_ANIMTIME 8

entity_field_alias_t custom_entity_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
		{"angles[0]", 0},
		{"angles[1]", 0},
		{"angles[2]", 0},
		{"skin", 0},
		{"sequence", 0},
		{"animtime", 0},
};

void Custom_Entity_FieldInit(struct delta_s* pFields)
{
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].name);
	custom_entity_field_alias[CUSTOMFIELD_SKIN].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].name);
	custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].name);
	custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].name);
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	int beamType;
	static bool initialized = false;

	if (!initialized)
	{
		Custom_Entity_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	beamType = t->rendermode & 0x0f;

	if (beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field);
	}

	if (beamType != BEAM_POINTS)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field);
	}

	if (beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field);
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ((int)f->animtime == (int)t->animtime)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field);
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders()
{
	DELTA_ADDENCODER("Entity_Encode", Entity_Encode);
	DELTA_ADDENCODER("Custom_Encode", Custom_Encode);
	DELTA_ADDENCODER("Player_Encode", Player_Encode);
}

// orig 0x1edc0 (unlisted). RC2: nothing for a transformed player (pev->playerclass set), and
// two more timers (the weapon's m_flNextTertiaryAttack and m_flNextQuaternaryAttack, as
// m_flPumpTime and fuser4). halflife-updated's per-weapon GetWeaponData call isn't in RC2.
int GetWeaponData(struct edict_s* player, struct weapon_data_s* info)
{
	memset(info, 0, MAX_WEAPONS * sizeof(weapon_data_t));

#if defined(CLIENT_WEAPONS)
	int i;
	weapon_data_t* item;
	entvars_t* pev = &player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
	CBasePlayerWeapon* gun;

	ItemInfo II;

	if (!pl)
		return 1;

	if (0 != pl->pev->playerclass)
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (pl->m_rgpPlayerItems[i])
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem* pPlayerItem = pl->m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				gun = pPlayerItem->GetWeaponPtr();
				if (gun && gun->UseDecrement())
				{
					// Get The ID.
					memset(&II, 0, sizeof(II));
					gun->GetItemInfo(&II);

					if (II.iId >= 0 && II.iId < MAX_WEAPONS)
					{
						item = &info[II.iId];

						item->m_iId = II.iId;
						item->m_iClip = gun->m_iClip;

						item->m_flTimeWeaponIdle = V_max(gun->m_flTimeWeaponIdle, -0.001f);
						item->m_flNextPrimaryAttack = V_max(gun->m_flNextPrimaryAttack, -0.001f);
						item->m_flNextSecondaryAttack = V_max(gun->m_flNextSecondaryAttack, -0.001f);
						item->m_fInReload = static_cast<int>(gun->m_fInReload);
						item->m_fInSpecialReload = gun->m_fInSpecialReload;
						item->fuser1 = V_max(gun->pev->fuser1, -0.001f);
						item->fuser2 = gun->m_flStartThrow;
						item->fuser3 = gun->m_flReleaseThrow;
						item->iuser1 = gun->m_chargeReady;
						item->iuser2 = gun->m_fInAttack;
						item->iuser3 = gun->m_fireState;
						item->m_flPumpTime = V_max(gun->m_flNextTertiaryAttack, -0.001f);
						item->fuser4 = V_max(gun->m_flNextQuaternaryAttack, -0.001f);

						//						item->m_flPumpTime				= V_max( gun->m_flPumpTime, -0.001f );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
#endif
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
// orig 0x1f000 (unlisted). RC2: iuser3 too, and no weapon data for a transformed player
// (pev->playerclass set); halflife-updated's RPG fields aren't in RC2.
void UpdateClientData(const edict_t* ent, int sendweapons, struct clientdata_s* cd)
{
	if (!ent || !ent->pvPrivateData)
		return;
	entvars_t* pev = (entvars_t*)&ent->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
	entvars_t* pevOrg = NULL;

	// if user is spectating different player in First person, override some vars
	if (pl && pl->pev->iuser1 == OBS_IN_EYE)
	{
		if (pl->m_hObserverTarget)
		{
			pevOrg = pev;
			pev = pl->m_hObserverTarget->pev;
			pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
		}
	}

	cd->flags = pev->flags;
	cd->health = pev->health;

	cd->viewmodel = MODEL_INDEX(STRING(pev->viewmodel));

	cd->waterlevel = pev->waterlevel;
	cd->watertype = pev->watertype;
	// halflife-updated stores the player's weapon bitmask in
	// CBasePlayer::m_WeaponBits (std::uint64_t) and ships the bits to its
	// own cl_dll via the custom gmsgWeapons message rather than the
	// engine's per-frame client_data.iWeaponBits delta. Vanilla HL clients
	// (i.e., a stock anniversary client connecting to this server) don't
	// have the matching cl_dll, so they only see cd->weapons / iWeaponBits.
	// Without this sync, pev->weapons stays 0, the WEAPON_SUIT bit (31)
	// never reaches the vanilla HUD, and the HEV display never appears.
	// Replicate the lower 32 bits of m_WeaponBits into cd->weapons so the
	// legacy channel carries WEAPON_SUIT and weapons 0..30 to vanilla
	// clients. The halflife-updated cl_dll ignores cdata->iWeaponBits
	// (see cl_dll/hud_update.cpp:38), so updated clients are unaffected.
	if (pl)
		cd->weapons = static_cast<int>(pl->m_WeaponBits & 0xFFFFFFFF);
	else
		cd->weapons = pev->weapons;

	// Vectors
	cd->origin = pev->origin;
	cd->velocity = pev->velocity;
	cd->view_ofs = pev->view_ofs;
	cd->punchangle = pev->punchangle;

	cd->bInDuck = pev->bInDuck;
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flDuckTime = pev->flDuckTime;
	cd->flSwimTime = pev->flSwimTime;
	cd->waterjumptime = pev->teleport_time;

	strcpy(cd->physinfo, ENGINE_GETPHYSINFO(ent));

	cd->maxspeed = pev->maxspeed;
	cd->fov = pl->m_iFOV;
	cd->weaponanim = pev->weaponanim;

	cd->pushmsec = pev->pushmsec;

	//Spectator mode
	if (pevOrg != NULL)
	{
		// don't use spec vars from chased player
		cd->iuser1 = pevOrg->iuser1;
		cd->iuser2 = pevOrg->iuser2;
	}
	else
	{
		cd->iuser1 = pev->iuser1;
		cd->iuser2 = pev->iuser2;
	}
	cd->iuser3 = pev->iuser3;



#if defined(CLIENT_WEAPONS)
	if (0 != sendweapons)
	{
		if (pl && 0 == pev->playerclass)
		{
			cd->m_flNextAttack = pl->m_flNextAttack;
			cd->fuser2 = pl->m_flNextAmmoBurn;
			cd->fuser3 = pl->m_flAmmoStartCharge;
			cd->vuser1.x = pl->ammo_9mm;
			cd->vuser1.y = pl->ammo_357;
			cd->vuser1.z = pl->ammo_argrens;
			cd->ammo_nails = pl->ammo_bolts;
			cd->ammo_shells = pl->ammo_buckshot;
			cd->ammo_rockets = pl->ammo_rockets;
			cd->ammo_cells = pl->ammo_uranium;
			cd->vuser2.x = pl->ammo_hornets;


			if (pl->m_pActiveItem)
			{
				CBasePlayerWeapon* gun = pl->m_pActiveItem->GetWeaponPtr();
				if (gun && gun->UseDecrement())
				{
					ItemInfo II;
					memset(&II, 0, sizeof(II));
					gun->GetItemInfo(&II);

					cd->m_iId = II.iId;

					cd->vuser3.z = gun->m_iSecondaryAmmoType;
					cd->vuser4.x = gun->m_iPrimaryAmmoType;
					cd->vuser4.y = pl->m_rgAmmo[gun->m_iPrimaryAmmoType];
					cd->vuser4.z = pl->m_rgAmmo[gun->m_iSecondaryAmmoType];
				}
			}
		}
	}
#endif
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart(const edict_t* player, const struct usercmd_s* cmd, unsigned int random_seed)
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	if (!pl)
		return;

	if (pl->pev->groupinfo != 0)
	{
		UTIL_SetGroupTrace(pl->pev->groupinfo, GROUP_OP_AND);
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd(const edict_t* player)
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	if (!pl)
		return;
	if (pl->pev->groupinfo != 0)
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
// orig 0x1f340 (unlisted; stock)
int ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	return static_cast<int>(PM_GetHullBounds(hullnumber, mins, maxs));
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
// orig 0x1f370
void CreateInstancedBaselines()
{
	int iret = 0;
	entity_state_t state;

	memset(&state, 0, sizeof(state));

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int InconsistentFile(const edict_t* player, const char* filename, char* disconnect_message)
{
	// Server doesn't care?
	if (CVAR_GET_FLOAT("mp_consistency") != 1)
		return 0;

	// Default behavior is to kick the player
	sprintf(disconnect_message, "Server is enforcing file consistency for %s\n", filename);

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
// orig 0x1f3c0 (unlisted; stock)
int AllowLagCompensation()
{
	return 1;
}

// orig 0x1f3d0: compiler-generated, the start of CBasePlayer's constructor (the CBaseMonster
// part RC2's GetClassPtr<CBasePlayer> calls before its member constructors), stock

// orig 0x1ad20 CBasePlayer::IsAlive, 0x1ad50 ShouldFadeOnDeath, 0x1ad60 IsPlayer, 0x1ad70
// IsNetClient, 0x1ad80 ObjectCaps: player.h's inline bodies, emitted here, stock (each "orig"
// listed: orig 0x1ad20, orig 0x1ad50, orig 0x1ad60, orig 0x1ad70, orig 0x1ad80)
