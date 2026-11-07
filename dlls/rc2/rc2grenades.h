/***
*
*	Rocket Crowbar 2: the hand grenade's four grenades (weapon_handgrenade throws the first
*	three; nothing in the server throws ultima_grenade). Each is a CGrenade that RC2 keeps in
*	its own file here (rc2.dll keeps no file names; ours are named after the class).
*
*	All four store who threw them in pev->dmg_inflictor at spawn (pev->owner is cleared once they
*	arm, and the hand grenade may hand ownership to the entity its player acts through), and
*	credit their damage to that entity.
*
****/

#pragma once

#include "p3b_temp.h"
#include "p3d_temp.h"

// The thrower as an attacker, as every one of these grenades works it out: the dmg_inflictor
// entity while it is valid, otherwise the grenade itself. Name ours.
inline entvars_t* RC2GrenadeAttacker(entvars_t* pev)
{
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		return pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;
	return pev;
}

// As RC2GrenadeAttacker, as an entity: NULL when there is no valid thrower. Name ours.
inline CBaseEntity* RC2GrenadeThrower(entvars_t* pev)
{
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		return CBaseEntity::Instance(pev->dmg_inflictor);
	return NULL;
}

class CGravGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override;

	void EXPORT PreDetonate();
	void EXPORT Detonating();
	void EXPORT EndDetonate();
	void EXPORT KillTouch(CBaseEntity* pOther);
	void StartDetonate(); // name ours

	int m_iPullPoint;			 // orig +0x2b8, ours: RC2AddPullPoint's slot, -1 for none
	unsigned short m_usDetonate; // orig +0x2bc, ours: events/gravgrenadedet.sc
	unsigned short m_usGlow;	 // orig +0x2be, ours: events/gravgrenadeglow.sc
};

class CSuckGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override;

	void EXPORT PreDetonate();
	void EXPORT Detonating();
	void EXPORT EndDetonate();
	void StartDetonate(); // name ours

	int m_iPullPoint; // orig +0x2b8, ours: RC2AddPullPoint's slot, -1 for none
};

class CAFGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT PreDetonate();
	void EXPORT Detonate();
	void BlastEntity(CBaseEntity* pTarget, entvars_t* pevAttacker, entvars_t* pevInflictor, float flDamage, float flTime, float flVelocity, Vector vecDir, TraceResult* ptr); // name ours

	unsigned short m_usDetonate; // orig +0x2b8, ours: events/afgrendet.sc
};

class CUltimaGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override;
	bool IsSeeThroughFor(edict_t* pViewed) override { return true; } // orig 0x5acb0

	void EXPORT PreDetonate();
	void EXPORT KillTouch(CBaseEntity* pOther);
	void EXPORT DetonateWarmup();
	void EXPORT DetonateExplode();
	void Arm();	  // name ours
	void Abort(); // name ours
	void Blast(); // name ours
	bool BlastLineClear(Vector vecStart, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iDepth); // name ours

	unsigned short m_usDetonate; // orig +0x2b8, ours: events/ultgrendet.sc
};
