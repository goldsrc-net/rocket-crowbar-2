/***
*
*	Rocket Crowbar 2: what the shotgun fires besides buckshot. A mini forklift that drives
*	along the floor and up to a wall, an HEV suit that flies under its own engine, a
*	screaming scientist, and a wooden crate that grows until it bursts. Each one blows up on
*	what it hits. RC2's code between scripted.cpp and shotgun.cpp in link order; the file
*	name is ours. The shotgun's attacks (shotgun.cpp) create them.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "hltv.h"
#include "rc2prefs.h"
#include "UserMessages.h"

#define CLTE_HEVFLAME 20 // name ours: short entindex; sent to the PVS when the suit spawns

// RC2's grenades here send this director hint before they explode. It writes 3 where
// halflife-updated's hltv.h has DRC_CMD_EVENT as 2.
static void SendBoomDirectorHint(CBaseEntity* pEntity, int iPriority) // name ours
{
	MESSAGE_BEGIN(MSG_SPEC, 50); // svc_hltv
	WRITE_BYTE(3);
	WRITE_SHORT(ENTINDEX(pEntity->edict()));
	WRITE_SHORT(0);
	WRITE_LONG(iPriority);
	MESSAGE_END();
}

// Warns monsters ahead of a moving projectile (bits_SOUND_DANGER as loud as it is fast).
static void InsertDangerAhead(entvars_t* pev) // name ours: the block each think repeats
{
	if (pev->dmgtime < gpGlobals->time)
	{
		// VERIFY: the volume is read as the speed (the __ftol of the fsqrt result left on the stack)
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5, (int)pev->velocity.Length(), 0.2);
		pev->dmgtime = gpGlobals->time + 0.2;
	}
}


//=========================================================
// shotgun_forklift
//=========================================================
class CShotgunForklift : public CGrenade
{
public:
	void Spawn() override;	  // orig 0xc4d90
	void Precache() override; // orig 0xc4d70
	void Killed(entvars_t* pevAttacker, int iGib) override { GoBoom(); } // orig 0xc5800
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x480; } // orig 0xc4d60

	void EXPORT GenericThink();
	void EXPORT OnGroundThink();
	void EXPORT DoTouch(CBaseEntity* pOther);
	void EXPORT GoBoom();

	bool m_fOnFloor;	  // orig +0x2b8, name ours: driving on a floor since the last touch
	bool m_fHasLanded;	  // orig +0x2b9, name ours: has touched a floor at least once
};
LINK_ENTITY_TO_CLASS(shotgun_forklift, CShotgunForklift); // orig 0xc4ce0

void CShotgunForklift::Precache()
{
	PRECACHE_MODEL("models/miniforklift.mdl");
	PRECACHE_SOUND("ambience/industrial4.wav");
}

void CShotgunForklift::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->friction = RC2Pref(RC2P_Shotgun_ForkliftFriction).f;
	SET_MODEL(ENT(pev), "models/miniforklift.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 8));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("shotgun_forklift");

	SetThink(&CShotgunForklift::GenericThink);
	SetTouch(&CShotgunForklift::DoTouch);
	pev->nextthink = gpGlobals->time + 0.01;

	pev->dmg = RC2Pref(RC2P_Shotgun_ForkliftDamage).f;
	pev->sequence = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = true;
	pev->health = 1.0;
	pev->takedamage = DAMAGE_YES;
	pev->dmg_inflictor = pev->owner;
	pev->dmgtime = gpGlobals->time;
	pev->air_finished = RC2Pref(RC2P_Shotgun_ForkliftMaxLife).f + gpGlobals->time; // when it blows anyway

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/industrial4.wav", 0.6, 1.1, 0, 130);

	m_fOnFloor = false;
	m_fHasLanded = false;
}

void CShotgunForklift::GenericThink() // orig 0xc4f20
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance(0.0);
	pev->angles = UTIL_VecToAngles(pev->velocity);
	ApplyExternalForce(0);

	InsertDangerAhead(pev);

	if (pev->air_finished < gpGlobals->time)
	{
		SetThink(&CShotgunForklift::GoBoom);
		pev->nextthink = gpGlobals->time + 0.01;
		return;
	}

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		SetThink(&CShotgunForklift::OnGroundThink);
		return;
	}

	m_fOnFloor = false;
	if (pev->velocity == g_vecZero)
	{
		SetThink(&CShotgunForklift::GoBoom);
		pev->nextthink = gpGlobals->time + 0.01;
	}
}

void CShotgunForklift::OnGroundThink() // orig 0xc50e0
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance(0.0);
	pev->angles = UTIL_VecToAngles(pev->velocity);
	ApplyExternalForce(0);

	InsertDangerAhead(pev);

	if (pev->air_finished < gpGlobals->time)
	{
		SetThink(&CShotgunForklift::GoBoom);
		pev->nextthink = gpGlobals->time + 0.01;
		return;
	}

	if ((pev->flags & FL_ONGROUND) == 0)
	{
		SetThink(&CShotgunForklift::GenericThink);
		m_fOnFloor = false;
		return;
	}

	if (pev->velocity == g_vecZero)
	{
		SetThink(&CShotgunForklift::GoBoom);
		pev->nextthink = gpGlobals->time + 0.01;
	}
}

void CShotgunForklift::DoTouch(CBaseEntity* pOther) // orig 0xc52a0
{
	TraceResult tr = UTIL_GetGlobalTrace();

	if (pev->owner != NULL && pOther->edict() != pev->owner && pOther->edict() != pev->dmg_inflictor)
		pev->owner = NULL; // it can hit its shooter once it has touched something else

	if (tr.flFraction < 1.0)
	{
		if (UTIL_IsValidEntity(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit != NULL && (pHit->pev->takedamage == DAMAGE_AIM || !pHit->IsBSPModel()))
			{
				m_fOnFloor = true;
				SetThink(&CShotgunForklift::GoBoom);
				SetTouch(NULL);
				pev->nextthink = gpGlobals->time + 0.01;
				return;
			}
		}

		// driving into a wall: stick to it and blow
		bool fWall = false;
		if (m_fOnFloor && tr.vecPlaneNormal.z < RC2Pref(RC2P_Shotgun_ForkliftMinFloorNormalZ).f && DotProduct(pev->velocity.Normalize(), tr.vecPlaneNormal) < RC2Pref(RC2P_Shotgun_ForkliftMinHitWallDot).f)
			fWall = true;
		else if (m_fHasLanded && fabs(tr.vecPlaneNormal.z) < RC2Pref(RC2P_Shotgun_ForkliftMinVertWallNormalZ).f && DotProduct(pev->velocity.Normalize(), tr.vecPlaneNormal) < RC2Pref(RC2P_Shotgun_ForkliftMinHitWallDot).f)
			fWall = true;

		if (fWall)
		{
			UTIL_SetOrigin(pev, tr.vecPlaneNormal * 32 + tr.vecEndPos);
			pev->velocity = -tr.vecPlaneNormal;
			pev->movetype = MOVETYPE_NONE;
			pev->solid = SOLID_NOT;
			m_fOnFloor = false;
			SetThink(&CShotgunForklift::GoBoom);
			SetTouch(NULL);
			pev->nextthink = gpGlobals->time + 0.01;
			return;
		}

		m_fHasLanded = true;
		m_fOnFloor = true;
		if (tr.vecPlaneNormal.z < 1.0)
			pev->velocity.z += tr.vecPlaneNormal.z * 100.0;
	}

	// keep driving at [Shotgun] ForkliftSpeed
	if ((pev->flags & FL_ONGROUND) != 0 && pev->velocity.Length() < RC2Pref(RC2P_Shotgun_ForkliftSpeed).f)
		pev->velocity = pev->velocity.Normalize() * RC2Pref(RC2P_Shotgun_ForkliftSpeed).f;
}

void CShotgunForklift::GoBoom() // orig 0xc5810
{
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->owner = pev->dmg_inflictor;
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/industrial4.wav", 0, 0, SND_STOP, 100);
	SetTouch(NULL);
	SetThink(NULL);

	TraceResult tr;
	if (!m_fOnFloor)
	{
		// in the air: along its path, then down
		UTIL_TraceLine(pev->origin - pev->velocity.Normalize() * 32, pev->origin + pev->velocity.Normalize() * 128, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0)
			UTIL_TraceLine(pev->origin + Vector(0, 0, 32), pev->origin - Vector(0, 0, 128), dont_ignore_monsters, ENT(pev), &tr);
	}
	else
	{
		// on the floor: down, then along its path
		UTIL_TraceLine(pev->origin + Vector(0, 0, 32), pev->origin - Vector(0, 0, 128), dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0)
			UTIL_TraceLine(pev->origin - pev->velocity.Normalize() * 32, pev->origin + pev->velocity.Normalize() * 128, dont_ignore_monsters, ENT(pev), &tr);
	}

	if (tr.flFraction >= 1.0)
	{
		// nothing near: blow where it is
		tr.vecEndPos = pev->origin;
		tr.vecPlaneNormal = Vector(0, 0, 1);
		tr.flFraction = 1.0;
	}

	SendBoomDirectorHint(this, 3);
	Explode(&tr, DMG_BLAST);
}


//=========================================================
// shotgun_hev
//=========================================================
class CShotgunHEV : public CGrenade
{
public:
	void Spawn() override;	  // orig 0xc5f00
	void Precache() override; // orig 0xc5ed0

	void EXPORT AccelThink();
	void EXPORT AimThink();
	void EXPORT BoomTouch(CBaseEntity* pOther);

	float m_flLastThink; // orig +0x2b8, name ours
};
LINK_ENTITY_TO_CLASS(shotgun_hev, CShotgunHEV); // orig 0xc5e50

void CShotgunHEV::Precache()
{
	PRECACHE_MODEL("models/shotgunsuit.mdl");
	PRECACHE_MODEL("sprites/animglow01.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

void CShotgunHEV::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/shotgunsuit.mdl");
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("shotgun_hev");

	SetThink(&CShotgunHEV::AccelThink);
	SetTouch(&CShotgunHEV::BoomTouch);
	pev->nextthink = gpGlobals->time + 0.01;
	m_flLastThink = gpGlobals->time;
	pev->dmgtime = gpGlobals->time + 0.01;
	pev->air_finished = RC2Pref(RC2P_Shotgun_HEVEngineTime).f + gpGlobals->time; // engine cut-off
	pev->dmg = RC2Pref(RC2P_Shotgun_HEVEngineDamage).f;

	// spins about its axis
	pev->avelocity.z = 800;
	if (0 != RANDOM_LONG(0, 1))
		pev->avelocity.z = pev->avelocity.z * -1;
	pev->angles.z = RANDOM_FLOAT(0, 360);

	pev->sequence = 0;
	ResetSequenceInfo();
	pev->dmg_inflictor = pev->owner;

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(CLTE_HEVFLAME);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// Under power: accelerates along pev->movedir (set by the shotgun) until the engine runs out.
void CShotgunHEV::AccelThink() // orig 0xc60c0
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance(0.0);

	pev->velocity = pev->movedir * RC2Pref(RC2P_Shotgun_HEVEngineAccel).f * flInterval + pev->velocity;

	const float flMaxVelocity = CVAR_GET_FLOAT("sv_maxvelocity") - 1.0; // the original reads its cvar pointer
	if (flMaxVelocity < pev->velocity.Length())
		pev->velocity = pev->velocity.Normalize() * (CVAR_GET_FLOAT("sv_maxvelocity") - 1.0);

	if (pev->dmgtime < gpGlobals->time)
	{
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->velocity * 0.5 + pev->origin, (int)pev->velocity.Length(), 0.2); // VERIFY: volume as in InsertDangerAhead
		pev->dmgtime = gpGlobals->time + 0.2;
	}

	if (pev->air_finished < gpGlobals->time)
	{
		// engine out: falls, and hits harder (HEVEngineOffDamage)
		SetThink(&CShotgunHEV::AimThink);
		pev->nextthink = gpGlobals->time + 0.01;
		pev->movetype = MOVETYPE_TOSS;
		pev->solid = SOLID_BBOX;
		UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
		UTIL_SetOrigin(pev, pev->origin);
		pev->gravity = RC2Pref(RC2P_Shotgun_HEVEngineOffGravity).f;
		pev->dmg = RC2Pref(RC2P_Shotgun_HEVEngineOffDamage).f;

		const float flRoll = pev->angles.z;
		pev->angles = UTIL_VecToAngles(pev->velocity);
		pev->angles.z = flRoll;
	}
}

void CShotgunHEV::AimThink() // orig 0xc64b0
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance(0.0);

	const float flRoll = pev->angles.z;
	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->angles.z = flRoll;

	InsertDangerAhead(pev);
}

void CShotgunHEV::BoomTouch(CBaseEntity* pOther) // orig 0xc65e0
{
	pev->solid = SOLID_NOT;
	pev->owner = pev->dmg_inflictor;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	ExplodeTouch(pOther);
}


//=========================================================
// shotgun_sci
//=========================================================
class CShotgunSci : public CGrenade
{
public:
	void Spawn() override;	  // orig 0xc6740
	void Precache() override; // orig 0xc66c0
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x480; } // orig 0xc66b0

	void EXPORT AimThink();
	void EXPORT BoomTouch(CBaseEntity* pOther);

	char m_szScream[128]; // orig +0x2b8, name ours: the scream it plays, stopped when it bursts
};
LINK_ENTITY_TO_CLASS(shotgun_sci, CShotgunSci); // orig 0xc6630

void CShotgunSci::Precache()
{
	PRECACHE_MODEL("models/shotgunsci.mdl");
	PRECACHE_SOUND("scientist/scream01.wav");
	PRECACHE_SOUND("scientist/scream02.wav");
	PRECACHE_SOUND("scientist/scream03.wav");
	PRECACHE_SOUND("scientist/scream04.wav");
	PRECACHE_SOUND("scientist/scream05.wav");
	PRECACHE_SOUND("scientist/scream06.wav");
	PRECACHE_SOUND("scientist/scream07.wav");
	PRECACHE_SOUND("scientist/scream08.wav");
	PRECACHE_SOUND("scientist/scream09.wav");
}

void CShotgunSci::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/shotgunsci.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));
	UTIL_SetOrigin(pev, pev->origin);

	pev->body = RANDOM_LONG(0, 3);
	if (pev->body == 2)
		pev->skin = 1; // the black head
	pev->gravity = RC2Pref(RC2P_Shotgun_ScientistGravity).f;
	pev->classname = MAKE_STRING("shotgun_sci");

	SetThink(&CShotgunSci::AimThink);
	SetTouch(&CShotgunSci::BoomTouch);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->dmg = RC2Pref(RC2P_Shotgun_ScientistDamage).f;
	pev->dmg_inflictor = pev->owner;

	pev->sequence = LookupSequence("fallingloop");
	ResetSequenceInfo();
	m_fSequenceLoops = true;
	pev->dmgtime = gpGlobals->time;

	sprintf(m_szScream, "scientist/scream0%i.wav", RANDOM_LONG(1, 9));
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, m_szScream, 1.0, ATTN_NORM, 0, RANDOM_LONG(-5, 5) + 100);
}

void CShotgunSci::AimThink() // orig 0xc68f0
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance(0.0);

	// flies feet first
	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->angles.y -= 180.0;
	pev->angles.z -= 180.0;
	pev->angles.x = -pev->angles.x;

	ApplyExternalForce(0);
	InsertDangerAhead(pev);

	// clear of the shooter: it can hit him now
	if (pev->owner != NULL && UTIL_IsValidEntity(pev->owner) && 128.0 < (pev->origin - VARS(pev->owner)->origin).Length())
		pev->owner = NULL;
}

void CShotgunSci::BoomTouch(CBaseEntity* pOther) // orig 0xc6b00
{
	if (pev->owner == NULL || pOther->edict() != pev->owner)
	{
		pev->owner = pev->dmg_inflictor;
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, m_szScream, 0, 0, SND_STOP, 100);
		pev->solid = SOLID_NOT;
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
		SetThink(NULL);
		SetTouch(NULL);
		ExplodeTouch(pOther);
	}
}


//=========================================================
// shotgun_crate
//=========================================================
class CShotgunCrate : public CGrenade
{
public:
	void Spawn() override;	  // orig 0xc6c70
	void Precache() override; // orig 0xc6c10

	void EXPORT ExpandThink();
	void EXPORT BoomTouch(CBaseEntity* pOther);
	void EXPORT GoBoom();

	float m_flLastThink;		// orig +0x2b8, name ours
	unsigned short m_usCrateHit; // orig +0x2c0, name ours: events/sgcratehit.sc
};
LINK_ENTITY_TO_CLASS(shotgun_crate, CShotgunCrate); // orig 0xc6b90

void CShotgunCrate::Precache()
{
	PRECACHE_MODEL("models/crate.mdl");
	PRECACHE_MODEL("models/woodgibs.mdl");
	PRECACHE_SOUND("debris/wood1.wav");
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");
	m_usCrateHit = PRECACHE_EVENT(1, "events/sgcratehit.sc");
}

void CShotgunCrate::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/crate.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("shotgun_crate");

	SetTouch(&CShotgunCrate::BoomTouch);
	SetThink(&CShotgunCrate::ExpandThink);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->dmg_inflictor = pev->owner;
	pev->scale = 0.1; // grows to full size in [Shotgun] CrateExpandTime
	m_flLastThink = gpGlobals->time;

	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 1000, 0.5);
}

void CShotgunCrate::ExpandThink() // orig 0xc6d50
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.025;

	pev->scale = flInterval * (1.0 / RC2Pref(RC2P_Shotgun_CrateExpandTime).f) + pev->scale;
	UTIL_SetSize(pev, Vector(-16, -16, -16) * pev->scale, Vector(16, 16, 16) * pev->scale);

	if (pev->scale >= 1.0)
	{
		pev->scale = 0; // full size (GoBoom reads 0 as 1)
		UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));
		SetThink(&CShotgunCrate::GoBoom);
		pev->nextthink = RC2Pref(RC2P_Shotgun_CrateMaxLife).f + gpGlobals->time;
	}
}

void CShotgunCrate::BoomTouch(CBaseEntity* pOther) // orig 0xc6e90
{
	GoBoom();
}

// Bursts for damage and radius in proportion to its size (at least half).
void CShotgunCrate::GoBoom() // orig 0xc6ea0
{
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->owner = pev->dmg_inflictor;
	SetThink(NULL);
	SetTouch(NULL);

	TraceResult tr;
	UTIL_TraceLine(pev->origin - pev->velocity.Normalize() * 32, pev->origin + pev->velocity.Normalize() * 128, dont_ignore_monsters, ENT(pev), &tr);

	float flScale = pev->scale;
	if (flScale == 0)
		flScale = 1.0;
	else if (flScale < 0.5)
		flScale = 0.5;

	// VERIFY: the blast centre read as tr.vecPlaneNormal * 16 + tr.vecEndPos (stack slots), the event origin as * 4
	const Vector vecCenter = tr.vecPlaneNormal * 16 + tr.vecEndPos;
	entvars_t* pevAttacker = UTIL_IsValidEntity(pev->owner) ? VARS(pev->owner) : pev;
	RC2RadiusDamage(pevAttacker, pev, edict(), vecCenter, RC2Pref(RC2P_Shotgun_CrateMaxRadius).f * flScale, RC2Pref(RC2P_Shotgun_CrateMaxDamage).f * flScale,
		RC2Pref(RC2P_Shotgun_CrateDamageFalloffExponent).f, DMG_BLAST, 1);

	PLAYBACK_EVENT_FULL(0, edict(), m_usCrateHit, 0.0, tr.vecPlaneNormal * 4 + tr.vecEndPos, g_vecZero, flScale, 0, UTIL_PackNormal(-tr.vecPlaneNormal), 0, false, false);

	SendBoomDirectorHint(this, 6);

	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
}
