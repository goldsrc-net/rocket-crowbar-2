/***
*
*	Rocket Crowbar 2: the freeze timer. Created with its victim as owner and the freeze's length
*	in dmg, it holds the victim (and, for a player, the players tied to him) still and blind to
*	his HUD's weapons until the time is up or he dies, and tells the clients to draw him frozen.
*	Reconstructed from rc2.dll, the run between flyingmonster.cpp and func_break.cpp
*	(0x41890-0x41e80); reference/dossiers/p2.md. The file name is ours.
*
*	OVERLOADS SOME ENTVARS:
*	  owner    the frozen entity
*	  dmg      how long (set by whoever creates it; 0.01 by default)
*	  dmgtime  when it thaws
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "UserMessages.h"
#include "freezetimer.h"

#define RC2_CLTE_FROZEN 0x2f // short entity: draw it frozen; RC2_CLTE_KILL-like 0xfe, short, byte 0x2f ends it (name ours)
#define RC2_CLTE_END 0xfe	 // short entity, byte the effect to end (name ours)
#define RC2_HOLD_FROZEN 2	 // the hold bit in CBasePlayer +0x648 (name ours)

// RC2's byte at CBaseEntity +0x66 (the third byte of the int at +0x64): the entity is held
static void SetHeld(CBaseEntity* pEnt, bool bHeld) // name ours
{
	pEnt->m_bFrozen = bHeld;
}

class CFreezeTimer : public CBaseEntity
{
public:
	void Spawn() override;

	void EXPORT StartFreeze();
	void EXPORT EntFrozen();

	bool OwnerGone();  // name ours
	void EndFreeze(); // name ours

	float m_flFreezeStart; // orig +0x70, name ours: never read here
};

LINK_ENTITY_TO_CLASS(freeze_timer, CFreezeTimer); // orig 0x41890

// orig 0x41900
void CFreezeTimer::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->dmg = 0.01;
	pev->dmgtime = -1.0;
	SetThink(&CFreezeTimer::StartFreeze);
	pev->nextthink = gpGlobals->time;
}

// orig 0x41960
bool CFreezeTimer::OwnerGone()
{
	if (!UTIL_IsValidEntity(pev->owner))
	{
		UTIL_Remove(this);
		return true;
	}
	return false;
}

static void FreezePlayer(CBasePlayer* pPlayer, bool bTied) // name ours
{
	pPlayer->AddPhysFlag2Source(RC2_HOLD_FROZEN);
	pPlayer->m_fShieldOn = false;
	if (bTied)
		SetHeld(pPlayer, true);
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS;
}

// orig 0x41990
void CFreezeTimer::StartFreeze()
{
	if (OwnerGone())
		return;

	// Original bug, kept: no check that the owner has an instance
	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	if (!pOwner->IsAlive() || pOwner->pev->deadflag != DEAD_NO || pOwner->pev->health <= 0)
	{
		UTIL_Remove(this);
		return;
	}

	SetHeld(pOwner, true);
	if (pOwner->IsPlayer())
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pOwner;
		FreezePlayer(pPlayer, false);

		int iTied[2] = {pPlayer->m_iShoulderRider[0], pPlayer->m_iShoulderRider[1]};
		for (int i = 0; i < 2; i++)
		{
			if (iTied[i] == 0)
				continue;
			CBasePlayer* pTied = (CBasePlayer*)UTIL_PlayerByIndex(iTied[i]);
			if (!pTied)
				continue;
			FreezePlayer(pTied, true);

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
			WRITE_BYTE(RC2_CLTE_FROZEN);
			WRITE_SHORT(ENTINDEX(pTied->edict()));
			MESSAGE_END();
		}
	}

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_FROZEN);
	WRITE_SHORT(ENTINDEX(pev->owner));
	MESSAGE_END();

	// Original oddity, kept: silences the timer's own channels, not the frozen entity's
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 1.0, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1.0, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1.0, ATTN_NORM, 0, 100);

	pev->dmgtime = pev->dmg + gpGlobals->time;
	SetThink(&CFreezeTimer::EntFrozen);
	pev->nextthink = gpGlobals->time + 0.01;
	m_flFreezeStart = gpGlobals->time;
}

// orig 0x41c40
void CFreezeTimer::EntFrozen()
{
	if (OwnerGone())
		return;

	entvars_t* pevOwner = VARS(pev->owner);
	if (pevOwner->deadflag == DEAD_NO && pevOwner->health > 0)
	{
		pev->nextthink = gpGlobals->time;
		if (gpGlobals->time <= pev->dmgtime)
			return;
	}
	EndFreeze();
}

static void ThawPlayer(CBasePlayer* pPlayer, bool bTied) // name ours
{
	pPlayer->RemovePhysFlag2Source(RC2_HOLD_FROZEN);
	if (bTied)
		SetHeld(pPlayer, false);
	pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
}

// orig 0x41cc0
void CFreezeTimer::EndFreeze()
{
	if (!UTIL_IsValidEntity(pev->owner))
	{
		SetThink(NULL);
		UTIL_Remove(this);
		return;
	}

	// Original bug, kept: no check that the owner has an instance
	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	SetHeld(pOwner, false);
	if (pOwner->IsPlayer())
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pOwner;
		pPlayer->RemovePhysFlag2Source(RC2_HOLD_FROZEN);
		pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;

		int iTied[2] = {pPlayer->m_iShoulderRider[0], pPlayer->m_iShoulderRider[1]};
		for (int i = 0; i < 2; i++)
		{
			if (iTied[i] == 0)
				continue;
			CBasePlayer* pTied = (CBasePlayer*)UTIL_PlayerByIndex(iTied[i]);
			if (!pTied)
				continue;
			ThawPlayer(pTied, true);

			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(RC2_CLTE_END);
			WRITE_SHORT(ENTINDEX(pTied->edict()));
			WRITE_BYTE(RC2_CLTE_FROZEN);
			MESSAGE_END();
		}
	}

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_END);
	WRITE_SHORT(ENTINDEX(pev->owner));
	WRITE_BYTE(RC2_CLTE_FROZEN);
	MESSAGE_END();

	SetThink(NULL);
	UTIL_Remove(this);
}

// orig 0x41e80, name ours: thaw pent now (every freeze timer on it)
void FreezeTimer_Thaw(edict_t* pent)
{
	for (CBaseEntity* pTimer = UTIL_FindEntityByClassname(NULL, "freeze_timer"); pTimer; pTimer = UTIL_FindEntityByClassname(pTimer, "freeze_timer"))
	{
		if (pTimer->pev->owner == pent)
			((CFreezeTimer*)pTimer)->EndFreeze();
	}
}
