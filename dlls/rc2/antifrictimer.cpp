/***
*
*	Rocket Crowbar 2: antifric_timer (CAntiFricTimer), what the antifriction grenade leaves on
*	an entity it hits: for pev->dmg seconds the owner is marked frictionless (the byte at orig
*	+0x67 of CBaseEntity, a player's physics flag 8), and the clients draw an effect on it (CLTE
*	0x35). The run between SDK 2.3's animation.cpp and apache.cpp (orig 0xa600-0xaa86).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

extern int gmsgCLTE; // "CLTE" (dlls/rc2/maintainent.cpp has p1's TEMP STUB)

class CAntiFricTimer : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT StartFriction();
	void EXPORT EntFrozen();

	bool OwnerGone(); // name ours
	void StopFriction(); // name ours

	float m_flStartTime; // orig +0x70, name ours: when the friction started (unread here)
};

LINK_ENTITY_TO_CLASS(antifric_timer, CAntiFricTimer); // orig 0xa600

// The owner's frictionless flag (CBaseEntity's byte at orig +0x67)
static bool& AntiFricFlag(CBaseEntity* pEntity) // name ours
{
	return pEntity->m_fRC2Frictionless;
}

// orig 0xa670
void CAntiFricTimer::Precache()
{
	PRECACHE_MODEL("sprites/aftex.spr");
}

// orig 0xa680. dmg is the duration; whoever creates the timer sets it (default 0.01 s).
void CAntiFricTimer::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->dmg = 0.01;
	pev->dmgtime = -1;
	SetThink(&CAntiFricTimer::StartFriction);
	pev->nextthink = gpGlobals->time;
}

// orig 0xa6f0. Removes the timer when its owner has gone.
bool CAntiFricTimer::OwnerGone()
{
	if (!UTIL_IsValidEntity(pev->owner))
	{
		UTIL_Remove(this);
		return true;
	}
	return false;
}

// The CLTE message that starts (0x35) or ends (0xfe, 0x35) the effect on an entity index
static void AntiFricEffect(bool fOn, int iEntIndex) // name ours
{
	if (fOn)
	{
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(0x35);
		WRITE_SHORT(iEntIndex);
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfe);
		WRITE_SHORT(iEntIndex);
		WRITE_BYTE(0x35);
		MESSAGE_END();
	}
}

// orig 0xa720
void CAntiFricTimer::StartFriction()
{
	if (OwnerGone())
		return;

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	AntiFricFlag(pOwner) = 1;
	if (pOwner->IsPlayer())
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pOwner);
		pPlayer->AddPhysFlags(8);
		// the players riding on its shoulders get the effect too
		if (0 != pPlayer->m_iRightRider)
			AntiFricEffect(true, pPlayer->m_iRightRider);
		if (0 != pPlayer->m_iLeftRider)
			AntiFricEffect(true, pPlayer->m_iLeftRider);
	}
	AntiFricEffect(true, ENTINDEX(pev->owner));

	pev->dmgtime = pev->dmg + gpGlobals->time;
	SetThink(&CAntiFricTimer::EntFrozen);
	pev->nextthink = gpGlobals->time + 0.01;
	m_flStartTime = gpGlobals->time;
}

// orig 0xa870. Until the time runs out or the owner dies.
void CAntiFricTimer::EntFrozen()
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
	StopFriction();
}

// orig 0xa8f0
void CAntiFricTimer::StopFriction()
{
	if (!UTIL_IsValidEntity(pev->owner))
	{
		SetThink(NULL);
		UTIL_Remove(this);
		return;
	}

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	AntiFricFlag(pOwner) = 0;
	if (pOwner->IsPlayer())
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pOwner);
		pPlayer->RemovePhysFlags(8);
		if (0 != pPlayer->m_iRightRider)
			AntiFricEffect(false, pPlayer->m_iRightRider);
		if (0 != pPlayer->m_iLeftRider)
			AntiFricEffect(false, pPlayer->m_iLeftRider);
	}
	AntiFricEffect(false, ENTINDEX(pev->owner));

	SetThink(NULL);
	UTIL_Remove(this);
}

// orig 0xaa40. Ends every antifriction on an entity at once (CBaseMonster::Killed and the
// player's Killed call it).
void StopAntiFriction(edict_t* pent) // name ours
{
	for (CBaseEntity* pTimer = UTIL_FindEntityByClassname(NULL, "antifric_timer"); pTimer; pTimer = UTIL_FindEntityByClassname(pTimer, "antifric_timer"))
	{
		if (pTimer->pev->owner == pent)
			static_cast<CAntiFricTimer*>(pTimer)->StopFriction();
	}
}

// TEMP STUB (p1): real ones belong in player.cpp (orig 0x9a730, 0x9a780: set / clear bits of
// the physics flags at +0x644 and send them as the physics key "phf", "%02X")
void CBasePlayer::AddPhysFlags(int iBits) {}
void CBasePlayer::RemovePhysFlags(int iBits) {}
