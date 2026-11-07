/***
*
*	Rocket Crowbar 2: suckmine.cpp (file name ours), the RPG's suck mine (suck_mine).
*	Original functions 0xb49c0-0xb5f60 of rc2.dll.
*
*	A sentry turret the RPG lobs. Once it lands it deploys, turns towards the best live target
*	in [RPG] SuckMineLockDist (closer and more in front is better), and when aimed locks on:
*	a player gets a tether to the mine (pulled by pm_shared), anything else is pulled through
*	the mine's GetPullOrg (CBaseEntity::ApplyExternalForce). The client draws the rope (CLTE
*	0x1f). It lets go past SuckMineSnapDist, inside SuckMineIgnoreDist, or when it loses sight.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "rc2prefs.h"
#include "p6stubs.h"

extern int gmsgCLTE; // RC2's "CLTE" user message (orig .bss 0x18f4b8); TEMP STUB in dlls/rc2/playerride.cpp

#define CLTE_SUCKMINE_ROPE 0x1f // ours: the rope from the mine to its target
#define CLTE_KILL 0xfe			// ours: kills a client effect of the entity (the subtype follows)

class CSuckMine : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_MACHINE; } // orig 0xb4a90
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool GetPullOrg(CBaseEntity* pPulled, Vector& vecOrigin, float& flForce, float& flOther) override;
	void ReleasePull(CBaseEntity* pPulled) override;
	int GetGuassHull() override { return 1; }				 // orig 0xb4a70
	float GetGuassDistOut() override { return 56.0f; }		 // orig 0xb4a80
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x14a1; } // orig 0xb4a60

	void EXPORT FlyingThink();
	void EXPORT ActivateTouch(CBaseEntity* pOther);
	void EXPORT ActiveThink();
	void EXPORT DieThink();

	void LetGo(); // ours: drop the tether and the rope

	float m_flLastThink;	   // orig +0x2b8, name ours
	int m_iTurnDir;			   // orig +0x2bc, name ours: 1 or -1, the idle spin direction
	EHANDLE m_hTarget;		   // orig +0x2c0, name ours
	int m_iTetherSlot;		   // orig +0x2c8, name ours: the target player's tether, -1 for none
	bool m_bLocked;			   // orig +0x2cc, name ours
	float m_flLockDist;		   // orig +0x2d0, name ours: [RPG] SuckMineLockDistMin..Max
	float m_flIgnoreDist;	   // orig +0x2d4, name ours: [RPG] SuckMineIgnoreDistMin..Max
	float m_flSnapDist;		   // orig +0x2d8, name ours: [RPG] SuckMineSnapDistMin..Max
	float m_flPullStrength;	   // orig +0x2dc, name ours: [RPG] SuckMinePullStrengthMin..Max
	float m_flTetherLength;	   // orig +0x2e0, name ours: m_flIgnoreDist - 20
	Vector m_vecTurretAngles;  // orig +0x2e4, name ours: x pitch (bone controller 1), y yaw (0)
	Vector m_vecTurretGoal;	   // orig +0x2f0, name ours
};

// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("suck_mine") (0xf5a20); the shared macro doesn't yet
LINK_ENTITY_TO_CLASS(suck_mine, CSuckMine); // orig 0xb49c0

void CSuckMine::Precache() // orig 0xb4aa0
{
	PRECACHE_MODEL("models/sentry.mdl");
	PRECACHE_MODEL("sprites/rope.spr");
	PRECACHE_SOUND("turret/tu_die.wav");
	PRECACHE_SOUND("turret/tu_deploy.wav");
}

// No think: the RPG sets FlyingThink when it fires the mine.
void CSuckMine::Spawn() // orig 0xb4ad0
{
	Precache();
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_YES;
	pev->deadflag = DEAD_NO;
	pev->health = RC2Pref(RC2P_RPG_SuckMineHealth).f;
	pev->gravity = RC2Pref(RC2P_RPG_SuckMineGravity).f;
	pev->sequence = 0;
	ResetSequenceInfo();
	InitBoneControllers();
	SET_MODEL(ENT(pev), "models/sentry.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 64));
	UTIL_SetOrigin(pev, pev->origin);

	m_vecTurretAngles = g_vecZero;
	m_vecTurretGoal = m_vecTurretAngles;
	m_iTurnDir = RANDOM_LONG(0, 1) != 0 ? -1 : 1;
	m_flLockDist = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SuckMineLockDistMin).f, RC2Pref(RC2P_RPG_SuckMineLockDistMax).f);
	m_flSnapDist = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SuckMineSnapDistMin).f, RC2Pref(RC2P_RPG_SuckMineSnapDistMax).f);
	m_flIgnoreDist = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SuckMineIgnoreDistMin).f, RC2Pref(RC2P_RPG_SuckMineIgnoreDistMax).f);
	m_flPullStrength = RANDOM_FLOAT(RC2Pref(RC2P_RPG_SuckMinePullStrengthMin).f, RC2Pref(RC2P_RPG_SuckMinePullStrengthMax).f);
	m_iTetherSlot = -1;
	m_flTetherLength = m_flIgnoreDist - 20;
	m_hTarget = NULL;
	m_bLocked = false;
	SetTouch(&CSuckMine::ActivateTouch);
	pev->dmg_inflictor = pev->owner; // the thrower, kept for the kill credit
}

void CSuckMine::FlyingThink() // orig 0xb4cc0
{
	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;
	ApplyExternalForce(flInterval);
}

// Touching anything but its thrower: deploy.
void CSuckMine::ActivateTouch(CBaseEntity* pOther) // orig 0xb4d00
{
	if (pOther->edict() == pev->owner)
		return;

	pev->sequence = 3;
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;
	m_flLastThink = gpGlobals->time;
	EMIT_SOUND_DYN(edict(), CHAN_BODY, "turret/tu_deploy.wav", 0.8, 0.8, 0, 100);
	SetTouch(NULL);
	SetThink(&CSuckMine::ActiveThink);
	pev->nextthink = gpGlobals->time + 0.01;
}

void CSuckMine::ActiveThink() // orig 0xb4da0
{
	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	ApplyExternalForce(flInterval);
	StudioFrameAdvance(0);
	pev->nextthink = gpGlobals->time + 0.1;

	// keep the thrower as owner (so it doesn't touch it) until it is clear of the thrower
	if (UTIL_IsValidEntity(pev->owner))
	{
		edict_t* pentOwner = pev->owner;
		pev->owner = NULL;
		TraceResult tr;
		UTIL_TraceHull(Vector(pev->origin.x, pev->origin.y, pev->origin.z + 36), Vector(pev->origin.x, pev->origin.y, pev->origin.z + 37),
			dont_ignore_monsters, human_hull, edict(), &tr);
		if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
			pev->owner = pentOwner;
	}
	else
	{
		pev->owner = NULL;
	}

	// deploying: wait for the deploy animation, then idle
	if (pev->sequence != 2)
	{
		if (!m_fSequenceFinished)
			return;
		pev->sequence = 2;
		ResetSequenceInfo();
		pev->frame = 0;
		pev->air_finished = gpGlobals->time + 1.0;
		return;
	}

	pev->framerate = 0.1;
	if (pev->air_finished < gpGlobals->time)
		pev->air_finished = gpGlobals->time + 1.0; // VERIFY: nothing else reads it here

	Vector vecToTarget;
	if (m_hTarget)
	{
		EHANDLE hOld = m_hTarget;
		Vector vecSrc(pev->origin.x, pev->origin.y, pev->origin.z + 48);
		Vector vecTo = m_hTarget->Center() - vecSrc;
		vecToTarget = vecTo;
		float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);

		bool bKeep = false;
		if (flDist <= m_flSnapDist && !(flDist < m_flIgnoreDist) && m_hTarget->IsAlive() && m_hTarget->pev->takedamage == DAMAGE_AIM && m_hTarget->pev->deadflag == DEAD_NO)
		{
			bKeep = true;
			if (pev->dmgtime < gpGlobals->time)
			{
				// recheck the sight five times a second
				pev->dmgtime = gpGlobals->time + 0.2;
				CBaseEntity* pTarget = m_hTarget;
				TraceResult tr;
				UTIL_TraceLine(Vector(pev->origin.x, pev->origin.y, pev->origin.z + 48), pTarget->Center(), dont_ignore_monsters, edict(), &tr);
				if (tr.flFraction < 1.0 && tr.pHit != m_hTarget->edict())
					bKeep = false;
			}
		}
		if (!bKeep)
			m_hTarget = NULL;

		if (!m_hTarget)
		{
			if (hOld)
			{
				if (!m_bLocked)
					goto unlocked;
				if (m_iTetherSlot != -1 && hOld->IsPlayer())
				{
					CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(hOld->edict());
					if (pPlayer)
						pPlayer->RemoveTether(m_iTetherSlot);
				}
			}
			if (m_bLocked)
			{
				MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
				WRITE_BYTE(CLTE_KILL);
				WRITE_SHORT(ENTINDEX(edict()));
				WRITE_BYTE(CLTE_SUCKMINE_ROPE);
				MESSAGE_END();
			}
		unlocked:
			m_iTetherSlot = -1;
			m_bLocked = false;
		}
	}

	if (m_bLocked && !m_hTarget)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(CLTE_KILL);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(CLTE_SUCKMINE_ROPE);
		MESSAGE_END();
		m_bLocked = false;
	}

	if (!m_hTarget)
	{
		// look for one: 3 for closeness plus 1 for being in front of the turret's aim
		float flBest = -9999;
		UTIL_MakeVectors(m_vecTurretAngles + pev->angles);
		float flFwdX, flFwdY;
		float flFwdLen = sqrt(gpGlobals->v_forward.x * gpGlobals->v_forward.x + gpGlobals->v_forward.y * gpGlobals->v_forward.y);
		if (flFwdLen == 0)
		{
			flFwdX = 0;
			flFwdY = 0;
		}
		else
		{
			flFwdX = gpGlobals->v_forward.x * (1.0 / flFwdLen);
			flFwdY = gpGlobals->v_forward.y * (1.0 / flFwdLen);
		}

		CBaseEntity* pOwner = NULL;
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pOwner = CBaseEntity::Instance(pev->dmg_inflictor);

		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityInSphere(pEnt, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 48), m_flLockDist)) != NULL)
		{
			if (pEnt->edict() == pev->dmg_inflictor || pEnt->edict() == edict())
				continue;
			if (!pEnt->IsAlive() || pEnt->pev->takedamage != DAMAGE_AIM || pEnt->pev->deadflag != DEAD_NO)
				continue;
			if ((pEnt->GetRCCaps(pOwner, this) & 0x10) == 0)
				continue;
			if ((pEnt->GetRCCaps2(pOwner, this) & 8) != 0)
				continue;
			if (pEnt->IsActivePlayer())
			{
				CBasePlayer* pPlayer = (CBasePlayer*)pEnt;
				if (pPlayer->m_bRC2State778 || pPlayer->m_iRidingIndex != 0)
					continue;
			}
			if (pOwner && pOwner->IsPlayer() && !g_pGameRules->PlayerCanAffect(pOwner, pEnt))
				continue;

			Vector vecSrc(pev->origin.x, pev->origin.y, pev->origin.z + 48);
			Vector vecTo = pEnt->Center() - vecSrc;
			float flDist = sqrt(vecTo.y * vecTo.y + vecTo.z * vecTo.z + vecTo.x * vecTo.x);
			vecToTarget = vecTo; // the original's aim below uses the last candidate measured, not the one picked
			if (flDist > m_flLockDist || flDist < m_flIgnoreDist)
				continue;

			float flNear = 1.0 - flDist / m_flLockDist;
			if (flNear < 0)
				flNear = 0;
			else if (flNear > 1)
				flNear = 1;

			float flLen2D = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y);
			float flDirX, flDirY;
			if (flLen2D == 0)
			{
				flDirX = 0;
				flDirY = 0;
			}
			else
			{
				flDirX = vecTo.x * (1.0 / flLen2D);
				flDirY = vecTo.y * (1.0 / flLen2D);
			}
			float flFront = (flDirY * flFwdY + flDirX * flFwdX + 1.0) * 0.5;
			if (flFront < 0)
				flFront = 0;
			else if (flFront > 1)
				flFront = 1;

			float flScore = flNear * 3.0 + flFront;
			if (flScore < flBest)
				continue;

			TraceResult tr;
			UTIL_TraceLine(Vector(pev->origin.x, pev->origin.y, pev->origin.z + 4), pEnt->Center(), dont_ignore_monsters, edict(), &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
				continue;

			m_hTarget = pEnt;
			flBest = flScore;
		}
		pev->dmgtime = gpGlobals->time + 0.2;
	}

	if (!m_hTarget)
	{
		// idle: spin at 90 a second, level
		m_vecTurretGoal.y = UTIL_AngleMod(m_iTurnDir * flInterval * 90.0 + m_vecTurretGoal.y);
		m_vecTurretGoal.x = 0;
	}
	else
	{
		// vecToTarget: the kept target's, or for one found just now the last candidate the
		// search measured (kept as the original does it)
		Vector vecDir = vecToTarget.Normalize();
		Vector vecAngles = UTIL_VecToAngles(vecDir);
		m_vecTurretGoal.y = UTIL_AngleMod(vecAngles.y - pev->angles.y);
		float flPitch = -vecAngles.x;
		m_vecTurretGoal.x = flPitch;
		if (flPitch < -180)
			m_vecTurretGoal.x = flPitch + 360;
		if (m_vecTurretGoal.x > 180)
			m_vecTurretGoal.x = m_vecTurretGoal.x - 360;
		if (m_vecTurretGoal.x < -90)
			m_vecTurretGoal.x = -90;
		else if (m_vecTurretGoal.x > 60)
			m_vecTurretGoal.x = 60;
	}

	if (m_vecTurretGoal != m_vecTurretAngles)
	{
		m_vecTurretAngles.y = UTIL_ApproachAngle(m_vecTurretGoal.y, m_vecTurretAngles.y, flInterval * 1440);
		m_vecTurretAngles.x = UTIL_Approach(m_vecTurretGoal.x, m_vecTurretAngles.x, flInterval * 360);
		SetBoneController(0, m_vecTurretAngles.y);
		SetBoneController(1, m_vecTurretAngles.x);
	}

	if (!m_hTarget)
		return;

	// aimed: lock on
	if (UTIL_AngleDiff(m_vecTurretAngles.y, m_vecTurretGoal.y) < 10 && fabs(m_vecTurretAngles.x - m_vecTurretGoal.x) < 10)
	{
		if (!m_bLocked)
		{
			if (m_hTarget->IsPlayer())
			{
				CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(m_hTarget->edict());
				if (pPlayer)
					m_iTetherSlot = pPlayer->AddTether(entindex(), Vector(0, 0, 48), m_flPullStrength, m_flTetherLength);
			}
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
			WRITE_BYTE(CLTE_SUCKMINE_ROPE);
			WRITE_SHORT(ENTINDEX(edict()));
			WRITE_SHORT(ENTINDEX(m_hTarget->edict()));
			MESSAGE_END();
		}
		m_bLocked = true;
	}
	if (m_bLocked)
		pev->framerate = 1.0;
}

// Folds up, then fades out once the death animation is done.
void CSuckMine::DieThink() // orig 0xb5bc0
{
	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	ApplyExternalForce(flInterval);
	StudioFrameAdvance(0);
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_vecTurretGoal != m_vecTurretAngles)
	{
		m_vecTurretAngles.y = UTIL_ApproachAngle(m_vecTurretGoal.y, m_vecTurretAngles.y, flInterval * 1440);
		m_vecTurretAngles.x = UTIL_Approach(m_vecTurretGoal.x, m_vecTurretAngles.x, flInterval * 360);
		SetBoneController(0, m_vecTurretAngles.y);
		SetBoneController(1, m_vecTurretAngles.x);
	}

	if (m_fSequenceFinished && pev->dmgtime + 4.0 < gpGlobals->time)
	{
		SetThink(&CSuckMine::SUB_StartFadeOut);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CSuckMine::LetGo() // ours: the release shared by Killed and ReleasePull
{
	if (m_bLocked)
	{
		if (m_iTetherSlot != -1 && m_hTarget->IsPlayer())
		{
			CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(m_hTarget->edict());
			if (pPlayer)
				pPlayer->RemoveTether(m_iTetherSlot);
		}
		if (m_bLocked)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(CLTE_KILL);
			WRITE_SHORT(ENTINDEX(edict()));
			WRITE_BYTE(CLTE_SUCKMINE_ROPE);
			MESSAGE_END();
		}
	}
	m_iTetherSlot = -1;
	m_bLocked = false;
}

void CSuckMine::Killed(entvars_t* pevAttacker, int iGib) // orig 0xb5d10
{
	pev->sequence = 5;
	ResetSequenceInfo();
	if (m_hTarget)
		LetGo();

	pev->solid = SOLID_NOT;
	EMIT_SOUND_DYN(edict(), CHAN_BODY, "turret/tu_die.wav", 0.8, 0.8, 0, 100);
	pev->dmgtime = gpGlobals->time;
	pev->deadflag = DEAD_DEAD;
	m_vecTurretGoal.x = 90; // the barrel droops
	SetThink(&CSuckMine::DieThink);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
}

// What it pulls (not players, whose tether pm_shared handles): to 48 above the mine, at
// m_flPullStrength past m_flTetherLength of slack.
bool CSuckMine::GetPullOrg(CBaseEntity* pPulled, Vector& vecOrigin, float& flForce, float& flOther) // orig 0xb5e90
{
	if (!m_hTarget)
		return false;
	if (m_hTarget->edict() != pPulled->edict())
		return false;
	if (!m_bLocked)
		return false;

	vecOrigin = Vector(pev->origin.x, pev->origin.y, pev->origin.z + 48);
	flForce = m_flPullStrength;
	flOther = m_flTetherLength;
	return true;
}

void CSuckMine::ReleasePull(CBaseEntity* pPulled) // orig 0xb5f60
{
	if (!m_hTarget || m_hTarget->edict() != pPulled->edict())
		return;
	LetGo();
}
