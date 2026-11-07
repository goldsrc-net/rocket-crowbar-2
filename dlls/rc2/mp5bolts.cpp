/***
*
*	Rocket Crowbar 2: the MP5's projectiles. All three are invisible point entities that fly
*	along pev->movedir at pev->speed, tracing each 0.05 s step, and die at pev->air_finished;
*	the client draws them from the hit events. pev->armorvalue, when the firing code sets it,
*	is a head start added to the first step. pev->impulse and pev->button ride along in each
*	hit event (iparam2 and bparam2).
*	  blaster_bolt  primary fire: hits once ([MP5] Primary*)
*	  phase_pulse   secondary fire: a radius blast where it hits ([MP5] Pulse*)
*	  bouncy_bolt   third fire: bounces off what it can't hurt ([MP5] Tertiary*)
*
*	In the original this code sits between mortar.cpp and mp5.cpp in link order; the file
*	name is ours.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "rc2prefs.h"
#include "p4_needs.h"

// The hit event every bolt plays: at vecOrigin, carrying the bolt's impulse and button.
// Name ours.
static void BoltHitEvent(entvars_t* pev, unsigned short usEvent, const Vector& vecOrigin, int iParam1)
{
	PLAYBACK_EVENT_FULL(0, NULL, usEvent, 0, (float*)&vecOrigin, (float*)&g_vecZero, 0, 0, iParam1, pev->impulse, 0, pev->button);
}

// The entvars the bolt's damage is credited to: its thrower in dmg_inflictor while that is
// live (NULL when the edict is NULL), the bolt otherwise. Name ours.
static entvars_t* BoltAttacker(entvars_t* pev)
{
	if (0 != UTIL_IsValidEdict(pev->dmg_inflictor))
		return VARS(pev->dmg_inflictor);
	return pev;
}

// How far the bolt flies this think: the time since the last one, plus any head start.
// Name ours.
static float BoltInterval(entvars_t* pev, float& flLastTime)
{
	float flInterval = gpGlobals->time - flLastTime;
	flLastTime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.05;
	if (pev->armorvalue > 0)
		flInterval += pev->armorvalue;
	pev->armorvalue = 0;
	return flInterval;
}

//=========================================================
// bouncy_bolt
//=========================================================
class CMP5BouncyBolt : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT BBThink();
	void BounceMove(Vector vecStart, Vector vecEnd, int iDepth);

	// Not saved, so the names are ours.
	float m_flLastTime;	   // orig +0x70
	int m_iBouncesLeft;	   // orig +0x74
	unsigned short m_usHit; // orig +0x78: events/mp5bouncyhit.sc
};

LINK_ENTITY_TO_CLASS(bouncy_bolt, CMP5BouncyBolt); // orig 0x7aa90

// orig 0x7ab00
void CMP5BouncyBolt::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CMP5BouncyBolt::BBThink);
	pev->nextthink = gpGlobals->time + 0.05;
	m_flLastTime = gpGlobals->time;
	pev->air_finished = RC2Pref(RC2P_MP5_TertiaryMaxLife).f + gpGlobals->time;
	pev->speed = RC2Pref(RC2P_MP5_TertiaryFlySpeed).f;
	m_iBouncesLeft = (unsigned char)RC2Pref(RC2P_MP5_TertiaryMaxBounces).c;
	pev->dmg_inflictor = pev->owner;
}

// orig 0x7abc0
void CMP5BouncyBolt::Precache()
{
	PRECACHE_MODEL("sprites/blflare.spr");
	PRECACHE_SOUND("debris/beamstart11.wav");
	m_usHit = PRECACHE_EVENT(1, "events/mp5bouncyhit.sc");
}

// orig 0x7abf0: one step from vecStart to vecEnd, bouncing (and recursing for the rest of the
// step) off what it doesn't hurt, iDepth steps deep at most. Name ours.
void CMP5BouncyBolt::BounceMove(Vector vecStart, Vector vecEnd, int iDepth)
{
	if (--iDepth <= 0)
		return;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, pev->owner, &tr);

	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
	{
		// the event is at the step's start (as the original has it)
		BoltHitEvent(pev, m_usHit, vecStart, 0);
		pev->flags |= FL_KILLME;
		return;
	}

	if (tr.flFraction >= 1.0 || (tr.pHit && tr.pHit == pev->owner))
	{
		UTIL_SetOrigin(pev, vecEnd);
		return;
	}

	if (0 != UTIL_IsValidEdict(tr.pHit) && CBaseEntity::Instance(tr.pHit))
	{
		CBaseEntity* pTarget = CBaseEntity::Instance(tr.pHit)->GetEffectedEnt();
		if (pTarget->pev->takedamage != 0)
		{
			entvars_t* pevAttacker = BoltAttacker(pev);
			// a player's shield also turns the hit normal into its own
			const float flDamage = ShieldAbsorbDamage(pTarget, RC2Pref(RC2P_MP5_TertiaryDamage).f, RC2Pref(RC2P_MP5_TertiaryShieldFraction).f, tr.vecEndPos, pev->movedir, NULL, &tr.vecPlaneNormal, 4);

			if (!pTarget->IsBSPModel() && pTarget->GetEffectedEnt()->IsAlive())
			{
				if (pTarget->GetEffectedEnt()->BloodColor() != DONT_BLEED && flDamage > 0)
					UTIL_BloodDrips(tr.vecEndPos, -pev->movedir, pTarget->GetEffectedEnt()->BloodColor(), 30); // VERIFY: 0xf2540 is UTIL_BloodDrips (0.88 like the SDK's)

				BoltHitEvent(pev, m_usHit, tr.vecEndPos + tr.vecPlaneNormal * 4, 0);
			}

			if (flDamage > 0)
			{
				ClearMultiDamage();
				pTarget->TraceAttack(pevAttacker, flDamage, pev->movedir, &tr, DMG_ENERGYBEAM);
				ApplyMultiDamage(pev, pevAttacker);
			}

			pev->flags |= FL_KILLME;
			return;
		}

		// what can't be hurt and isn't a brush stops it
		if (!pTarget->IsBSPModel())
		{
			pev->flags |= FL_KILLME;
			return;
		}
	}

	if (m_iBouncesLeft <= 0)
	{
		pev->flags |= FL_KILLME;
		return;
	}

	// bounce: whatever owned it can be hit now, and the rest of the step goes the new way
	m_iBouncesLeft--;
	pev->owner = NULL;

	const float flLeft = (1.0 - tr.flFraction) * (vecEnd - vecStart).Length();
	pev->movedir = UTIL_ReflectVector(pev->movedir, tr.vecPlaneNormal);

	const Vector vecPos = tr.vecEndPos + tr.vecPlaneNormal;
	UTIL_SetOrigin(pev, vecPos);

	if (flLeft <= 1.0)
		return;

	BounceMove(vecPos, vecPos + pev->movedir * flLeft, iDepth);
}

// orig 0x7b0e0
void CMP5BouncyBolt::BBThink()
{
	const float flInterval = BoltInterval(pev, m_flLastTime);
	BounceMove(pev->origin, pev->origin + pev->movedir * pev->speed * flInterval, 8);

	if (pev->air_finished < gpGlobals->time)
		UTIL_Remove(this);
}

//=========================================================
// phase_pulse
//=========================================================
class CMP5Pulse : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT BBThink();
	void Explode(Vector vecSrc);

	// Not saved, so the names are ours.
	float m_flLastTime;	   // orig +0x70
	unsigned short m_usHit; // orig +0x74: events/mp5pulsehit.sc
};

LINK_ENTITY_TO_CLASS(phase_pulse, CMP5Pulse); // orig 0x7b1f0

// orig 0x7b260
void CMP5Pulse::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CMP5Pulse::BBThink);
	pev->nextthink = gpGlobals->time + 0.05;
	m_flLastTime = gpGlobals->time;
	pev->air_finished = RC2Pref(RC2P_MP5_PulseMaxLife).f + gpGlobals->time;
	pev->speed = RC2Pref(RC2P_MP5_PulseFlySpeed).f;
	pev->dmg_inflictor = pev->owner;
}

// orig 0x7b310
void CMP5Pulse::Precache()
{
	PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_MODEL("sprites/white.spr");
	PRECACHE_MODEL("sprites/bigflare1.spr");
	m_usHit = PRECACHE_EVENT(1, "events/mp5pulsehit.sc");
}

// orig 0x7b350: the blast: [MP5] PulseBlastMaxDamage falling off to 0 at PulseBlastRadius by
// PulseBlastFalloffExponent, once per effected entity in sight of vecSrc. Name ours.
void CMP5Pulse::Explode(Vector vecSrc)
{
	const float flRadius = RC2Pref(RC2P_MP5_PulseBlastRadius).f;
	const float flMaxDamage = RC2Pref(RC2P_MP5_PulseBlastMaxDamage).f;
	const float flExponent = RC2Pref(RC2P_MP5_PulseBlastFalloffExponent).f;

	entvars_t* pevAttacker = BoltAttacker(pev);
	pev->owner = NULL;

	UTIL_PushEntityMask();
	CBaseEntity* pEntity = NULL;
	float flDist;
	Vector vecSpot;
	while ((pEntity = UTIL_FindEntityInSphereDist(pEntity, vecSrc, flRadius, &flDist, &vecSpot)) != NULL)
	{
		if (pEntity->pev->takedamage == 0)
			continue;
		if (UTIL_IsEntityMarked(ENTINDEX(pEntity->GetEffectedEnt()->edict())))
			continue;
		if (!(flDist < flRadius))
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSpot, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		double flBlast = flMaxDamage - pow(flDist / flRadius, flExponent) * flMaxDamage;
		if (flBlast < 0.0)
			flBlast = 0.0;
		else if (flBlast > flMaxDamage)
			flBlast = flMaxDamage;

		const float flDamage = ShieldAbsorbDamage(pEntity->GetEffectedEnt(), (float)flBlast, RC2Pref(RC2P_MP5_PulseBlastShieldFraction).f, tr.vecEndPos, (vecSpot - vecSrc).Normalize(), NULL, NULL, 6);
		if (flDamage <= 0)
			continue;

		UTIL_MarkEntity(ENTINDEX(pEntity->GetEffectedEnt()->edict()));
		pEntity->GetEffectedEnt()->TakeDamage(pev, pevAttacker, flDamage, DMG_ENERGYBEAM | DMG_RADIATION);
	}
	UTIL_PopEntityMask();
}

// orig 0x7b5a0
void CMP5Pulse::BBThink()
{
	const float flInterval = BoltInterval(pev, m_flLastTime);
	const Vector vecEnd = pev->origin + (pev->movedir * pev->speed) * flInterval;

	TraceResult tr;
	UTIL_TraceLine(pev->origin, vecEnd, dont_ignore_monsters, pev->owner, &tr);
	if (tr.flFraction >= 1.0)
	{
		UTIL_SetOrigin(pev, vecEnd);
		if (pev->air_finished < gpGlobals->time)
			UTIL_Remove(this);
		return;
	}

	const Vector vecHit = tr.vecEndPos + tr.vecPlaneNormal * 4;
	if (0 != UTIL_IsValidEdict(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && !pHit->IsBSPModel() && pHit->GetEffectedEnt()->IsAlive())
			BoltHitEvent(pev, m_usHit, vecHit, UTIL_PackDirection(pev->movedir));
	}

	Explode(vecHit);
	pev->flags |= FL_KILLME;
}

//=========================================================
// blaster_bolt
//=========================================================
class CMP5BlasterBolt : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT BBThink();

	// Not saved, so the names are ours.
	float m_flLastTime;	   // orig +0x70
	unsigned short m_usHit; // orig +0x74: events/mp5blasterhit.sc
};

LINK_ENTITY_TO_CLASS(blaster_bolt, CMP5BlasterBolt); // orig 0x7b8a0

// orig 0x7b910
void CMP5BlasterBolt::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CMP5BlasterBolt::BBThink);
	pev->nextthink = gpGlobals->time + 0.05;
	m_flLastTime = gpGlobals->time;
	pev->air_finished = RC2Pref(RC2P_MP5_PrimaryMaxLife).f + gpGlobals->time;
	pev->speed = RC2Pref(RC2P_MP5_PrimaryFlySpeed).f;
	pev->dmg_inflictor = pev->owner;
}

// orig 0x7b9c0
void CMP5BlasterBolt::Precache()
{
	PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_MODEL("sprites/blflare.spr");
	m_usHit = PRECACHE_EVENT(1, "events/mp5blasterhit.sc");
}

// orig 0x7b9f0
void CMP5BlasterBolt::BBThink()
{
	const float flInterval = BoltInterval(pev, m_flLastTime);
	const Vector vecEnd = pev->origin + (pev->movedir * pev->speed) * flInterval;

	TraceResult tr;
	UTIL_TraceLine(pev->origin, vecEnd, dont_ignore_monsters, pev->owner, &tr);
	if (tr.flFraction >= 1.0)
	{
		UTIL_SetOrigin(pev, vecEnd);
		if (pev->air_finished < gpGlobals->time)
			UTIL_Remove(this);
		return;
	}

	// its own shooter: it waits (the trace ignores the owner, so this is a guard)
	if (tr.pHit && 0 != UTIL_IsValidEdict(tr.pHit) && tr.pHit == pev->owner)
		return;

	const Vector vecHit = tr.vecEndPos + tr.vecPlaneNormal * 4;
	if (0 != UTIL_IsValidEdict(tr.pHit) && CBaseEntity::Instance(tr.pHit))
	{
		CBaseEntity* pTarget = CBaseEntity::Instance(tr.pHit)->GetEffectedEnt();
		entvars_t* pevAttacker = BoltAttacker(pev);
		const float flDamage = ShieldAbsorbDamage(pTarget, RC2Pref(RC2P_MP5_PrimaryDamage).f, RC2Pref(RC2P_MP5_PrimaryShieldFraction).f, tr.vecEndPos, pev->movedir, NULL, NULL, 4);

		// the hit effect only shows on something alive; walls and the world get none
		if (!pTarget->IsBSPModel() && pTarget->GetEffectedEnt()->IsAlive())
		{
			if (pTarget->GetEffectedEnt()->BloodColor() != DONT_BLEED && flDamage > 0)
				UTIL_BloodDrips(tr.vecEndPos, -pev->movedir, pTarget->GetEffectedEnt()->BloodColor(), 35); // VERIFY: as in BounceMove

			BoltHitEvent(pev, m_usHit, vecHit, 0);
		}

		if (flDamage > 0)
		{
			ClearMultiDamage();
			pTarget->TraceAttack(pevAttacker, flDamage, pev->movedir, &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, pevAttacker);
		}
	}

	pev->flags |= FL_KILLME;
}
