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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "shake.h"
#include "UserMessages.h"

// orig 0x94240 (in player.cpp in SDK 2.3). RC2's set, in its order and with its sizes: SDK 2.3's
// with SelAmmo, CurWeapon 4 bytes, ScoreInfo 6, AmmoX 3, and no StatusText, StatusValue or
// Weapons, then 19 of its own. The names of RC2's gmsg variables are ours.
void LinkUserMessages()
{
	// Already taken care of?
	if (0 != gmsgSelAmmo)
	{
		return;
	}

	gmsgSelAmmo = REG_USER_MSG("SelAmmo", 4);
	gmsgCurWeapon = REG_USER_MSG("CurWeapon", 4);
	gmsgGeigerRange = REG_USER_MSG("Geiger", 1);
	gmsgFlashlight = REG_USER_MSG("Flashlight", 2);
	gmsgFlashBattery = REG_USER_MSG("FlashBat", 1);
	gmsgHealth = REG_USER_MSG("Health", 2);
	gmsgDamage = REG_USER_MSG("Damage", 12);
	gmsgBattery = REG_USER_MSG("Battery", 2);
	gmsgTrain = REG_USER_MSG("Train", 1);
	gmsgHudText = REG_USER_MSG("HudText", -1);
	gmsgSayText = REG_USER_MSG("SayText", -1);
	gmsgTextMsg = REG_USER_MSG("TextMsg", -1);
	gmsgWeaponList = REG_USER_MSG("WeaponList", -1);
	gmsgResetHUD = REG_USER_MSG("ResetHUD", 1); // called every respawn
	gmsgInitHUD = REG_USER_MSG("InitHUD", 0);	// called every time a new player joins the server
	gmsgShowGameTitle = REG_USER_MSG("GameTitle", 1);
	gmsgDeathMsg = REG_USER_MSG("DeathMsg", -1);
	gmsgScoreInfo = REG_USER_MSG("ScoreInfo", 6);
	gmsgTeamInfo = REG_USER_MSG("TeamInfo", -1);   // sets the name of a player's team
	gmsgTeamScore = REG_USER_MSG("TeamScore", -1); // sets the score of a team on the scoreboard
	gmsgGameMode = REG_USER_MSG("GameMode", 1);
	gmsgMOTD = REG_USER_MSG("MOTD", -1);
	gmsgServerName = REG_USER_MSG("ServerName", -1);
	gmsgAmmoPickup = REG_USER_MSG("AmmoPickup", 2);
	gmsgWeapPickup = REG_USER_MSG("WeapPickup", 1);
	gmsgItemPickup = REG_USER_MSG("ItemPickup", -1);
	gmsgHideWeapon = REG_USER_MSG("HideWeapon", 1);
	gmsgSetFOV = REG_USER_MSG("SetFOV", 1);
	gmsgShowMenu = REG_USER_MSG("ShowMenu", -1);
	gmsgShake = REG_USER_MSG("ScreenShake", sizeof(ScreenShake));
	gmsgFade = REG_USER_MSG("ScreenFade", sizeof(ScreenFade));
	gmsgAmmoX = REG_USER_MSG("AmmoX", 3);
	gmsgTeamNames = REG_USER_MSG("TeamNames", -1);

	// Rocket Crowbar 2's own
	gmsgEnergy = REG_USER_MSG("Energy", 1);
	gmsgEIcons = REG_USER_MSG("EIcons", 1);
	gmsgAttrConf = REG_USER_MSG("AttrConf", 0);
	gmsgSetAttr = REG_USER_MSG("SetAttr", -1);
	gmsgThisClient = REG_USER_MSG("ThisClient", 1);
	gmsgViewMode = REG_USER_MSG("ViewMode", 1);
	gmsgScoreMode = REG_USER_MSG("ScoreMode", 1);
	gmsgVGUIDebugOn = REG_USER_MSG("VGUIDebugOn", 1);
	gmsgDBGCounts = REG_USER_MSG("DBGCounts", -1);
	gmsgDBGEntInfo = REG_USER_MSG("DBGEntInfo", -1);
	gmsgCLTE = REG_USER_MSG("CLTE", -1);
	gmsgPullerCtrl = REG_USER_MSG("PullerCtrl", -1);
	gmsgWeaponSync = REG_USER_MSG("WeaponSync", -1);
	gmsgRCCFG = REG_USER_MSG("RCCFG", -1);
	gmsgTformStat = REG_USER_MSG("TformStat", -1);
	gmsgPlrStat = REG_USER_MSG("PlrStat", 2);
	gmsgRaceStat = REG_USER_MSG("RaceStat", -1);
	gmsgVoteBegin = REG_USER_MSG("VoteBegin", -1);
	gmsgVoteUpdate = REG_USER_MSG("VoteUpdate", 3);

	// Not in RC2: halflife-updated code still sends these, so they stay registered (after
	// RC2's, so its messages keep their numbers) until nothing does. reference/DEVIATIONS.md
	gmsgStatusText = REG_USER_MSG("StatusText", -1);
	gmsgStatusValue = REG_USER_MSG("StatusValue", 3);
	gmsgWeapons = REG_USER_MSG("Weapons", 8);
}
