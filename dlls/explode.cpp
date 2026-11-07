/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

===== explode.cpp ========================================================

  Explosion-related code

*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "decals.h"
#include "explode.h"
#include "weapons.h"
#include "gamerules.h"
#include "soundent.h"
#include "p2shared.h"

// Rocket Crowbar 2 (p2): in multiplayer env_explosion and ExplodeAt leave the fireball, decal and
// sparks to the client's explosion event; reference/dossiers/p2.md.
extern unsigned short g_usRC2Explode; // orig .data 0x1afd18, name ours: events/explode.sc (W_Precache)
// orig 0xf5920 (util), name ours: a direction packed into an int, 10 bits a component (9 of
// magnitude * 511 and a sign bit)
extern int UTIL_PackDirection(Vector vecDir);
// orig 0xf3900 (util), name ours: RC2's other radius damage. VERIFY: the arguments' meaning

// orig 0x40960: Vector::operator float*(), the header's inline (vector.h), emitted here

// Spark Shower
class CShower : public CBaseEntity
{
	void Spawn() override;
	void Think() override;
	void Touch(CBaseEntity* pOther) override;
	int ObjectCaps() override { return FCAP_DONT_SAVE; } // orig 0x40010
};

LINK_ENTITY_TO_CLASS(spark_shower, CShower); // orig 0x3ffa0

// orig 0x40020. RC2's is SDK 2.3's (it differs from the VC6 build only by the base's offsets).
void CShower::Spawn()
{
	pev->velocity = RANDOM_FLOAT(200, 300) * pev->angles;
	pev->velocity.x += RANDOM_FLOAT(-100.f, 100.f);
	pev->velocity.y += RANDOM_FLOAT(-100.f, 100.f);
	if (pev->velocity.z >= 0)
		pev->velocity.z += 200;
	else
		pev->velocity.z -= 200;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.5;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->solid = SOLID_NOT;
	SET_MODEL(edict(), "models/grenade.mdl"); // Need a model, just use the grenade, we don't draw it anyway
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->speed = RANDOM_FLOAT(0.5, 1.5);

	pev->angles = g_vecZero;
}


// orig 0x401a0, SDK 2.3's
void CShower::Think()
{
	UTIL_Sparks(pev->origin);

	pev->speed -= 0.1;
	if (pev->speed > 0)
		pev->nextthink = gpGlobals->time + 0.1;
	else
		UTIL_Remove(this);
	pev->flags &= ~FL_ONGROUND;
}

void CShower::Touch(CBaseEntity* pOther)
{
	if ((pev->flags & FL_ONGROUND) != 0)
		pev->velocity = pev->velocity * 0.1;
	else
		pev->velocity = pev->velocity * 0.6;

	if ((pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y) < 10.0)
		pev->speed = 0;
}

class CEnvExplosion : public CBaseMonster
{
public:
	void Spawn() override;
	void EXPORT Smoke();
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int m_iMagnitude;  // how large is the fireball? how much damage?
	int m_spriteScale; // what's the exact fireball sprite scale?
};

TYPEDESCRIPTION CEnvExplosion::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvExplosion, m_iMagnitude, FIELD_INTEGER),
		DEFINE_FIELD(CEnvExplosion, m_spriteScale, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CEnvExplosion, CBaseMonster); // orig 0x40310 (Save), orig 0x40340 (Restore)
LINK_ENTITY_TO_CLASS(env_explosion, CEnvExplosion);  // orig 0x40370

bool CEnvExplosion::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "iMagnitude"))
	{
		m_iMagnitude = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CEnvExplosion::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
	/*
	if ( m_iMagnitude > 250 )
	{
		m_iMagnitude = 250;
	}
	*/

	float flSpriteScale;
	flSpriteScale = (m_iMagnitude - 50) * 0.6;

	/*
	if ( flSpriteScale > 50 )
	{
		flSpriteScale = 50;
	}
	*/
	if (flSpriteScale < 10)
	{
		flSpriteScale = 10;
	}

	m_spriteScale = (int)flSpriteScale;
}

// orig 0x404c0. RC2's is SDK 2.3's in single player; in multiplayer the client's explosion event
// draws it, the activator is the attacker, and it goes at once (unless repeatable) without the
// smoke think.
void CEnvExplosion::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	TraceResult tr;

	pev->model = iStringNull; //invisible
	pev->solid = SOLID_NOT;	  // intangible

	Vector vecSpot; // trace starts here!

	vecSpot = pev->origin + Vector(0, 0, 8);

	UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -40), ignore_monsters, ENT(pev), &tr);

	if (g_pGameRules->IsMultiplayer())
	{
		UTIL_SetOrigin(pev, tr.vecEndPos);
		PLAYBACK_EVENT_FULL(0, edict(), g_usRC2Explode, 0, tr.vecEndPos, g_vecZero, (float)m_iMagnitude, 0, UTIL_PackDirection(-tr.vecPlaneNormal), 0, 0, 0);

		// Pull out of the wall a bit
		if (tr.flFraction != 1.0)
			pev->origin = tr.vecEndPos + (tr.vecPlaneNormal * (m_iMagnitude - 24) * 0.6);

		if ((pev->spawnflags & SF_ENVEXPLOSION_NODAMAGE) == 0)
			::RadiusDamage(pev->origin, pev, pActivator ? pActivator->pev : pev, (float)m_iMagnitude, (float)m_iMagnitude * 2.5, CLASS_NONE, DMG_BLAST);

		if ((pev->spawnflags & SF_ENVEXPLOSION_REPEATABLE) == 0)
			UTIL_Remove(this);
		return;
	}

	// Pull out of the wall a bit
	if (tr.flFraction != 1.0)
	{
		pev->origin = tr.vecEndPos + (tr.vecPlaneNormal * (m_iMagnitude - 24) * 0.6);
	}
	else
	{
		pev->origin = pev->origin;
	}

	// draw decal
	if ((pev->spawnflags & SF_ENVEXPLOSION_NODECAL) == 0)
	{
		if (RANDOM_FLOAT(0, 1) < 0.5)
		{
			UTIL_DecalTrace(&tr, DECAL_SCORCH1);
		}
		else
		{
			UTIL_DecalTrace(&tr, DECAL_SCORCH2);
		}
	}

	// draw fireball
	if ((pev->spawnflags & SF_ENVEXPLOSION_NOFIREBALL) == 0)
	{
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE((byte)m_spriteScale); // scale * 10
		WRITE_BYTE(15);					 // framerate
		WRITE_BYTE(TE_EXPLFLAG_NONE);
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(0);	// no sprite
		WRITE_BYTE(15); // framerate
		WRITE_BYTE(TE_EXPLFLAG_NONE);
		MESSAGE_END();
	}

	// do damage
	if ((pev->spawnflags & SF_ENVEXPLOSION_NODAMAGE) == 0)
	{
		RadiusDamage(pev, pev, m_iMagnitude, CLASS_NONE, DMG_BLAST);
	}

	SetThink(&CEnvExplosion::Smoke);
	pev->nextthink = gpGlobals->time + 0.3;

	// draw sparks
	if ((pev->spawnflags & SF_ENVEXPLOSION_NOSPARKS) == 0)
	{
		int sparkCount = RANDOM_LONG(0, 3);

		for (int i = 0; i < sparkCount; i++)
		{
			Create("spark_shower", pev->origin, tr.vecPlaneNormal, NULL);
		}
	}
}

void CEnvExplosion::Smoke()
{
	if ((pev->spawnflags & SF_ENVEXPLOSION_NOSMOKE) == 0)
	{
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE((byte)m_spriteScale); // scale * 10
		WRITE_BYTE(12);					 // framerate
		MESSAGE_END();
	}

	if ((pev->spawnflags & SF_ENVEXPLOSION_REPEATABLE) == 0)
	{
		UTIL_Remove(this);
	}
}


// HACKHACK -- create one of these and fake a keyvalue to get the right explosion setup
void ExplosionCreate(const Vector& center, const Vector& angles, edict_t* pOwner, int magnitude, bool doDamage)
{
	KeyValueData kvd;
	char buf[128];

	CBaseEntity* pExplosion = CBaseEntity::Create("env_explosion", center, angles, pOwner);
	sprintf(buf, "%3d", magnitude);
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue(&kvd);
	if (!doDamage)
		pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->Use(NULL, NULL, USE_TOGGLE, 0);
}


// orig 0x40ab0, name ours: an invisible explosion of class pszClassname (its kills read as that)
// at vecOrigin, against a surface facing back along vecDir; the client's explosion event draws
// it. bOtherDamage uses RC2's other radius damage (0xf3900).
void ExplodeAt(Vector vecOrigin, Vector vecDir, float flDamage, const char* pszClassname, entvars_t* pevOwner, bool bOtherDamage)
{
	CBaseEntity* pExplosion = GetClassPtr((CBaseEntity*)NULL);
	pExplosion->Spawn();
	pExplosion->pev->effects |= EF_NODRAW;
	pExplosion->pev->classname = MAKE_STRING(pszClassname);

	entvars_t* pev = pExplosion->pev;
	if (!bOtherDamage)
	{
		if (pevOwner)
			::RadiusDamage(vecOrigin - vecDir * 8, pev, pevOwner, flDamage, flDamage * 2.5, CLASS_NONE, DMG_BLAST);
		else
			::RadiusDamage(vecOrigin - vecDir * 8, pev, pev, flDamage, flDamage * 2.5, CLASS_NONE, DMG_BLAST);
	}
	else
	{
		if (pevOwner)
			RC2RadiusDamage(pevOwner, pev, NULL, vecOrigin - vecDir * 8, flDamage * 2.5, flDamage, 1.0, DMG_BLAST, 1);
		else
			RC2RadiusDamage(pev, pev, NULL, vecOrigin - vecDir * 8, flDamage * 2.5, flDamage, 1.0, DMG_BLAST, 1);
	}

	UTIL_SetOrigin(pev, vecOrigin);
	PLAYBACK_EVENT_FULL(0, pExplosion->edict(), g_usRC2Explode, 0, vecOrigin, g_vecZero, flDamage, 0, UTIL_PackDirection(vecDir), 0, 0, 0);
	pev->flags |= FL_KILLME;

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, vecOrigin - vecDir * 16, 1024, 3.0);
}
