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
//
// teamplay_gamerules.cpp
//
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "skill.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"
#include "hltv.h"
#include "trains.h"
#include "UserMessages.h"
#include "rc2prefs.h"
#include "rc2/killmessages.h"

#include <algorithm>

// From elsewhere in rc2.dll
extern int g_iRC2ScoreMode; // world.cpp, orig .bss 0x1afd38: mp_scoremode as CWorld::Spawn read it (0 or 1)

// orig 0x7efd0. The player's userinfo "gender": 1 for 1, f or F; 2 for 2, n, N, i, I, u or U;
// 0 otherwise.
int PlayerGender(edict_t* pPlayer) // name ours
{
	const char* pszGender = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer), "gender");
	if (!pszGender)
		return 0;

	switch (pszGender[0])
	{
	case '1':
	case 'f':
	case 'F':
		return 1;
	case '2':
	case 'n':
	case 'N':
	case 'i':
	case 'I':
	case 'u':
	case 'U':
		return 2;
	}
	return 0;
}

#define ITEM_RESPAWN_TIME 30
#define WEAPON_RESPAWN_TIME 20
#define AMMO_RESPAWN_TIME 20

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME 120

// The statics below are stock; the original's compiler-generated code for them: orig 0x7deb0
// (g_VoiceGameMgr's initializer), orig 0x7dec0 (its constructor), orig 0x7ded0 (its atexit),
// orig 0x7dee0 (the destructor that atexit runs), orig 0x7def0 (the helper's initializer),
// orig 0x7df00 (the helper's constructor), orig 0x7df10
// (CMultiplayGameMgrHelper::CanPlayerHearPlayer), orig 0x7df40 (the helper's atexit), orig 0x7df50
// (the destructor it runs), orig 0x7df60 (its deleting destructor)
CVoiceGameMgr g_VoiceGameMgr;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	bool CanPlayerHearPlayer(CBasePlayer* pListener, CBasePlayer* pTalker) override
	{
		if (g_teamplay)
		{
			if (g_pGameRules->PlayerRelationship(pListener, pTalker) != GR_TEAMMATE)
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay::CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	RefreshSkillData();

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if (IS_DEDICATED_SERVER())
	{
		// this code has been moved into engine, to only run server.cfg once
	}
	else
	{
		// listen server
		char* lservercfgfile = (char*)CVAR_GET_STRING("lservercfgfile");

		if (lservercfgfile && '\0' != lservercfgfile[0])
		{
			char szCommand[256];

			ALERT(at_console, "Executing listen server config file\n");
			sprintf(szCommand, "exec %s\n", lservercfgfile);
			SERVER_COMMAND(szCommand);
		}
	}
}

bool CHalfLifeMultiplay::ClientCommand(CBasePlayer* pPlayer, const char* pcmd)
{
	if (g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return true;

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

void CHalfLifeMultiplay::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	pPlayer->SetPrefsFromUserinfo(infobuffer);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData()
{
	// load all default values
	CGameRules::RefreshSkillData();

	// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;

	// Crowbar whack
	gSkillData.plrDmgCrowbar = 25;

	// Glock Round
	gSkillData.plrDmg9MM = 12;

	// 357 Round
	// SDK 2.3's (stock in RC2); halflife-updated has HL25's 50
	gSkillData.plrDmg357 = 40;

	// MP5 Round
	gSkillData.plrDmgMP5 = 12;

	// M203 grenade
	gSkillData.plrDmgM203Grenade = 100;

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = 20; // fewer pellets in deathmatch

	// Crossbow
	gSkillData.plrDmgCrossbowClient = 20;

	// RPG
	gSkillData.plrDmgRPG = 120;

	// Egon
	gSkillData.plrDmgEgonWide = 20;
	gSkillData.plrDmgEgonNarrow = 10;

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = 100;

	// Satchel Charge
	gSkillData.plrDmgSatchel = 120;

	// Tripmine
	gSkillData.plrDmgTripmine = 150;

	// hornet
	gSkillData.plrDmgHornet = 10;
}

//=========================================================
// orig 0x7e130. RC2: mp_chattime is at least 10; with mp_scoremode 1 the frag limit applies to
// the damage-based score, and the frags left go out with two decimals.
//=========================================================
void CHalfLifeMultiplay::Think()
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	///// Check game rules /////
	static float last_frags;
	static int last_time;

	float frags_remaining = 0;
	int time_remaining = 0;

	if (g_fGameOver) // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT("mp_chattime");
		if (time < 10)
			CVAR_SET_STRING("mp_chattime", "10");
		else if (time > MAX_INTERMISSION_TIME)
			CVAR_SET_STRING("mp_chattime", UTIL_dtos1(MAX_INTERMISSION_TIME));

		m_flIntermissionEndTime = m_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->time)
		{
			if (m_iEndIntermissionButtonHit // check that someone has pressed a key, or the max intermission time is over
				|| ((m_flIntermissionStartTime + MAX_INTERMISSION_TIME) < gpGlobals->time))
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)(0 != flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);

	if (flTimeLimit != 0 && gpGlobals->time >= flTimeLimit)
	{
		GoToIntermission();
		return;
	}

	if (0 != flFragLimit)
	{
		float bestfrags = 1e16; // VERIFY: with nobody on, the original writes (int)1e16 to mp_fragsleft
		float remain;

		// check if any player is over the frag limit
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));

			if (!pPlayer)
				continue;

			const float flScore = g_iRC2ScoreMode != 0 ? pPlayer->m_flDmgScore : pPlayer->pev->frags;

			if (flScore >= flFragLimit)
			{
				GoToIntermission();
				return;
			}

			remain = flFragLimit - flScore;
			if (remain < bestfrags)
			{
				bestfrags = remain;
			}
		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if (frags_remaining != last_frags)
	{
		if (g_iRC2ScoreMode != 0)
			g_engfuncs.pfnCvar_DirectSet(&fragsleft, UTIL_VarArgs("%.2f", frags_remaining));
		else
			g_engfuncs.pfnCvar_DirectSet(&fragsleft, UTIL_VarArgs("%i", (int)frags_remaining));
	}

	// Updates once per second
	if (timeleft.value != last_time)
	{
		g_engfuncs.pfnCvar_DirectSet(&timeleft, UTIL_VarArgs("%i", time_remaining));
	}

	last_frags = frags_remaining;
	last_time = time_remaining;
}


//=========================================================
//=========================================================
// orig 0x7e400
bool CHalfLifeMultiplay::IsMultiplayer()
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x7e410
bool CHalfLifeMultiplay::IsDeathmatch()
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x7e420
bool CHalfLifeMultiplay::IsCoOp()
{
	return 0 != gpGlobals->coop;
}

//=========================================================
// orig 0x7e430. RC2: a client with the userinfo key noautoswitch set to nonzero never switches;
// halflife-updated's cl_autowepswitch (m_iAutoWepSwitch) stays, RC2's client never sends it.
//=========================================================
bool CHalfLifeMultiplay::FShouldSwitchWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	char* infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
	if (infobuffer)
	{
		const char* noautoswitch = g_engfuncs.pfnInfoKeyValue(infobuffer, "noautoswitch");
		if (noautoswitch && atoi(noautoswitch) != 0)
			return false;
	}

	if (!pWeapon->CanDeploy())
	{
		// that weapon can't deploy anyway.
		return false;
	}

	if (!pPlayer->m_pActiveItem)
	{
		// player doesn't have an active item!
		return true;
	}

	if (!pPlayer->m_pActiveItem->CanHolster())
	{
		// can't put away the active item.
		return false;
	}

	//Never switch
	if (pPlayer->m_iAutoWepSwitch == 0)
	{
		return false;
	}

	//Only switch if not attacking
	if (pPlayer->m_iAutoWepSwitch == 2 && (pPlayer->m_afButtonLast & (IN_ATTACK | IN_ATTACK2)) != 0)
	{
		return false;
	}

	if (pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight())
	{
		return true;
	}

	return false;
}

//=========================================================
//=========================================================
bool CHalfLifeMultiplay::ClientConnected(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	g_VoiceGameMgr.ClientConnected(pEntity);
	return true;
}

void CHalfLifeMultiplay::UpdateGameMode(CBasePlayer* pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, NULL, pPlayer->edict());
	WRITE_BYTE(0); // game mode none
	MESSAGE_END();
}

// orig 0x7e660. RC2: ScoreInfo is RC2's six bytes (no class short), every player's score goes
// out twice (the damage-based score as a long with 0x80 on the index, then frags and deaths),
// and no MOTD here.
void CHalfLifeMultiplay::InitHUD(CBasePlayer* pl)
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("%s has joined the game\n",
											 (!FStringNull(pl->pev->netname) && STRING(pl->pev->netname)[0] != 0) ? STRING(pl->pev->netname) : "unconnected"));

	// team match?
	if (g_teamplay)
	{
		UTIL_LogPrintf("\"%s<%i><%s><%s>\" entered the game\n",
			STRING(pl->pev->netname),
			GETPLAYERUSERID(pl->edict()),
			GETPLAYERAUTHID(pl->edict()),
			g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pl->edict()), "model"));
	}
	else
	{
		UTIL_LogPrintf("\"%s<%i><%s><%i>\" entered the game\n",
			STRING(pl->pev->netname),
			GETPLAYERUSERID(pl->edict()),
			GETPLAYERAUTHID(pl->edict()),
			GETPLAYERUSERID(pl->edict()));
	}

	UpdateGameMode(pl);

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN(MSG_ONE, gmsgScoreInfo, NULL, pl->edict());
	WRITE_BYTE(ENTINDEX(pl->edict()));
	WRITE_SHORT(0);
	WRITE_SHORT(0);
	WRITE_BYTE(0);
	MESSAGE_END();

	// loop through all active players and send their score info to the new client
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer* plr = (CBasePlayer*)UTIL_PlayerByIndex(i);

		if (plr)
		{
			int iScoreBits;
			memcpy(&iScoreBits, &plr->m_flDmgScore, sizeof(iScoreBits));

			MESSAGE_BEGIN(MSG_ONE, gmsgScoreInfo, NULL, pl->edict());
			WRITE_BYTE(i | 0x80); // client number, damage-based score
			WRITE_LONG(iScoreBits);
			WRITE_BYTE(GetTeamIndex(plr->m_szTeamName) + 1);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ONE, gmsgScoreInfo, NULL, pl->edict());
			WRITE_BYTE(i); // client number
			WRITE_SHORT(plr->pev->frags);
			WRITE_SHORT(plr->m_iDeaths);
			WRITE_BYTE(GetTeamIndex(plr->m_szTeamName) + 1);
			MESSAGE_END();
		}
	}

	if (g_fGameOver)
	{
		MESSAGE_BEGIN(MSG_ONE, SVC_INTERMISSION, NULL, pl->edict());
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::ClientDisconnected(edict_t* pClient)
{
	if (pClient)
	{
		CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pClient);

		if (pPlayer)
		{
			FireTargets("game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0);

			// team match?
			if (g_teamplay)
			{
				UTIL_LogPrintf("\"%s<%i><%s><%s>\" disconnected\n",
					STRING(pPlayer->pev->netname),
					GETPLAYERUSERID(pPlayer->edict()),
					GETPLAYERAUTHID(pPlayer->edict()),
					g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model"));
			}
			else
			{
				UTIL_LogPrintf("\"%s<%i><%s><%i>\" disconnected\n",
					STRING(pPlayer->pev->netname),
					GETPLAYERUSERID(pPlayer->edict()),
					GETPLAYERAUTHID(pPlayer->edict()),
					GETPLAYERUSERID(pPlayer->edict()));
			}

			pPlayer->RemoveAllItems(true); // destroy all of the players weapons and items
		}
	}
}

//=========================================================
// orig 0x7e9e0 (not in files-rc2.tsv: Ghidra made no function of it). RC2: no fall damage in
// multiplayer, and no mp_falldamage.
//=========================================================
float CHalfLifeMultiplay::FlPlayerFallDamage(CBasePlayer* pPlayer)
{
	return 0;
}

//=========================================================
//=========================================================
// orig 0x7e9f0
bool CHalfLifeMultiplay::FPlayerCanTakeDamage(CBasePlayer* pPlayer, CBaseEntity* pAttacker)
{
	return true;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerThink(CBasePlayer* pPlayer)
{
	if (g_fGameOver)
	{
		// check for button presses
		if ((pPlayer->m_afButtonPressed & (IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP)) != 0)
			m_iEndIntermissionButtonHit = true;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
// orig 0x7ea40 (not in files-rc2.tsv). RC2: without a game_player_equip the crowbar and the
// weapons the player chose (CBasePlayer::m_iAttrWeapons), with the extra ammo rc2.ini gives
// for each; mp_random leaves what is given alone meanwhile (g_fRC2Randomize cleared).
// halflife-updated's forced switch to the first weapon stays.
void CHalfLifeMultiplay::PlayerSpawn(CBasePlayer* pPlayer)
{
	bool addDefault;
	CBaseEntity* pWeaponEntity = NULL;

	//Ensure the player switches to the Glock on spawn regardless of setting
	const int originalAutoWepSwitch = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;

	pPlayer->SetHasSuit(true);

	addDefault = true;
	g_fRC2Randomize = false;

	while ((pWeaponEntity = UTIL_FindEntityByClassname(pWeaponEntity, "game_player_equip")) != NULL)
	{
		pWeaponEntity->Touch(pPlayer);
		addDefault = false;
	}

	if (addDefault)
	{
		const int iWeapons = pPlayer->m_iAttrWeapons;
		int i9mm = 0, i357 = 0, iARGrenades = 0, iBuckshot = 0, iBolts = 0, iRockets = 0, iUranium = 0;

		pPlayer->GiveNamedItem("weapon_crowbar");
		if ((iWeapons & 0x001) != 0)
		{
			pPlayer->GiveNamedItem("weapon_9mmhandgun");
			i9mm = (unsigned short)RC2Pref(RC2P_Attributes_GlockExtraAmmoGive).s;
		}
		if ((iWeapons & 0x002) != 0)
		{
			pPlayer->GiveNamedItem("weapon_357");
			i357 = (unsigned short)RC2Pref(RC2P_Attributes_PythonExtraAmmoGive).s;
		}
		if ((iWeapons & 0x004) != 0)
		{
			pPlayer->GiveNamedItem("weapon_mp5");
			i9mm += (unsigned short)RC2Pref(RC2P_Attributes_MP5ExtraAmmoGive).s;
			iARGrenades = (unsigned short)RC2Pref(RC2P_Attributes_MP5ExtraAmmoGiveGrenades).s;
		}
		if ((iWeapons & 0x008) != 0)
		{
			pPlayer->GiveNamedItem("weapon_shotgun");
			iBuckshot = (unsigned short)RC2Pref(RC2P_Attributes_ShotgunExtraAmmoGive).s;
		}
		if ((iWeapons & 0x010) != 0)
		{
			pPlayer->GiveNamedItem("weapon_crossbow");
			iBolts = (unsigned short)RC2Pref(RC2P_Attributes_CrossbowExtraAmmoGive).s;
		}
		if ((iWeapons & 0x020) != 0)
		{
			pPlayer->GiveNamedItem("weapon_rpg");
			iRockets = (unsigned short)RC2Pref(RC2P_Attributes_RPGExtraAmmoGive).s;
		}
		if ((iWeapons & 0x040) != 0)
		{
			pPlayer->GiveNamedItem("weapon_egon");
			iUranium = (unsigned short)RC2Pref(RC2P_Attributes_EgonExtraAmmoGive).s;
		}
		if ((iWeapons & 0x080) != 0)
		{
			pPlayer->GiveNamedItem("weapon_gauss");
			iUranium += (unsigned short)RC2Pref(RC2P_Attributes_GaussExtraAmmoGive).s;
		}
		if ((iWeapons & 0x100) != 0)
			pPlayer->GiveNamedItem("weapon_hornetgun");

		pPlayer->GiveAmmo(i9mm, "9mm", RC2Pref(RC2P_Ammo_MaxCarry9mmBullets).s, NULL);
		pPlayer->GiveAmmo(i357, "357", 36, NULL);
		pPlayer->GiveAmmo(iARGrenades, "ARgrenades", 15, NULL);
		pPlayer->GiveAmmo(iBuckshot, "buckshot", RC2Pref(RC2P_Ammo_MaxCarryShells).s, NULL);
		pPlayer->GiveAmmo(iBolts, "bolts", RC2Pref(RC2P_Ammo_MaxCarryCrossbowBolts).s, NULL);
		pPlayer->GiveAmmo(iRockets, "rockets", 10, NULL);
		pPlayer->GiveAmmo(iUranium, "uranium", RC2Pref(RC2P_Ammo_MaxCarryCells).s, NULL);
	}

	g_fRC2Randomize = true;
	pPlayer->m_iAutoWepSwitch = originalAutoWepSwitch;
}

// orig 0x804b0 (not in files-rc2.tsv; RC2's CGameRules slot 57): a player may be ridden when
// mp_riding is on and his userinfo allowriding is nonzero. VERIFY: the second argument goes unused
bool CHalfLifeMultiplay::PlayerAllowsRiding(CBasePlayer* pRidden, CBasePlayer* pRider)
{
	if ((int)CVAR_GET_FLOAT("mp_riding") == 0)
		return false;

	char* infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pRidden->edict());
	if (!infobuffer)
		return false;

	const char* pszAllow = g_engfuncs.pfnInfoKeyValue(infobuffer, "allowriding");
	return pszAllow && atoi(pszAllow) != 0;
}

//=========================================================
//=========================================================
// orig 0x7ecc0
bool CHalfLifeMultiplay::FPlayerCanRespawn(CBasePlayer* pPlayer)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x7ecd0
float CHalfLifeMultiplay::FlPlayerSpawnTime(CBasePlayer* pPlayer)
{
	return gpGlobals->time; //now!
}

bool CHalfLifeMultiplay::AllowAutoTargetCrosshair()
{
	return (aimcrosshair.value != 0);
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
// orig 0x7ed00
int CHalfLifeMultiplay::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
// orig 0x7ed10. RC2: a killer is a player when IsPlayer() (SDK 2.3 asked Classify()), he gains
// energy for the kill, mp_scoremode 1 keeps the damage-based score too, ScoreInfo is RC2's six
// bytes, and no satchels are deactivated. halflife-updated's func_vehicle driver stays.
//=========================================================
void CHalfLifeMultiplay::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	CBasePlayer* peKiller = NULL;
	CBaseEntity* ktmp = CBaseEntity::Instance(pKiller);
	if (ktmp && ktmp->IsPlayer())
		peKiller = (CBasePlayer*)ktmp;
	else if (ktmp && (ktmp->Classify() == CLASS_VEHICLE))
	{
		CBasePlayer* pDriver = ((CFuncVehicle*)ktmp)->m_pDriver;
		if (pDriver != NULL)
		{
			peKiller = pDriver;
			ktmp = pDriver;
			pKiller = pDriver->pev;
		}
	}

	DeathNotice(pVictim, pKiller, pInflictor);

	pVictim->m_iDeaths += 1;

	FireTargets("game_playerdie", pVictim, pVictim, USE_TOGGLE, 0);

	if (pVictim->pev == pKiller)
	{ // killed self
		pKiller->frags -= 1;
		if (g_iRC2ScoreMode == 1)
			pVictim->m_flDmgScore += RC2Pref(RC2P_Score_DmgBased_ForSuicide).f;
	}
	else if (ktmp && ktmp->IsPlayer())
	{
		peKiller->m_flEnergy = std::clamp(peKiller->m_flEnergy + RC2Pref(RC2P_Attributes_EnergyGivenForKill).f, 0.0f, 100.0f);

		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill(peKiller, pVictim);

		FireTargets("game_playerkill", ktmp, ktmp, USE_TOGGLE, 0);

		if (g_iRC2ScoreMode == 1)
		{
			peKiller->m_flDmgScore += RC2Pref(RC2P_Score_DmgBased_ForPlayerKill).f;
			pVictim->m_flDmgScore += RC2Pref(RC2P_Score_DmgBased_ForPlayerDeath).f;
		}
	}
	else
	{ // killed by the world
		pKiller->frags -= 1;
		if (g_iRC2ScoreMode == 1)
			pVictim->m_flDmgScore += RC2Pref(RC2P_Score_DmgBased_ForOtherDeath).f;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(ENTINDEX(pVictim->edict()));
	WRITE_SHORT(pVictim->pev->frags);
	WRITE_SHORT(pVictim->m_iDeaths);
	WRITE_BYTE(GetTeamIndex(pVictim->m_szTeamName) + 1);
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity* ep = CBaseEntity::Instance(pKiller);
	if (ep && ep->IsPlayer())
	{
		CBasePlayer* PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
		WRITE_BYTE(ENTINDEX(PK->edict()));
		WRITE_SHORT(PK->pev->frags);
		WRITE_SHORT(PK->m_iDeaths);
		WRITE_BYTE(GetTeamIndex(PK->m_szTeamName) + 1);
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
}

//=========================================================
// Deathnotice.
// orig 0x7f030. RC2: DeathMsg also carries RC2's death message number (chosen by the weapon)
// and the victim's gender, a dedicated server prints the message text, and logs keep
// halflife-updated's auth ids where RC2 printed WON ids.
//=========================================================
void CHalfLifeMultiplay::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor)
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity* Killer = CBaseEntity::Instance(pKiller);

	const char* killer_weapon_name = "world"; // by default, the player is killed by the world
	int killer_index = 0;

	// Hack to fix name change
	const char* tau = "tau_cannon";
	const char* gluon = "gluon gun";

	if ((pKiller->flags & FL_CLIENT) != 0)
	{
		killer_index = ENTINDEX(ENT(pKiller));

		if (pevInflictor)
		{
			if (pevInflictor == pKiller)
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pKiller);

				if (pPlayer->m_pActiveItem)
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING(pevInflictor->classname); // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = STRING(pevInflictor->classname);
	}

	// RC2 picks its death message from the full name, before the prefix comes off
	const char* killer_weapon_name_full = killer_weapon_name;

	// strip the monster_* or weapon_* from the inflictor's classname
	if (strncmp(killer_weapon_name, "weapon_", 7) == 0)
		killer_weapon_name += 7;
	else if (strncmp(killer_weapon_name, "monster_", 8) == 0)
		killer_weapon_name += 8;
	else if (strncmp(killer_weapon_name, "func_", 5) == 0)
		killer_weapon_name += 5;

	const int iDeathMessage = KillMsg_Pick(killer_weapon_name_full, (pVictim->pev == pKiller || (pKiller->flags & FL_CLIENT) == 0), RANDOM_LONG(0, 1000));
	const int iGender = PlayerGender(pVictim->edict());

	MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
	WRITE_BYTE(killer_index);				// the killer
	WRITE_BYTE(ENTINDEX(pVictim->edict())); // the victim
	WRITE_SHORT(iDeathMessage);				// RC2's death message
	WRITE_BYTE(iGender);
	WRITE_STRING(killer_weapon_name); // what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if (0 == strcmp(killer_weapon_name, "egon"))
		killer_weapon_name = gluon;
	else if (0 == strcmp(killer_weapon_name, "gauss"))
		killer_weapon_name = tau;

	if (IS_DEDICATED_SERVER())
	{
		g_engfuncs.pfnServerPrint(KillMsg_Get(iDeathMessage, STRING(pVictim->pev->netname), STRING(pKiller->netname), killer_weapon_name, PlayerGender(pVictim->edict()), pVictim->pev == pKiller));
		g_engfuncs.pfnServerPrint("\n");
	}

	if (pVictim->pev == pKiller)
	{
		// killed self

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",
				STRING(pVictim->pev->netname),
				GETPLAYERUSERID(pVictim->edict()),
				GETPLAYERAUTHID(pVictim->edict()),
				g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pVictim->edict()), "model"),
				killer_weapon_name);
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",
				STRING(pVictim->pev->netname),
				GETPLAYERUSERID(pVictim->edict()),
				GETPLAYERAUTHID(pVictim->edict()),
				GETPLAYERUSERID(pVictim->edict()),
				killer_weapon_name);
		}
	}
	else if ((pKiller->flags & FL_CLIENT) != 0)
	{
		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",
				STRING(pKiller->netname),
				GETPLAYERUSERID(ENT(pKiller)),
				GETPLAYERAUTHID(ENT(pKiller)),
				g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(ENT(pKiller)), "model"),
				STRING(pVictim->pev->netname),
				GETPLAYERUSERID(pVictim->edict()),
				GETPLAYERAUTHID(pVictim->edict()),
				g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pVictim->edict()), "model"),
				killer_weapon_name);
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",
				STRING(pKiller->netname),
				GETPLAYERUSERID(ENT(pKiller)),
				GETPLAYERAUTHID(ENT(pKiller)),
				GETPLAYERUSERID(ENT(pKiller)),
				STRING(pVictim->pev->netname),
				GETPLAYERUSERID(pVictim->edict()),
				GETPLAYERAUTHID(pVictim->edict()),
				GETPLAYERUSERID(pVictim->edict()),
				killer_weapon_name);
		}
	}
	else
	{
		// killed by the world

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING(pVictim->pev->netname),
				GETPLAYERUSERID(pVictim->edict()),
				GETPLAYERAUTHID(pVictim->edict()),
				g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pVictim->edict()), "model"),
				killer_weapon_name);
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING(pVictim->pev->netname),
				GETPLAYERUSERID(pVictim->edict()),
				GETPLAYERAUTHID(pVictim->edict()),
				GETPLAYERUSERID(pVictim->edict()),
				killer_weapon_name);
		}
	}

	MESSAGE_BEGIN(MSG_SPEC, SVC_DIRECTOR);
	WRITE_BYTE(9);							 // command length in bytes
	WRITE_BYTE(DRC_CMD_EVENT);				 // player killed
	WRITE_SHORT(ENTINDEX(pVictim->edict())); // index number of primary entity
	if (pevInflictor)
		WRITE_SHORT(ENTINDEX(ENT(pevInflictor))); // index number of secondary entity
	else
		WRITE_SHORT(ENTINDEX(ENT(pKiller))); // index number of secondary entity
	WRITE_LONG(7 | DRC_FLAG_DRAMATIC);		 // eventflags (priority and flags)
	MESSAGE_END();

	//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
			/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
// orig 0x7f5b0
void CHalfLifeMultiplay::PlayerGotWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay::FlWeaponRespawnTime(CBasePlayerItem* pWeapon)
{
	if (weaponstay.value > 0)
	{
		// make sure it's only certain weapons
		if ((pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) == 0)
		{
			return gpGlobals->time + 0; // weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE 100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
// orig 0x7f610
float CHalfLifeMultiplay::FlWeaponTryRespawn(CBasePlayerItem* pWeapon)
{
	if (pWeapon && WEAPON_NONE != pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) != 0)
	{
		if (NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE))
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime(pWeapon);
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecWeaponRespawnSpot(CBasePlayerItem* pWeapon)
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay::WeaponShouldRespawn(CBasePlayerItem* pWeapon)
{
	if ((pWeapon->pev->spawnflags & SF_NORESPAWN) != 0)
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns false if the player is not allowed
// to pick up this weapon
//=========================================================
// orig 0x7f6c0 (not in files-rc2.tsv; the vtable's slot 30). Rocket Crowbar 2: with
// mp_weaponstay on, a weapon the player already carries goes to the first player linked with him
// (m_iShoulderRider) whom these rules let have it.
bool CHalfLifeMultiplay::CanHavePlayerItem(CBasePlayer* pPlayer, CBasePlayerItem* pItem)
{
	if (weaponstay.value > 0)
	{
		if ((pItem->iFlags() & ITEM_FLAG_LIMITINWORLD) != 0)
			return CGameRules::CanHavePlayerItem(pPlayer, pItem);

		// check if the player already has this weapon
		for (int i = 0; i < MAX_ITEM_TYPES; i++)
		{
			CBasePlayerItem* it = pPlayer->m_rgpPlayerItems[i];

			while (it != NULL)
			{
				if (it->m_iId == pItem->m_iId)
				{
					for (int j = 0; j < 2; j++)
					{
						if (pPlayer->m_iShoulderRider[j] == 0)
							continue;

						CBasePlayer* pLinked = (CBasePlayer*)UTIL_PlayerByIndex(pPlayer->m_iShoulderRider[j]);
						// Original bug (crash), guarded: the original calls in without checking
						// UTIL_PlayerByIndex's result, NULL for a rider slot with no player
						if (pLinked && CanHavePlayerItem(pLinked, pItem))
							return CGameRules::CanHavePlayerItem(pLinked, pItem);
					}
					return false;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem(pPlayer, pItem);
}

//=========================================================
//=========================================================
// orig 0x7f7c0
bool CHalfLifeMultiplay::CanHaveItem(CBasePlayer* pPlayer, CItem* pItem)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x7f7d0
void CHalfLifeMultiplay::PlayerGotItem(CBasePlayer* pPlayer, CItem* pItem)
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn(CItem* pItem)
{
	if ((pItem->pev->spawnflags & SF_NORESPAWN) != 0)
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime(CItem* pItem)
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot(CItem* pItem)
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
// orig 0x7f840
void CHalfLifeMultiplay::PlayerGotAmmo(CBasePlayer* pPlayer, char* szName, int iCount)
{
}

//=========================================================
//=========================================================
// orig 0x7f850
bool CHalfLifeMultiplay::IsAllowedToSpawn(CBaseEntity* pEntity)
{
	//	if ( pEntity->pev->flags & FL_MONSTER )
	//		return false;

	return true;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn(CBasePlayerAmmo* pAmmo)
{
	if ((pAmmo->pev->spawnflags & SF_NORESPAWN) != 0)
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime(CBasePlayerAmmo* pAmmo)
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot(CBasePlayerAmmo* pAmmo)
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
// orig 0x7f8c0
float CHalfLifeMultiplay::FlHealthChargerRechargeTime()
{
	return 60;
}


// orig 0x7f8d0
float CHalfLifeMultiplay::FlHEVChargerRechargeTime()
{
	return 30;
}

//=========================================================
//=========================================================
// orig 0x7f8e0
int CHalfLifeMultiplay::DeadPlayerWeapons(CBasePlayer* pPlayer)
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
// orig 0x7f8f0
int CHalfLifeMultiplay::DeadPlayerAmmo(CBasePlayer* pPlayer)
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

edict_t* CHalfLifeMultiplay::GetPlayerSpawnSpot(CBasePlayer* pPlayer)
{
	edict_t* pentSpawnSpot = CGameRules::GetPlayerSpawnSpot(pPlayer);
	if (IsMultiplayer() && !FStringNull(pentSpawnSpot->v.target))
	{
		FireTargets(STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0);
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
// orig 0x7f950
int CHalfLifeMultiplay::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

bool CHalfLifeMultiplay::PlayFootstepSounds(CBasePlayer* pl, float fvol)
{
	if (g_footsteps && g_footsteps->value == 0)
		return false;

	if (pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220)
		return true; // only make step sounds in multiplayer if the player is moving fast enough

	return false;
}

bool CHalfLifeMultiplay::FAllowFlashlight()
{
	return flashlight.value != 0;
}

//=========================================================
//=========================================================
// orig 0x7f9f0 (not in files-rc2.tsv). RC2 always allows monsters; it has no mp_allowmonsters.
bool CHalfLifeMultiplay::FAllowMonsters()
{
	return true;
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME 6

// orig 0x7fa00 (not in files-rc2.tsv). RC2: mp_chattime at least 10, as in Think
void CHalfLifeMultiplay::GoToIntermission()
{
	if (g_fGameOver)
		return; // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT("mp_chattime");
	if (time < 10)
		CVAR_SET_STRING("mp_chattime", "10");
	else if (time > MAX_INTERMISSION_TIME)
		CVAR_SET_STRING("mp_chattime", UTIL_dtos1(MAX_INTERMISSION_TIME));

	m_flIntermissionEndTime = gpGlobals->time + ((int)mp_chattime.value);
	m_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = true;
	m_iEndIntermissionButtonHit = false;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s* next;

	char mapname[32];
	int minplayers, maxplayers;
	char rulebuffer[MAX_RULE_BUFFER];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s* items;
	struct mapcycle_item_s* next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle(mapcycle_t* cycle)
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if (p)
	{
		start = p;
		p = p->next;
		while (p != start)
		{
			n = p->next;
			delete p;
			p = n;
		}

		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[1500];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char* COM_Parse(char* data)
{
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL; // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while ('\0' != *data && *data != '\n')
			data++;
		goto skipwhite;
	}


	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;
			if (c == '\"' || '\0' == c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',')
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
bool COM_TokenWaiting(char* buffer)
{
	char* p;

	p = buffer;
	while ('\0' != *p && *p != '\n')
	{
		if (0 == isspace(*p) || 0 != isalnum(*p))
			return true;

		p++;
	}

	return false;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
bool ReloadMapCycleFile(char* filename, mapcycle_t* cycle)
{
	char szBuffer[MAX_RULE_BUFFER];
	char szMap[32];
	int length;
	char* pFileList;
	char* aFileList = pFileList = (char*)LOAD_FILE_FOR_ME(filename, &length);
	bool hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if (pFileList && 0 != length)
	{
		// the first map name in the file becomes the default
		while (true)
		{
			hasbuffer = false;
			memset(szBuffer, 0, MAX_RULE_BUFFER);

			pFileList = COM_Parse(pFileList);
			if (strlen(com_token) <= 0)
				break;

			strncpy(szMap, com_token, sizeof(szMap));
			szMap[sizeof(szMap) - 1] = '\0';

			// Any more tokens on this line?
			if (COM_TokenWaiting(pFileList))
			{
				pFileList = COM_Parse(pFileList);
				if (strlen(com_token) > 0)
				{
					hasbuffer = true;
					strncpy(szBuffer, com_token, sizeof(szBuffer));
					szBuffer[sizeof(szBuffer) - 1] = '\0';
				}
			}

			// Check map
			if (IS_MAP_VALID(szMap))
			{
				// Create entry
				char* s;

				item = new mapcycle_item_s;

				strcpy(item->mapname, szMap);

				item->minplayers = 0;
				item->maxplayers = 0;

				memset(item->rulebuffer, 0, MAX_RULE_BUFFER);

				if (hasbuffer)
				{
					s = g_engfuncs.pfnInfoKeyValue(szBuffer, "minplayers");
					if (s && '\0' != s[0])
					{
						item->minplayers = atoi(s);
						item->minplayers = V_max(item->minplayers, 0);
						item->minplayers = V_min(item->minplayers, gpGlobals->maxClients);
					}
					s = g_engfuncs.pfnInfoKeyValue(szBuffer, "maxplayers");
					if (s && '\0' != s[0])
					{
						item->maxplayers = atoi(s);
						item->maxplayers = V_max(item->maxplayers, 0);
						item->maxplayers = V_min(item->maxplayers, gpGlobals->maxClients);
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey(szBuffer, "minplayers");
					g_engfuncs.pfnInfo_RemoveKey(szBuffer, "maxplayers");

					strcpy(item->rulebuffer, szBuffer);
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT(at_console, "Skipping %s from mapcycle, not a valid map\n", szMap);
			}
		}

		FREE_FILE(aFileList);
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while (item)
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if (!item)
	{
		return false;
	}

	while (item->next)
	{
		item = item->next;
	}
	item->next = cycle->items;

	cycle->next_item = item->next;

	return true;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers()
{
	int num = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pEnt = UTIL_PlayerByIndex(i);

		if (pEnt)
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
// orig 0x7ff40
void ExtractCommandString(char* s, char* szCommand)
{
	// Now make rules happen
	char pkey[512];
	char value[512]; // use two buffers so compares
					 // work without stomping on each other
	char* o;

	if (*s == '\\')
		s++;

	while (true)
	{
		o = pkey;
		while (*s != '\\')
		{
			if ('\0' == *s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && '\0' != *s)
		{
			if ('\0' == *s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat(szCommand, pkey);
		if (strlen(value) > 0)
		{
			strcat(szCommand, " ");
			strcat(szCommand, value);
		}
		strcat(szCommand, "\n");

		if ('\0' == *s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay::ChangeLevel()
{
	static char szPreviousMapCycleFile[256];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[1500];
	char szRules[1500];
	int minplayers = 0, maxplayers = 0;
	strcpy(szFirstMapInList, "bootcamp"); // the absolute default level. RC2: bootcamp (orig 0x80070, not in files-rc2.tsv)

	int curplayers;
	bool do_cycle = true;

	// find the map to change to
	char* mapcfile = (char*)CVAR_GET_STRING("mapcyclefile");
	ASSERT(mapcfile != NULL);

	szCommands[0] = '\0';
	szRules[0] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if (stricmp(mapcfile, szPreviousMapCycleFile))
	{
		strcpy(szPreviousMapCycleFile, mapcfile);

		DestroyMapCycle(&mapcycle);

		if (!ReloadMapCycleFile(mapcfile, &mapcycle) || (!mapcycle.items))
		{
			ALERT(at_console, "Unable to load map cycle file %s\n", mapcfile);
			do_cycle = false;
		}
	}

	if (do_cycle && mapcycle.items)
	{
		bool keeplooking = false;
		bool found = false;
		mapcycle_item_s* item;

		// Assume current map
		strcpy(szNextMap, STRING(gpGlobals->mapname));
		strcpy(szFirstMapInList, STRING(gpGlobals->mapname));

		// Traverse list
		for (item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next)
		{
			keeplooking = false;

			ASSERT(item != NULL);

			if (item->minplayers != 0)
			{
				if (curplayers >= item->minplayers)
				{
					found = true;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = true;
				}
			}

			if (item->maxplayers != 0)
			{
				if (curplayers <= item->maxplayers)
				{
					found = true;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = true;
				}
			}

			if (keeplooking)
				continue;

			found = true;
			break;
		}

		if (!found)
		{
			item = mapcycle.next_item;
		}

		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy(szNextMap, item->mapname);

		ExtractCommandString(item->rulebuffer, szCommands);
		strcpy(szRules, item->rulebuffer);
	}

	if (!IS_MAP_VALID(szNextMap))
	{
		strcpy(szNextMap, szFirstMapInList);
	}

	g_fGameOver = true;

	ALERT(at_console, "CHANGE LEVEL: %s\n", szNextMap);
	if (0 != minplayers || 0 != maxplayers)
	{
		ALERT(at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers);
	}
	if (strlen(szRules) > 0)
	{
		ALERT(at_console, "RULES:  %s\n", szRules);
	}

	CHANGE_LEVEL(szNextMap, NULL);
	if (strlen(szCommands) > 0)
	{
		SERVER_COMMAND(szCommands);
	}
}

#define MAX_MOTD_CHUNK 60
#define MAX_MOTD_LENGTH 1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay::SendMOTDToClient(edict_t* client)
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char* pFileList;
	char* aFileList = pFileList = (char*)LOAD_FILE_FOR_ME((char*)CVAR_GET_STRING("motdfile"), &length);

	// send the server name
	MESSAGE_BEGIN(MSG_ONE, gmsgServerName, NULL, client);
	WRITE_STRING(CVAR_GET_STRING("hostname"));
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while (pFileList && '\0' != *pFileList && char_count < MAX_MOTD_LENGTH)
	{
		char chunk[MAX_MOTD_CHUNK + 1];

		if (strlen(pFileList) < MAX_MOTD_CHUNK)
		{
			strcpy(chunk, pFileList);
		}
		else
		{
			strncpy(chunk, pFileList, MAX_MOTD_CHUNK);
			chunk[MAX_MOTD_CHUNK] = 0; // strncpy doesn't always append the null terminator
		}

		char_count += strlen(chunk);
		if (char_count < MAX_MOTD_LENGTH)
			pFileList = aFileList + char_count;
		else
			*pFileList = 0;

		MESSAGE_BEGIN(MSG_ONE, gmsgMOTD, NULL, client);
		WRITE_BYTE(static_cast<int>('\0' == *pFileList)); // false means there is still more message to come
		WRITE_STRING(chunk);
		MESSAGE_END();
	}

	FREE_FILE(aFileList);
}

//=========================================================
//=========================================================
// Busters Gamerules
//=========================================================
//=========================================================

#define EGON_BUSTING_TIME 10

bool IsBustingGame()
{
	return sv_busters.value == 1;
}

bool IsPlayerBusting(CBaseEntity* pPlayer)
{
	if (!pPlayer || !pPlayer->IsPlayer() || !IsBustingGame())
		return false;

	return ((CBasePlayer*)pPlayer)->HasPlayerItemFromID(WEAPON_EGON);
}

bool BustingCanHaveItem(CBasePlayer* pPlayer, CBaseEntity* pItem)
{
	bool bIsWeaponOrAmmo = false;

	if (strstr(STRING(pItem->pev->classname), "weapon_") || strstr(STRING(pItem->pev->classname), "ammo_"))
	{
		bIsWeaponOrAmmo = true;
	}

	// Busting players can't have ammo nor weapons
	if (IsPlayerBusting(pPlayer) && bIsWeaponOrAmmo)
		return false;

	return true;
}

//=========================================================
CMultiplayBusters::CMultiplayBusters()
{
	m_flEgonBustingCheckTime = -1;
}

//=========================================================
void CMultiplayBusters::Think()
{
	CheckForEgons();

	CHalfLifeMultiplay::Think();
}

//=========================================================
int CMultiplayBusters::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	// If the attacker is busting, they get a point per kill
	if (IsPlayerBusting(pAttacker))
		return 1;

	// If the victim is busting, then the attacker gets a point
	if (IsPlayerBusting(pKilled))
		return 2;

	return 0;
}

//=========================================================
void CMultiplayBusters::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	if (IsPlayerBusting(pVictim))
	{
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "The Buster is dead!!");

		// Reset egon check time
		m_flEgonBustingCheckTime = -1;

		CBasePlayer* peKiller = NULL;
		CBaseEntity* ktmp = CBaseEntity::Instance(pKiller);

		if (ktmp && (ktmp->Classify() == CLASS_PLAYER))
		{
			peKiller = (CBasePlayer*)ktmp;
		}
		else if (ktmp && (ktmp->Classify() == CLASS_VEHICLE))
		{
			CBasePlayer* pDriver = ((CFuncVehicle*)ktmp)->m_pDriver;

			if (pDriver != NULL)
			{
				peKiller = pDriver;
				ktmp = pDriver;
				pKiller = pDriver->pev;
			}
		}

		if (peKiller)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s has has killed the Buster!\n", STRING(peKiller->pev->netname)));
		}

		pVictim->pev->renderfx = kRenderFxNone;
		pVictim->pev->rendercolor = g_vecZero;
		// pVictim->pev->effects &= ~EF_BRIGHTFIELD;
	}

	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
}

//=========================================================
void CMultiplayBusters::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor)
{
	// Only death notices that the Buster was involved in in Busting game mode
	if (!IsPlayerBusting(pVictim) && !IsPlayerBusting(CBaseEntity::Instance(pKiller)))
		return;

	CHalfLifeMultiplay::DeathNotice(pVictim, pKiller, pevInflictor);
}

//=========================================================
int CMultiplayBusters::WeaponShouldRespawn(CBasePlayerItem* pWeapon)
{
	if (pWeapon->m_iId == WEAPON_EGON)
		return GR_WEAPON_RESPAWN_NO;

	return CHalfLifeMultiplay::WeaponShouldRespawn(pWeapon);
}


//=========================================================
// CheckForEgons:
// Check to see if any player has an egon
// If they don't then get the lowest player on the scoreboard and give them one
// Then check to see if any weapon boxes out there has an egon, and delete it
//=========================================================
void CMultiplayBusters::CheckForEgons()
{
	if (m_flEgonBustingCheckTime <= 0.0f)
	{
		m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		return;
	}

	if (m_flEgonBustingCheckTime <= gpGlobals->time)
	{
		m_flEgonBustingCheckTime = -1.0f;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(i);

			// Someone is busting, no need to continue
			if (IsPlayerBusting(pPlayer))
				return;
		}

		int bBestFrags = 9999;
		CBasePlayer* pBestPlayer = NULL;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->pev->frags <= bBestFrags)
			{
				bBestFrags = pPlayer->pev->frags;
				pBestPlayer = pPlayer;
			}
		}

		if (pBestPlayer)
		{
			pBestPlayer->GiveNamedItem("weapon_egon");

			CBaseEntity* pEntity = NULL;

			// Find a weaponbox that includes an Egon, then destroy it
			while ((pEntity = UTIL_FindEntityByClassname(pEntity, "weaponbox")) != NULL)
			{
				CWeaponBox* pWeaponBox = (CWeaponBox*)pEntity;

				if (pWeaponBox)
				{
					CBasePlayerItem* pWeapon;

					for (int i = 0; i < MAX_ITEM_TYPES; i++)
					{
						pWeapon = pWeaponBox->m_rgpPlayerItems[i];

						while (pWeapon)
						{
							// There you are, bye box
							if (pWeapon->m_iId == WEAPON_EGON)
							{
								pWeaponBox->Kill();
								break;
							}

							pWeapon = pWeapon->m_pNext;
						}
					}
				}
			}
		}
	}
}

//=========================================================
bool CMultiplayBusters::CanHavePlayerItem(CBasePlayer* pPlayer, CBasePlayerItem* pItem)
{
	// Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem(pPlayer, pItem) == false)
	{
		return false;
	}

	return CHalfLifeMultiplay::CanHavePlayerItem(pPlayer, pItem);
}

//=========================================================
bool CMultiplayBusters::CanHaveItem(CBasePlayer* pPlayer, CItem* pItem)
{
	// Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem(pPlayer, pItem) == false)
	{
		return false;
	}

	return CHalfLifeMultiplay::CanHaveItem(pPlayer, pItem);
}

//=========================================================
void CMultiplayBusters::PlayerGotWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	if (pWeapon->m_iId == WEAPON_EGON)
	{
		pPlayer->RemoveAllItems(false);

		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Long live the new Buster!");
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s is busting!\n", STRING(pPlayer->pev->netname)));

		SetPlayerModel(pPlayer);

		pPlayer->pev->health = pPlayer->pev->max_health;
		pPlayer->pev->armorvalue = 100;

		pPlayer->pev->renderfx = kRenderFxGlowShell;
		pPlayer->pev->renderamt = 25;
		pPlayer->pev->rendercolor = Vector(0, 75, 250);

		CBasePlayerWeapon* pEgon = (CBasePlayerWeapon*)pWeapon;

		pEgon->m_iDefaultAmmo = 100;
		pPlayer->m_rgAmmo[pEgon->m_iPrimaryAmmoType] = pEgon->m_iDefaultAmmo;

		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "ivan");
	}
}

void CMultiplayBusters::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	SetPlayerModel(pPlayer);

	// Set preferences
	pPlayer->SetPrefsFromUserinfo(infobuffer);
}

void CMultiplayBusters::PlayerSpawn(CBasePlayer* pPlayer)
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);
	SetPlayerModel(pPlayer);
}

void CMultiplayBusters::SetPlayerModel(CBasePlayer* pPlayer)
{
	if (IsPlayerBusting(pPlayer))
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "ivan");
	}
	else
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "skeleton");
	}
}
