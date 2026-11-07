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

class CAirtank : public CGrenade
{
	void Spawn() override;
	void Precache() override;
	void EXPORT TankThink();
	void EXPORT TankTouch(CBaseEntity* pOther);
	int BloodColor() override { return DONT_BLEED; } // orig 0x9050 (stock)
	void Killed(entvars_t* pevAttacker, int iGib) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	bool m_state;
};


LINK_ENTITY_TO_CLASS(item_airtank, CAirtank); // orig 0x8fd0 (stock)
TYPEDESCRIPTION CAirtank::m_SaveData[] =
	{
		DEFINE_FIELD(CAirtank, m_state, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CAirtank, CGrenade); // orig 0x9060 Save, orig 0x9090 Restore (stock)


void CAirtank::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_oxygen.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 36));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CAirtank::TankTouch);
	SetThink(&CAirtank::TankThink);

	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_YES;
	pev->health = 20;
	pev->dmg = 50;
	m_state = true;
}

// orig 0x91b0 (stock)
void CAirtank::Precache()
{
	PRECACHE_MODEL("models/w_oxygen.mdl");
	PRECACHE_SOUND("doors/aliendoor3.wav");
}


void CAirtank::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->owner = ENT(pevAttacker);

	// UNDONE: this should make a big bubble cloud, not an explosion

	Explode(pev->origin, Vector(0, 0, -1));
}


void CAirtank::TankThink()
{
	// Fire trigger
	m_state = true;
	SUB_UseTargets(this, USE_TOGGLE, 0);
}


// orig 0x9250. RC2: an active player (IsActivePlayer) where SDK 2.3 asked IsPlayer.
void CAirtank::TankTouch(CBaseEntity* pOther)
{
	if (!pOther->IsActivePlayer())
		return;

	if (!m_state)
	{
		// "no oxygen" sound
		EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 1.0, ATTN_NORM);
		return;
	}

	// give player 12 more seconds of air
	pOther->pev->air_finished = gpGlobals->time + 12;

	// suit recharge sound
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "doors/aliendoor3.wav", 1.0, ATTN_NORM);

	// recharge airtank in 30 seconds
	pev->nextthink = gpGlobals->time + 30;
	m_state = false;
	SUB_UseTargets(this, USE_TOGGLE, 1);
}
