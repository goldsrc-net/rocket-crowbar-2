/***
*
*	Rocket Crowbar 2: gravity_grenade, the hand grenade's tertiary throw.
*
*	It bounces like a frag grenade for [Grenade] SharedMaxTimer seconds, rises off the ground in
*	its last 0.75 s, then hangs in the air for GravGrenDetonateTime seconds as a pull point
*	(GravGrenPullRadius, ...PlayerConstant, ...MonsterConstant) that annihilates whatever it pulls
*	in (GravGrenAnnihilate, slot 72), and ends with a blast of GravGrenFinalBlastDamage.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "rc2grenades.h"

LINK_ENTITY_TO_CLASS(gravity_grenade, CGravGrenade); // orig 0x590d0

// orig 0x59150: 2 while it hangs in the air (MOVETYPE_FLY), 0 before
int CGravGrenade::GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker)
{
	return pev->movetype == MOVETYPE_FLY ? 2 : 0;
}

// orig 0x59170
void CGravGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("sprites/b-tele1.spr");
	PRECACHE_MODEL("sprites/xflare1.spr");
	PRECACHE_MODEL("sprites/animglow01.spr");
	PRECACHE_MODEL("sprites/blueflare1.spr");
	PRECACHE_MODEL("sprites/flare1.spr");
	PRECACHE_SOUND("weapons/grenade_hit1.wav");
	PRECACHE_SOUND("weapons/grenade_hit2.wav");
	PRECACHE_SOUND("weapons/grenade_hit3.wav");
	PRECACHE_SOUND("weapons/gravgren.wav");
	m_usDetonate = PRECACHE_EVENT(1, "events/gravgrenadedet.sc");
	m_usGlow = PRECACHE_EVENT(1, "events/gravgrenadeglow.sc");
}

// orig 0x59210
void CGravGrenade::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	m_iPullPoint = -1;

	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->classname = MAKE_STRING("gravity_grenade");
	pev->dmg_inflictor = pev->owner; // the thrower, kept after pev->owner is cleared

	SetTouch(&CGrenade::BounceTouch);
	SetThink(&CGravGrenade::PreDetonate);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Grenade_SharedMaxTimer).f + gpGlobals->time;

	pev->sequence = RANDOM_LONG(3, 6); // no ResetSequenceInfo here, unlike the other three
	pev->framerate = 1.0;
	pev->gravity = RC2Pref(RC2P_Grenade_GravGrenGravityScaler).f;
	pev->friction = RC2Pref(RC2P_Grenade_GravGrenFrictionScaler).f;
	m_fRegisteredSound = false;
}

// orig 0x59320
void CGravGrenade::PreDetonate()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (pev->dmgtime - 2.0 < gpGlobals->time)
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1);

	// on the ground in its last 0.75 s: rise into the air
	if (pev->dmgtime - 0.75 < gpGlobals->time && (pev->flags & FL_ONGROUND) != 0)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector(0, 0, RC2Pref(RC2P_Grenade_GravGrenArmRiseHeight).f * 1.3333334f);
		pev->flags &= ~FL_ONGROUND;
		pev->origin.z += 1.0;
		UTIL_SetOrigin(pev, pev->origin);
		SetTouch(NULL);
		pev->sequence = RANDOM_LONG(3, 5);
		ResetSequenceInfo();
		pev->framerate = 3.0;
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		StartDetonate();
		return;
	}

	if (pev->waterlevel != 0 && pev->movetype != MOVETYPE_FLY)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

// orig 0x59540: stop in the air and start pulling
void CGravGrenade::StartDetonate()
{
	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_FLY;
	SetTouch(&CGravGrenade::KillTouch);
	SetThink(&CGravGrenade::Detonating);
	pev->nextthink = gpGlobals->time;

	pev->sequence = RANDOM_LONG(3, 5);
	ResetSequenceInfo();
	pev->framerate = 2.0;
	pev->dmgtime = RC2Pref(RC2P_Grenade_GravGrenDetonateTime).f + gpGlobals->time;
	pev->owner = NULL;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));

	m_iPullPoint = RC2AddPullPoint(pev->origin, RC2Pref(RC2P_Grenade_GravGrenPullRadius).f,
		RC2Pref(RC2P_Grenade_GravGrenPullPlayerConstant).f, RC2Pref(RC2P_Grenade_GravGrenPullMonsterConstant).f);
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 500, RC2Pref(RC2P_Grenade_GravGrenDetonateTime).f);

	PLAYBACK_EVENT_FULL(0, ENT(pev), m_usDetonate, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
	PLAYBACK_EVENT_FULL(0, ENT(pev), m_usGlow, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
}

// orig 0x596f0: annihilate what has been pulled within 8 units, or within 24 and in sight
void CGravGrenade::Detonating()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.05;

	if (pev->dmgtime < gpGlobals->time)
	{
		EndDetonate();
		return;
	}

	CBaseEntity* pAttacker = this;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pAttacker = CBaseEntity::Instance(pev->dmg_inflictor);

	float flDist = 0;
	CBaseEntity* pEntity = NULL;
	while ((pEntity = RC2FindEntityNear(pEntity, pev->origin, 24.0, &flDist, NULL)) != NULL)
	{
		if (flDist > 8.0)
		{
			TraceResult tr;
			UTIL_TraceLine(pev->origin, pEntity->Center(), ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
				continue;
		}
		pEntity->GravGrenAnnihilate(this, pAttacker);
	}
}

// orig 0x59820
void CGravGrenade::EndDetonate()
{
	PLAYBACK_EVENT_FULL(FEV_GLOBAL, ENT(pev), m_usDetonate, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
	RadiusDamage(pev->origin, pev, RC2GrenadeAttacker(pev), RC2Pref(RC2P_Grenade_GravGrenFinalBlastDamage).f, CLASS_NONE, DMG_BLAST);
	RC2RemovePullPoint(m_iPullPoint);
	SetThink(NULL);
	SetTouch(NULL);
	UTIL_Remove(this);
}

// orig 0x598e0: anything that touches it while it pulls is annihilated
void CGravGrenade::KillTouch(CBaseEntity* pOther)
{
	CBaseEntity* pAttacker = this;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pAttacker = CBaseEntity::Instance(pev->dmg_inflictor);
	pOther->GravGrenAnnihilate(this, pAttacker);
}
