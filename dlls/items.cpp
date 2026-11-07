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
/*

===== items.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "skill.h"
#include "items.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "game.h"
#include "p4_needs.h"

class CWorldItem : public CBaseEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Spawn() override;
	int m_iType;
};

LINK_ENTITY_TO_CLASS(world_items, CWorldItem); // orig 0x6c250

// orig 0x6c2c0
bool CWorldItem::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iType = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

// orig 0x6c330
void CWorldItem::Spawn()
{
	CBaseEntity* pEntity = NULL;

	switch (m_iType)
	{
	case 44: // ITEM_BATTERY:
		pEntity = CBaseEntity::Create("item_battery", pev->origin, pev->angles);
		break;
	case 42: // ITEM_ANTIDOTE:
		pEntity = CBaseEntity::Create("item_antidote", pev->origin, pev->angles);
		break;
	case 43: // ITEM_SECURITY:
		pEntity = CBaseEntity::Create("item_security", pev->origin, pev->angles);
		break;
	case 45: // ITEM_SUIT:
		pEntity = CBaseEntity::Create("item_suit", pev->origin, pev->angles);
		break;
	}

	if (!pEntity)
	{
		ALERT(at_console, "unable to create world_item %d\n", m_iType);
	}
	else
	{
		pEntity->pev->target = pev->target;
		pEntity->pev->targetname = pev->targetname;
		pEntity->pev->spawnflags = pev->spawnflags;
	}

	REMOVE_ENTITY(edict());
}


// orig 0x6c410
void CItem::Spawn()
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	SetTouch(&CItem::ItemTouch);

	// Rocket Crowbar 2: items no longer drop to the floor (nor get removed for falling out of
	// the level); with mp_random set, multiplayer swaps them for a random one
	if (g_pGameRules->IsMultiplayer() && mp_random.value != 0)
		ReplaceWithRandomItem(this);
}

// orig 0x6c4c0
void CItem::ItemTouch(CBaseEntity* pOther)
{
	// if it's not a player, ignore
	if (!pOther->IsActivePlayer()) // Rocket Crowbar 2: IsActivePlayer
	{
		return;
	}

	CBasePlayer* pPlayer = (CBasePlayer*)pOther;

	// ok, a player is touching this item, but can he have it?
	if (!g_pGameRules->CanHaveItem(pPlayer, this))
	{
		// no? Ignore the touch.
		return;
	}

	if (MyTouch(pPlayer))
	{
		SUB_UseTargets(pOther, USE_TOGGLE, 0);
		SetTouch(NULL);

		// player grabbed the item.
		g_pGameRules->PlayerGotItem(pPlayer, this);
		if (g_pGameRules->ItemShouldRespawn(this) == GR_ITEM_RESPAWN_YES)
		{
			Respawn();
		}
		else
		{
			UTIL_Remove(this);
		}
	}
	else if (gEvilImpulse101)
	{
		UTIL_Remove(this);
	}
}

// orig 0x6c560
CBaseEntity* CItem::Respawn()
{
	SetTouch(NULL);
	pev->effects |= EF_NODRAW;

	UTIL_SetOrigin(pev, g_pGameRules->VecItemRespawnSpot(this)); // blip to whereever you should respawn.

	SetThink(&CItem::Materialize);
	pev->nextthink = g_pGameRules->FlItemRespawnTime(this);
	return this;
}

// orig 0x6c5d0
void CItem::Materialize()
{
	SetTouch(&CItem::ItemTouch);

	if ((pev->effects & EF_NODRAW) != 0)
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150);
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	// Rocket Crowbar 2: mp_random above 2 swaps it for a random item on every respawn too
	if (g_pGameRules->IsMultiplayer() && mp_random.value > 2)
		ReplaceWithRandomItem(this);
}

#define SF_SUIT_SHORTLOGON 0x0001

class CItemSuit : public CItem
{
	// orig 0x6c6d0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_suit.mdl");
		CItem::Spawn();
	}
	// orig 0x6c700
	void Precache() override
	{
		PRECACHE_MODEL("models/w_suit.mdl");
	}
	// orig 0x6c710
	bool MyTouch(CBasePlayer* pPlayer) override
	{
		if (pPlayer->HasSuit())
			return false;

		// Rocket Crowbar 2: not while turned into a creature
		if (0 != pPlayer->m_iTransformType)
			return false;

		if ((pev->spawnflags & SF_SUIT_SHORTLOGON) != 0)
			EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_A0"); // short version of suit logon,
		else
			EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_AAx"); // long version of suit logon

		pPlayer->SetHasSuit(true);
		return true;
	}
};

LINK_ENTITY_TO_CLASS(item_suit, CItemSuit); // orig 0x6c660



class CItemBattery : public CItem
{
	// orig 0x6c800
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_battery.mdl");
		CItem::Spawn();
	}
	// orig 0x6c830
	void Precache() override
	{
		PRECACHE_MODEL("models/w_battery.mdl");
		PRECACHE_SOUND("items/gunpickup2.wav");
	}
	// orig 0x6c850
	bool MyTouch(CBasePlayer* pPlayer) override
	{
		if (pPlayer->pev->deadflag != DEAD_NO)
		{
			return false;
		}

		// Rocket Crowbar 2: not while turned into a creature
		if (0 != pPlayer->m_iTransformType)
		{
			return false;
		}

		// Rocket Crowbar 2: the cap is the player's attribute 6 (when over 100) plus attribute 7
		int iMaxArmor = MAX_NORMAL_BATTERY;
		if (RC2_PlayerAttribute(6, ENTINDEX(pPlayer->edict())) > 100)
			iMaxArmor = (int)RC2_PlayerAttribute(6, ENTINDEX(pPlayer->edict()));
		iMaxArmor = (int)(RC2_PlayerAttribute(7, ENTINDEX(pPlayer->edict())) + iMaxArmor);

		if ((pPlayer->pev->armorvalue < (float)iMaxArmor) &&
			pPlayer->HasSuit())
		{
			int pct;
			char szcharge[64];

			pPlayer->pev->armorvalue += gSkillData.batteryCapacity;
			pPlayer->pev->armorvalue = V_min(pPlayer->pev->armorvalue, (float)iMaxArmor);

			EMIT_SOUND(pPlayer->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);

			MESSAGE_BEGIN(MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev);
			WRITE_STRING(STRING(pev->classname));
			MESSAGE_END();


			// Suit reports new power level
			// For some reason this wasn't working in release build -- round it.
			pct = (int)((float)(pPlayer->pev->armorvalue * 100.0) * (1.0 / iMaxArmor) + 0.5);
			pct = (pct / 5);
			if (pct > 0)
				pct--;

			sprintf(szcharge, "!HEV_%1dP", pct);

			//EMIT_SOUND_SUIT(ENT(pev), szcharge);
			pPlayer->SetSuitUpdate(szcharge, false, SUIT_NEXT_IN_30SEC);
			return true;
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery); // orig 0x6c790


class CItemAntidote : public CItem
{
	// orig 0x6cab0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_antidote.mdl");
		CItem::Spawn();
	}
	// orig 0x6cae0
	void Precache() override
	{
		PRECACHE_MODEL("models/w_antidote.mdl");
	}
	// orig 0x6caf0
	bool MyTouch(CBasePlayer* pPlayer) override
	{
		// Rocket Crowbar 2: not while turned into a creature
		if (0 != pPlayer->m_iTransformType)
			return false;

		pPlayer->SetSuitUpdate("!HEV_DET4", false, SUIT_NEXT_IN_1MIN);

		pPlayer->m_rgItems[ITEM_ANTIDOTE] += 1;
		return true;
	}
};

LINK_ENTITY_TO_CLASS(item_antidote, CItemAntidote); // orig 0x6ca40


class CItemSecurity : public CItem
{
	// orig 0x6cba0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_security.mdl");
		CItem::Spawn();
	}
	// orig 0x6cbd0
	void Precache() override
	{
		PRECACHE_MODEL("models/w_security.mdl");
	}
	bool MyTouch(CBasePlayer* pPlayer) override
	{
		pPlayer->m_rgItems[ITEM_SECURITY] += 1;
		return true;
	}
};

LINK_ENTITY_TO_CLASS(item_security, CItemSecurity); // orig 0x6cb30

class CItemLongJump : public CItem
{
	// orig 0x6cc70
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_longjump.mdl");
		CItem::Spawn();
	}
	// orig 0x6cca0
	void Precache() override
	{
		PRECACHE_MODEL("models/w_longjump.mdl");
	}
	// orig 0x6ccb0
	bool MyTouch(CBasePlayer* pPlayer) override
	{
		if (pPlayer->m_fLongJump)
		{
			return false;
		}

		// Rocket Crowbar 2: not while turned into a creature
		if (0 != pPlayer->m_iTransformType)
		{
			return false;
		}

		if (pPlayer->HasSuit())
		{
			pPlayer->m_fLongJump = true; // player now has longjump module

			g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "slj", "1");

			MESSAGE_BEGIN(MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev);
			WRITE_STRING(STRING(pev->classname));
			MESSAGE_END();

			EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_A1"); // Play the longjump sound UNDONE: Kelly? correct sound?
			return true;
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS(item_longjump, CItemLongJump); // orig 0x6cc00
