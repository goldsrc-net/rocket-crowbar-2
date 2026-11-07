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
#include "soundent.h"
#include "gamerules.h"

enum w_squeak_e
{
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN,
};

#include "rc2prefs.h"
#ifdef CLIENT_DLL
#define g_iPlayWeaponEvents g_runfuncs // the original's one event flag, g_runfuncs on the client
extern bool g_runfuncs;				   // cl_dll/com_weapons.cpp
inline int RC2PrefWord(rc2pref_e index) { return (unsigned short)RC2Pref(index).s; } // as p8_needs.h's
static void UTIL_SetMaxTime(float& flTime, float flBase, float flAdd) { UTIL_ExtendTime(&flTime, flBase, flAdd); }
#else
#include "rc2/p8_needs.h"
#endif

#ifndef CLIENT_DLL

#include <algorithm>

#include "rc2/p7_extern.h"

// Rocket Crowbar 2's snark: rc2.ini's [Snark] keys replace the skill values, and while it hunts
// it fires snark_project bolts at what it is facing. CMiniGrunt (mini_grunt) is the snark
// launcher's grunt: it runs along the ground toward a target and bursts on contact.

class CSqueakGrenade : public CGrenade
{
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	void EXPORT SuperBounceTouch(CBaseEntity* pOther);
	void EXPORT HuntThink();
	int BloodColor() override { return BLOOD_COLOR_YELLOW; } // orig 0xd0a80
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void GibMonster() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x37ff; } // orig 0xd0a70. VERIFY: meaning of the bits

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	static float m_flNextBounceSoundTime;

	// CBaseEntity *m_pTarget;
	float m_flDie;		 // orig +0x2b8
	Vector m_vecTarget;	 // orig +0x2bc
	float m_flNextHunt;	 // orig +0x2c8
	float m_flNextHit;	 // orig +0x2cc
	Vector m_posPrev;	 // orig +0x2d0
	EHANDLE m_hOwner;	 // orig +0x2dc
	int m_iMyClass;		 // orig +0x2e4
	float m_flNextFire; // orig +0x2e8, name ours: when the snark may fire its next snark_project
};

float CSqueakGrenade::m_flNextBounceSoundTime = 0;

LINK_ENTITY_TO_CLASS(monster_snark, CSqueakGrenade); // orig 0xd09d0
TYPEDESCRIPTION CSqueakGrenade::m_SaveData[] =
	{
		DEFINE_FIELD(CSqueakGrenade, m_flDie, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_vecTarget, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakGrenade, m_flNextHunt, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_flNextHit, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_posPrev, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CSqueakGrenade, m_hOwner, FIELD_EHANDLE),
};

// RC2's Save/Restore chain to CBaseMonster's (SDK 2.3: CGrenade's, which has no save table of
// its own, so the result is the same)
IMPLEMENT_SAVERESTORE(CSqueakGrenade, CGrenade); // orig 0xd0a90 Save, orig 0xd0ac0 Restore

int CSqueakGrenade::Classify()
{
	if (m_iMyClass != 0)
		return m_iMyClass; // protect against recursion

	if (m_hEnemy != NULL)
	{
		m_iMyClass = CLASS_INSECT; // no one cares about it
		switch (m_hEnemy->Classify())
		{
		case CLASS_PLAYER:
		case CLASS_HUMAN_PASSIVE:
		case CLASS_HUMAN_MILITARY:
			m_iMyClass = 0;
			return CLASS_ALIEN_MILITARY; // barney's get mad, grunts get mad at it
		}
		m_iMyClass = 0;
	}

	return CLASS_ALIEN_BIOWEAPON;
}

void CSqueakGrenade::Spawn() // orig 0xd0b60
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CSqueakGrenade::SuperBounceTouch);
	SetThink(&CSqueakGrenade::HuntThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time + 1E6;

	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = RC2Pref(RC2P_Snark_SnarkHealth).f;
	pev->gravity = RC2Pref(RC2P_Snark_SnarkGravity).f;
	pev->friction = RC2Pref(RC2P_Snark_SnarkFriction).f;

	pev->dmg = RC2Pref(RC2P_Snark_SnarkPopDamage).f;

	m_flNextFire = gpGlobals->time + RC2Pref(RC2P_Snark_SnarkStartNextFireDelay).f;
	m_flDie = gpGlobals->time + RC2Pref(RC2P_Snark_SnarkLifetime).f;
	// a later air_finished set by whoever made the snark wins
	if (gpGlobals->time < pev->air_finished) // VERIFY: who sets pev->air_finished (orig pev +0x1fc) on a snark
		m_flDie = pev->air_finished;

	m_flFieldOfView = RC2Pref(RC2P_Snark_SnarkFOVDot).f;

	if (pev->owner)
		m_hOwner = Instance(pev->owner);

	m_flNextBounceSoundTime = gpGlobals->time; // reset each time a snark is spawned.

	pev->sequence = WSQUEAK_RUN;
	ResetSequenceInfo();
}

void CSqueakGrenade::Precache() // orig 0xd0d30
{
	PRECACHE_MODEL("models/w_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("squeek/sqk_die1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	PRECACHE_SOUND("squeek/sqk_deploy1.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	UTIL_PrecacheOther("snark_project");
}


void CSqueakGrenade::Killed(entvars_t* pevAttacker, int iGib) // orig 0xd0db0
{
	pev->model = iStringNull; // make invisible
	SetThink(&CSqueakGrenade::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;

	// since squeak grenades never leave a body behind, clear out their takedamage now.
	// Squeaks do a bit of radius damage when they pop, and that radius damage will
	// continue to call this function unless we acknowledge the Squeak's death now. (sjb)
	pev->takedamage = DAMAGE_NO;

	// play squeek blast
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5, 0, PITCH_NORM);

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, SMALL_EXPLOSION_VOLUME, 3.0);

	UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 80);

	if (m_hOwner != NULL)
		RadiusDamage(pev, m_hOwner->pev, pev->dmg, CLASS_NONE, DMG_BLAST);
	else
		RadiusDamage(pev, pev, pev->dmg, CLASS_NONE, DMG_BLAST);

	// reset owner so death message happens
	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();

	CBaseMonster::Killed(pevAttacker, GIB_ALWAYS);
}

void CSqueakGrenade::GibMonster()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200);
}

// Rocket Crowbar 2: snarks take no damage from snark bolts
bool CSqueakGrenade::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) // orig 0xd1a80
{
	if (pevInflictor && FClassnameIs(pevInflictor, "snark_project"))
		return false;
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}


void CSqueakGrenade::HuntThink() // orig 0xd0f00
{
	// ALERT( at_console, "think\n" );

	if (!IsInWorld())
	{
		SetTouch(NULL);
		UTIL_Remove(this);
		return;
	}

	const float flInterval = StudioFrameAdvance(0.0);
	pev->nextthink = gpGlobals->time + 0.1;
	ApplyExternalForce(flInterval);

	// explode when ready
	if (gpGlobals->time >= m_flDie)
	{
		g_vecAttackDir = pev->velocity.Normalize();
		pev->health = -1;
		Killed(pev, 0);
		return;
	}

	// float
	if (pev->waterlevel != 0)
	{
		if (pev->movetype == MOVETYPE_BOUNCE)
		{
			pev->movetype = MOVETYPE_FLY;
		}
		pev->velocity = pev->velocity * 0.9;
		pev->velocity.z += 8.0;
	}
	else
	{
		// SDK 2.3's "else if (pev->movetype = MOVETYPE_FLY)", kept as RC2 has it (halflife-updated
		// fixed the assignment): out of water the snark is always set back to bounce
		pev->movetype = MOVETYPE_FLY;
		pev->movetype = MOVETYPE_BOUNCE;
	}

	// return if not time to hunt
	if (m_flNextHunt > gpGlobals->time)
		return;

	m_flNextHunt = gpGlobals->time + 2.0;

	Vector vecDir;

	Vector vecFlat = pev->velocity;
	vecFlat.z = 0;
	vecFlat = vecFlat.Normalize();

	UTIL_MakeVectors(pev->angles);

	if (m_hEnemy == NULL || !m_hEnemy->IsAlive())
	{
		// find target, bounce a bit towards it.
		Look((int)RC2Pref(RC2P_Snark_SnarkSightDistance).f);
		m_hEnemy = BestVisibleEnemy();

		if (m_flNextFire < gpGlobals->time)
			m_flNextFire = gpGlobals->time + RANDOM_FLOAT(0.1, 0.15);
	}

	// squeek if it's about time blow up
	if ((m_flDie - gpGlobals->time <= 0.5) && (m_flDie - gpGlobals->time >= 0.3))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(0, 0x3F));
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 256, 0.25);
	}

	if (m_hEnemy != NULL)
	{
		if (FVisible(m_hEnemy))
		{
			vecDir = m_hEnemy->EyePosition() - pev->origin;
			m_vecTarget = vecDir.Normalize();
		}

		float flVel = pev->velocity.Length();
		float flAdj = 50.0 / (flVel + 10.0);

		if (flAdj > 1.2)
			flAdj = 1.2;

		pev->velocity = pev->velocity * flAdj + m_vecTarget * RC2Pref(RC2P_Snark_SnarkGroundVel).f;

		// Rocket Crowbar 2: fire a snark bolt when facing the target
		UTIL_MakeAimVectors(pev->angles);
		if (DotProduct(gpGlobals->v_forward, m_vecTarget) > RC2Pref(RC2P_Snark_SnarkFireDot).f && m_flNextFire < gpGlobals->time)
		{
			m_flNextFire = gpGlobals->time + RANDOM_FLOAT(RC2Pref(RC2P_Snark_SnarkFireTimeMin).f, RC2Pref(RC2P_Snark_SnarkFireTimeMax).f);
			CBaseEntity* pBolt = CBaseEntity::Create("snark_project", pev->origin, pev->angles, edict());
			// VERIFY: the original uses the result unchecked; Create can return NULL
			if (m_hOwner != NULL)
				pBolt->pev->dmg_inflictor = m_hOwner->edict();
			pBolt->pev->velocity = gpGlobals->v_forward * pBolt->pev->speed;
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart10.wav", 0.5, 0.9, 0, RANDOM_LONG(120, 130));
		}
	}

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		pev->avelocity = Vector(0, 0, 0);
	}
	else
	{
		if (pev->avelocity == Vector(0, 0, 0))
		{
			pev->avelocity.x = RANDOM_FLOAT(-100, 100);
			pev->avelocity.z = RANDOM_FLOAT(-100, 100);
		}
	}

	if ((pev->origin - m_posPrev).Length() < 1.0)
	{
		pev->velocity.x = RANDOM_FLOAT(-100, 100);
		pev->velocity.y = RANDOM_FLOAT(-100, 100);
	}
	m_posPrev = pev->origin;

	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->angles.z = 0;
	pev->angles.x = 0;
}


void CSqueakGrenade::SuperBounceTouch(CBaseEntity* pOther) // orig 0xd1700
{
	float flpitch;

	TraceResult tr = UTIL_GetGlobalTrace();

	// don't hit the guy that launched this grenade
	if (pev->owner && pOther->edict() == pev->owner)
		return;

	// at least until we've bounced once
	pev->owner = NULL;

	pev->angles.x = 0;
	pev->angles.z = 0;

	// avoid bouncing too much
	if (m_flNextHit > gpGlobals->time)
		return;

	// higher pitch as squeeker gets closer to detonation time (RC2: over its rc2.ini lifetime)
	flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / RC2Pref(RC2P_Snark_SnarkLifetime).f);

	// RC2 has SDK 2.3's test, without halflife-updated's FL_WORLDBRUSH exclusion
	if (0 != pOther->pev->takedamage && m_flNextAttack < gpGlobals->time)
	{
		// attack!

		// make sure it's me who has touched them
		if (tr.pHit == pOther->edict())
		{
			// and it's not another squeakgrenade
			if (tr.pHit->v.modelindex != pev->modelindex)
			{
				// ALERT( at_console, "hit enemy\n");
				ClearMultiDamage();
				pOther->TraceAttack(pev, RC2Pref(RC2P_Snark_SnarkBiteDamage).f, gpGlobals->v_forward, &tr, DMG_SLASH);
				if (m_hOwner != NULL)
					ApplyMultiDamage(pev, m_hOwner->pev);
				else
					ApplyMultiDamage(pev, pev);

				// add more explosion damage. RC2 adds the skill value, not rc2.ini's
				// [Snark] SnarkBiteGainPopDamage (unused), and caps it at SnarkMaxPopDamage
				pev->dmg += gSkillData.snarkDmgPop;
				if (pev->dmg > RC2Pref(RC2P_Snark_SnarkMaxPopDamage).f)
					pev->dmg = RC2Pref(RC2P_Snark_SnarkMaxPopDamage).f;
				m_flDie += RC2Pref(RC2P_Snark_SnarkBiteGainLife).f; // add more life

				// make bite sound
				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1.0, ATTN_NORM, 0, (int)flpitch);
				m_flNextAttack = gpGlobals->time + 0.5;
			}
		}
		else
		{
			// ALERT( at_console, "been hit\n");
		}
	}

	m_flNextHit = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time;

	if (g_pGameRules->IsMultiplayer())
	{
		// in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
		if (gpGlobals->time < m_flNextBounceSoundTime)
		{
			// too soon!
			return;
		}
	}

	if ((pev->flags & FL_ONGROUND) == 0)
	{
		// play bounce sound
		float flRndSound = RANDOM_FLOAT(0, 1);

		if (flRndSound <= 0.33)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt1.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else if (flRndSound <= 0.66)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, (int)flpitch);
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 256, 0.25);
	}
	else
	{
		// skittering sound
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 100, 0.1);
	}

	m_flNextBounceSoundTime = gpGlobals->time + 0.5; // half second.
}


//=========================================================
// CMiniGrunt (mini_grunt): the snark weapon's grunt. It runs along the ground at
// [Snark] GruntBaseSpeed, picks the best target in a cone ahead, steers its heading toward
// where that target will be, and bursts (event minigruntblast.sc) on touching anything
// damageable, a wall it can't climb, or when its owner goes or its life runs out.
//=========================================================
class CMiniGrunt : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x480; } // orig 0xd1ba0. VERIFY: meaning of the bits

	void EXPORT AimThink();
	void EXPORT DoTouch(CBaseEntity* pOther);
	void StepSound(); // name ours

	// none saved; names ours
	bool m_fLanded;			  // orig +0x2b8 (byte): has touched ground it can stand on
	Vector m_vecGroundNormal; // orig +0x2bc: the plane it last landed on
	Vector m_vecHeading;	  // orig +0x2c8: unit direction it runs in
	float m_flSpeedScale;	  // orig +0x2d4: 1..GruntMaxSpeedScale, scales speed and framerate
	EHANDLE m_hTarget;		  // orig +0x2d8
	float m_flNextStepSound;  // orig +0x2e0
	unsigned short m_usBlast; // orig +0x2e4, "events/minigruntblast.sc"
};

LINK_ENTITY_TO_CLASS(mini_grunt, CMiniGrunt); // orig 0xd1b00

static const char* g_pszMiniGruntTalk[] = // name ours
	{
		"hgrunt/bastard!.wav",
		"hgrunt/bogies!.wav",
		"hgrunt/go!.wav",
		"hgrunt/movement!.wav",
		"hgrunt/shit!.wav",
};

void CMiniGrunt::Precache() // orig 0xd1bb0
{
	PRECACHE_MODEL("models/minigrunt.mdl");
	for (const char* pszSound : g_pszMiniGruntTalk)
		PRECACHE_SOUND(pszSound);
	// the original precaches npc_step2 twice and never npc_step4, which StepSound plays
	PRECACHE_SOUND("common/npc_step1.wav");
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	m_usBlast = PRECACHE_EVENT(1, "events/minigruntblast.sc");
}

void CMiniGrunt::Spawn() // orig 0xd1c30
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->friction = 1e-5;
	SET_MODEL(ENT(pev), "models/minigrunt.mdl");
	UTIL_SetSize(pev, Vector(-2, -2, 0), Vector(2, 2, 8));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("mini_grunt");

	SetThink(&CMiniGrunt::AimThink);
	SetTouch(&CMiniGrunt::DoTouch);
	pev->nextthink = gpGlobals->time;

	pev->sequence = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = true;

	pev->health = 1;
	pev->takedamage = DAMAGE_YES;
	pev->dmg_inflictor = pev->owner; // the player who sent it, kept after owner is cleared
	pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_Snark_GruntMaxLife).f;
	pev->speed = RC2Pref(RC2P_Snark_GruntBaseSpeed).f;

	m_fLanded = false;
	m_flNextStepSound = gpGlobals->time;
	m_hTarget = NULL;
	m_flSpeedScale = 1.0;

	pev->angles.z = 0;
	pev->angles.x = 0;
	UTIL_MakeVectors(pev->angles);
	m_vecHeading = gpGlobals->v_forward;
}

void CMiniGrunt::Killed(entvars_t* pevAttacker, int iGib) // orig 0xd30d0
{
	// bursts as if it had touched itself
	DoTouch(this);
}

void CMiniGrunt::AimThink() // orig 0xd1de0
{
	if (!UTIL_IsValidEntity(pev->dmg_inflictor) || pev->dmgtime < gpGlobals->time)
	{
		DoTouch(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	const float flInterval = StudioFrameAdvance(0.0);
	ApplyExternalForce(flInterval);

	pev->angles = UTIL_VecToAngles(pev->velocity.Normalize());
	pev->angles.z = 0;
	pev->angles.x = 0;

	if (m_hTarget != NULL)
	{
		if (!m_hTarget->IsAlive())
			m_hTarget = NULL;

		if (m_hTarget != NULL)
		{
			// the original tests the unnormalized offset, x and y only
			const Vector vecTo = m_hTarget->Center() - pev->origin;
			if (vecTo.x * m_vecHeading.x + vecTo.y * m_vecHeading.y < RC2Pref(RC2P_Snark_GruntSightDotFOV).f)
				m_hTarget = NULL;
		}
	}

	if (m_hTarget == NULL)
	{
		const Vector vecSrc = pev->origin + Vector(0, 0, 6);
		const Vector vecLook = pev->velocity.Normalize();
		const float flRange = RC2Pref(RC2P_Snark_GruntSightDistance).f;
		const float flMinDot = RC2Pref(RC2P_Snark_GruntSightDotFOV).f;
		float flBest = -9999;
		CBaseEntity* pBest = NULL;

		CBaseEntity* pEntity = NULL;
		while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRange)) != NULL)
		{
			if (!pEntity->IsAlive() || pEntity->IsBSPModel() || pEntity->edict() == pev->dmg_inflictor || pEntity->pev->takedamage != DAMAGE_AIM || (pEntity->GetRCCaps2(0, this) & 8) != 0)
				continue;

			Vector vecTo = pEntity->Center() - vecSrc;
			const float flDist = vecTo.Length();
			Vector vecToDir = vecTo;
			if (flDist != 0)
				vecToDir = vecTo / flDist;

			float flDot = DotProduct(vecLook, vecToDir);
			if (flDot < flMinDot)
				continue;

			flDot = std::clamp((flDot + 1.0f) * 0.5f, 0.0f, 1.0f);
			const float flNear = std::clamp(1.0f - flDist / flRange, 0.0f, 1.0f);
			const float flScore = flNear + flDot + flDot;
			if (flScore > flBest)
			{
				TraceResult tr;
				UTIL_TraceLine(vecSrc, pEntity->Center(), ignore_monsters, ENT(pev), &tr);
				if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
				{
					flBest = flScore;
					pBest = pEntity;
				}
			}
		}

		if (pBest)
		{
			m_hTarget = pBest;
			const int iPitch = RANDOM_LONG(150, 160);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pszMiniGruntTalk[RANDOM_LONG(0, 4)], 0.6, ATTN_NORM, 0, iPitch);
		}
	}

	if (m_hTarget == NULL)
	{
		m_flSpeedScale -= RC2Pref(RC2P_Snark_GruntSpeedScaleDeccelRate).f * flInterval;
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.05;
		m_flSpeedScale += RC2Pref(RC2P_Snark_GruntSpeedScaleAccelRate).f * flInterval;

		// lead the target by the time it takes to cover the flat distance to it
		Vector vecCenter = m_hTarget->Center();
		const Vector vecFlat(vecCenter.x - pev->origin.x, vecCenter.y - pev->origin.y, 0);
		const float flTime = vecFlat.Length() / (pev->speed * m_flSpeedScale);
		const Vector vecLead = m_hTarget->pev->velocity * flTime;
		const Vector vecAim = vecLead + m_hTarget->Center();
		const Vector vecDir = Vector(vecAim.x - pev->origin.x, vecAim.y - pev->origin.y, 0).Normalize();

		const float flSwing = RC2Pref(RC2P_Snark_GruntTurnSwingConstant).f;
		const float flFacing = std::clamp((DotProduct(vecDir, m_vecHeading) + 1.0f) * 0.5f, 0.0f, 1.0f);
		const Vector vecTurn = vecDir * (flFacing * flSwing + (1.0f - flSwing)) * RC2Pref(RC2P_Snark_GruntTurnScaleConstant).f * flInterval;
		m_vecHeading = m_vecHeading + vecTurn;
		m_vecHeading = m_vecHeading.Normalize();
	}

	if (m_flSpeedScale < 1.0)
		m_flSpeedScale = 1.0;
	else if (m_flSpeedScale > RC2Pref(RC2P_Snark_GruntMaxSpeedScale).f)
		m_flSpeedScale = RC2Pref(RC2P_Snark_GruntMaxSpeedScale).f;
	pev->framerate = m_flSpeedScale;
}

void CMiniGrunt::StepSound() // orig 0xd2780
{
	if (m_flNextStepSound > gpGlobals->time)
		return;

	m_flNextStepSound = gpGlobals->time + RANDOM_FLOAT(0.2, 0.3);

	const char* pszSound = NULL;
	switch (RANDOM_LONG(0, 3))
	{
	case 0:
		pszSound = "common/npc_step1.wav";
		break;
	case 1:
		pszSound = "common/npc_step2.wav";
		break;
	case 2:
		pszSound = "common/npc_step3.wav";
		break;
	case 3:
		pszSound = "common/npc_step4.wav"; // not precached (original bug)
		break;
	}

	const float flFast = (m_flSpeedScale - 1.0) * 0.5;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, pszSound, flFast * 0.4 + 0.5, ATTN_NORM, 0, (int)(flFast * 40 + 100));
}

void CMiniGrunt::DoTouch(CBaseEntity* pOther) // orig 0xd2860
{
	TraceResult tr = UTIL_GetGlobalTrace();

	// once it touches something other than the one who launched it, it can hit him
	if (pev->owner && pOther->edict() != pev->owner)
		pev->owner = NULL;

	Vector vecSrc = pev->origin + Vector(0, 0, 2);
	Vector vecDir;

	if (pOther->edict() == edict())
	{
		// Killed, or out of life: burst upward
		vecDir = Vector(0, 0, 1);
	}
	else if (pOther->pev->takedamage == DAMAGE_NO)
	{
		if (!m_fLanded)
		{
			if (tr.vecPlaneNormal.z >= 0.5)
			{
				// landed: run flat at base speed
				m_fLanded = true;
				pev->velocity.z = 0;
				pev->velocity = pev->velocity.Normalize() * pev->speed;
				m_vecGroundNormal = tr.vecPlaneNormal;
				StepSound();
				return;
			}
		}
		else if (DotProduct(tr.vecPlaneNormal, m_vecGroundNormal) >= 0.4 && tr.vecPlaneNormal.z >= 0.5)
		{
			// still on walkable ground: run along the heading (or straight ahead without a target),
			// lifted by how steep the ground is
			Vector vecRun;
			if (m_hTarget == NULL)
			{
				UTIL_MakeVectors(pev->angles);
				vecRun.x = gpGlobals->v_forward.x;
				vecRun.y = gpGlobals->v_forward.y;
			}
			else
			{
				vecRun.x = m_vecHeading.x;
				vecRun.y = m_vecHeading.y;
			}
			vecRun.z = (1.0 - fabs(tr.vecPlaneNormal.z)) * 1.3;
			pev->velocity = vecRun.Normalize() * pev->speed * m_flSpeedScale;
			m_vecGroundNormal = tr.vecPlaneNormal;
			StepSound();
			return;
		}

		// a wall: burst off it
		vecDir = Vector(tr.vecPlaneNormal.x, tr.vecPlaneNormal.y, tr.vecPlaneNormal.z + 0.5).Normalize();
	}
	else
	{
		vecDir = pev->velocity.Normalize();

		entvars_t* pevAttacker = pev;
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pevAttacker = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;

		const float flDamage = ShieldDeflect(pOther, RC2Pref(RC2P_Snark_GruntHitDamage).f, RC2Pref(RC2P_Snark_GruntHitShieldFraction).f, vecSrc, vecDir, &vecSrc, NULL, 6);
		if (flDamage > 0)
			pOther->TakeDamage(pev, pevAttacker, flDamage, DMG_CLUB | DMG_NEVERGIB);

		vecDir = -vecDir;
		vecDir.z += 0.5;
		vecDir = vecDir.Normalize();
	}

	PLAYBACK_EVENT_FULL(0, edict(), m_usBlast, 0.0, vecSrc, g_vecZero, 0.0, 0.0, UTIL_PackDirection(vecDir), 0, 0, 0);

	SetThink(NULL);
	SetTouch(NULL);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 1.0, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1.0, ATTN_NORM, 0, 100);
	UTIL_Remove(this);
}

#endif

// orig client 0x449d0 (and its GetClassPtr<CSqueak>, orig client 0x45a90); the inline iItemSlot and
// UseDecrement are orig client 0x3be20 and orig client 0x3be40
LINK_ENTITY_TO_CLASS(weapon_snark, CSqueak); // orig 0xd30e0


void CSqueak::Spawn() // orig 0xd3170, orig client 0x44a00
{
	Precache();
	m_iId = WEAPON_SNARK;
	SET_MODEL(ENT(pev), "models/w_sqknest.mdl");

	FallInit(); //get ready to fall down.

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_SnarkPickupGive).s;

	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
	m_flNextWeaponSync = gpGlobals->time;
}


void CSqueak::Precache() // orig 0xd31f0, orig client 0x44aa0
{
	PRECACHE_MODEL("models/w_sqknest.mdl");
	PRECACHE_MODEL("models/v_squeak.mdl");
	PRECACHE_MODEL("models/p_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	UTIL_PrecacheOther("monster_snark");
	UTIL_PrecacheOther("snark_cam");
	UTIL_PrecacheOther("mini_grunt");

	m_usSnarkFire = PRECACHE_EVENT(1, "events/snarkfire.sc");
	m_usSnarkCamPlace = PRECACHE_EVENT(1, "events/snarkcamplace.sc");
}


bool CSqueak::GetItemInfo(ItemInfo* p) // orig 0xd3280, orig client 0x44b60
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Snarks";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarrySnarks).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_SNARK;
	p->iWeight = SNARK_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return true;
}



bool CSqueak::Deploy() // orig 0xd32f0, orig client 0x44bf0
{
	// play hunt sound
	float flRndSound = RANDOM_FLOAT(0, 1);

	if (flRndSound <= 0.5)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 100);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 100);

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_flNextWeaponSync = gpGlobals->time;

	// RC2 returns DefaultDeploy's result as SDK 2.3 does, without halflife-updated's idle delay
	return DefaultDeploy("models/v_squeak.mdl", "models/p_squeak.mdl", SQUEAK_UP, "squeak");
}


void CSqueak::Holster() // orig 0xd3390, orig client 0x44cb0
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CSqueak::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	SendWeaponAnim(SQUEAK_DOWN);
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}


// Rocket Crowbar 2: the snark weapon's four attacks and its weapon-sync pair (rc2.dll 0xd3440-0xd44ee,
// client.dll 0x44e20-0x45a80), moved here from dlls/rc2/squeak.cpp (c1) because the client's
// prediction compiles them too (reference/dossiers/c1.md): there it throws, spends and times as
// the server does, but creates, finds and counts nothing. Primary throws a snark, secondary a mini
// grunt, tertiary sticks a snark cam on what the player aims at and quaternary looks through it.
// Each mode has its own refire times for all four attacks in rc2.ini [Snark].
// Primary: throw a snark, as SDK 2.3 did, but capped by SnarkMaxDeployed and costing
// SnarkAmmoCost, and owned by what the player drives when he drives something.
// orig 0xd3440, orig client 0x44e20
void CSqueak::PrimaryAttack()
{
	if (m_iSnarksDeployed > RC2PrefWord(RC2P_Snark_SnarkMaxDeployed))
		return;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2PrefWord(RC2P_Snark_SnarkAmmoCost))
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	TraceResult tr;
	Vector trace_origin;

	// HACK HACK:  Ugly hacks to handle change in origin based on new physics code for players
	// Move origin up if crouched and start trace a bit outside of body ( 20 units instead of 16 )
	trace_origin = m_pPlayer->pev->origin;
	if ((m_pPlayer->pev->flags & FL_DUCKING) != 0)
	{
		trace_origin = trace_origin - (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	// find place to toss monster
	UTIL_TraceLine(trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr);

	if (0 != g_iPlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usSnarkFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
	{
		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
		CBaseEntity* pSqueak = CBaseEntity::Create("monster_snark", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer->edict());
		pSqueak->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Snark_SnarkLaunchVel).f + m_pPlayer->pev->velocity;

		if (0 != m_pPlayer->m_iRidingIndex)
		{
			edict_t* pentDriven = INDEXENT(m_pPlayer->m_iRidingIndex);
			if (UTIL_IsValidEntity(pentDriven))
				pSqueak->pev->owner = pentDriven;
		}
#endif

		// play hunt sound
		float flRndSound = RANDOM_FLOAT(0, 1);

		if (flRndSound <= 0.5)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);

		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2PrefWord(RC2P_Snark_SnarkAmmoCost);

		m_fJustThrown = true;

		UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefirePrimary).f);
		UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefireSecondary).f);
		UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefireTertiary).f);
		UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefireQuaternary).f);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		m_flNextWeaponSync = gpGlobals->time;
	}
}

// Secondary: toss a mini grunt the same way, upright (no pitch), at its own speed.
// orig 0xd38b0, orig client 0x45270
void CSqueak::SecondaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2PrefWord(RC2P_Snark_GruntAmmoCost))
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	TraceResult tr;
	Vector trace_origin;

	trace_origin = m_pPlayer->pev->origin;
	if ((m_pPlayer->pev->flags & FL_DUCKING) != 0)
	{
		trace_origin = trace_origin - (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	UTIL_TraceLine(trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr);

	if (0 != g_iPlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usSnarkFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
	{
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
		Vector vecAngles(0, m_pPlayer->pev->v_angle.y, m_pPlayer->pev->v_angle.z);
		CBaseEntity* pGrunt = CBaseEntity::Create("mini_grunt", tr.vecEndPos, vecAngles, m_pPlayer->edict());
		pGrunt->pev->velocity = gpGlobals->v_forward * pGrunt->pev->speed;

		if (0 != m_pPlayer->m_iRidingIndex)
		{
			edict_t* pentDriven = INDEXENT(m_pPlayer->m_iRidingIndex);
			if (UTIL_IsValidEntity(pentDriven))
				pGrunt->pev->owner = pentDriven;
		}
#endif

		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2PrefWord(RC2P_Snark_GruntAmmoCost);

		m_fJustThrown = true;

		UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefirePrimary).f);
		UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefireSecondary).f);
		UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefireTertiary).f);
		UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefireQuaternary).f);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	}
}

// Tertiary: trace 128 units from the gun and stick a snark cam on the surface hit, unless an
// HEV shield there soaks up CamDeployShieldDamage. A miss still costs half the refire times.
// orig 0xd3cb0, orig client 0x45630 (the client's stops after the half refire)
void CSqueak::TertiaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2PrefWord(RC2P_Snark_CamAmmoCost))
		return;
	if (m_iCamsDeployed > RC2PrefWord(RC2P_Snark_CamMaxDeployed))
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	if (0 != g_iPlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usSnarkCamPlace, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefirePrimary).f * 0.5);
	UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireSecondary).f * 0.5);
	UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireTertiary).f * 0.5);
	UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireQuaternary).f * 0.5);

#ifndef CLIENT_DLL
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 128, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.flFraction >= 1.0)
		return;

	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);

	if (RC2_ShieldFilterDamage(pHit, RC2Pref(RC2P_Snark_CamDeployShieldDamage).f, 1.0, tr.vecEndPos, gpGlobals->v_forward, NULL, NULL, 4) < RC2Pref(RC2P_Snark_CamDeployShieldDamage).f)
		return;
	if (!pHit)
		return;
	if ((pHit->pev->flags & FL_CONVEYOR) != 0) // VERIFY: the original tests bit 4 (FL_CONVEYOR) of the surface hit
		return;

	Vector vecAngles = UTIL_VecToAngles(tr.vecPlaneNormal);
	CBaseEntity::Create("snark_cam", tr.vecEndPos + tr.vecPlaneNormal * 6, vecAngles, m_pPlayer->edict());
	m_flNextWeaponSync = gpGlobals->time;

	if (RANDOM_FLOAT(0, 1) <= 0.5)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 0.6, ATTN_NORM, 0, 105);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 0.6, ATTN_NORM, 0, 105);

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2PrefWord(RC2P_Snark_CamAmmoCost);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		RetireWeapon();
		return;
	}

	m_fJustThrown = true;

	UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefirePrimary).f);
	UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireSecondary).f);
	UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireTertiary).f);
	UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	m_flNextWeaponSync = gpGlobals->time;
#endif
}

// Quaternary: look through the first of this player's snark cams (a cam keeps its owner in
// pev->dmg_inflictor); with none deployed, only the refire times move.
// orig 0xd4170, orig client 0x45810 (the client's only moves the refire times)
void CSqueak::QuaternaryAttack()
{
#ifndef CLIENT_DLL
	CBaseEntity* pCam = UTIL_FindEntityByClassname(NULL, "snark_cam");
	while (pCam)
	{
		if (pCam->pev->dmg_inflictor == m_pPlayer->edict())
		{
			SnarkCam_TakeView(pCam);
			return;
		}
		pCam = UTIL_FindEntityByClassname(pCam, "snark_cam");
	}
#endif

	UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefirePrimary).f);
	UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefireSecondary).f);
	UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefireTertiary).f);
	UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefireQuaternary).f);
}

// Each frame: count this player's live snarks and cams, and every half second send both to
// the client (gmsgWeaponSync: the weapon id, then the two counts).
// orig 0xd4370, orig client 0x45a20 (empty on the client)
void CSqueak::WeaponFrame()
{
#ifndef CLIENT_DLL
	m_iSnarksDeployed = 0;
	CBaseEntity* pEnt = UTIL_FindEntityByClassname(NULL, "monster_snark");
	while (pEnt)
	{
		CBaseEntity* pOwner = SqueakGrenade_Owner(pEnt); // its m_hOwner (orig +0x2dc)
		if (pOwner && pOwner->edict() == m_pPlayer->edict())
			m_iSnarksDeployed++;
		pEnt = UTIL_FindEntityByClassname(pEnt, "monster_snark");
	}

	m_iCamsDeployed = 0;
	pEnt = UTIL_FindEntityByClassname(NULL, "snark_cam");
	while (pEnt)
	{
		if (pEnt->pev->dmg_inflictor == m_pPlayer->edict())
			m_iCamsDeployed++;
		pEnt = UTIL_FindEntityByClassname(pEnt, "snark_cam");
	}

	if (m_flNextWeaponSync < gpGlobals->time)
	{
		m_flNextWeaponSync = gpGlobals->time + 0.5;
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_SNARK);
		WRITE_BYTE(m_iSnarksDeployed);
		WRITE_BYTE(m_iCamsDeployed);
		MESSAGE_END();
	}
#endif
}

// The client's side of the message above, compiled into the server too, where the readers
// return 0.
// orig 0xd44b0, orig client 0x45a40
void CSqueak::ReadWeaponState(void* pData, int iSize)
{
	m_iSnarksDeployed = RC2ReadStateByte(&pData, &iSize);
	m_iCamsDeployed = RC2ReadStateByte(&pData, &iSize);
}


// p8: unchanged from SDK 2.3
// orig 0xd4250, orig client 0x458b0
void CSqueak::WeaponIdle()
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_fJustThrown)
	{
		m_fJustThrown = false;

		if (0 == m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()])
		{
			RetireWeapon();
			return;
		}

		SendWeaponAnim(SQUEAK_UP);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		return;
	}

	int iAnim;
	float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
	if (flRand <= 0.75)
	{
		iAnim = SQUEAK_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = SQUEAK_FIDGETFIT;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 70.0 / 16.0;
	}
	else
	{
		iAnim = SQUEAK_FIDGETNIP;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 80.0 / 16.0;
	}
	SendWeaponAnim(iAnim);
}
