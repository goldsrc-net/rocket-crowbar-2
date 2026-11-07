/***
*
*	Rocket Crowbar 2: antifriction_grenade, the hand grenade's primary throw.
*
*	A glowing grenade that bounces for [Grenade] SharedMaxTimer seconds and goes off in a blast
*	of AFGrenBlastRadius that throws what it catches (AFGrenBlastMinVel + up to MaxAddVell) and
*	hurts it a little (AFGrenBlastMinDamage + up to MaxAddDamage, through the shield at
*	AFGrenBlastDamageShieldFraction), and gives it an antifric_timer (CAntiFricTimer, another
*	package's) for AFGrenBlastMinTime + up to MaxAddTime seconds of no friction. Falloff by
*	distance is (1 - (dist / radius) ^ AFGrenBlastFalloff).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "rc2prefs.h"
#include "rc2grenades.h"

LINK_ENTITY_TO_CLASS(antifriction_grenade, CAFGrenade); // orig 0x59fa0

// orig 0x5a020
void CAFGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("sprites/c-tele1.spr");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("debris/beamstart4.wav");
	m_usDetonate = PRECACHE_EVENT(1, "events/afgrendet.sc");
	UTIL_PrecacheOther("antifric_timer");
}

// orig 0x5a080
void CAFGrenade::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->classname = MAKE_STRING("antifriction_grenade");
	pev->dmg_inflictor = pev->owner;

	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector(255, 0, 255);
	pev->renderamt = 16;

	SetTouch(&CGrenade::BounceTouch);
	SetThink(&CAFGrenade::PreDetonate);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Grenade_SharedMaxTimer).f + gpGlobals->time;

	pev->sequence = RANDOM_LONG(3, 6); // no ResetSequenceInfo, as the gravity grenade
	pev->framerate = 1.0;
	pev->gravity = RC2Pref(RC2P_Grenade_AFGrenGravityScaler).f;
	pev->friction = RC2Pref(RC2P_Grenade_AFGrenFrictionScaler).f;
	m_fRegisteredSound = false;
}

// orig 0x5a1e0
void CAFGrenade::PreDetonate()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (pev->dmgtime - 2.0 < gpGlobals->time)
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1);

	if (pev->dmgtime <= gpGlobals->time)
	{
		Detonate();
		return;
	}

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

// orig 0x5a320
void CAFGrenade::Detonate()
{
	PLAYBACK_EVENT_FULL(0, ENT(pev), m_usDetonate, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, (pev->flags & FL_ONGROUND) != 0, 0);

	const float flRadius = RC2Pref(RC2P_Grenade_AFGrenBlastRadius).f;
	CBaseEntity* pAttacker = NULL;
	entvars_t* pevAttacker = pev;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		pAttacker = CBaseEntity::Instance(pev->dmg_inflictor);
		pevAttacker = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;
	}

	Vector vecSrc = pev->origin;
	if ((pev->flags & FL_ONGROUND) != 0)
		vecSrc.z += RC2Pref(RC2P_Grenade_AFGrenOnGroundZBoost).f;

	RC2EntMarkPush();
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRadius)) != NULL)
	{
		CBaseEntity* pTarget = pEntity->GetEffectedEnt();
		if (RC2EntMarkTest(ENTINDEX(pTarget->edict())))
			continue;
		// VERIFY: GetRCCaps' arguments are entity pointers (attacker, this) here; cbase.h types the first int
		if ((pTarget->GetRCCaps((int)(intptr_t)pAttacker, this) & 0x800) == 0)
			continue;
		if ((pTarget->pev->flags & FL_CLIENT) != 0 && !pTarget->IsActivePlayer())
			continue;
		if (pTarget->pev->takedamage == DAMAGE_NO)
			continue;
		if (pTarget->IsPlayer() && static_cast<CBasePlayer*>(pTarget)->m_iRC2LinkedEnt0 != 0)
			continue;
		// gamerules orig slot 58 (p3b_temp.h's name), asked (attacker, victim)
		if (pAttacker && pAttacker->IsPlayer() && !RC2GameRulesAllowGaussTeleport(static_cast<CBasePlayer*>(pAttacker), pTarget))
			continue;

		Vector vecSpot = pEntity->Center();
		Vector vecDir = vecSpot - vecSrc;
		float flDist = vecDir.Length();
		if (flDist > flRadius)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSpot, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		if (flDist != 0)
			vecDir = vecDir / flDist;

		float flFraction = 1.0 - pow(flDist / flRadius, RC2Pref(RC2P_Grenade_AFGrenBlastFalloff).f);
		if (flFraction < 0.0)
			flFraction = 0.0;
		else if (flFraction > 1.0)
			flFraction = 1.0;

		const float flTime = RC2Pref(RC2P_Grenade_AFGrenBlastMaxAddTime).f * flFraction + RC2Pref(RC2P_Grenade_AFGrenBlastMinTime).f;
		const float flVelocity = RC2Pref(RC2P_Grenade_AFGrenBlastMaxAddVell).f * flFraction + RC2Pref(RC2P_Grenade_AFGrenBlastMinVel).f;
		const float flDamage = RC2Pref(RC2P_Grenade_AFGrenBlastMaxAddDamage).f * flFraction + RC2Pref(RC2P_Grenade_AFGrenBlastMinDamage).f;

		RC2EntMarkSet(ENTINDEX(pTarget->edict()));
		BlastEntity(pTarget, pevAttacker, pev, flDamage, flTime, flVelocity, vecDir, &tr);
	}
	RC2EntMarkPop();

	SetThink(NULL);
	SetTouch(NULL);
	UTIL_Remove(this);
}

// orig 0x5a760: hurt pTarget, then, if it is still there (and, when it was alive, still alive),
// throw it along vecDir and add flTime to its antifric_timer, making one if it has none
void CAFGrenade::BlastEntity(CBaseEntity* pTarget, entvars_t* pevAttacker, entvars_t* pevInflictor, float flDamage, float flTime, float flVelocity, Vector vecDir, TraceResult* ptr)
{
	EHANDLE hTarget;
	hTarget = pTarget;
	// pevAttacker is never NULL here: Detonate only passes NULL with a valid dmg_inflictor
	CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker->pContainingEntity);

	if ((hTarget->GetRCCaps((int)(intptr_t)pAttacker, NULL) & 0x200) == 0) // VERIFY: as in Detonate
		return;

	bool bWasAlive = hTarget->IsAlive() && hTarget->pev->deadflag == DEAD_NO && hTarget->pev->health > 0;
	if ((hTarget->pev->flags & FL_MONSTER) != 0)
		bWasAlive = bWasAlive && (static_cast<CBaseMonster*>(CBaseEntity::Instance(hTarget.Get()))->m_afMemory & bits_MEMORY_KILLED) == 0;

	const float flBaseDamage = flDamage;
	flDamage = RC2ShieldDamage(pTarget, flDamage, RC2Pref(RC2P_Grenade_AFGrenBlastDamageShieldFraction).f, ptr->vecEndPos, vecDir, NULL, NULL, 4);
	if (flDamage <= 0)
		return;
	if (flBaseDamage != 0)
		flTime = flDamage / flBaseDamage * flTime;

	hTarget->TakeDamage(pevInflictor, pevAttacker, flDamage, DMG_NEVERGIB);

	if (!hTarget || !UTIL_IsValidEntity(hTarget.Get()))
		return;

	if (bWasAlive)
	{
		// killed by the blast: leave the body where it is
		if (!hTarget->IsAlive() || hTarget->pev->deadflag != DEAD_NO || hTarget->pev->health <= 0)
			return;
		if ((hTarget->pev->flags & FL_MONSTER) != 0 && (static_cast<CBaseMonster*>(CBaseEntity::Instance(hTarget.Get()))->m_afMemory & bits_MEMORY_KILLED) != 0)
			return;
	}

	// straight up or down: lean it a little at random
	if (vecDir.x == 0 && vecDir.y == 0)
	{
		vecDir.x = RANDOM_FLOAT(-0.25, 0.25);
		vecDir.y = RANDOM_FLOAT(-0.25, 0.25);
		const float flLength = sqrt(vecDir.x * vecDir.x + vecDir.y * vecDir.y + vecDir.z * vecDir.z);
		if (flLength == 0)
			vecDir = Vector(0, 0, 1);
		else
			vecDir = vecDir * (1.0f / flLength);
	}

	hTarget->pev->velocity = hTarget->pev->velocity + vecDir * flVelocity;
	if (hTarget->pev->velocity.z > 0)
		hTarget->pev->velocity.z += 100.0;
	hTarget->pev->flags &= ~FL_ONGROUND;

	CBaseEntity* pTimer = NULL;
	while ((pTimer = UTIL_FindEntityByClassname(pTimer, "antifric_timer")) != NULL)
	{
		if (pTimer->pev->owner == hTarget->edict())
			break;
	}
	if (!pTimer)
		pTimer = CBaseEntity::Create("antifric_timer", g_vecZero, g_vecZero, hTarget->edict());
	if (!pTimer)
		return;

	// VERIFY: the timer's fields as CAntiFricTimer reads them (dmgtime -1 means: count in dmg)
	if (pTimer->pev->dmgtime == -1.0f)
		pTimer->pev->dmg += flTime;
	else
		pTimer->pev->dmgtime += flTime;
}
