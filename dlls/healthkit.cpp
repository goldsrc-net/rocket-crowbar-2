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
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "items.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "rc2prefs.h"

class CHealthKit : public CItem
{
	void Spawn() override;
	void Precache() override;
	bool MyTouch(CBasePlayer* pPlayer) override;

	/*
	int		Save( CSave &save ) override; 
	int		Restore( CRestore &restore ) override;
	
	static	TYPEDESCRIPTION m_SaveData[];
*/
};


LINK_ENTITY_TO_CLASS(item_healthkit, CHealthKit); // orig 0x5ee80

/*
TYPEDESCRIPTION	CHealthKit::m_SaveData[] = 
{

};


IMPLEMENT_SAVERESTORE( CHealthKit, CItem);
*/

// orig 0x5eef0
void CHealthKit::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_medkit.mdl");

	CItem::Spawn();
}

// orig 0x5ef20
void CHealthKit::Precache()
{
	PRECACHE_MODEL("models/w_medkit.mdl");
	PRECACHE_SOUND("items/smallmedkit1.wav");
}

// orig 0x5ef40
bool CHealthKit::MyTouch(CBasePlayer* pPlayer)
{
	if (pPlayer->pev->deadflag != DEAD_NO)
	{
		return false;
	}

	// Rocket Crowbar 2: some of the creatures a player can be turned into can't use one
	switch (pPlayer->m_iTransformType)
	{
	case 2:
	case 3:
	case 4:
	case 5:
		return false;
	}

	// Rocket Crowbar 2: a player at full health can still take a kit, up to
	// [Attributes] BaseHealthKitSuperFraction of max_health (truncated before the multiply)
	bool bTaken = pPlayer->TakeHealth(gSkillData.healthkitCapacity, DMG_GENERIC);
	if (!bTaken && (int)pPlayer->pev->max_health * RC2Pref(RC2P_Attributes_BaseHealthKitSuperFraction).f > pPlayer->pev->health)
	{
		const float flCap = (int)pPlayer->pev->max_health * RC2Pref(RC2P_Attributes_BaseHealthKitSuperFraction).f;
		pPlayer->pev->health += gSkillData.healthkitCapacity;
		if (flCap < pPlayer->pev->health)
			pPlayer->pev->health = flCap;
		bTaken = true;
	}

	if (bTaken)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev);
		WRITE_STRING(STRING(pev->classname));
		MESSAGE_END();

		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/smallmedkit1.wav", 1, ATTN_NORM);

		//TODO: incorrect check here, but won't respawn due to respawn delay being -1 in singleplayer
		if (0 != g_pGameRules->ItemShouldRespawn(this))
		{
			Respawn();
		}
		else
		{
			UTIL_Remove(this);
		}

		return true;
	}

	return false;
}



//-------------------------------------------------------------
// Wall mounted health kit
//-------------------------------------------------------------
class CWallHealth : public CBaseToggle
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT Off();
	void EXPORT Recharge();
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	// orig 0x5f210
	int ObjectCaps() override { return (CBaseToggle::ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	float m_flNextCharge;
	int m_iReactivate; // DeathMatch Delay until reactvated
	int m_iJuice;
	int m_iOn; // 0 = off, 1 = startup, 2 = going
	float m_flSoundTime;
};

TYPEDESCRIPTION CWallHealth::m_SaveData[] =
	{
		DEFINE_FIELD(CWallHealth, m_flNextCharge, FIELD_TIME),
		DEFINE_FIELD(CWallHealth, m_iReactivate, FIELD_INTEGER),
		DEFINE_FIELD(CWallHealth, m_iJuice, FIELD_INTEGER),
		DEFINE_FIELD(CWallHealth, m_iOn, FIELD_INTEGER),
		DEFINE_FIELD(CWallHealth, m_flSoundTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CWallHealth, CBaseEntity); // orig 0x5f140 (Save), orig 0x5f170 (Restore)

LINK_ENTITY_TO_CLASS(func_healthcharger, CWallHealth); // orig 0x5f1a0


// orig 0x5f220
bool CWallHealth::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "style") ||
		FStrEq(pkvd->szKeyName, "height") ||
		FStrEq(pkvd->szKeyName, "value1") ||
		FStrEq(pkvd->szKeyName, "value2") ||
		FStrEq(pkvd->szKeyName, "value3"))
	{
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(pkvd->szValue);
		return true;
	}

	return CBaseToggle::KeyValue(pkvd);
}

// orig 0x5f3a0
void CWallHealth::Spawn()
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin); // set size and link into world
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model));
	m_iJuice = gSkillData.healthchargerCapacity;
	pev->frame = 0;
}

// orig 0x5f430
void CWallHealth::Precache()
{
	PRECACHE_SOUND("items/medshot4.wav");
	PRECACHE_SOUND("items/medshotno1.wav");
	PRECACHE_SOUND("items/medcharge4.wav");
}


// orig 0x5f460
void CWallHealth::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Make sure that we have a caller
	if (!pActivator)
		return;
	// if it's not a player, ignore
	if (!pActivator->IsActivePlayer())
		return;

	auto player = static_cast<CBasePlayer*>(pActivator);

	// Rocket Crowbar 2: some of the creatures a player can be turned into can't use one
	switch (player->m_iTransformType)
	{
	case 2:
	case 3:
	case 4:
	case 5:
		return;
	}

	// if there is no juice left, turn it off
	if (m_iJuice <= 0)
	{
		pev->frame = 1;
		Off();
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise
	if ((m_iJuice <= 0) || !player->HasSuit())
	{
		if (m_flSoundTime <= gpGlobals->time)
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshotno1.wav", 1.0, ATTN_NORM);
		}
		return;
	}

	pev->nextthink = pev->ltime + 0.25;
	SetThink(&CWallHealth::Off);

	// Time to recharge yet?

	if (m_flNextCharge >= gpGlobals->time)
		return;

	// Play the on sound or the looping charging sound
	if (0 == m_iOn)
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshot4.wav", 1.0, ATTN_NORM);
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->time))
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "items/medcharge4.wav", 1.0, ATTN_NORM);
	}


	// charge the player
	if (player->TakeHealth(1, DMG_GENERIC))
	{
		m_iJuice--;
	}

	// Rocket Crowbar 2: past max_health, up to [Attributes] BaseHealthWallSuperFraction of it.
	// Runs on the same tick as a TakeHealth that succeeded too, so a player just short of full
	// gets 2 points for 2 juice.
	if ((int)(RC2Pref(RC2P_Attributes_BaseHealthWallSuperFraction).f * player->pev->max_health) > player->pev->health)
	{
		player->pev->health += 1;
		const int iCap = (int)(RC2Pref(RC2P_Attributes_BaseHealthWallSuperFraction).f * player->pev->max_health);
		if (iCap < player->pev->health)
			player->pev->health = iCap;
		m_iJuice--;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CWallHealth::Recharge()
{
	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshot4.wav", 1.0, ATTN_NORM);
	m_iJuice = gSkillData.healthchargerCapacity;
	pev->frame = 0;
	SetThink(&CWallHealth::SUB_DoNothing);
}

void CWallHealth::Off()
{
	// Stop looping sound.
	if (m_iOn > 1)
		STOP_SOUND(ENT(pev), CHAN_STATIC, "items/medcharge4.wav");

	m_iOn = 0;

	if ((0 == m_iJuice) && ((m_iReactivate = g_pGameRules->FlHealthChargerRechargeTime()) > 0))
	{
		pev->nextthink = pev->ltime + m_iReactivate;
		SetThink(&CWallHealth::Recharge);
	}
	else
		SetThink(&CWallHealth::SUB_DoNothing);
}
