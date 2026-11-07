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
#include "items.h"
#include "UserMessages.h"

//=========================================================
//=========================================================
CHalfLifeRules::CHalfLifeRules()
{
	SERVER_COMMAND( "exec spserver.cfg\n" );

	RefreshSkillData();
}

//=========================================================
//=========================================================
// orig 0xca4c0
void CHalfLifeRules::Think()
{
}

//=========================================================
//=========================================================
// orig 0xca4d0
bool CHalfLifeRules::IsMultiplayer()
{
	return false;
}

//=========================================================
//=========================================================
// orig 0xca4e0
bool CHalfLifeRules::IsDeathmatch()
{
	return false;
}

//=========================================================
//=========================================================
// orig 0xca4f0
bool CHalfLifeRules::IsCoOp()
{
	return false;
}


//=========================================================
//=========================================================
bool CHalfLifeRules::FShouldSwitchWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	if (!pPlayer->m_pActiveItem)
	{
		// player doesn't have an active item!
		return true;
	}

	if (!pPlayer->m_pActiveItem->CanHolster())
	{
		return false;
	}

	return true;
}

//=========================================================
//=========================================================
// orig 0xca530
bool CHalfLifeRules::GetNextBestWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pCurrentWeapon, bool alwaysSearch)
{
	//If this is an exhaustible weapon and it's out of ammo, always try to switch even in singleplayer.
	if (alwaysSearch || ((pCurrentWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE) != 0 && pCurrentWeapon->PrimaryAmmoIndex() != -1 && pPlayer->m_rgAmmo[pCurrentWeapon->PrimaryAmmoIndex()] == 0))
	{
		return CGameRules::GetNextBestWeapon(pPlayer, pCurrentWeapon);
	}

	return false;
}

//=========================================================
//=========================================================
// orig 0xca540
bool CHalfLifeRules::ClientConnected(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	return true;
}

// orig 0xca550
void CHalfLifeRules::InitHUD(CBasePlayer* pl)
{
}

//=========================================================
//=========================================================
// orig 0xca560
void CHalfLifeRules::ClientDisconnected(edict_t* pClient)
{
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlPlayerFallDamage(CBasePlayer* pPlayer)
{
	// subtract off the speed at which a player is allowed to fall without being hurt,
	// so damage will be based on speed beyond that, not the entire fall
	pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

//=========================================================
//=========================================================
// orig 0xca590
void CHalfLifeRules::PlayerSpawn(CBasePlayer* pPlayer)
{
}

//=========================================================
//=========================================================
// orig 0xca5a0
bool CHalfLifeRules::AllowAutoTargetCrosshair()
{
	return (g_iSkillLevel == SKILL_EASY);
}

//=========================================================
//=========================================================
// orig 0xca5b0
void CHalfLifeRules::PlayerThink(CBasePlayer* pPlayer)
{
}


//=========================================================
//=========================================================
// orig 0xca5c0
bool CHalfLifeRules::FPlayerCanRespawn(CBasePlayer* pPlayer)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0xca5d0
float CHalfLifeRules::FlPlayerSpawnTime(CBasePlayer* pPlayer)
{
	return gpGlobals->time; //now!
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
// orig 0xca5e0
int CHalfLifeRules::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	return 1;
}

//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
// orig 0xca5f0
void CHalfLifeRules::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
}

//=========================================================
// Deathnotice
//=========================================================
// orig 0xca600
void CHalfLifeRules::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
// orig 0xca610
void CHalfLifeRules::PlayerGotWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
// orig 0xca620
float CHalfLifeRules::FlWeaponRespawnTime(CBasePlayerItem* pWeapon)
{
	return -1;
}

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
// orig 0xca630
float CHalfLifeRules::FlWeaponTryRespawn(CBasePlayerItem* pWeapon)
{
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeRules::VecWeaponRespawnSpot(CBasePlayerItem* pWeapon)
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
// orig 0xca670
int CHalfLifeRules::WeaponShouldRespawn(CBasePlayerItem* pWeapon)
{
	return GR_WEAPON_RESPAWN_NO;
}

//=========================================================
//=========================================================
// orig 0xca680
bool CHalfLifeRules::CanHaveItem(CBasePlayer* pPlayer, CItem* pItem)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0xca690
void CHalfLifeRules::PlayerGotItem(CBasePlayer* pPlayer, CItem* pItem)
{
}

//=========================================================
//=========================================================
// orig 0xca6a0
int CHalfLifeRules::ItemShouldRespawn(CItem* pItem)
{
	return GR_ITEM_RESPAWN_NO;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
// orig 0xca6b0
float CHalfLifeRules::FlItemRespawnTime(CItem* pItem)
{
	return -1;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeRules::VecItemRespawnSpot(CItem* pItem)
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
// orig 0xca6f0
bool CHalfLifeRules::IsAllowedToSpawn(CBaseEntity* pEntity)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0xca700
void CHalfLifeRules::PlayerGotAmmo(CBasePlayer* pPlayer, char* szName, int iCount)
{
}

//=========================================================
//=========================================================
// orig 0xca710
int CHalfLifeRules::AmmoShouldRespawn(CBasePlayerAmmo* pAmmo)
{
	return GR_AMMO_RESPAWN_NO;
}

//=========================================================
//=========================================================
// orig 0xca720
float CHalfLifeRules::FlAmmoRespawnTime(CBasePlayerAmmo* pAmmo)
{
	return -1;
}

//=========================================================
//=========================================================
Vector CHalfLifeRules::VecAmmoRespawnSpot(CBasePlayerAmmo* pAmmo)
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
// orig 0xca760
float CHalfLifeRules::FlHealthChargerRechargeTime()
{
	return 0; // don't recharge
}

//=========================================================
//=========================================================
// orig 0xca770
int CHalfLifeRules::DeadPlayerWeapons(CBasePlayer* pPlayer)
{
	return GR_PLR_DROP_GUN_NO;
}

//=========================================================
//=========================================================
// orig 0xca780
int CHalfLifeRules::DeadPlayerAmmo(CBasePlayer* pPlayer)
{
	return GR_PLR_DROP_AMMO_NO;
}

//=========================================================
//=========================================================
// orig 0xca790
int CHalfLifeRules::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
	// why would a single player in half life need this?
	return GR_NOTTEAMMATE;
}

//=========================================================
//=========================================================
// orig 0xca7a0
bool CHalfLifeRules::FAllowMonsters()
{
	return true;
}
