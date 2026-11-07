/***
*
*	Rocket Crowbar 2: the gauss beacon (gauss_becon). The gauss's quaternary attack throws it
*	(CGauss::QuaternaryAttack, gauss.cpp); it bounces until it touches a BSP model, sticks to
*	it (following a moving brush entity), and is where the gauss's teleports land. It dies in
*	the sky, when its flight time (rc2.ini [Gauss] BeaconMaxFlightTime) runs out, when shot to
*	death, or when the brush it sticks to goes away.
*
*	The original sits in .text between gargantua.cpp and gauss.cpp (files-rc2.tsv run
*	"new: gargantua .. gauss"), so the file name is ours.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "rc2prefs.h"
#include "gaussbecon.h"
#include "p3b_temp.h" // TEMP STUB: gmsgCLTE

// RC2's CLTE message types the beacon sends (byte 0 of the message). Names ours.
#define RC2_CLTE_ATTACH 0x19	// short beacon, short entity, coord offset x3, coord angle offset x3
#define RC2_CLTE_STUCK 0x22		// short beacon

LINK_ENTITY_TO_CLASS(gauss_becon, CGaussBecon); // orig 0x4bae0

// orig 0x4bb50. VERIFY: the bit's meaning (gauss.cpp tests 0x20 only)
int CGaussBecon::GetRCCaps(int iAsker, CBaseEntity* pAsker)
{
	return 0x400;
}

// orig 0x4bb60: 2 once stuck. VERIFY: the bit's meaning
int CGaussBecon::GetRCCaps2(int iAsker, CBaseEntity* pAsker)
{
	return m_blAttached ? 2 : 0;
}

// orig 0x4bb70
void CGaussBecon::Precache()
{
	PRECACHE_MODEL("models/gaussbecon.mdl");
	PRECACHE_MODEL("sprites/redflare1.spr");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("turret/tu_ping.wav");
}

// orig 0x4bba0. The thrower is pev->dmg_inflictor (CGauss::FindBecon looks it up there).
void CGaussBecon::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;
	pev->health = RC2Pref(RC2P_Gauss_BeaconHealth).f;
	pev->gravity = RC2Pref(RC2P_Gauss_BeaconGravityScaler).f;
	pev->friction = 0.3;
	pev->classname = MAKE_STRING("gauss_becon");
	SET_MODEL(ENT(pev), "models/gaussbecon.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	SetTouch(&CGaussBecon::AttachTouch);
	SetThink(&CGaussBecon::FlyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->air_finished = gpGlobals->time + RC2Pref(RC2P_Gauss_BeaconMaxFlightTime).f;
	pev->dmg_inflictor = pev->owner;
	m_blAttached = false;

	pev->avelocity = Vector(RANDOM_FLOAT(200, 250), 0, 0);
	if (RANDOM_LONG(0, 1))
		pev->avelocity.x *= -1;
}

// orig 0x4c980: a small explosion, then it goes
void CGaussBecon::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		WRITE_SHORT(g_sModelIndexFireball);
	else
		WRITE_SHORT(g_sModelIndexWExplosion);
	WRITE_BYTE(10); // scale * 10
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	SetThink(&CGaussBecon::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
}

// orig 0x4bd00: in flight. Faces along its path (keeping its tumbling pitch), slows in water,
// dies when the flight time is up.
void CGaussBecon::FlyingThink()
{
	if (gpGlobals->time > pev->air_finished)
	{
		Killed(pev, GIB_NORMAL);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	ApplyExternalForce(0);

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.7;
		pev->avelocity = pev->avelocity * 0.9;
	}

	float flPitch = pev->angles.x;
	pev->angles = UTIL_VecToAngles(pev->velocity.Normalize());
	pev->angles.x = flPitch;
}

// orig 0x4bec0: sticks to the BSP model it hit (anything else just slows it), or dies in the
// sky
void CGaussBecon::AttachTouch(CBaseEntity* pOther)
{
	if (!pOther->IsBSPModel())
	{
		pev->velocity = pev->velocity * 0.3;
		return;
	}

	edict_t* pentIgnore = ENT(pev);
	Vector vecEnd = pev->origin + pev->velocity.Normalize() * 128;
	Vector vecStart = pev->origin - pev->velocity.Normalize() * 16;
	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pentIgnore, &tr);

	if (tr.flFraction < 1.0 && UTIL_PointContents(tr.vecEndPos) != CONTENTS_SKY && UTIL_PointContents(UTIL_GetGlobalTrace().vecEndPos) != CONTENTS_SKY)
	{
		pev->angles = UTIL_VecToAngles(tr.vecPlaneNormal);
		m_blAttached = true;

		if (ENTINDEX(pOther->edict()) == 0)
		{
			// on the world: stays put
			m_vecAttachOrigin = tr.vecEndPos;
			m_vecAttachAngles = pev->angles;
			m_blAttachedToWorld = true;
			m_vecPlaneNormal = tr.vecPlaneNormal;
		}
		else
		{
			// on a brush entity: rides it, its offset kept in the entity's own axes
			UTIL_MakeVectors(pOther->pev->angles);
			Vector vecDelta = tr.vecEndPos - pOther->pev->origin;
			m_vecAttachOrigin.x = DotProduct(vecDelta, gpGlobals->v_forward);
			m_vecAttachOrigin.y = DotProduct(vecDelta, gpGlobals->v_right);
			m_vecAttachOrigin.z = DotProduct(vecDelta, gpGlobals->v_up);
			m_vecAttachAngles = pev->angles - pOther->pev->angles;
			m_blAttachedToWorld = false;
			pev->owner = pOther->edict();
			m_vecPlaneNormal = tr.vecPlaneNormal;

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
			WRITE_BYTE(RC2_CLTE_ATTACH);
			WRITE_SHORT(ENTINDEX(edict()));
			WRITE_SHORT(ENTINDEX(pOther->edict()));
			WRITE_COORD(m_vecAttachOrigin.x);
			WRITE_COORD(m_vecAttachOrigin.y);
			WRITE_COORD(m_vecAttachOrigin.z);
			WRITE_COORD(m_vecAttachAngles.x);
			WRITE_COORD(m_vecAttachAngles.y);
			WRITE_COORD(m_vecAttachAngles.z);
			MESSAGE_END();
		}

		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/mine_deploy.wav", 1.0, 0.8, 0, PITCH_NORM);
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = g_vecZero;

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(RC2_CLTE_STUCK);
		WRITE_SHORT(ENTINDEX(edict()));
		MESSAGE_END();

		pev->solid = SOLID_BBOX;
		UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
		SetTouch(NULL);
		SetThink(&CGaussBecon::AttachedThink);
		pev->nextthink = gpGlobals->time + 0.01;
		return;
	}

	SetThink(&CGaussBecon::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
}

// orig 0x4c580: while stuck. On a brush entity it follows it and dies with it; on the world its
// thrower stays its owner (so they don't collide) until nothing is within 16 units in front of it.
void CGaussBecon::AttachedThink()
{
	if (!UTIL_IsValidEntity(pev->owner) && !m_blAttachedToWorld)
	{
		pev->owner = NULL;
		Killed(pev, GIB_NORMAL);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	pev->velocity = g_vecZero;

	if (!m_blAttachedToWorld)
	{
		entvars_t* pevOwner = VARS(pev->owner);
		pev->angles = m_vecAttachAngles + pevOwner->angles;
		UTIL_MakeVectors(pevOwner->angles);
		Vector vecOrigin = gpGlobals->v_up * m_vecAttachOrigin.z + (gpGlobals->v_right * m_vecAttachOrigin.y + (gpGlobals->v_forward * m_vecAttachOrigin.x + pevOwner->origin));
		UTIL_SetOrigin(pev, vecOrigin);
		UTIL_MakeVectors(pev->angles);
		m_vecPlaneNormal = gpGlobals->v_forward;
	}
	else
	{
		pev->angles = m_vecAttachAngles;
		UTIL_SetOrigin(pev, m_vecAttachOrigin);
	}

	if (pev->owner && m_blAttachedToWorld)
	{
		if (!UTIL_IsValidEntity(pev->owner))
		{
			pev->owner = NULL;
		}
		else
		{
			edict_t* pentOwner = pev->owner;
			pev->owner = NULL;
			TraceResult tr;
			UTIL_TraceLine(pev->origin + m_vecPlaneNormal, pev->origin + m_vecPlaneNormal * 16, dont_ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
				pev->owner = pentOwner;
		}
	}
}
