/***
*
*	Rocket Crowbar 2: the admin commands and the LCS entity scripting at the start of rc2.dll
*	(dlls/rc2/maintainent.cpp). Names ours.
*
****/

#pragma once

class CBaseEntity;
class CBasePlayer;

// Who a player is. RC2 used the WON id, which today's engines don't issue (every player reports
// the same value); we use the Steam ID instead (reference/DEVIATIONS.md). NULL for a bot or a LAN
// or not yet validated id.
const char* RC2_PlayerSteamId(edict_t* pEntity);
// A nonzero number per player for telling a new client in a slot from the same one back
// (CBasePlayer::InitNewClient): from the Steam ID, else the connection's user id
int RC2_PlayerClientKey(edict_t* pEntity);

// Admin rights
bool Admin_IsListenHost(edict_t* pEntity); // the listen server's host (entity 1 on a non-dedicated server)
int Admin_GetLevel(edict_t* pEntity);	   // the level adminprim.cfg gives the player's Steam ID; the listen host is 2
bool Admin_IsTempAdmin(edict_t* pEntity);  // the player's Steam ID is listed in sv_tempadmins

// A player by index, else the first whose name contains pszName
CBasePlayer* Admin_FindPlayer(const char* pszName);
// What the player pevAdmin aims at (an 8192-unit trace, its GetEffectedEnt; worldspawn
// excluded), else the player's selection (m_pentDebug); NULL when neither
CBaseEntity* Admin_GetAimedEntity(entvars_t* pevAdmin);

// The client command "rcadmin"
void Admin_Command(edict_t* pEntity);

// RC2's entity scripting (the alteration language, dlls/rc2/alteration.h has the expressions):
// run pszScript on pent once; create pszClassname and run it before and after the spawn; keep
// running it on pent every frame (a maintainer_ent); run the map's player script on a player.
// iFirstArg is the first token the script's alterations start at.
void LCS_RunOnEntity(edict_t* pent, const char* pszScript, int iFirstArg);
void LCS_CreateWithScript(const char* pszClassname, const char* pszScript, int iFirstArg);
void LCS_MaintainOnEntity(edict_t* pent, const char* pszScript, int iFirstArg);
void LCS_RunPlayerScript(edict_t* pent);

// The map's player script (an .lcs setspawncommand line; CWorld clears it) and the token its
// alterations start at
extern char g_szLCSPlayerScript[0x200];
extern int g_iLCSPlayerScriptFirstArg;

// The indices a p or e without a number means in an alteration expression: the entity the
// script runs for and the script's selection
extern int g_iLCSCallerIndex;
extern int g_iLCSSelectedIndex;
