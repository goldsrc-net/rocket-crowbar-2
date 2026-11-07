/***
*
*	Rocket Crowbar 2: balllightning.cpp (file name ours). The python's ball lightning and the
*	chain lightning hurt (see balllightning.h). Original functions 0xaad10-0xab850 of rc2.dll.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"
#include "weapons.h"
#include "rc2prefs.h"
#include "shield.h"
#include "rc2/balllightning.h"


// CLTE 0x0f: the ball's glow on the client; 0xfe kills a client effect (subtype 0x0f here)
#define CLTE_BALLLIGHTNING 0x0f // ours
#define CLTE_KILL 0xfe			// ours

// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("ball_lightning") (0xf5a20); the shared macro doesn't yet
LINK_ENTITY_TO_CLASS(ball_lightning, CBallLighting); // orig 0xaad10

void CBallLighting::Spawn() // orig 0xaad90
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	pev->dmg_inflictor = pev->owner; // the shooter, for the kill credit

	SetTouch(&CBallLighting::ImpactTouch);
	SetThink(&CBallLighting::BallThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->air_finished = gpGlobals->time + RC2Pref(RC2P_Python_BallMaxLife).f; // when it fizzles out

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(CLTE_BALLLIGHTNING);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

void CBallLighting::Precache() // orig 0xaaed0
{
	PRECACHE_MODEL("sprites/blflare.spr");
	PRECACHE_MODEL("sprites/bldet.spr");
	PRECACHE_MODEL("sprites/bigflare1.spr");
	PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_SOUND("weapons/blfly.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	m_usBallImpact = PRECACHE_EVENT(1, "events/pythballimpact.sc");
}

// Every entity in [Python] BallBlastRadius of vecOrigin that takes damage, isn't under water
// and can be seen from there takes BallBlastDamage, falling off as (dist/radius)^exponent,
// through its shield. The first one standing in water also sets off a water discharge.
void CBallLighting::BlastDamage(Vector vecOrigin) // orig 0xaaf40
{
	float flDamage = RC2Pref(RC2P_Python_BallBlastDamage).f;
	float flRadius = RC2Pref(RC2P_Python_BallBlastRadius).f;
	float flExponent = RC2Pref(RC2P_Python_BallBlastFalloffExponent).f;
	entvars_t* pevAttacker = pev;
	bool bDischarged = false;

	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pevAttacker = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;
	pev->owner = NULL; // so the traces below don't skip the shooter

	UTIL_PushEntityMarks();
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEnt = CBaseEntity::Instance(pent);
		if (!pEnt || pEnt->pev->takedamage == DAMAGE_NO || pEnt->pev->waterlevel >= 2)
			continue;
		if (UTIL_IsEntityMarked(ENTINDEX(pEnt->GetEffectedEnt()->edict())))
			continue;

		Vector vecCenter = pEnt->Center();
		Vector vecTo = vecCenter - vecOrigin;
		float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);
		if (!(flDist < flRadius))
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecOrigin, vecCenter, ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		float flHurt = flDamage - pow(flDist / flRadius, flExponent) * flDamage;
		if (flHurt < 0)
			flHurt = 0;
		else if (flHurt > flDamage)
			flHurt = flDamage;

		flHurt = ShieldDeflect(pEnt, flHurt, RC2Pref(RC2P_Python_BallBlastShieldFraction).f, tr.vecEndPos, vecTo.Normalize(), NULL, NULL, 6);
		if (!(flHurt > 0))
			continue;

		// where the discharge starts: 12 above the feet (the entity is never under water
		// here, so the Center() branch is dead)
		Vector vecFeet;
		if (pEnt->pev->waterlevel > 1)
			vecFeet = pEnt->Center();
		else
			vecFeet = Vector(pEnt->pev->origin.x, pEnt->pev->origin.y, pEnt->pev->mins.z + 12.0 + pEnt->pev->origin.z);
		bool bInWater = pEnt->pev->waterlevel > 0;

		pEnt->GetEffectedEnt()->TakeDamage(pev, pevAttacker, flHurt, DMG_SHOCK | DMG_ENERGYBEAM);
		UTIL_MarkEntity(ENTINDEX(pEnt->GetEffectedEnt()->edict()));

		if (!bDischarged && bInWater)
		{
			bDischarged = true;
			UTIL_RC2WaterDischarge(vecFeet, RC2Pref(RC2P_Python_BallSecondaryDischargeDamage).f, UTIL_RC2DamageInflictor("357_discharge"), pev);
		}
	}
	UTIL_PopEntityMarks();
}

void CBallLighting::ImpactTouch(CBaseEntity* pOther) // orig 0xab330
{
	TraceResult tr = UTIL_GetGlobalTrace();

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_KILL);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(CLTE_BALLLIGHTNING);
	MESSAGE_END();

	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_WATER)
	{
		PLAYBACK_EVENT_FULL(0, edict(), m_usBallImpact, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, true, false);
		entvars_t* pevAttacker = pev;
		if (UTIL_IsValidEntity(pev->owner))
			pevAttacker = pev->owner ? VARS(pev->owner) : NULL;
		UTIL_RC2WaterDischarge(pev->origin, RC2Pref(RC2P_Python_BallImpactDischargeDamage).f, UTIL_RC2DamageInflictor("357_discharge"), pevAttacker);
	}
	else
	{
		PLAYBACK_EVENT_FULL(0, edict(), m_usBallImpact, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, UTIL_PackNormal(-tr.vecPlaneNormal), 0, false, false);
		BlastDamage(tr.vecEndPos + tr.vecPlaneNormal * 16);
	}

	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CBallLighting::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.01;
}

void CBallLighting::BallThink() // orig 0xab5b0
{
	if (pev->air_finished < gpGlobals->time)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(CLTE_KILL);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(CLTE_BALLLIGHTNING);
		MESSAGE_END();
		SetThink(NULL);
		SetTouch(NULL);
		UTIL_Remove(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		return;

	// flew into water: discharge where it is
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_KILL);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(CLTE_BALLLIGHTNING);
	MESSAGE_END();
	PLAYBACK_EVENT_FULL(0, edict(), m_usBallImpact, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, true, false);
	entvars_t* pevAttacker = pev;
	if (UTIL_IsValidEntity(pev->owner))
		pevAttacker = pev->owner ? VARS(pev->owner) : NULL;
	UTIL_RC2WaterDischarge(pev->origin, RC2Pref(RC2P_Python_BallImpactDischargeDamage).f, UTIL_RC2DamageInflictor("357_discharge"), pevAttacker);

	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CBallLighting::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.01;
}

// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("chain_hurter") (0xf5a20); the shared macro doesn't yet
LINK_ENTITY_TO_CLASS(chain_hurter, CChainHurter); // orig 0xab760

void CChainHurter::Spawn() // orig 0xab7d0
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CChainHurter::HurtEnt);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->dmg = RC2Pref(RC2P_Python_ChainDamage).f;
}

void CChainHurter::HurtEnt() // orig 0xab850
{
	if (!UTIL_IsValidEntity(pev->owner))
		pev->owner = edict(); // VERIFY: then the hurter is its own attacker

	if (m_hTarget && m_hTarget->pev->takedamage == DAMAGE_AIM)
	{
		ClearMultiDamage();
		m_hTarget->TraceAttack(VARS(pev->owner), pev->dmg, m_vecDir, &m_tr, DMG_SHOCK | DMG_ENERGYBEAM);
		ApplyMultiDamage(UTIL_RC2DamageInflictor("chain_lightning"), pev->owner ? VARS(pev->owner) : NULL);
	}
	UTIL_Remove(this);
}
