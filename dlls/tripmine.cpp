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
#include "effects.h"
#include "gamerules.h"
#include "rc2prefs.h"
#ifndef CLIENT_DLL
#include "rc2/shield.h"
#include "UserMessages.h"
#else
#endif

#define TRIPMINE_PRIMARY_VOLUME 450

#ifndef CLIENT_DLL

#include "soundent.h"
#include "decals.h"

// Rocket Crowbar 2's tripmines (orig 0xe7e60-0xeca6f). SDK 2.3's mine that exploded when its
// beam was crossed is gone: RC2's beam is a laser that hurts what it touches (each target's
// GlobalBeamDamage, slot 62, as DMG_ENERGYBEAM, after RC2_ShieldFilterDamage lets it through)
// and the mine itself only blows up when shot, when its life runs out or when what it sticks to
// moves. Four kinds share the code: monster_tripmine (one beam), bouncy_tripmine (a beam that
// reflects off surfaces), punch_tripmine (a beam that bores through walls) and target_tripmine
// (a short upward beam that zaps targets in range). The three new ones' class names are lost
// (their vtables have no exported methods): ours.
//
// The mine has no save table in RC2 (CBaseMonster's Save/Restore are in its vtable).
// pev fields it reuses: dmgtime = power-up time, air_finished = end of life, armorvalue =
// how long a beam may go without a full update, impulse = "update the beam now" (set when it
// powers up and after it hurts something).

// orig 0xe7e60: VC6's inline CBasePlayer constructor (vtable 0x113a18, the route arrays), which
// the linker placed among tripmine.obj's functions; called from CTriggerHurt's code. The
// compiler writes it, so there is no source for it.

// CTripmineGrenade: weapons.h

LINK_ENTITY_TO_CLASS(monster_tripmine, CTripmineGrenade); // orig 0xe7ea0

// orig 0xe7f60
bool CTripmineGrenade::IsViewOfPlayer(CBaseEntity* pPlayer)
{
	// the planter sees through his mine until he has walked away from it
	if (m_hOwner != NULL && pPlayer->edict() == m_hOwner->edict() && !m_bOwnerLeft)
		return true;

	return CBaseEntity::IsViewOfPlayer(pPlayer);
}

// orig 0xe7fc0
void CTripmineGrenade::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;

	SET_MODEL(ENT(pev), "models/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_WORLD;
	ResetSequenceInfo();
	pev->framerate = 0;
	m_iAmmoCost = RC2PrefWord(RC2P_Tripmine_PrimaryAmmoCost);

	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	UTIL_SetOrigin(pev, pev->origin);

	SetThink(&CTripmineGrenade::PowerupThink);
	pev->nextthink = gpGlobals->time + 0.1;

	if ((pev->spawnflags & 1) != 0)
		pev->dmgtime = gpGlobals->time + 1.0; // power up quickly
	else
		pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_Tripmine_GlobalPowerupTime).f;

	m_flLastBeamTime = gpGlobals->time;
	pev->dmg = RC2Pref(RC2P_Tripmine_GlobalExplodeDamage).f;
	pev->health = RC2Pref(RC2P_Tripmine_GlobalHealth).f;

	// find the surface it sits on
	TraceResult tr;
	UTIL_MakeAimVectors(pev->angles);
	UTIL_TraceLine(pev->origin + gpGlobals->v_forward * 8, pev->origin - gpGlobals->v_forward * 64, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0)
	{
		Killed(pev, GIB_NORMAL);
		return;
	}

	pev->takedamage = DAMAGE_YES;
	pev->armorvalue = 0.2;

	if (UTIL_IsValidEntity(pev->owner))
	{
		// play deploy sound
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav", 1.0, ATTN_NORM);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/mine_charge.wav", 0.2, ATTN_NORM); // chargeup
		m_hOwner = CBaseEntity::Instance(pev->owner);
		m_bOwnerLeft = false;
	}
	else
	{
		m_hOwner = NULL;
		m_bOwnerLeft = true;
	}
	pev->owner = NULL;

	// what it sticks to becomes pev->owner and is followed, unless it is the world
	CBaseEntity* pAttach = NULL;
	if (!UTIL_IsValidEntity(tr.pHit))
	{
		m_bOnWorld = true;
	}
	else
	{
		pAttach = CBaseEntity::Instance(tr.pHit);
		if (pAttach)
		{
			if (0 == ENTINDEX(pAttach->edict()))
				m_bOnWorld = true;
			if (!m_bOnWorld)
			{
				m_posOwner = pAttach->pev->origin;
				m_angleOwner = pAttach->pev->angles;
			}
		}
	}

	m_vecOrigin = pev->origin;
	pev->angles = UTIL_VecToAngles(tr.vecPlaneNormal);
	m_vecDir = tr.vecPlaneNormal;
	m_vecEnd = g_vecZero;
	pev->air_finished = gpGlobals->time + RC2Pref(RC2P_Tripmine_GlobalMaxLife).f;

	if (pAttach && !m_bOnWorld)
		pev->owner = pAttach->edict();

	MakeBeam();
}

// orig 0xe8440
void CTripmineGrenade::Precache()
{
	PRECACHE_MODEL("models/v_tripmine.mdl");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/mine_activate.wav");
	PRECACHE_SOUND("weapons/mine_charge.wav");
	PRECACHE_MODEL("sprites/zbeam2.spr");
}

// orig 0xe8480
void CTripmineGrenade::KillThink()
{
	// the planter gets the credit
	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();
	else
		pev->owner = edict();

	TraceResult tr;
	UTIL_MakeAimVectors(pev->angles);
	UTIL_TraceLine(pev->origin + gpGlobals->v_forward * 8, pev->origin - gpGlobals->v_forward * 64, ignore_monsters, ENT(pev), &tr);

	RadialExplode(&tr);
}

// iGib 0x10 is the tripmine's own "explode now" (CheckAttach): no delay.
// orig 0xe85b0
void CTripmineGrenade::Killed(entvars_t* pevAttacker, int iGib)
{
	KillBeam();
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	SetTouch(NULL);

	if (iGib != 0x10)
	{
		SetThink(&CTripmineGrenade::KillThink);
		pev->nextthink = gpGlobals->time + 0.05;
		return;
	}

	SetThink(NULL);
	KillThink();
}

// Follows what the mine sticks to: when that is gone the mine's life ends, when it has moved
// the mine explodes (bExplode) or turns back into a weapon_tripmine holding m_iAmmoCost mines.
// orig 0xe8610
void CTripmineGrenade::CheckAttach(bool bExplode)
{
	if (!UTIL_IsValidEntity(pev->owner))
	{
		if (!m_bOnWorld)
		{
			pev->owner = NULL;
			pev->air_finished = gpGlobals->time - 1.0;
		}
		return;
	}

	if (m_bOnWorld)
		return;

	if (pev->owner->v.origin == m_posOwner && pev->owner->v.angles == m_angleOwner)
		return;

	pev->owner = NULL;
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav", 0, 0, SND_STOP, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/mine_charge.wav", 0, 0, SND_STOP, 100);

	if (bExplode)
	{
		Killed(pev, 0x10);
		return;
	}

	g_fRC2Randomize = 0;
	CBaseEntity* pMine = Create("weapon_tripmine", pev->origin + m_vecDir * 24, pev->angles, NULL);
	pMine->pev->spawnflags |= SF_NORESPAWN;
	((CBasePlayerWeapon*)pMine)->m_iDefaultAmmo = m_iAmmoCost;
	g_fRC2Randomize = 1;
	KillBeam();
	UTIL_Remove(this);
}

// orig 0xe8830
void CTripmineGrenade::PowerupThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->origin != m_vecOrigin)
		UTIL_SetOrigin(pev, m_vecOrigin);

	if (pev->dmgtime < gpGlobals->time)
	{
		if (!m_bOnWorld)
		{
			// still on what it was planted on? (traces skip pev->owner, so clear it)
			edict_t* oldowner = pev->owner;
			pev->owner = NULL;
			TraceResult tr;
			UTIL_TraceLine(pev->origin + m_vecDir * 8, pev->origin - m_vecDir * 64, dont_ignore_monsters, ENT(pev), &tr);
			pev->owner = oldowner;

			if (tr.flFraction >= 1.0 || tr.pHit != pev->owner)
			{
				g_fRC2Randomize = 0;
				CBaseEntity* pMine = Create("weapon_tripmine", pev->origin + m_vecDir * 24, pev->angles, NULL);
				pMine->pev->spawnflags |= SF_NORESPAWN;
				((CBasePlayerWeapon*)pMine)->m_iDefaultAmmo = m_iAmmoCost;
				g_fRC2Randomize = 1;
				KillBeam();
				UTIL_Remove(this);
				return;
			}
		}

		m_flLastBeamTime = gpGlobals->time;
		pev->impulse = 1;
		pev->solid = SOLID_BBOX;

		// play enabled sound (flags 1, SDK 2.3's 1.0)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/mine_activate.wav", 0.5, ATTN_NORM, 1, 75);

		SetThink(&CTripmineGrenade::TraceThink);
		pev->nextthink = gpGlobals->time;

		if (m_pBeam)
			m_pBeam->pev->renderamt = 200;
	}

	CheckAttach(false);
}

// orig 0xe8b70
void CTripmineGrenade::TraceThink()
{
	if (pev->air_finished < gpGlobals->time)
	{
		Killed(pev, GIB_NORMAL);
		return;
	}

	if (pev->origin != m_vecOrigin)
		UTIL_SetOrigin(pev, m_vecOrigin);

	if (!m_bOwnerLeft && m_hOwner != NULL && (m_hOwner->pev->origin - pev->origin).Length() > 64)
		m_bOwnerLeft = true;

	BeamUpdate();
	CheckAttach(true);
}

// orig 0xe8c60
void CTripmineGrenade::MakeBeam()
{
	m_pBeam = CBeam::BeamCreate("sprites/zbeam2.spr", 26);
	if (m_pBeam)
	{
		m_pBeam->PointsInit(pev->origin, pev->origin);
		m_pBeam->pev->rendercolor = Vector(255, 0, 0);
		m_pBeam->pev->animtime = 32; // scroll rate
		m_pBeam->pev->renderamt = 0; // lit by PowerupThink
		m_pBeam->m_iRC2Flags = 8;	 // VERIFY: what bit 8 of +0x68 means
	}
}

// orig 0xe8cf0
void CTripmineGrenade::KillBeam()
{
	if (m_pBeam)
	{
		m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
	}
	m_pBeam = NULL;
}

// Hurts what the beam's end touches, scaled by the time since the last full update, then
// gives back the shield's deflection point (zero when none). Used by the plain and the punch
// mine (flShieldDamagePerSec is the kind's rc2.ini key); ours, the original repeats the block
// inline in each kind's BeamUpdate (the bouncy and target mines' differ and stay inline).
static void TripmineBeamHurt(CTripmineGrenade* pMine, TraceResult& tr, const Vector& vecDir, float flShieldDamagePerSec, float flDelta, Vector* pvecBeamEndOut)
{
	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
	if (!pHit)
		return;
	if (pHit->GetEffectedEnt()->GlobalBeamDamage() <= 0)
		return;

	entvars_t* pevAttacker = pMine->pev;
	if (pMine->m_hOwner != NULL)
		pevAttacker = pMine->m_hOwner->pev;

	Vector vecOut = g_vecZero;
	if (ShieldDeflect(pHit, flShieldDamagePerSec * flDelta, 1.0, tr.vecEndPos, vecDir, &vecOut, NULL, 0) != 0)
	{
		ClearMultiDamage();
		CBaseEntity* pEffected = pHit->GetEffectedEnt();
		pEffected->TraceAttack(pevAttacker, pHit->GetEffectedEnt()->GlobalBeamDamage(), vecDir, &tr, DMG_ENERGYBEAM);
		ApplyMultiDamage(pMine->pev, pevAttacker);
		pMine->pev->impulse = 1;
	}

	// a shield's deflection point, whether or not damage got through
	*pvecBeamEndOut = vecOut;
}

// orig 0xe8d30
void CTripmineGrenade::BeamUpdate()
{
	bool bTargetShielded = false;
	bool bFound = false;
	Vector vecHit;
	CBaseEntity* pTarget = NULL;

	pev->nextthink = gpGlobals->time;
	float flDelta = gpGlobals->time - m_flLastBeamTime;

	if (flDelta < pev->armorvalue && 0 == pev->impulse)
	{
		// between full updates, only look again when a player's box crosses the beam
		float flBest = 9999.0;
		if (gpGlobals->maxClients <= 1)
			return;
		for (int i = 1; i < gpGlobals->maxClients; i++) // original bug: the last player slot is never checked
		{
			edict_t* pent = INDEXENT(i);
			if (!UTIL_IsValidEntity(pent))
				continue;
			CBaseEntity* pEnt = CBaseEntity::Instance(pent);
			if (!pEnt)
				continue;
			CBaseEntity* pEff = pEnt->GetEffectedEnt();
			if (pEff->edict() == edict() || pEff->GlobalBeamDamage() <= 0 || pEff->pev->takedamage == 0)
				continue;

			float flDist;
			if (UTIL_RayIntersectsBox(pev->origin, m_vecDir, pEff->pev->absmin, pEff->pev->absmax, &flDist))
			{
				bFound = true;
				if (flDist < flBest)
				{
					vecHit = pev->origin + m_vecDir * flDist;
					pTarget = pEff;
					bTargetShielded = pEff->IsActivePlayer() && static_cast<CBasePlayer*>(pEff)->m_fShieldOn;
					flBest = flDist;
				}
			}
		}
		// Original bug (crash), guarded: a crossing 9999 or more away sets bFound but no
		// target, and the original reads pTarget through NULL below; ours counts it as none
		if (!bFound || !pTarget)
			return;
		pev->nextthink = gpGlobals->time + 0.1;
	}

	pev->impulse = 0;
	m_flLastBeamTime = gpGlobals->time;
	Vector vecOldEnd = m_vecEnd;

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	if (!bFound)
	{
		UTIL_TraceLine(pev->origin, pev->origin + m_vecDir * 8192, dont_ignore_monsters, ENT(pev), &tr);
	}
	else
	{
		UTIL_TraceLine(pev->origin, vecHit, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0)
		{
			// a shield stops the beam at the box, not at the shield's surface
			if (tr.pHit == pTarget->edict() && bTargetShielded)
				tr.vecEndPos = vecHit;
		}
		else
		{
			// the box was crossed but the trace (simple box) missed: count it as a hit
			tr.vecEndPos = vecHit;
			tr.pHit = pTarget->edict();
			tr.flFraction = 0.9;
		}
	}

	if (m_pBeam)
	{
		m_pBeam->pev->angles = tr.vecEndPos; // SetEndPos
		m_pBeam->pev->origin = pev->origin;	 // SetStartPos
	}
	pev->armorvalue = 0.2;

	if (tr.flFraction >= 1.0 || !UTIL_IsValidEntity(tr.pHit))
	{
		m_vecEnd = tr.vecEndPos;
	}
	else
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && pHit->GetEffectedEnt()->GlobalBeamDamage() > 0)
		{
			Vector vecBeamEnd = g_vecZero;
			TripmineBeamHurt(this, tr, m_vecDir, RC2Pref(RC2P_Tripmine_PrimaryShieldDamagePerSec).f, flDelta, &vecBeamEnd);
			if (vecBeamEnd != g_vecZero && m_pBeam)
				m_pBeam->pev->angles = vecBeamEnd;

			if (pHit->IsBSPModel())
			{
				m_vecEnd = tr.vecEndPos;
				if (0 != ENTINDEX(pHit->edict()))
					pev->armorvalue = 0.1; // a moving brush: update more often
			}
		}
	}

	if (m_vecEnd != vecOldEnd)
		UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
}


// bouncy_tripmine: up to BouncyMaxBounces reflections, a beam per segment and a glow at
// each bounce.
class CBouncyTripmine : public CTripmineGrenade // name ours
{
public:
	void Spawn() override;
	void Precache() override;
	void MakeBeam() override;
	void KillBeam() override;
	void BeamUpdate() override;

	int m_iMaxBounces;	  // orig +0x310
	CBeam** m_ppBeams;	  // orig +0x314: m_iMaxBounces + 1 segments (malloc)
	CSprite** m_ppGlows;  // orig +0x318: one per bounce point
	Vector* m_pvecPoints; // orig +0x31c: each segment's end
};

LINK_ENTITY_TO_CLASS(bouncy_tripmine, CBouncyTripmine); // orig 0xe93d0

// orig 0xe9470
void CBouncyTripmine::Spawn()
{
	m_iMaxBounces = RC2PrefWord(RC2P_Tripmine_BouncyMaxBounces);
	m_pBeam = NULL;
	m_ppBeams = (CBeam**)calloc(m_iMaxBounces + 1, sizeof(CBeam*));
	m_pvecPoints = (Vector*)calloc(m_iMaxBounces + 1, sizeof(Vector));
	if (0 == m_iMaxBounces)
		m_ppGlows = NULL;
	else
		m_ppGlows = (CSprite**)calloc(m_iMaxBounces, sizeof(CSprite*));

	Precache();
	CTripmineGrenade::Precache();
	CTripmineGrenade::Spawn();
	m_iAmmoCost = RC2PrefWord(RC2P_Tripmine_BouncyAmmoCost);
}

// orig 0xe9570
void CBouncyTripmine::Precache()
{
	PRECACHE_MODEL("sprites/glow03.spr");
}

// orig 0xe9580
void CBouncyTripmine::MakeBeam()
{
	if (m_ppBeams)
	{
		for (int i = 0; i <= m_iMaxBounces; i++)
		{
			m_ppBeams[i] = CBeam::BeamCreate("sprites/zbeam2.spr", 26);
			CBeam* pBeam = m_ppBeams[i];
			if (pBeam)
			{
				pBeam->PointsInit(pev->origin, pev->origin);
				pBeam->pev->rendercolor = Vector(0, 255, 0);
				pBeam->pev->animtime = 32;
				pBeam->pev->renderamt = 0;
				pBeam->m_iRC2Flags = 8;
			}
		}
	}

	if (m_ppGlows)
	{
		for (int i = 0; i < m_iMaxBounces; i++)
		{
			m_ppGlows[i] = CSprite::SpriteCreate("sprites/glow03.spr", pev->origin, false);
			CSprite* pGlow = m_ppGlows[i];
			if (pGlow)
			{
				pGlow->pev->rendermode = kRenderGlow;
				pGlow->pev->rendercolor = Vector(0, 255, 0);
				pGlow->pev->renderamt = 0;
				pGlow->pev->renderfx = kRenderFxNoDissipation;
				pGlow->pev->scale = 0.333;
				pGlow->pev->angles.z = RANDOM_FLOAT(0, 360);
				pGlow->m_iRC2Flags = 8;
			}
		}
	}
}

// orig 0xe9730
void CBouncyTripmine::KillBeam()
{
	if (m_ppBeams)
	{
		for (int i = 0; i <= m_iMaxBounces; i++)
		{
			if (m_ppBeams[i])
			{
				m_ppBeams[i]->SetThink(&CBaseEntity::SUB_Remove);
				m_ppBeams[i]->pev->nextthink = gpGlobals->time + 0.01;
			}
			m_ppBeams[i] = NULL;
		}
		free(m_ppBeams);
		m_ppBeams = NULL;
	}

	if (m_ppGlows)
	{
		for (int i = 0; i < m_iMaxBounces; i++)
		{
			if (m_ppGlows[i])
			{
				m_ppGlows[i]->SUB_Remove();
				m_ppGlows[i] = NULL;
			}
		}
		free(m_ppGlows);
		m_ppGlows = NULL;
	}

	if (m_pvecPoints)
	{
		free(m_pvecPoints);
		m_pvecPoints = NULL;
	}
}

// VERIFY: read from the decompile (4 KB, its stack vectors badly merged); the reflection loop
// and the per-segment crossing test want a pass over the disassembly
// orig 0xe9830
void CBouncyTripmine::BeamUpdate()
{
	if (!m_pvecPoints)
	{
		UTIL_Remove(this);
		return;
	}

	bool bFound = false;
	bool bTargetShielded = false;
	int iHitSegment = -1;
	Vector vecHit;
	CBaseEntity* pTarget = NULL;
	int j;

	pev->nextthink = gpGlobals->time;
	float flDelta = gpGlobals->time - m_flLastBeamTime;

	if (flDelta < pev->armorvalue && 0 == pev->impulse)
	{
		// between full updates: does a player's box cross any segment?
		float flBest = 9999.0;
		if (gpGlobals->maxClients <= 1)
			return;
		for (int i = 1; i < gpGlobals->maxClients; i++) // original bug: misses the last slot, as the plain mine
		{
			edict_t* pent = INDEXENT(i);
			if (!UTIL_IsValidEntity(pent))
				continue;
			CBaseEntity* pEnt = CBaseEntity::Instance(pent);
			if (!pEnt)
				continue;
			CBaseEntity* pEff = pEnt->GetEffectedEnt();
			if (pEff->edict() == edict() || pEff->GlobalBeamDamage() <= 0 || pEff->pev->takedamage == 0)
				continue;

			float flTravelled = 0;
			Vector vecStart = pev->origin;
			Vector vecSegEnd = m_pvecPoints[0];
			Vector vecDir = m_vecDir;
			float flSegLength = (vecSegEnd - vecStart).Length();
			float flDist;
			for (j = 0; j <= m_iMaxBounces; j++)
			{
				if (m_pvecPoints[j] == g_vecZero)
					continue;
				if (UTIL_RayIntersectsBox(vecStart, vecDir, pEff->pev->absmin, pEff->pev->absmax, &flDist) && flDist <= flSegLength)
					break;
				if (j < m_iMaxBounces)
				{
					flTravelled += (vecSegEnd - vecStart).Length();
					vecStart = m_pvecPoints[j];
					vecSegEnd = m_pvecPoints[j + 1];
					vecDir = vecSegEnd - vecStart;
					flSegLength = vecDir.Length();
					if (flSegLength == 0)
						vecDir = Vector(0, 0, 1);
					else
						vecDir = vecDir / flSegLength;
				}
			}

			if (j <= m_iMaxBounces)
			{
				bFound = true;
				if (flDist + flTravelled < flBest)
				{
					vecHit = vecStart + vecDir * flDist;
					pTarget = pEff;
					iHitSegment = j;
					bTargetShielded = pEff->IsActivePlayer() && static_cast<CBasePlayer*>(pEff)->m_fShieldOn;
					flBest = flDist + flTravelled;
				}
			}
		}
		if (!bFound)
			return;
		pev->nextthink = gpGlobals->time + 0.1;
	}

	pev->impulse = 0;
	m_flLastBeamTime = gpGlobals->time;
	Vector vecOldEnd = m_vecEnd;

	// trace the beam bounce by bounce
	TraceResult tr;
	Vector vecStart = pev->origin;
	Vector vecDir = m_vecDir;
	bool bStopped = false; // ended on the sky or on something the beam hurts
	for (j = 0; j <= m_iMaxBounces; j++)
	{
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(vecStart, vecStart + vecDir * 8192, dont_ignore_monsters, ENT(pev), &tr);

		if (bFound)
		{
			if (iHitSegment == j)
			{
				m_pvecPoints[j] = vecHit;
				tr.flFraction = 1.0;
				break;
			}
			if ((m_pvecPoints[j] - tr.vecEndPos).Length() > 2)
			{
				// the path has changed since the crossing was found
				iHitSegment = -1;
				bFound = false;
			}
		}

		m_pvecPoints[j] = tr.vecEndPos;
		if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
		{
			bStopped = true;
			break;
		}
		if (UTIL_IsValidEntity(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit && pHit->GetEffectedEnt()->GlobalBeamDamage() > 0 && pHit->pev->takedamage != 0)
			{
				bStopped = true;
				break;
			}
		}

		// bounce: the next segment starts just off the surface
		m_pvecPoints[j] = tr.vecEndPos + tr.vecPlaneNormal;
		if (j < m_iMaxBounces)
		{
			Vector vecNewDir = UTIL_ReflectVector(vecDir, tr.vecPlaneNormal).Normalize();
			if (-vecNewDir == vecDir)
				break; // straight back the way it came
			vecDir = vecNewDir;
			vecStart = tr.vecEndPos;
		}
	}

	if (!bStopped && bFound && iHitSegment != -1)
	{
		if (tr.flFraction < 1.0)
		{
			if (tr.pHit == pTarget->edict() && bTargetShielded)
				tr.vecEndPos = vecHit;
		}
		else
		{
			tr.vecEndPos = vecHit;
			tr.pHit = pTarget->edict();
			tr.flFraction = 0.9;
		}
	}

	int iLast = j; // the segment the beam ends on
	if (iLast > m_iMaxBounces)
		iLast = m_iMaxBounces;

	if (m_ppBeams && m_ppBeams[0])
	{
		m_ppBeams[0]->pev->origin = pev->origin;
		m_ppBeams[0]->pev->angles = m_pvecPoints[0];
		m_ppBeams[0]->pev->renderamt = 200;
	}

	for (int k = 0; k < iLast; k++)
	{
		if (m_ppBeams && m_ppBeams[k + 1])
		{
			m_ppBeams[k + 1]->pev->origin = m_pvecPoints[k];
			m_ppBeams[k + 1]->pev->angles = m_pvecPoints[k + 1];
			m_ppBeams[k + 1]->pev->renderamt = 200;
		}
		if (m_ppGlows && m_ppGlows[k])
		{
			UTIL_SetOrigin(m_ppGlows[k]->pev, m_pvecPoints[k]);
			m_ppGlows[k]->pev->rendermode = kRenderGlow;
			m_ppGlows[k]->pev->rendercolor = Vector(0, 255, 0);
			m_ppGlows[k]->pev->renderamt = 250;
			m_ppGlows[k]->pev->renderfx = kRenderFxNoDissipation;
		}
	}

	for (int k = iLast; k < m_iMaxBounces; k++)
	{
		if (m_ppBeams && m_ppBeams[k + 1])
			m_ppBeams[k + 1]->pev->renderamt = 0;
		m_pvecPoints[k + 1] = g_vecZero;
		if (m_ppGlows && m_ppGlows[k])
		{
			m_ppGlows[k]->pev->rendermode = kRenderGlow;
			m_ppGlows[k]->pev->rendercolor = Vector(0, 255, 0);
			m_ppGlows[k]->pev->renderamt = 0;
			m_ppGlows[k]->pev->renderfx = kRenderFxNoDissipation;
		}
	}

	pev->armorvalue = 0.1;

	if (tr.flFraction >= 1.0 || !UTIL_IsValidEntity(tr.pHit))
	{
		m_vecEnd = tr.vecEndPos;
	}
	else
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && pHit->GetEffectedEnt()->GlobalBeamDamage() > 0 && pHit->pev->takedamage != 0)
		{
			entvars_t* pevAttacker = pev;
			if (m_hOwner != NULL)
				pevAttacker = m_hOwner->pev;

			Vector vecOut = g_vecZero;
			if (ShieldDeflect(pHit, RC2Pref(RC2P_Tripmine_BouncyShieldDamagePerSec).f * flDelta, 1.0, tr.vecEndPos, vecDir, &vecOut, NULL, 0) == 0)
			{
				// held off by a shield: the beam stops at the shield
				if (vecOut != g_vecZero)
				{
					m_pvecPoints[iLast] = vecOut;
					m_ppBeams[iLast]->pev->angles = vecOut; // no null checks, as the original; Spawn fills every slot and BeamUpdate stops with KillBeam
				}
			}
			else
			{
				ClearMultiDamage();
				CBaseEntity* pEffected = pHit->GetEffectedEnt();
				pEffected->TraceAttack(pevAttacker, pHit->GetEffectedEnt()->GlobalBeamDamage(), vecDir, &tr, DMG_ENERGYBEAM);
				ApplyMultiDamage(pev, pevAttacker);
				pev->impulse = 1;
			}

			if (pHit->IsBSPModel())
			{
				m_vecEnd = tr.vecEndPos;
				if (0 != ENTINDEX(pHit->edict()))
					pev->armorvalue = 0.1;
			}
		}
	}

	if (m_vecEnd != vecOldEnd)
		UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
}


// punch_tripmine: the beam bores through up to PunchMaxWallPunches walls, spending at most
// PunchMaxTotalPunchDistance units of solid in all; a glow marks each wall's way in and out.
class CPunchTripmine : public CTripmineGrenade // name ours
{
public:
	void Spawn() override;
	void Precache() override;
	void MakeBeam() override;
	void KillBeam() override;
	void BeamUpdate() override;

	int m_iMaxPunches;		  // orig +0x310
	float m_flMaxPunchDist;	  // orig +0x314
	CSprite** m_ppGlows;	  // orig +0x318: two per wall (malloc)
	int m_iPunchUnused31c;	  // orig +0x31c. VERIFY: never read or written in p8's code (the class is 0x320 bytes)
};

LINK_ENTITY_TO_CLASS(punch_tripmine, CPunchTripmine); // orig 0xea860

// orig 0xea900
void CPunchTripmine::Spawn()
{
	m_iMaxPunches = RC2PrefWord(RC2P_Tripmine_PunchMaxWallPunches);
	m_flMaxPunchDist = RC2Pref(RC2P_Tripmine_PunchMaxTotalPunchDistance).f;
	m_pBeam = NULL;
	if (0 == m_iMaxPunches)
		m_ppGlows = NULL;
	else
		m_ppGlows = (CSprite**)calloc(m_iMaxPunches * 2, sizeof(CSprite*));

	Precache();
	CTripmineGrenade::Precache();
	CTripmineGrenade::Spawn();
	m_iAmmoCost = RC2PrefWord(RC2P_Tripmine_PunchAmmoCost);
}

// orig 0xea990
void CPunchTripmine::Precache()
{
	PRECACHE_MODEL("sprites/glow03.spr");
}

// orig 0xea9a0
void CPunchTripmine::MakeBeam()
{
	m_pBeam = CBeam::BeamCreate("sprites/zbeam2.spr", 26);
	if (m_pBeam)
	{
		m_pBeam->PointsInit(pev->origin, pev->origin);
		m_pBeam->pev->rendercolor = Vector(0, 0, 255);
		m_pBeam->pev->animtime = 32;
		m_pBeam->pev->renderamt = 0;
		m_pBeam->m_iRC2Flags = 8;
	}

	if (m_ppGlows)
	{
		for (int i = 0; i < m_iMaxPunches * 2; i++)
		{
			m_ppGlows[i] = CSprite::SpriteCreate("sprites/glow03.spr", pev->origin, false);
			CSprite* pGlow = m_ppGlows[i];
			if (pGlow)
			{
				pGlow->pev->rendermode = kRenderGlow;
				pGlow->pev->rendercolor = Vector(0, 0, 255);
				pGlow->pev->renderamt = 0;
				pGlow->pev->renderfx = kRenderFxNoDissipation;
				pGlow->pev->scale = 0.333;
				pGlow->pev->angles.z = RANDOM_FLOAT(0, 360);
				pGlow->m_iRC2Flags = 8;
			}
		}
	}
}

// orig 0xeab10
void CPunchTripmine::KillBeam()
{
	if (m_pBeam)
	{
		m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
	}
	m_pBeam = NULL;

	if (m_ppGlows)
	{
		for (int i = 0; i < m_iMaxPunches * 2; i++)
		{
			if (m_ppGlows[i])
			{
				m_ppGlows[i]->SUB_Remove();
				m_ppGlows[i] = NULL;
			}
		}
		free(m_ppGlows);
		m_ppGlows = NULL;
	}
}

// VERIFY: read from the decompile; the boring loop's trace back and its refund were checked in
// the disassembly, the rest of the loop was not
// orig 0xeabb0
void CPunchTripmine::BeamUpdate()
{
	bool bFound = false;
	bool bTargetShielded = false;
	Vector vecHit;
	CBaseEntity* pTarget = NULL;

	pev->nextthink = gpGlobals->time;
	float flDelta = gpGlobals->time - m_flLastBeamTime;
	float flBest = 9999.0;

	if (flDelta < pev->armorvalue && 0 == pev->impulse)
	{
		// between full updates: does a player's box cross the straight line of the beam?
		if (gpGlobals->maxClients <= 1)
			return;
		for (int i = 1; i < gpGlobals->maxClients; i++) // original bug: misses the last slot
		{
			edict_t* pent = INDEXENT(i);
			if (!UTIL_IsValidEntity(pent))
				continue;
			CBaseEntity* pEnt = CBaseEntity::Instance(pent);
			if (!pEnt)
				continue;
			CBaseEntity* pEff = pEnt->GetEffectedEnt();
			if (pEff->edict() == edict() || pEff->GlobalBeamDamage() <= 0 || pEff->pev->takedamage == 0)
				continue;

			float flDist;
			if (UTIL_RayIntersectsBox(pev->origin, m_vecDir, pEff->pev->absmin, pEff->pev->absmax, &flDist))
			{
				bFound = true;
				if (flDist < flBest)
				{
					vecHit = pev->origin + m_vecDir * flDist;
					pTarget = pEff;
					bTargetShielded = pEff->IsActivePlayer() && static_cast<CBasePlayer*>(pEff)->m_fShieldOn;
					flBest = flDist;
				}
			}
		}
		// Original bug (crash), guarded: as the plain mine's, a crossing 9999 or more away
		// leaves pTarget NULL for the found branch below
		if (!bFound || !pTarget)
			return;
		pev->nextthink = gpGlobals->time + 0.1;
	}

	pev->impulse = 0;
	m_flLastBeamTime = gpGlobals->time;
	Vector vecOldEnd = m_vecEnd;
	edict_t* pentIgnore = edict();

	if (m_ppGlows)
	{
		for (int i = 0; i < m_iMaxPunches * 2; i++)
		{
			if (m_ppGlows[i])
			{
				m_ppGlows[i]->pev->rendermode = kRenderGlow;
				m_ppGlows[i]->pev->rendercolor = Vector(0, 0, 255);
				m_ppGlows[i]->pev->renderamt = 0;
				m_ppGlows[i]->pev->renderfx = kRenderFxNoDissipation;
			}
		}
	}

	Vector vecStart = pev->origin;
	Vector vecEnd = vecStart + m_vecDir * 8192;
	if (bFound)
		vecEnd = vecHit;
	Vector vecDir = (vecEnd - vecStart).Normalize(); // a zero vector gives (0, 0, 1), as the original

	float flPunchLeft = m_flMaxPunchDist;
	int iGlow = 0;
	TraceResult tr;
	for (int i = 0; i <= m_iMaxPunches; i++)
	{
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, pentIgnore, &tr);
		if (tr.flFraction >= 1.0)
			break;

		CBaseEntity* pHit = NULL;
		if (UTIL_IsValidEntity(tr.pHit))
			pHit = CBaseEntity::Instance(tr.pHit);

		if (pHit)
		{
			// something the beam hurts stops it
			if (pHit->pev->takedamage != 0 && pHit->GetEffectedEnt()->GlobalBeamDamage() > 0)
				break;

			if (!pHit->IsBSPModel())
			{
				// pass through anything else that isn't a brush
				vecStart = tr.vecEndPos + vecDir;
				pentIgnore = pHit->edict();
				continue;
			}
		}

		// bore through the wall in steps of 8 until out of the solid or out of distance
		Vector vecWayIn = tr.vecEndPos - vecDir;
		Vector vecBack = vecStart;
		vecStart = tr.vecEndPos + vecDir;
		while (flPunchLeft > 0)
		{
			if (flPunchLeft < 8)
			{
				vecStart = vecStart + vecDir * flPunchLeft;
				flPunchLeft = 0;
			}
			else
			{
				vecStart = vecStart + vecDir * 8;
				flPunchLeft -= 8;
			}

			if (UTIL_PointContents(vecStart) != CONTENTS_SOLID)
			{
				// out: trace back to the wall's far face, and give back what overshot it
				TraceResult trBack;
				UTIL_TraceLine(vecStart, vecBack, ignore_monsters, ENT(pev), &trBack);
				vecStart = trBack.vecEndPos + vecDir;
				flPunchLeft += trBack.flFraction * 8;

				if (m_ppGlows && iGlow < m_iMaxPunches * 2 - 1)
				{
					if (m_ppGlows[iGlow])
					{
						UTIL_SetOrigin(m_ppGlows[iGlow]->pev, vecWayIn);
						m_ppGlows[iGlow]->pev->rendermode = kRenderGlow;
						m_ppGlows[iGlow]->pev->rendercolor = Vector(0, 0, 255);
						m_ppGlows[iGlow]->pev->renderamt = 254;
						m_ppGlows[iGlow]->pev->renderfx = kRenderFxNoDissipation;
						iGlow++;
					}
					if (m_ppGlows[iGlow])
					{
						UTIL_SetOrigin(m_ppGlows[iGlow]->pev, vecStart);
						m_ppGlows[iGlow]->pev->rendermode = kRenderGlow;
						m_ppGlows[iGlow]->pev->rendercolor = Vector(0, 0, 255);
						m_ppGlows[iGlow]->pev->renderamt = 254;
						m_ppGlows[iGlow]->pev->renderfx = kRenderFxNoDissipation;
						iGlow++;
					}
				}
				break;
			}
		}

		if (flPunchLeft <= 0)
			break;
	}

	if (bFound)
	{
		if (tr.flFraction < 1.0)
		{
			if (tr.pHit == pTarget->edict() && bTargetShielded)
				tr.vecEndPos = vecHit;
		}
		else
		{
			tr.vecEndPos = vecHit;
			tr.pHit = pTarget->edict();
			tr.flFraction = 0.9;
		}
	}

	if (m_pBeam)
	{
		m_pBeam->pev->renderamt = 250;
		m_pBeam->pev->angles = tr.vecEndPos;
		m_pBeam->pev->origin = pev->origin;
	}
	pev->armorvalue = 0.2;

	if (tr.flFraction >= 1.0 || !UTIL_IsValidEntity(tr.pHit))
	{
		m_vecEnd = tr.vecEndPos;
	}
	else
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && pHit->GetEffectedEnt()->GlobalBeamDamage() > 0)
		{
			Vector vecBeamEnd = g_vecZero;
			TripmineBeamHurt(this, tr, m_vecDir, RC2Pref(RC2P_Tripmine_PunchShieldDamagePerSec).f, flDelta, &vecBeamEnd);
			if (vecBeamEnd != g_vecZero && m_pBeam)
				m_pBeam->pev->angles = vecBeamEnd;

			if (pHit->IsBSPModel())
			{
				m_vecEnd = tr.vecEndPos;
				if (0 != ENTINDEX(pHit->edict()))
					pev->armorvalue = 0.1;
			}
		}
	}

	if (m_vecEnd != vecOldEnd)
		UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
}


// target_tripmine: a short beam (TargetUpBeamDistance) with a glow at its tip; every
// TargetFireInterval it zaps the best target within TargetMaxShootDistance of the tip in front
// of it, and the zap goes on through up to ten things the beam hurts in a line. The client
// draws the zaps from gmsgCLTE.
class CTargetTripmine : public CTripmineGrenade // name ours
{
public:
	void Spawn() override;
	void Precache() override;
	void MakeBeam() override;
	void KillBeam() override;
	void BeamUpdate() override;

	float m_flNextFire; // orig +0x310, name ours
	CSprite* m_pGlow;	// orig +0x314
};

LINK_ENTITY_TO_CLASS(target_tripmine, CTargetTripmine); // orig 0xeb9e0

// orig 0xeba80
void CTargetTripmine::Spawn()
{
	m_pBeam = NULL;
	m_pGlow = NULL;
	Precache();
	CTripmineGrenade::Precache();
	CTripmineGrenade::Spawn();
	m_iAmmoCost = RC2PrefWord(RC2P_Tripmine_TargetAmmoCost);
}

// orig 0xebac0
void CTargetTripmine::Precache()
{
	PRECACHE_MODEL("sprites/glow03.spr");
	PRECACHE_SOUND("ambience/labdrone2.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
}

// orig 0xebaf0
void CTargetTripmine::MakeBeam()
{
	m_pBeam = CBeam::BeamCreate("sprites/zbeam2.spr", 26);
	if (m_pBeam)
	{
		m_pBeam->PointsInit(pev->origin, pev->origin);
		m_pBeam->pev->rendercolor = Vector(255, 0, 0);
		m_pBeam->pev->animtime = 32;
		m_pBeam->pev->renderamt = 0;
		m_pBeam->m_iRC2Flags = 8;
	}

	m_pGlow = CSprite::SpriteCreate("sprites/glow03.spr", pev->origin, false);
	if (m_pGlow)
	{
		m_pGlow->pev->rendermode = kRenderGlow;
		m_pGlow->pev->rendercolor = Vector(255, 0, 0);
		m_pGlow->pev->renderamt = 0;
		m_pGlow->pev->renderfx = kRenderFxNoDissipation;
		m_pGlow->pev->scale = 1.0;
		m_pGlow->m_iRC2Flags = 8;
	}
}

// orig 0xebc00
void CTargetTripmine::KillBeam()
{
	if (m_pBeam)
	{
		m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
	}
	m_pBeam = NULL;

	if (m_pGlow)
	{
		m_pGlow->SUB_Remove();
		m_pGlow = NULL;
	}

	// tell the clients to drop this mine's effects. VERIFY: the meaning of 0xfe and 0x39 is the client's
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(entindex());
	WRITE_BYTE(0x39);
	MESSAGE_END();
}

// orig 0xebca0
void CTargetTripmine::BeamUpdate()
{
	float flDelta = gpGlobals->time - m_flLastBeamTime;
	m_flLastBeamTime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;

	// the up beam: hurts what it touches like the others
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + m_vecDir * RC2Pref(RC2P_Tripmine_TargetUpBeamDistance).f, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0)
	{
		m_vecEnd = pev->origin + m_vecDir * RC2Pref(RC2P_Tripmine_TargetUpBeamDistance).f;
	}
	else
	{
		m_vecEnd = tr.vecEndPos - m_vecDir * 8;
		if (UTIL_IsValidEntity(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit && pHit->GetEffectedEnt()->GlobalBeamDamage() > 0)
			{
				Vector vecOut; // filled by the shield filter
				if (ShieldDeflect(pHit, RC2Pref(RC2P_Tripmine_TargetUpBeamShieldDamagePerSec).f * flDelta, 1.0, tr.vecEndPos, m_vecDir, &vecOut, NULL, 0) == 0)
				{
					m_vecEnd = vecOut;
				}
				else
				{
					entvars_t* pevAttacker = pev;
					if (m_hOwner != NULL)
						pevAttacker = m_hOwner->pev;
					ClearMultiDamage();
					CBaseEntity* pEffected = pHit->GetEffectedEnt();
					pEffected->TraceAttack(pevAttacker, pHit->GetEffectedEnt()->GlobalBeamDamage(), m_vecDir, &tr, DMG_ENERGYBEAM);
					ApplyMultiDamage(pev, pevAttacker);
				}
			}
		}
	}

	if (m_pBeam)
	{
		m_pBeam->pev->angles = m_vecEnd;
		m_pBeam->pev->origin = pev->origin;
	}
	if (m_pGlow)
		UTIL_SetOrigin(m_pGlow->pev, m_vecEnd);

	if (0 != pev->impulse)
	{
		// just powered up: light up and announce
		m_flNextFire = gpGlobals->time + RC2Pref(RC2P_Tripmine_TargetFireInterval).f;
		if (m_pBeam)
			m_pBeam->pev->renderamt = 100;
		if (m_pGlow)
			m_pGlow->pev->renderamt = 100;

		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0x39);
		WRITE_SHORT(ENTINDEX(edict()));
		if (m_pBeam)
			WRITE_SHORT(ENTINDEX(m_pBeam->edict()));
		else
			WRITE_SHORT(0);
		if (m_pGlow)
			WRITE_SHORT(ENTINDEX(m_pGlow->edict()));
		else
			WRITE_SHORT(0);
		MESSAGE_END();

		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 800, 1.0);
		pev->impulse = 0;
		return;
	}

	if (m_flNextFire >= gpGlobals->time)
		return;

	// pick a target: close and in front of the beam scores higher, and it must be visible
	// (for a player: his middle, his eyes or his feet)
	const float flBlindspot = RC2Pref(RC2P_Tripmine_TargetDotBlindspot).f;
	const float flMaxDist = RC2Pref(RC2P_Tripmine_TargetMaxShootDistance).f;
	float flBestScore = -9999.0;
	CBaseEntity* pBest = NULL;
	TraceResult trBest;
	Vector vecAim, vecBestAim, vecBestDir;

	for (CBaseEntity* pEnt = UTIL_FindEntityInSphere(NULL, m_vecEnd, flMaxDist); pEnt; pEnt = UTIL_FindEntityInSphere(pEnt, m_vecEnd, flMaxDist))
	{
		if (pEnt->GetEffectedEnt()->pev->takedamage == 0)
			continue;
		if (pEnt->GetEffectedEnt()->GlobalBeamDamage() <= 0)
			continue;
		if ((pEnt->GetRCCaps2(0, this) & 0x20) != 0)
			continue;

		Vector vecDelta = pEnt->Center() - m_vecEnd;
		float flDist = vecDelta.Length();
		if (flDist == 0)
			flDist = 1.0;
		Vector vecTo = vecDelta / flDist;
		float flDot = DotProduct(vecTo, m_vecDir);
		if (flDot < flBlindspot)
			continue;

		float flDistScore = 1.0 - flDist / flMaxDist;
		if (flDistScore < 0)
			flDistScore = 0;
		else if (flDistScore > 1.0)
			flDistScore = 1.0;
		// original oddity: flDot >= flBlindspot makes this ratio at least 1, so it always counts 1
		float flDotScore = (flDot + 2.0) / (flBlindspot + 2.0);
		if (flDotScore < 0)
			flDotScore = 0;
		else if (flDotScore > 1.0)
			flDotScore = 1.0;
		float flScore = flDistScore * 3.0 + flDotScore + flDotScore;
		if (flScore < flBestScore)
			continue;

		vecAim = pEnt->Center();
		UTIL_TraceLine(m_vecEnd, vecAim, ignore_monsters, pEnt->edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
		{
			if (!pEnt->GetEffectedEnt()->IsActivePlayer())
				continue;

			vecAim = pEnt->pev->origin + pEnt->pev->view_ofs;
			vecAim.z -= 8;
			UTIL_TraceLine(m_vecEnd, vecAim, ignore_monsters, pEnt->edict(), &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			{
				vecAim = Vector(pEnt->pev->origin.x, pEnt->pev->origin.y, pEnt->pev->origin.z + pEnt->pev->mins.z + 8);
				UTIL_TraceLine(m_vecEnd, vecAim, ignore_monsters, pEnt->edict(), &tr);
				if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
					continue;
			}
		}

		flBestScore = flScore;
		trBest = tr;
		vecBestAim = vecAim;
		vecBestDir = vecTo;
		pBest = pEnt;
	}

	if (!pBest)
	{
		m_flNextFire = gpGlobals->time + 0.2;
		return;
	}

	// zap it, and on through whatever the beam hurts behind it, up to ten
	Vector vecPoint = vecBestAim;
	CBaseEntity* pZap = pBest;
	for (int i = 0;;)
	{
		Vector vecOut;
		if (ShieldDeflect(pZap, RC2Pref(RC2P_Tripmine_TargetMainShieldDamage).f, 1.0, vecPoint, vecBestDir, &vecOut, NULL, 8) == 0)
			break;

		entvars_t* pevAttacker = pev;
		if (m_hOwner != NULL)
			pevAttacker = m_hOwner->pev;

		EHANDLE hZapped;
		hZapped = pZap;

		// tell spectator proxies. VERIFY: SDK 2.3's director messages are MSG_SPEC, SVC_DIRECTOR (51)
		// with a length byte first; this one is message 50 and starts with DRC_CMD_EVENT (3)
		MESSAGE_BEGIN(MSG_SPEC, 50);
		WRITE_BYTE(3);
		WRITE_SHORT(ENTINDEX(pZap->edict()));
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_LONG(i > 0 ? 7 : 6);
		MESSAGE_END();

		ClearMultiDamage();
		CBaseEntity* pEffected = pZap->GetEffectedEnt();
		// original oddity: every zap passes the first target's trace
		pEffected->TraceAttack(pevAttacker, pZap->GetEffectedEnt()->GlobalBeamDamage(), vecBestDir, &trBest, DMG_ENERGYBEAM);
		ApplyMultiDamage(pev, pevAttacker);

		if (hZapped != NULL)
			UTIL_TraceLine(vecPoint, vecPoint + vecBestDir * 8192, dont_ignore_monsters, hZapped->edict(), &tr);
		else
			UTIL_TraceLine(vecPoint, vecPoint + vecBestDir * 8192, dont_ignore_monsters, NULL, &tr);
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
		vecPoint = tr.vecEndPos + vecBestDir * 4;

		if (tr.flFraction >= 1.0 || !UTIL_IsValidEntity(tr.pHit))
			break;
		pZap = CBaseEntity::Instance(tr.pHit);
		if (!pZap || pZap->GetEffectedEnt()->pev->takedamage == 0)
			break;
		if (pZap->GetEffectedEnt()->GlobalBeamDamage() <= 0)
			break;
		if (++i >= 10)
			break;
	}

	// the zap's line, from the tip to where it ended
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x3a);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_COORD(m_vecEnd.x);
	WRITE_COORD(m_vecEnd.y);
	WRITE_COORD(m_vecEnd.z);
	WRITE_COORD(vecPoint.x);
	WRITE_COORD(vecPoint.y);
	WRITE_COORD(vecPoint.z);
	MESSAGE_END();

	m_flNextFire = gpGlobals->time + RC2Pref(RC2P_Tripmine_TargetFireInterval).f;
}

#endif

// orig client 0x45b10 (and its GetClassPtr<CTripmine>, orig client 0x46c70)
LINK_ENTITY_TO_CLASS(weapon_tripmine, CTripmine); // orig 0xeca70

// Rocket Crowbar 2: clears its own members, and a pickup gives rc2.ini's TripminePickupGive.
// orig 0xecbc0, orig client 0x45b40
void CTripmine::Spawn()
{
	Precache();
	m_iId = WEAPON_TRIPMINE;
	SET_MODEL(ENT(pev), "models/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_GROUND;
	// ResetSequenceInfo( );
	pev->framerate = 0;

	m_iMinesOut[0] = 0;
	m_iMinesOut[1] = 0;
	m_iMinesOut[2] = 0;
	m_iMinesOut[3] = 0;
	m_flNextMinesOutSend = gpGlobals->time;

	FallInit(); // get ready to fall down

	m_iDefaultAmmo = RC2PrefWord(RC2P_Ammo_TripminePickupGive);

	// halflife-updated's client pev->body = 0 hack is not in RC2's client

#ifdef CLIENT_DLL
	if (!bIsMultiplayer())
#else
	if (!g_pGameRules->IsDeathmatch())
#endif
	{
		UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 28));
	}
}

// Rocket Crowbar 2: and the three new mines
// orig 0xeccb0, orig client 0x45c70
void CTripmine::Precache()
{
	PRECACHE_MODEL("models/v_tripmine.mdl");
	PRECACHE_MODEL("models/p_tripmine.mdl");
	UTIL_PrecacheOther("monster_tripmine");
	UTIL_PrecacheOther("bouncy_tripmine");
	UTIL_PrecacheOther("punch_tripmine");
	UTIL_PrecacheOther("target_tripmine");

	m_usTripFire = PRECACHE_EVENT(1, "events/tripfire.sc");
}

// Rocket Crowbar 2: the carry limit from rc2.ini, and weight 0
// orig 0xecd10, orig client 0x45cf0
bool CTripmine::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Trip Mine";
	p->iMaxAmmo1 = RC2PrefWord(RC2P_Ammo_MaxCarryTripmines);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_TRIPMINE;
	p->iWeight = 0;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return true;
}

// Rocket Crowbar 2: notes the time (no pev->body = 0 any more)
// orig 0xecd80, orig client 0x45d80
bool CTripmine::Deploy()
{
	m_flNextMinesOutSend = gpGlobals->time;
	return DefaultDeploy("models/v_tripmine.mdl", "models/p_tripmine.mdl", TRIPMINE_DRAW, "trip");
}


// unchanged from SDK 2.3
// orig 0xecdb0, orig client 0x45dc0
void CTripmine::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		// out of mines
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CTripmine::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
	}

	SendWeaponAnim(TRIPMINE_HOLSTER);
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

// Rocket Crowbar 2: the tripmine weapon's own code, the run of RC2's functions right after SDK
// 2.3's (rc2.dll 0xece60-0xee0a0, client.dll 0x45f30-0x46bd0), moved here from
// dlls/rc2/tripmine_weapon.cpp (c1) because the client's prediction compiles it too
// (reference/dossiers/c1.md): there it spends and times the same, but sticks, finds and counts
// nothing. Four mines, one per attack, each with its [Tripmine] ammo cost, limit and refire keys.
// what tells the four mines apart: everything else in the original's four copies is the same
struct tripmineattack_t
{
	const char* pszClassname;
	rc2pref_e iAmmoCost;	   // short: ammo used per mine
	rc2pref_e iMaxOut;		   // char: how many may be out at once (one more is let through)
	rc2pref_e iDeployDistance; // float: how far ahead the mine can be stuck on
	rc2pref_e iRefire[4];	   // float: delays this attack puts on the primary..quaternary attacks
};

static const tripmineattack_t kMineAttacks[4] = {
	{"monster_tripmine", RC2P_Tripmine_PrimaryAmmoCost, RC2P_Tripmine_PrimaryMaxOut, RC2P_Tripmine_PrimaryDeployDistance,
		{RC2P_Tripmine_PrimaryRefirePrimary, RC2P_Tripmine_PrimaryRefireSecondary, RC2P_Tripmine_PrimaryRefireTertiary, RC2P_Tripmine_PrimaryRefireQuaternary}},
	{"bouncy_tripmine", RC2P_Tripmine_BouncyAmmoCost, RC2P_Tripmine_BouncyMaxOut, RC2P_Tripmine_BouncyDeployDistance,
		{RC2P_Tripmine_SecondaryRefirePrimary, RC2P_Tripmine_SecondaryRefireSecondary, RC2P_Tripmine_SecondaryRefireTertiary, RC2P_Tripmine_SecondaryRefireQuaternary}},
	{"punch_tripmine", RC2P_Tripmine_PunchAmmoCost, RC2P_Tripmine_PunchMaxOut, RC2P_Tripmine_PunchDeployDistance,
		{RC2P_Tripmine_TertiaryRefirePrimary, RC2P_Tripmine_TertiaryRefireSecondary, RC2P_Tripmine_TertiaryRefireTertiary, RC2P_Tripmine_TertiaryRefireQuaternary}},
	{"target_tripmine", RC2P_Tripmine_TargetAmmoCost, RC2P_Tripmine_TargetMaxOut, RC2P_Tripmine_TargetDeployDistance,
		{RC2P_Tripmine_QuaternaryRefirePrimary, RC2P_Tripmine_QuaternaryRefireSecondary, RC2P_Tripmine_QuaternaryRefireTertiary, RC2P_Tripmine_QuaternaryRefireQuaternary}},
};

// how many mines of this kind the player who has this weapon has out
int CTripmine::CountMines(const char* szClassname) // orig 0xece60, orig client 0x45f30 (0 on the client)
{
	int iCount = 0;
#ifndef CLIENT_DLL
	CBaseEntity* pMine = NULL;

	while ((pMine = UTIL_FindEntityByClassname(pMine, szClassname)) != NULL)
	{
		CBaseEntity* pPlacer = static_cast<CTripmineGrenade*>(pMine)->m_hOwner;
		if (pPlacer && pPlacer->edict() == m_pPlayer->edict())
			iCount++;
	}
#endif

	return iCount;
}

// the body of the four attacks (the original has it four times over, each with its keys)
void CTripmine::MineAttack(int iMine)
{
	const tripmineattack_t& attack = kMineAttacks[iMine];
	const int iAmmoCost = (unsigned short)RC2Pref(attack.iAmmoCost).s;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < iAmmoCost)
		return;

	// Original bug kept: "greater than", so one mine more than the limit goes out
	if (m_iMinesOut[iMine] > (unsigned char)RC2Pref(attack.iMaxOut).c)
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * RC2Pref(attack.iDeployDistance).f, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usTripFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

#ifndef CLIENT_DLL
	if (tr.flFraction < 1.0)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		// Rocket Crowbar 2: a shielded player's shield can stop the mine going on
		if (pEntity && (pEntity->pev->flags & FL_CONVEYOR) == 0 &&
			ShieldDeflect(pEntity, RC2Pref(RC2P_Tripmine_GlobalDeployShieldDamage).f, 1.0f, tr.vecEndPos, gpGlobals->v_forward, NULL, NULL, 4) >= RC2Pref(RC2P_Tripmine_GlobalDeployShieldDamage).f)
		{
			Vector angles = UTIL_VecToAngles(tr.vecPlaneNormal);

			CBaseEntity::Create(attack.pszClassname, tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict());

			m_flNextMinesOutSend = gpGlobals->time; // send the new count next frame

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= iAmmoCost;

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);

			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
			{
				// no more mines!
				RetireWeapon();
				return;
			}
		}
	}
#endif

	UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[0]).f);
	UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[1]).f);
	UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[2]).f);
	UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[3]).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

void CTripmine::PrimaryAttack() // orig 0xecee0, orig client 0x45f50
{
	MineAttack(0);
}

void CTripmine::SecondaryAttack() // orig 0xed2e0, orig client 0x46220
{
	MineAttack(1);
}

void CTripmine::TertiaryAttack() // orig 0xed6e0, orig client 0x464f0
{
	MineAttack(2);
}

void CTripmine::QuaternaryAttack() // orig 0xedae0, orig client 0x467c0
{
	MineAttack(3);
}

// SDK 2.3's (halflife-updated's pev->body = 0 at the top is in neither library)
void CTripmine::WeaponIdle() // orig 0xedee0, orig client 0x46a90
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		SendWeaponAnim(TRIPMINE_DRAW);
	}
	else
	{
		RetireWeapon();
		return;
	}

	int iAnim;
	float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
	if (flRand <= 0.25)
	{
		iAnim = TRIPMINE_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 30.0;
	}
	else if (flRand <= 0.75)
	{
		iAnim = TRIPMINE_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 30.0;
	}
	else
	{
		iAnim = TRIPMINE_FIDGET;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 100.0 / 30.0;
	}

	SendWeaponAnim(iAnim);
}

// counts the mines out and, twice a second, tells the client
void CTripmine::WeaponFrame() // orig 0xedfc0, orig client 0x46bb0 (empty on the client)
{
#ifndef CLIENT_DLL
	for (int i = 0; i < 4; i++)
		m_iMinesOut[i] = CountMines(kMineAttacks[i].pszClassname);

	if (m_flNextMinesOutSend < gpGlobals->time)
	{
		m_flNextMinesOutSend = gpGlobals->time + 0.5;

		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_TRIPMINE);
		WRITE_BYTE(m_iMinesOut[0]);
		WRITE_BYTE(m_iMinesOut[1]);
		WRITE_BYTE(m_iMinesOut[2]);
		WRITE_BYTE(m_iMinesOut[3]);
		MESSAGE_END();
	}
#endif
}

// reads the counts back (only the client's readers read anything)
void CTripmine::ReadWeaponState(void* pData, int iSize) // orig 0xee0a0, orig client 0x46bd0
{
	m_iMinesOut[0] = RC2ReadStateByte(&pData, &iSize);
	m_iMinesOut[1] = RC2ReadStateByte(&pData, &iSize);
	m_iMinesOut[2] = RC2ReadStateByte(&pData, &iSize);
	m_iMinesOut[3] = RC2ReadStateByte(&pData, &iSize);
}
