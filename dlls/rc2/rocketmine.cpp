/***
*
*	Rocket Crowbar 2: rocketmine.cpp (file name ours), the RPG's rocket mine, the "Roman
*	candle" (rocket_mine), and the homing rockets it launches (rm_rocket). Original functions
*	0xb8010-0xb9420 of rc2.dll.
*
*	The mine is tossed by the RPG and sits where it lands; when a target comes into range and
*	sight it fires a volley of rockets straight up, which then pick a target and home in.
*
****/

#include <algorithm>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "rc2prefs.h"

class CRocketMine : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetGuassHull() override { return 1; }			   // orig 0xb80a0
	float GetGuassDistOut() override { return 56.0f; }	   // orig 0xb80b0
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x14a1; } // orig 0xb8090

	void EXPORT FlyingThink();
	void EXPORT ActivateTouch(CBaseEntity* pOther);
	void EXPORT ActiveThink();

	float m_flLastThink; // orig +0x2b8, name ours
};

// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("rocket_mine") (0xf5a20); the shared macro doesn't yet
LINK_ENTITY_TO_CLASS(rocket_mine, CRocketMine); // orig 0xb8010

void CRocketMine::Spawn() // orig 0xb80f0
{
	Precache();
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_YES;
	pev->deadflag = DEAD_NO;
	pev->health = RC2Pref(RC2P_RPG_RomanCandleHealth).f;
	pev->gravity = RC2Pref(RC2P_RPG_RomanCandleGravity).f;
	pev->armorvalue = RANDOM_FLOAT(RC2Pref(RC2P_RPG_RomanCandleTriggerRangeMin).f, RC2Pref(RC2P_RPG_RomanCandleTriggerRangeMax).f); // the trigger range
	pev->dmg = RC2Pref(RC2P_RPG_RomanCandleKilledBlastDamage).f;
	pev->sequence = 0;
	ResetSequenceInfo();
	InitBoneControllers();

	SET_MODEL(ENT(pev), "models/remine.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 34));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CRocketMine::ActivateTouch);
	SetThink(&CRocketMine::FlyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmg_inflictor = pev->owner; // the thrower, kept for the kill credit
}

void CRocketMine::Precache() // orig 0xb80c0
{
	PRECACHE_MODEL("models/remine.mdl");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	UTIL_PrecacheOther("rm_rocket");
}

void CRocketMine::FlyingThink() // orig 0xb8240
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;
	ApplyExternalForce(flInterval);
}

void CRocketMine::ActivateTouch(CBaseEntity* pOther) // orig 0xb8280
{
	// the first touch of anything but the thrower arms it
	if (pOther->edict() != pev->owner)
	{
		m_flLastThink = gpGlobals->time;
		pev->dmgtime = gpGlobals->time;
		SetTouch(NULL);
		SetThink(&CRocketMine::ActiveThink);
		pev->nextthink = gpGlobals->time + 0.01;
	}
}

void CRocketMine::ActiveThink() // orig 0xb82e0
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	ApplyExternalForce(flInterval);
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// drop the thrower as owner once he is clear of it
	if (UTIL_IsValidEntity(pev->owner))
	{
		edict_t* pOwner = pev->owner;
		pev->owner = NULL;
		TraceResult tr;
		UTIL_TraceHull(pev->origin + Vector(0, 0, 36), pev->origin + Vector(0, 0, 37), dont_ignore_monsters, human_hull, edict(), &tr);
		if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
			pev->owner = pOwner;
	}
	else
	{
		pev->owner = NULL;
	}

	if (pev->impulse < 1)
	{
		if (pev->dmgtime < gpGlobals->time)
		{
			// look for something to fire at
			CBaseEntity* pOwner = NULL;
			if (UTIL_IsValidEntity(pev->dmg_inflictor))
				pOwner = CBaseEntity::Instance(pev->dmg_inflictor);

			CBaseEntity* pEntity = NULL;
			while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin + Vector(0, 0, 34), pev->armorvalue)) != NULL)
			{
				// VERIFY: slot 75's first argument is a CBaseEntity* (this) in the original, an int in cbase.h
				if ((pEntity->GetEffectedEnt()->GetRCCaps(this, pOwner) & 2) == 0)
					continue;
				// VERIFY: as above, slot 76's first argument is the thrower's CBaseEntity*
				if ((pEntity->GetEffectedEnt()->GetRCCaps2(pOwner, this) & 8) != 0)
					continue;
				if (!pEntity->IsAlive() || pEntity->pev->takedamage != DAMAGE_AIM || pEntity->pev->deadflag != DEAD_NO)
					continue;

				TraceResult tr;
				UTIL_TraceLine(pev->origin + Vector(0, 0, 34), pEntity->Center(), ignore_monsters, edict(), &tr);
				if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
				{
					pev->impulse = (unsigned short)RC2Pref(RC2P_RPG_RomanCandleVolleySize).s;
					pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_RPG_RomanCandleTriggerDelay).f;
					return;
				}
			}
		}
	}
	else if (pev->dmgtime < gpGlobals->time)
	{
		// launch one rocket of the volley, roughly straight up
		pev->impulse--;
		pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_RPG_RomanCandleVolleyDelay).f;
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "weapons/rocketfire1.wav", 1.0, ATTN_NORM, 0, 100);

		CBaseEntity* pRocket = CBaseEntity::Create("rm_rocket", pev->origin + Vector(0, 0, 20), pev->angles, edict());
		pRocket->pev->dmg_inflictor = pev->dmg_inflictor;
		pRocket->pev->velocity.x = RANDOM_FLOAT(-1, 1);
		pRocket->pev->velocity.y = RANDOM_FLOAT(-1, 1);
		pRocket->pev->velocity.z = RANDOM_FLOAT(2.5, 3);
		pRocket->pev->velocity = pRocket->pev->velocity.Normalize() * pRocket->pev->speed;

		if (pev->impulse < 1)
			pev->dmgtime = gpGlobals->time + 8.0;
	}
}

void CRocketMine::Killed(entvars_t* pevAttacker, int iGib) // orig 0xb8820
{
	// the thrower gets the credit for the blast (pev->dmg, RomanCandleKilledBlastDamage)
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pev->owner = pev->dmg_inflictor;

	CGrenade::Killed(pevAttacker, iGib);
}

//=========================================================
// rm_rocket: the Roman candle's homing rocket
//=========================================================
class CRMRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT HomeThink();
	void EXPORT BoomTouch(CBaseEntity* pOther);

	float m_flLastThink; // orig +0x2b8, name ours
	EHANDLE m_hTarget;	 // orig +0x2bc, name ours
	int m_iTrail;		 // orig +0x2c4, name ours: sprites/smoke.spr
	int m_iUnused2c8;	 // orig +0x2c8, name ours. VERIFY: nothing in this file reads or writes it
};

// VERIFY: also AddToPrecachedList("rm_rocket") in the original's factory, as above
LINK_ENTITY_TO_CLASS(rm_rocket, CRMRocket); // orig 0xb8860

void CRMRocket::Spawn() // orig 0xb8910
{
	Precache();
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
	SetThink(&CRMRocket::HomeThink);
	SetTouch(&CRMRocket::BoomTouch);
	pev->nextthink = gpGlobals->time + 0.01;
	m_flLastThink = gpGlobals->time;
	pev->speed = RANDOM_FLOAT(RC2Pref(RC2P_RPG_RomanCandleRocketSpeedMin).f, RC2Pref(RC2P_RPG_RomanCandleRocketSpeedMax).f);
	pev->armorvalue = RANDOM_FLOAT(RC2Pref(RC2P_RPG_RomanCandleRocketAccelMin).f, RC2Pref(RC2P_RPG_RomanCandleRocketAccelMax).f);			  // the turn acceleration
	pev->max_health = RANDOM_FLOAT(RC2Pref(RC2P_RPG_RomanCandleRocketTargetDistanceMin).f, RC2Pref(RC2P_RPG_RomanCandleRocketTargetDistanceMax).f); // the search radius
	pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_RPG_RomanCandleRocketMaxLife).f;
	m_hTarget = NULL;
	pev->dmg = RC2Pref(RC2P_RPG_RomanCandleRocketDamage).f;
	pev->effects |= EF_LIGHT;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_SHORT(m_iTrail);
	WRITE_BYTE(30); // life
	WRITE_BYTE(4);	// width
	WRITE_BYTE(224);
	WRITE_BYTE(224);
	WRITE_BYTE(255);
	WRITE_BYTE(255); // brightness
	MESSAGE_END();

	EMIT_SOUND_DYN(edict(), CHAN_STATIC, "weapons/rocket1.wav", 1.0, ATTN_NORM, 0, 100);
}

void CRMRocket::Precache() // orig 0xb88e0
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_SOUND("weapons/rocket1.wav");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
}

void CRMRocket::HomeThink() // orig 0xb8ae0
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.05;

	if (gpGlobals->time > pev->dmgtime)
	{
		// burnt out
		CGrenade::ExplodeTouch(this);
		return;
	}

	if (m_hTarget != NULL)
	{
		if (!m_hTarget->IsAlive() || m_hTarget->pev->deadflag != DEAD_NO || m_hTarget->pev->takedamage != DAMAGE_AIM)
			m_hTarget = NULL;

		if (m_hTarget != NULL)
		{
			// out of range: let go
			if ((m_hTarget->Center() - pev->origin).Length() > pev->max_health * 1.4)
				m_hTarget = NULL;
		}
	}

	if (m_hTarget == NULL)
	{
		// look for the best target ahead: in front scores highest, then near
		float flBest = -9999;
		const float flBlindDot = RC2Pref(RC2P_RPG_RomanCandleRocketBlindDot).f;
		CBaseEntity* pOwner = NULL;
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pOwner = CBaseEntity::Instance(pev->dmg_inflictor);

		const Vector vecHeading = pev->velocity.Normalize();

		CBaseEntity* pEntity = NULL;
		while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, pev->max_health)) != NULL)
		{
			if (!pEntity->IsAlive() || pEntity->pev->deadflag != DEAD_NO || pEntity->pev->takedamage != DAMAGE_AIM)
				continue;
			// VERIFY: slot 75/76 first arguments are CBaseEntity* in the original (see rocket mine)
			if ((pEntity->GetEffectedEnt()->GetRCCaps(this, pOwner) & 2) == 0)
				continue;
			if ((pEntity->GetRCCaps2(pOwner, this) & 8) != 0)
				continue;

			Vector vecTo = pEntity->Center() - pev->origin;
			const float flDist = vecTo.Length();
			const float flDot = DotProduct(vecHeading, vecTo.Normalize());
			if (flDot < flBlindDot)
				continue;

			const float flFacing = std::clamp((flDot + 1.0f) * 0.5f, 0.0f, 1.0f);
			const float flNear = std::clamp(1.0f - flDist / pev->max_health, 0.0f, 1.0f);
			const float flScore = flFacing * 3 + flNear + flNear;
			if (flScore < flBest)
				continue;

			TraceResult tr;
			UTIL_TraceLine(pev->origin, pEntity->Center(), ignore_monsters, edict(), &tr);
			if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
			{
				flBest = flScore;
				m_hTarget = pEntity;
			}
		}
	}

	if (m_hTarget == NULL)
	{
		pev->velocity = pev->velocity.Normalize() * pev->speed;
	}
	else
	{
		// steer toward it, harder the more it is already ahead
		Vector vecDir = (m_hTarget->Center() - pev->origin).Normalize();
		Vector vecHeading = pev->velocity.Normalize();
		float flAhead = std::clamp((DotProduct(vecHeading, vecDir) + 1.0f) * 0.5f, 0.0f, 1.0f);
		const float flScale = flAhead * 0.35 + 0.65;
		pev->velocity = pev->velocity + vecDir * pev->armorvalue * flInterval * flScale;
		pev->velocity = pev->velocity.Normalize() * pev->speed;
	}

	pev->angles = UTIL_VecToAngles(pev->velocity.Normalize());
}

void CRMRocket::BoomTouch(CBaseEntity* pOther) // orig 0xb93d0
{
	STOP_SOUND(edict(), CHAN_STATIC, "weapons/rocket1.wav");
	pev->owner = pev->dmg_inflictor; // the thrower gets the credit
	CGrenade::ExplodeTouch(pOther);
}
