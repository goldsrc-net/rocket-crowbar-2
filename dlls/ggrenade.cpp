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
/*

===== generic grenade.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "decals.h"


//===================grenade


LINK_ENTITY_TO_CLASS(grenade, CGrenade); // orig 0x50910 (files-rc2.tsv puts it in "new: genericmonster .. ggrenade")

// Grenades flagged with this will be triggered when the owner calls detonateSatchelCharges
#define SF_DETONATE 0x0001

//
// Grenade Explode
//
void CGrenade::Explode(Vector vecSrc, Vector vecAim)
{
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -32), ignore_monsters, ENT(pev), &tr);

	Explode(&tr, DMG_BLAST);
}

// UNDONE: temporary scorching for PreAlpha - find a less sleazy permenant solution.
// orig 0x509f0
// Rocket Crowbar 2: the explosion is the client's events/explode.sc now (sprites, decal,
// debris sounds and sparks all went there), so the server only moves the grenade out of the
// wall, plays the event, does the damage and removes itself (no Smoke think).
void CGrenade::Explode(TraceResult* pTrace, int bitsDamageType)
{
	pev->model = iStringNull; //invisible
	pev->solid = SOLID_NOT;	  // intangible

	pev->takedamage = DAMAGE_NO;

	UTIL_KillEntityEffects(ENT(pev));

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pTrace->vecEndPos + pTrace->vecPlaneNormal * 16, NORMAL_EXPLOSION_VOLUME, 3.0);

	// Pull out of the wall a bit
	if (pTrace->flFraction != 1.0)
		UTIL_SetOrigin(pev, pTrace->vecEndPos + pTrace->vecPlaneNormal * 16);

	PlayExplosionEvent(pTrace);

	if (pTrace->flFraction != 1.0)
	{
		// RC2 moves it again, by SDK 2.3's pull-out distance (halflife-updated's fix of that
		// expression doesn't apply: this is RC2's own line, with a clamp SDK 2.3 didn't have)
		float flPull = (pev->dmg - 24) * 0.6;
		if (flPull < 0)
			flPull = 0;
		else if (flPull > 256)
			flPull = 256;
		UTIL_SetOrigin(pev, pTrace->vecEndPos + pTrace->vecPlaneNormal * flPull);
	}

	entvars_t* pevOwner;
	if (pev->owner)
		pevOwner = VARS(pev->owner);
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	// SDK 2.3's member RadiusDamage, from pev->origin (halflife-updated's "counteract the + 1"
	// adjustment was part of the stock body RC2 replaced)
	RadiusDamage(pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType);

	SetThink(&CGrenade::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

// Rocket Crowbar 2: the PLAYBACK_EVENT_FULL both explosions send (inline in each in the original):
// fparam1 the damage, iparam1 the packed normal facing out of the surface, bparam2 set when the
// trace hit nothing.
void CGrenade::PlayExplosionEvent(TraceResult* pTrace) // name ours
{
	PLAYBACK_EVENT_FULL(0, edict(), g_usExplodeEvent, 0.0, pev->origin, g_vecZero, pev->dmg, 0.0,
		UTIL_PackNormal(-pTrace->vecPlaneNormal), 0, 0, pTrace->flFraction == 1.0 ? 1 : 0);
}

// orig 0x50cc0
// Rocket Crowbar 2: the explosion of Detonate2 and ExplodeTouch2. Explode's, with RC2's own
// radius damage (radius 2.5 times the damage, falloff exponent 1, blast) instead of the stock one.
void CGrenade::RadialExplode(TraceResult* pTrace) // name ours
{
	pev->model = iStringNull;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;

	UTIL_KillEntityEffects(ENT(pev));

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pTrace->vecEndPos + pTrace->vecPlaneNormal * 16, NORMAL_EXPLOSION_VOLUME, 3.0);

	if (pTrace->flFraction != 1.0)
	{
		float flPull = (pev->dmg - 24) * 0.6;
		if (flPull < 0)
			flPull = 0;
		else if (flPull > 256)
			flPull = 256;
		UTIL_SetOrigin(pev, pTrace->vecEndPos + pTrace->vecPlaneNormal * flPull);
	}

	PlayExplosionEvent(pTrace);

	entvars_t* pevOwner;
	if (pev->owner)
		pevOwner = VARS(pev->owner);
	else
		pevOwner = NULL;

	pev->owner = NULL;

	RC2RadiusDamage(pevOwner, pev, edict(), pev->origin, pev->dmg * 2.5, pev->dmg, 1.0, DMG_BLAST, 1);

	SetThink(&CGrenade::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}


void CGrenade::Smoke()
{
	if (UTIL_PointContents(pev->origin) == CONTENTS_WATER)
	{
		UTIL_Bubbles(pev->origin - Vector(64, 64, 64), pev->origin + Vector(64, 64, 64), 100);
	}
	else
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE((pev->dmg - 50) * 0.80); // scale * 10
		WRITE_BYTE(12);						// framerate
		MESSAGE_END();
	}
	UTIL_Remove(this);
}

// orig 0x51070
void CGrenade::Killed(entvars_t* pevAttacker, int iGib)
{
	Detonate();
}


// Timed grenade, this think is called when time runs out.
// orig 0x51080
void CGrenade::DetonateUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	SetThink(&CGrenade::Detonate);
	pev->nextthink = gpGlobals->time;
}

void CGrenade::PreDetonate()
{
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 400, 0.3);

	SetThink(&CGrenade::Detonate);
	pev->nextthink = gpGlobals->time + 1;
}


void CGrenade::Detonate()
{
	TraceResult tr;
	Vector vecSpot; // trace starts here!

	vecSpot = pev->origin + Vector(0, 0, 8);
	UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -40), ignore_monsters, ENT(pev), &tr);

	Explode(&tr, DMG_BLAST);
}


// orig 0x51190
// Rocket Crowbar 2: Detonate with the radial explosion. Slow (64 or less): a flying grenade
// explodes where it is with no surface, others trace 64 down from 8 above; fast: traced along
// its flight like ExplodeTouch2.
void CGrenade::Detonate2()
{
	TraceResult tr;

	if (pev->velocity.Length() <= 64)
	{
		if (pev->movetype == MOVETYPE_FLY || pev->movetype == MOVETYPE_BOUNCEMISSILE)
		{
			// the original fills only these three fields of an uninitialized TraceResult;
			// RadialExplode reads no others
			memset(&tr, 0, sizeof(tr));
			tr.flFraction = 1.0;
			tr.vecEndPos = pev->origin;
			tr.vecPlaneNormal = g_vecZero;
		}
		else
		{
			Vector vecSpot = pev->origin + Vector(0, 0, 8);
			UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -64), ignore_monsters, ENT(pev), &tr);
		}
	}
	else
	{
		Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
		UTIL_TraceLine(vecSpot, vecSpot + pev->velocity.Normalize() * 128, ignore_monsters, ENT(pev), &tr);
	}

	RadialExplode(&tr);
}


//
// Contact grenade, explode when it touches something
//
void CGrenade::ExplodeTouch(CBaseEntity* pOther)
{
	TraceResult tr;
	Vector vecSpot; // trace starts here!

	pev->enemy = pOther->edict();

	vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine(vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr);

	Explode(&tr, DMG_BLAST);
}


// orig 0x51710
// Rocket Crowbar 2: ExplodeTouch with the radial explosion and a 128-unit trace
void CGrenade::ExplodeTouch2(CBaseEntity* pOther)
{
	TraceResult tr;

	pev->enemy = pOther->edict();

	Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine(vecSpot, vecSpot + pev->velocity.Normalize() * 128, ignore_monsters, ENT(pev), &tr);

	RadialExplode(&tr);
}


void CGrenade::DangerSoundThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5, pev->velocity.Length(), 0.2);
	pev->nextthink = gpGlobals->time + 0.2;

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
	}
}


void CGrenade::BounceTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t* pevOwner = VARS(pev->owner);
		if (pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace();
			ClearMultiDamage();
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB);
			ApplyMultiDamage(pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;
	// pev->avelocity = Vector (300, 300, 300);

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity.
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = pev->velocity;
	vecTestVelocity.z *= 0.45;

	if (!m_fRegisteredSound && vecTestVelocity.Length() <= 60)
	{
		//ALERT( at_console, "Grenade Registered!: %f\n", vecTestVelocity.Length() );

		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving.
		// go ahead and emit the danger sound.

		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3);
		m_fRegisteredSound = true;
	}

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.8;

		pev->sequence = RANDOM_LONG(1, 1);
		// SDK 2.3's (stock in RC2); halflife-updated called ResetSequenceInfo() here
	}
	else
	{
		// play bounce sound
		BounceSound();
	}
	pev->framerate = pev->velocity.Length() / 200.0;
	if (pev->framerate > 1.0)
		pev->framerate = 1;
	else if (pev->framerate < 0.5)
		pev->framerate = 0; // SDK 2.3's (stock in RC2); halflife-updated also reset the frame to 0
}



void CGrenade::SlideTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// pev->avelocity = Vector (300, 300, 300);

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;

		if (pev->velocity.x != 0 || pev->velocity.y != 0)
		{
			// maintain sliding sound
		}
	}
	else
	{
		BounceSound();
	}
}

void CGrenade::BounceSound()
{
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/grenade_hit1.wav", 0.25, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/grenade_hit2.wav", 0.25, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/grenade_hit3.wav", 0.25, ATTN_NORM);
		break;
	}
}

// orig 0x51ea0
void CGrenade::TumbleThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1);
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink(&CGrenade::Detonate);
	}
	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}


void CGrenade::Spawn()
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING("grenade");

	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/grenade.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->dmg = 100;
	m_fRegisteredSound = false;
}


CGrenade* CGrenade::ShootContact(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity)
{
	CGrenade* pGrenade = GetClassPtr((CGrenade*)NULL);
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.5; // lower gravity since grenade is aerodynamic and engine doesn't know it.
	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);

	// make monsters afaid of it while in the air
	pGrenade->SetThink(&CGrenade::DangerSoundThink);
	pGrenade->pev->nextthink = gpGlobals->time;

	// Tumble in air
	pGrenade->pev->avelocity.x = RANDOM_FLOAT(-100, -500);

	// Explode on contact
	pGrenade->SetTouch(&CGrenade::ExplodeTouch);

	pGrenade->pev->dmg = gSkillData.plrDmgM203Grenade;

	return pGrenade;
}


// orig 0x521b0
CGrenade* CGrenade::ShootTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time)
{
	CGrenade* pGrenade = GetClassPtr((CGrenade*)NULL);
	pGrenade->Spawn();
	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);

	pGrenade->SetTouch(&CGrenade::BounceTouch); // Bounce if touched

	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed().

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink(&CGrenade::TumbleThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector(0, 0, 0);
	}

	pGrenade->pev->sequence = RANDOM_LONG(3, 6);
	pGrenade->pev->framerate = 1.0;

	// Tumble through the air
	// pGrenade->pev->avelocity.x = -400;

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8;

	SET_MODEL(ENT(pGrenade->pev), "models/w_grenade.mdl");
	// SDK 2.3's (stock in RC2); halflife-updated set the model before the sequence and called
	// ResetSequenceInfo() here
	pGrenade->pev->dmg = 100;

	return pGrenade;
}


CGrenade* CGrenade::ShootSatchelCharge(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity)
{
	CGrenade* pGrenade = GetClassPtr((CGrenade*)NULL);
	pGrenade->pev->movetype = MOVETYPE_BOUNCE;
	pGrenade->pev->classname = MAKE_STRING("grenade");

	pGrenade->pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pGrenade->pev), "models/grenade.mdl"); // Change this to satchel charge model

	UTIL_SetSize(pGrenade->pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pGrenade->pev->dmg = 200;
	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = g_vecZero;
	pGrenade->pev->owner = ENT(pevOwner);

	// Detonate in "time" seconds
	pGrenade->SetThink(&CGrenade::SUB_DoNothing);
	pGrenade->SetUse(&CGrenade::DetonateUse);
	pGrenade->SetTouch(&CGrenade::SlideTouch);
	pGrenade->pev->spawnflags = SF_DETONATE;

	pGrenade->pev->friction = 0.9;

	return pGrenade;
}



// orig 0x52530
void CGrenade::UseSatchelCharges(entvars_t* pevOwner, SATCHELCODE code)
{
	edict_t* pentFind;
	edict_t* pentOwner;

	if (!pevOwner)
		return;

	CBaseEntity* pOwner = CBaseEntity::Instance(pevOwner);

	pentOwner = pOwner->edict();

	pentFind = FIND_ENTITY_BY_CLASSNAME(NULL, "grenade");
	while (!FNullEnt(pentFind))
	{
		CBaseEntity* pEnt = Instance(pentFind);
		if (pEnt)
		{
			if (FBitSet(pEnt->pev->spawnflags, SF_DETONATE) && pEnt->pev->owner == pentOwner)
			{
				if (code == SATCHEL_DETONATE)
					pEnt->Use(pOwner, pOwner, USE_ON, 0);
				else // SATCHEL_RELEASE
					pEnt->pev->owner = NULL;
			}
		}
		pentFind = FIND_ENTITY_BY_CLASSNAME(pentFind, "grenade");
	}
}

//======================end grenade
