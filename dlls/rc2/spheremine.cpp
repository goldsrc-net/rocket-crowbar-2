/***
*
*	Rocket Crowbar 2: spheremine.cpp (file name ours), the RPG's sphere mine (sphere_mine) and
*	the homing shot it fires (sm_project). Original functions 0xb6070-0xb7800 of rc2.dll.
*
*	The mine is lobbed by the RPG, bounces until it slows down, unfolds, then turns toward the
*	best target in range and fires bursts of homing projectiles at it.
*
****/

#include <algorithm>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "p6stubs.h"

extern int gmsgCLTE; // RC2's "CLTE" user message (orig .bss 0x18f4b8); TEMP STUB in dlls/rc2/playerride.cpp

class CSphereMine : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_MACHINE; } // orig 0xb6120
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetGuassHull() override { return 3; }			   // orig 0xb6100
	float GetGuassDistOut() override { return 36.0f; }	   // orig 0xb6110
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x14a1; } // orig 0xb60f0

	void EXPORT BounceTouch(CBaseEntity* pOther);
	void EXPORT PreactThink();
	void EXPORT ActiveThink();

	float m_flLastThink;   // orig +0x2b8, name ours: the time of the last ActiveThink (PreactThink doesn't update it)
	float m_flVelDropRate; // orig +0x2bc, name ours: [RPG] SphereMineVelDropRate, the drag per second
	float m_flRange;	   // orig +0x2c0, name ours: [RPG] SphereMineRange, the search radius
	EHANDLE m_hTarget;	   // orig +0x2c4, name ours
};

// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("sphere_mine") (0xf5a20); the shared macro doesn't yet
LINK_ENTITY_TO_CLASS(sphere_mine, CSphereMine); // orig 0xb6070

void CSphereMine::Spawn() // orig 0xb6160
{
	Precache();
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->takedamage = DAMAGE_YES;
	pev->deadflag = DEAD_NO;
	pev->health = RC2Pref(RC2P_RPG_SphereMineHealth).f;
	pev->friction = 0.6;
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->dmg_inflictor = pev->owner; // the thrower, kept for the kill credit while owner is cleared

	SET_MODEL(ENT(pev), "models/shootsphere.mdl");
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	UTIL_SetOrigin(pev, pev->origin);

	m_flVelDropRate = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SphereMineVelDropRateMin).f, RC2Pref(RC2P_RPG_SphereMineVelDropRateMax).f);
	SetThink(&CSphereMine::PreactThink);
	SetTouch(&CSphereMine::BounceTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flLastThink = gpGlobals->time;
	m_flRange = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SphereMineRangeMin).f, RC2Pref(RC2P_RPG_SphereMineRangeMax).f);
	m_hTarget = NULL;
	pev->impulse = 0;
	pev->dmgtime = gpGlobals->time;
}

void CSphereMine::Precache() // orig 0xb6130
{
	PRECACHE_MODEL("models/shootsphere.mdl");
	PRECACHE_SOUND("debris/beamstart8.wav");
	UTIL_PrecacheOther("sm_project");
}

void CSphereMine::BounceTouch(CBaseEntity* pOther) // orig 0xb62e0
{
	if (pOther->IsBSPModel())
	{
		pev->velocity = pev->velocity * 0.8;
		return;
	}

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecNormal = tr.vecPlaneNormal; // unused

	if (tr.pHit == edict()) // VERIFY: compares the global trace's pHit with the mine itself, as read
	{
		if (pOther->pev->groundentity == edict())
		{
			// something standing on it: knock it down
			pev->velocity.z -= 200;
			return;
		}
		pev->velocity = pev->velocity + pOther->pev->velocity * 0.25;
		return;
	}

	pev->velocity = pev->velocity * 0.8;
}

// The common start of both thinks (inlined twice in the original): drop the thrower as owner
// once the mine is clear of him, so he can collide with it.
static void SphereMineReleaseOwner(CBaseEntity* pMine) // name ours
{
	entvars_t* pev = pMine->pev;
	if (UTIL_IsValidEntity(pev->owner))
	{
		edict_t* pOwner = pev->owner;
		pev->owner = NULL;
		TraceResult tr;
		UTIL_TraceHull(pev->origin + Vector(0, 0, 36), pev->origin + Vector(0, 0, 37), dont_ignore_monsters, head_hull, pMine->edict(), &tr);
		if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
			pev->owner = pOwner;
	}
	else
	{
		pev->owner = NULL;
	}
}

// Drag shared by both thinks: take flDrop of the velocity off, where flDrop is the interval times
// the drop rate, at least 0.05, and (oddly) no more than the speed itself.
static void SphereMineDrag(entvars_t* pev, float flInterval, float flDropRate) // name ours
{
	if (pev->velocity.Length() <= 2.0)
	{
		pev->velocity = g_vecZero;
		return;
	}

	float flDrop = flInterval * flDropRate;
	if (flDrop < 0.05)
		flDrop = 0.05;
	else if (flDrop > pev->velocity.Length())
		flDrop = pev->velocity.Length(); // original oddity: a fraction clamped to a speed

	pev->velocity = pev->velocity - pev->velocity * flDrop;
}

void CSphereMine::PreactThink() // orig 0xb64d0
{
	const float flInterval = gpGlobals->time - m_flLastThink; // m_flLastThink is Spawn's time here: it grows
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();
	ApplyExternalForce(flInterval);

	SphereMineReleaseOwner(this);

	pev->angles = UTIL_VecToAngles(pev->velocity.Normalize());

	SphereMineDrag(pev, flInterval, m_flVelDropRate);

	if (pev->sequence == 0)
	{
		if (pev->velocity.Length() < 50.0)
		{
			// slow enough: unfold
			pev->sequence = 2;
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}
	else if (m_fSequenceFinished)
	{
		SetThink(&CSphereMine::ActiveThink);
		pev->nextthink = gpGlobals->time + 0.01;
		pev->frame = 255;
	}
}

void CSphereMine::ActiveThink() // orig 0xb6850
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	pev->nextthink = gpGlobals->time + 0.05;
	m_flLastThink = gpGlobals->time;
	StudioFrameAdvance();
	ApplyExternalForce(flInterval);

	SphereMineReleaseOwner(this);
	SphereMineDrag(pev, flInterval, m_flVelDropRate);

	const bool bHadTarget = m_hTarget != NULL;
	Vector vecDelta;

	if (m_hTarget == NULL)
	{
		// no target: spin down
		pev->avelocity.x = std::clamp(pev->avelocity.x, -720.0f, 720.0f);
		pev->avelocity.y = std::clamp(pev->avelocity.y, -720.0f, 720.0f);
		pev->angles.x = UTIL_AngleMod(pev->angles.x);
		pev->angles.y = UTIL_AngleMod(pev->angles.y);
		pev->angles.z = 0;
		pev->avelocity = pev->avelocity - pev->avelocity * flInterval * 0.75;
	}
	else
	{
		if (!m_hTarget->IsAlive() || m_hTarget->pev->takedamage != DAMAGE_AIM || m_hTarget->pev->deadflag != DEAD_NO)
			m_hTarget = NULL;

		if (m_hTarget != NULL)
		{
			vecDelta = m_hTarget->Center() - pev->origin;
			// out of range (with hysteresis) and not in the middle of a burst: let go
			if (vecDelta.Length() > RC2Pref(RC2P_RPG_SphereMineDistanceHysteresis).f * m_flRange && pev->impulse <= 0)
				m_hTarget = NULL;

			// recheck line of sight five times a second
			if (m_hTarget != NULL && pev->air_finished < gpGlobals->time)
			{
				pev->air_finished = gpGlobals->time + 0.2;
				TraceResult tr;
				UTIL_TraceLine(pev->origin, m_hTarget->Center(), ignore_monsters, edict(), &tr);
				if (tr.flFraction < 1.0 && tr.pHit != m_hTarget->edict())
					m_hTarget = NULL;
			}

			if (m_hTarget != NULL)
			{
				Vector vecDir = vecDelta.Normalize();
				Vector vecAngles = UTIL_VecToAngles(vecDir);
				vecAngles.x = UTIL_AngleMod(vecAngles.x);
				vecAngles.y = UTIL_AngleMod(vecAngles.y);
				pev->angles.x = UTIL_AngleMod(pev->angles.x);
				pev->angles.y = UTIL_AngleMod(pev->angles.y);
				pev->angles.z = 0;

				// turn toward it at up to 360 degrees a second
				const float flYaw = UTIL_ApproachAngle(vecAngles.y, pev->angles.y, flInterval * 360);
				const float flPitch = UTIL_ApproachAngle(vecAngles.x, pev->angles.x, flInterval * 360);
				if (flInterval > 0)
				{
					pev->avelocity.x = (flPitch - pev->angles.x) / flInterval;
					pev->avelocity.y = (flYaw - pev->angles.y) / flInterval;
					pev->avelocity.z = -pev->angles.z / flInterval;
				}

				if (pev->dmgtime < gpGlobals->time && pev->impulse < 1)
				{
					UTIL_MakeAimVectors(pev->angles);
					if (DotProduct(vecDir, gpGlobals->v_forward) > RC2Pref(RC2P_RPG_SphereMineFireDot).f)
					{
						// facing it: start a burst
						pev->impulse = (unsigned short)RC2Pref(RC2P_RPG_SphereMineBurstCount).s;
						pev->dmgtime = gpGlobals->time - 1.0;
					}
				}
			}
		}
	}

	if (pev->impulse < 1)
	{
		if (bHadTarget && m_hTarget == NULL)
			pev->dmgtime = gpGlobals->time - 1.0; // lost it: search again right away

		if (m_hTarget == NULL && pev->dmgtime < gpGlobals->time)
		{
			// look for the best target: in front and near scores highest
			float flBest = -9999;
			pev->dmgtime = gpGlobals->time + 0.1;
			UTIL_MakeAimVectors(pev->angles);
			const Vector vecForward = gpGlobals->v_forward;

			CBaseEntity* pEntity = NULL;
			while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, m_flRange)) != NULL)
			{
				if (!pEntity->IsAlive() || pEntity->pev->deadflag != DEAD_NO || pEntity->pev->takedamage != DAMAGE_AIM)
					continue;
				if ((pEntity->GetRCCaps2(0, this) & 0x28) != 0)
					continue;
				if (pEntity->edict() == edict())
					continue;

				Vector vecTo = pEntity->Center() - pev->origin;
				const float flDist = vecTo.Length();
				if (flDist > m_flRange)
					continue;

				const float flFacing = std::clamp((DotProduct(vecTo.Normalize(), vecForward) + 1.0f) * 0.5f, 0.0f, 1.0f);
				const float flNear = std::clamp(1.0f - flDist / m_flRange, 0.0f, 1.0f);
				const float flScore = flFacing * 2 + flNear * 3;
				if (flScore < flBest)
					continue;

				TraceResult tr;
				UTIL_TraceLine(pev->origin, pEntity->Center(), ignore_monsters, edict(), &tr);
				if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
				{
					m_hTarget = pEntity;
					pev->air_finished = gpGlobals->time + 0.2;
					flBest = flScore;
				}
			}
		}
	}
	else if (pev->dmgtime < gpGlobals->time)
	{
		// fire one shot of the burst
		pev->impulse--;
		pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_RPG_SphereMineBurstDelay).f;
		pev->sequence = 1;
		ResetSequenceInfo();
		pev->frame = 0;
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "debris/beamstart8.wav", 0.7, ATTN_NORM, 0, RANDOM_LONG(150, 160));

		UTIL_MakeAimVectors(pev->angles);
		CBaseEntity* pShot = CBaseEntity::Create("sm_project", pev->origin, pev->angles, edict());
		pShot->pev->dmg_inflictor = pev->dmg_inflictor; // the thrower gets the credit
		pShot->pev->velocity = gpGlobals->v_forward * pShot->pev->speed;
		if (m_hTarget != NULL)
			pShot->pev->enemy = m_hTarget->edict();

		if (pev->impulse < 1)
			pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_RPG_SphereMineBurstRefire).f;
	}
}

bool CSphereMine::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) // orig 0xb7450
{
	// its own shots don't hurt it
	if (pevInflictor && 0 == strcmp(STRING(pevInflictor->classname), "sm_project"))
		return false;

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CSphereMine::Killed(entvars_t* pevAttacker, int iGib) // orig 0xb74d0
{
	pev->owner = pev->dmg_inflictor;
	pev->takedamage = DAMAGE_NO;
	pev->takedamage = DAMAGE_NO;
	UTIL_RC2KillEntityEffects(edict());
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 1024, 3.0);
	PLAYBACK_EVENT_FULL(0, edict(), g_usRC2ExplodeEvent, 0.0, pev->origin, g_vecZero, pev->dmg, 0.0, 0, 0, false, true);

	entvars_t* pevOwner = NULL;
	if (UTIL_IsValidEntity(pev->dmg_inflictor) && pev->dmg_inflictor)
		pevOwner = VARS(pev->dmg_inflictor);
	pev->owner = NULL;

	RC2RadiusDamage(pevOwner, pev, edict(), pev->origin, RC2Pref(RC2P_RPG_SphereMineBlastRadius).f,
		RC2Pref(RC2P_RPG_SphereMineBlastDamage).f, RC2Pref(RC2P_RPG_SphereMineBlastFalloffExponent).f, DMG_BLAST, 1);

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

//=========================================================
// sm_project: the sphere mine's homing shot
//=========================================================
class CSMProjectile : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 2; } // orig 0xb7680

	void EXPORT HomeThink();
	void EXPORT ExplodeTouch(CBaseEntity* pOther);

	float m_flLastThink;	   // orig +0x70, name ours
	int m_iUnused74;		   // orig +0x74, name ours. VERIFY: nothing in this file reads or writes it
	unsigned short m_usEvent;  // orig +0x78, name ours: events/rpgsmimpact.sc
};

// VERIFY: also AddToPrecachedList("sm_project") in the original's factory, as above
LINK_ENTITY_TO_CLASS(sm_project, CSMProjectile); // orig 0xb7610

void CSMProjectile::Spawn() // orig 0xb76d0
{
	Precache();
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
	pev->effects |= EF_NODRAW; // the client draws it (gmsgCLTE below)
	m_iRC2Flags = 2;		   // VERIFY: meaning of the base's +0x68 value 2
	SetThink(&CSMProjectile::HomeThink);
	SetTouch(&CSMProjectile::ExplodeTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flLastThink = gpGlobals->time;
	pev->speed = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SphereMineProjectileSpeedMin).f, RC2Pref(RC2P_RPG_SphereMineProjectileSpeedMax).f);
	pev->armorvalue = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SphereMineProjectileAccelMin).f, RC2Pref(RC2P_RPG_SphereMineProjectileAccelMax).f);
	pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_RPG_SphereMineProjectileMaxLife).f;

	// VERIFY: client effect 0x20 on this entity (RC2's CLTE message; the client package says what it draws)
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x20);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

void CSMProjectile::Precache() // orig 0xb7690
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/sqflare.spr");
	PRECACHE_SOUND("debris/beamstart4.wav");
	m_usEvent = PRECACHE_EVENT(1, "events/rpgsmimpact.sc");
}

void CSMProjectile::HomeThink() // orig 0xb7800
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.2;

	if (pev->enemy && !UTIL_IsValidEntity(pev->enemy))
		pev->enemy = NULL;

	if (pev->enemy)
	{
		CBaseEntity* pEnemy = CBaseEntity::Instance(pev->enemy);
		if (!pEnemy || !pEnemy->IsAlive() || pEnemy->pev->deadflag != DEAD_NO || pEnemy->pev->takedamage != DAMAGE_AIM)
			pev->enemy = NULL;
	}

	if (pev->dmgtime < gpGlobals->time)
	{
		// burnt out
		ExplodeTouch(this);
		return;
	}

	if (!pev->enemy)
	{
		pev->velocity = pev->velocity.Normalize() * pev->speed;
		return;
	}

	CBaseEntity* pEnemy = CBaseEntity::Instance(pev->enemy);
	if (!pEnemy)
		return;

	// lead the target by up to a quarter second of its movement
	float flAccel = pev->armorvalue;
	float flLead = std::clamp((pEnemy->Center() - pev->origin).Length() / pev->speed, 0.0f, 0.25f);
	Vector vecLead = pEnemy->pev->velocity * flLead;
	Vector vecAim = (pEnemy->Center() + vecLead) - pev->origin;
	const float flDist = vecAim.Length();
	Vector vecDir = vecAim.Normalize();
	Vector vecHeading = pev->velocity.Normalize();
	const float flDot = DotProduct(vecDir, vecHeading);

	if (flDot <= 0.01)
		flAccel *= 0.5; // facing away: turn slower
	else if ((flDot > 0.76 && flDist < 256) || flDist < 64)
		flAccel *= 8; // close in: turn hard

	pev->velocity = pev->velocity + vecDir * flAccel * flInterval;
	pev->velocity = pev->velocity.Normalize() * pev->speed;
}

void CSMProjectile::ExplodeTouch(CBaseEntity* pOther) // orig 0xb7d40
{
	entvars_t* pevOwner = pev;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pevOwner = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecSpot = pev->origin;

	if (pOther->IsBSPModel())
	{
		// hit the world: the impact sits just off the wall, facing out of it
		vecSpot = tr.vecEndPos + tr.vecPlaneNormal * 4.0;
		PLAYBACK_EVENT_FULL(0, edict(), m_usEvent, 0.0, vecSpot, g_vecZero, 0.0, 0.0, UTIL_RC2PackNormal(-tr.vecPlaneNormal), 0, true, false);
	}
	else
	{
		PLAYBACK_EVENT_FULL(0, edict(), m_usEvent, 0.0, vecSpot, g_vecZero, 0.0, 0.0, UTIL_RC2PackNormal(pev->velocity.Normalize()), 0, false, false);

		if (pOther->IsAlive())
		{
			// VERIFY: CBasePlayer +0x778 (RC2's) suppresses the blood on players
			if (!pOther->IsActivePlayer() || !static_cast<CBasePlayer*>(pOther)->m_bRC2State778)
				UTIL_BloodDrips(tr.vecEndPos, tr.vecPlaneNormal, pOther->GetEffectedEnt()->BloodColor(), 60);
		}
	}

	RC2RadiusDamage(pevOwner, pev, edict(), pev->origin, RC2Pref(RC2P_RPG_SphereMineProjectileBlastRadius).f,
		RC2Pref(RC2P_RPG_SphereMineProjectileBlastDamage).f, RC2Pref(RC2P_RPG_SphereMineProjectileBlastFalloffExponent).f, DMG_BLAST, 0);

	SetThink(NULL);
	SetTouch(NULL);
	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
}
