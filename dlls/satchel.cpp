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

// Rocket Crowbar 2's satchel: the weapon throws roach charges (monster_satchel/roach_charge,
// walking bombs that home on targets and blow when armed), conc charges (sticky concussion
// charges that knock everything around) and control forklifts.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "rc2/p7_extern.h"
#include <algorithm>

#ifndef CLIENT_DLL

// CLTE effect types this file sends (gmsgCLTE: byte type, short entindex, ...); 254 removes
// the effect named by the trailing byte from the entity. Names ours.
#define CLTE_KILLEFFECT 254
#define CLTE_ATTACHED 25   // conc charge stuck to something: short entity, 6 coords
#define CLTE_ROACHWALK 59  // roach charge: byte armed
#define CLTE_CONCCHARGE 60 // conc charge: byte 0

// RC2 renamed SDK 2.3's CSatchelCharge to CRoachCharge (its exported methods are
// CRoachCharge's); monster_satchel and roach_charge both make one.
class CRoachCharge : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x400; } // orig 0xbaf70

	void EXPORT RoachBounce(CBaseEntity* pOther);
	void EXPORT RoachThink();
	void EXPORT RoachUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void FindTarget(); // name ours

	EHANDLE m_hTarget;	  // orig +0x2b8, name ours: what it homes on
	float m_flTurnRadius; // orig +0x2c0, name ours: +-(62..66), its turning circle; the sign is the side
	float m_flSpeed;	  // orig +0x2c4, name ours: 190..210, its walking speed
};
LINK_ENTITY_TO_CLASS(monster_satchel, CRoachCharge); // orig 0xbaef0
LINK_ENTITY_TO_CLASS(roach_charge, CRoachCharge);	 // orig 0xbaf80

void CRoachCharge::Precache() // orig 0xbb000
{
	PRECACHE_MODEL("models/roachcharge.mdl");
	PRECACHE_SOUND("roach/rch_walk.wav");
}

void CRoachCharge::Spawn() // orig 0xbb020
{
	pev->classname = MAKE_STRING("roach_charge");
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/roachcharge.mdl");
	UTIL_SetSize(pev, Vector(-1, -1, 0), Vector(1, 1, 2));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CRoachCharge::RoachBounce);
	SetUse(&CRoachCharge::RoachUse);
	SetThink(&CRoachCharge::RoachThink);
	pev->nextthink = gpGlobals->time + 0.01;

	pev->gravity = RC2Pref(RC2P_Satchel_RoachGravity).f;
	pev->friction = 0.8;
	pev->impulse = 0; // armed
	m_hTarget = NULL;
	pev->dmg = RC2Pref(RC2P_Satchel_RoachDamage).f;
	pev->dmgtime = gpGlobals->time; // the next target search, then (armed) the detonation
	pev->dmg_inflictor = pev->owner; // who threw it; pev->owner may be replaced

	const int iSide = RANDOM_LONG(0, 1);
	m_flTurnRadius = RANDOM_FLOAT(62, 66) * (iSide != 0 ? 1 : -1);
	m_flSpeed = RANDOM_FLOAT(190, 210);

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(CLTE_ROACHWALK);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0);
	MESSAGE_END();
}

void CRoachCharge::RoachBounce(CBaseEntity* pOther) // orig 0xbb1f0
{
	if ((pev->flags & FL_ONGROUND) != 0)
		pev->velocity = pev->velocity * 0.95;
}

// Five times a second, while not armed: the best visible target in [Satchel]
// RoachHomeLookDistance, nearer being better and the current one counting double.
void CRoachCharge::FindTarget() // orig 0xbb260
{
	if (pev->dmgtime < gpGlobals->time)
	{
		CBaseEntity* pEntity = NULL;
		CBaseEntity* pBest = NULL;
		float flBest = -9999;
		CBaseEntity* pThrower = NULL;

		pev->dmgtime = gpGlobals->time + 0.2;
		const float flLook = RC2Pref(RC2P_Satchel_RoachHomeLookDistance).f;

		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pThrower = CBaseEntity::Instance(pev->dmg_inflictor);

		const Vector vecSrc = pev->origin + Vector(0, 0, 4);
		float flDist;
		Vector vecNearest;

		while ((pEntity = UTIL_FindEntityNearestInSphere(pEntity, vecSrc, flLook, &flDist, &vecNearest)) != NULL)
		{
			if (pEntity->GetEffectedEnt()->pev->takedamage != DAMAGE_AIM || flDist > flLook)
				continue;
			if (pEntity->GetEffectedEnt()->pev->pContainingEntity == pev->owner)
				continue;
			if ((pEntity->GetEffectedEnt()->GetRCCaps2(0, this) & 0x28) != 0)
				continue;
			// VERIFY: CGameRules slot 51 read as PlayerRelationship (SDK 2.3's slot 50; RC2 has one more before it)
			if (pThrower != NULL && g_pGameRules->PlayerRelationship(pThrower, pEntity) == GR_TEAMMATE)
				continue;

			float flScore = std::clamp(1.0f - flDist / flLook, 0.0f, 1.0f);
			if (m_hTarget && pEntity->edict() == ((CBaseEntity*)m_hTarget)->edict())
				flScore += flScore;

			if (flScore >= flBest)
			{
				TraceResult tr;
				UTIL_TraceLine(vecSrc, vecNearest, ignore_monsters, ENT(pev), &tr);
				if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
				{
					flBest = flScore;
					pBest = pEntity;
				}
			}
		}

		m_hTarget = pBest;
	}
}

void CRoachCharge::RoachThink() // orig 0xbb500
{
	const float flInterval = StudioFrameAdvance(0.0);
	pev->nextthink = gpGlobals->time + 0.05;
	ApplyExternalForce(flInterval);

	float flSpeed = m_flSpeed;

	if (0 != pev->impulse)
	{
		if (pev->dmgtime < gpGlobals->time)
		{
			SetThink(NULL);
			SetUse(NULL);
			SetTouch(NULL);

			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(CLTE_KILLEFFECT);
			WRITE_SHORT(ENTINDEX(edict()));
			WRITE_BYTE(CLTE_ROACHWALK);
			MESSAGE_END();

			pev->owner = pev->dmg_inflictor;

			TraceResult tr;
			UTIL_TraceLine(pev->origin + Vector(0, 0, 8), pev->origin - Vector(0, 0, 128), ignore_monsters, ENT(pev), &tr);
			Explode(&tr, DMG_BLAST);
			return;
		}

		// speeds up while the arm time runs
		const float flArmed = std::clamp(1.0f - (pev->dmgtime - gpGlobals->time) / RC2Pref(RC2P_Satchel_RoachArmTime).f, 0.0f, 1.0f);
		flSpeed = flArmed * m_flSpeed * RC2Pref(RC2P_Satchel_RoachArmedAccelFraction).f + flSpeed;
	}

	pev->angles = UTIL_VecToAngles(pev->velocity.Normalize());
	pev->angles.x = 0;

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// walk in a circle: the sideways acceleration that keeps it on m_flTurnRadius
		UTIL_MakeVectors(pev->angles);
		const float flTurn = (pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y) / m_flTurnRadius;
		pev->velocity = pev->velocity + gpGlobals->v_right * flTurn * flInterval;

		FindTarget();

		if (m_hTarget)
		{
			Vector vecToTarget = ((CBaseEntity*)m_hTarget)->Center() - pev->origin;
			vecToTarget.z = 0;
			pev->velocity = pev->velocity + vecToTarget.Normalize() * fabs(flTurn) * RC2Pref(RC2P_Satchel_RoachHomePullFraction).f * flInterval;
		}

		const float flVel = pev->velocity.Length();
		if (flVel < flSpeed && flVel != 0)
			pev->velocity = (pev->velocity / flVel) * flSpeed;
		// (the original divides by the speed measured before the line above; both can't hold at once)
		if (flSpeed * 1.5 < flVel && flVel != 0)
			pev->velocity = (pev->velocity / flVel) * (RC2Pref(RC2P_Satchel_RoachMaxOverVelFraction).f * flSpeed);
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 8.0 * flInterval; // as the original computes it
		pev->velocity.z += flInterval * 80.0;
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		pev->velocity.z -= flInterval * 80.0;
	}
}

void CRoachCharge::RoachUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) // orig 0xbbc50
{
	if (0 == pev->impulse)
	{
		pev->impulse = 1;
		pev->dmgtime = RANDOM_FLOAT(0, 0.1) + RC2Pref(RC2P_Satchel_RoachArmTime).f + gpGlobals->time;

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(CLTE_ROACHWALK);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(1);
		MESSAGE_END();

		SetUse(NULL);
	}
}


// The conc charge: sticks where it lands (to the world, or riding on whatever it hit),
// deflects off player shields, and after [Satchel] ConcArmTime throws everything in
// ConcBlastRadius away from it.
class CConcCharge : public CGrenade
{
public:
	void Spawn() override;	  // orig 0xbbe10
	void Precache() override; // orig 0xbbd90
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x400; } // orig 0xbbd80

	void EXPORT AttachTouch(CBaseEntity* pOther);
	void EXPORT FlyingThink();
	void EXPORT ChargeThink();
	void EXPORT Detonate();

	bool m_fAttachedToWorld;	 // orig +0x2b8, name ours
	bool m_fAttachedToLiving;	 // orig +0x2b9, name ours: blows when what it rides on dies
	Vector m_vecAttachAngles;	 // orig +0x2bc, name ours: world angles, or relative to what it rides on
	Vector m_vecAttachOffset;	 // orig +0x2c8, name ours: world origin, or forward/right/up offsets on it
	Vector m_vecLocalNormal;	 // orig +0x2d4, name ours: the surface normal in that frame
	Vector m_vecSurfaceNormal;	 // orig +0x2e0, name ours: the surface normal in the world
	unsigned short m_usExplode; // orig +0x2ec, name ours: events/concchgexp.sc
};
LINK_ENTITY_TO_CLASS(conc_charge, CConcCharge); // orig 0xbbce0

void CConcCharge::Precache()
{
	PRECACHE_MODEL("models/w_satchel.mdl");
	PRECACHE_MODEL("sprites/xflare1.spr");
	PRECACHE_MODEL("sprites/c-tele1.spr");
	PRECACHE_SOUND("fvox/beep.wav");
	PRECACHE_SOUND("ambience/dronemachine2.wav");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/mortarhit.wav");
	PRECACHE_SOUND("ambience/biggun3.wav");
	m_usExplode = PRECACHE_EVENT(1, "events/concchgexp.sc");
}

void CConcCharge::Spawn()
{
	pev->classname = MAKE_STRING("conc_charge");
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_satchel.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CConcCharge::AttachTouch);
	SetThink(&CConcCharge::FlyingThink);
	pev->nextthink = gpGlobals->time + 0.01;

	pev->gravity = RC2Pref(RC2P_Satchel_ConcGravity).f;
	pev->friction = 0.8;
	m_vecSurfaceNormal = Vector(0, 0, 1);
	pev->sequence = 1;
	ResetSequenceInfo();

	m_fAttachedToWorld = false;
	m_fAttachedToLiving = false;
	pev->dmgtime = gpGlobals->time + 30.0; // gone if it never lands
	pev->dmg_inflictor = pev->owner;

	const int iSide = RANDOM_LONG(0, 1) != 0 ? 1 : -1;
	pev->avelocity.y = RANDOM_FLOAT(200, 300) * iSide;
	pev->angles.x = 0;
}

void CConcCharge::AttachTouch(CBaseEntity* pOther) // orig 0xbbfc0
{
	if (pOther->edict() == pev->owner)
		return;

	TraceResult tr = UTIL_GetGlobalTrace();
	const Vector vecDir = pev->velocity.Normalize();
	Vector vecDeflectPos, vecDeflectDir;

	if (ShieldDeflect(pOther, RC2Pref(RC2P_Satchel_ConcShieldDeflectDamage).f, 1.0, tr.vecEndPos, vecDir, &vecDeflectPos, &vecDeflectDir, 6) < 5.0)
	{
		// the shield took it: bounce off and keep flying, now as the shield owner's
		const float flSpeed = pev->velocity.Length();
		pev->velocity = UTIL_Reflect(vecDir, vecDeflectDir) * flSpeed;
		pev->origin = vecDeflectDir * 2 + vecDeflectPos;
		UTIL_SetOrigin(pev, pev->origin);
		pev->owner = pOther->edict();
		return;
	}

	Vector vecAngles = UTIL_VecToAngles(tr.vecPlaneNormal);
	vecAngles.x -= 90;
	m_vecSurfaceNormal = tr.vecPlaneNormal;

	if (ENTINDEX(pOther->edict()) == 0)
	{
		m_vecAttachAngles = vecAngles;
		m_vecAttachOffset = tr.vecPlaneNormal + tr.vecEndPos;
		UTIL_SetOrigin(pev, m_vecAttachOffset);
		m_fAttachedToWorld = true;
		pev->owner = NULL;
	}
	else
	{
		m_vecAttachAngles = vecAngles - pOther->pev->angles;
		UTIL_MakeVectors(pOther->pev->angles);
		UTIL_SetOrigin(pev, tr.vecPlaneNormal + tr.vecEndPos);

		const Vector vecLocal = pev->origin - pOther->pev->origin;
		m_vecAttachOffset.x = DotProduct(vecLocal, gpGlobals->v_forward);
		m_vecAttachOffset.y = DotProduct(vecLocal, gpGlobals->v_right);
		m_vecAttachOffset.z = DotProduct(vecLocal, gpGlobals->v_up);
		m_vecLocalNormal.x = DotProduct(gpGlobals->v_forward, m_vecSurfaceNormal);
		m_vecLocalNormal.y = DotProduct(gpGlobals->v_right, m_vecSurfaceNormal);
		m_vecLocalNormal.z = DotProduct(gpGlobals->v_up, m_vecSurfaceNormal);
		m_fAttachedToWorld = false;
		pev->owner = pOther->edict();
	}

	m_fAttachedToLiving = pOther->IsAlive() && pOther->pev->takedamage == DAMAGE_AIM && pOther->pev->deadflag == DEAD_NO;

	SetTouch(NULL);
	SetThink(&CConcCharge::ChargeThink);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Satchel_ConcArmTime).f + gpGlobals->time;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->velocity = g_vecZero;

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(CLTE_CONCCHARGE);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(CLTE_ATTACHED);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_SHORT(ENTINDEX(pOther->edict()));
	WRITE_COORD(m_vecAttachOffset.x);
	WRITE_COORD(m_vecAttachOffset.y);
	WRITE_COORD(m_vecAttachOffset.z);
	WRITE_COORD(m_vecAttachAngles.x);
	WRITE_COORD(m_vecAttachAngles.y);
	WRITE_COORD(m_vecAttachAngles.z);
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav", 0.6, ATTN_NORM, 0, 100);
}

void CConcCharge::FlyingThink() // orig 0xbc6d0
{
	ApplyExternalForce(0);
	pev->nextthink = gpGlobals->time + 0.1;
	if (pev->dmgtime < gpGlobals->time)
		UTIL_Remove(this);
}

void CConcCharge::ChargeThink() // orig 0xbc720
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (pev->dmgtime < gpGlobals->time)
	{
		Detonate();
		return;
	}

	if (!m_fAttachedToWorld && !UTIL_IsValidEntity(pev->owner))
	{
		pev->owner = NULL;
		Detonate();
		return;
	}

	if (m_fAttachedToLiving)
	{
		CBaseEntity* pRider = CBaseEntity::Instance(pev->owner);
		if (pRider == NULL || !pRider->IsAlive() || pRider->pev->takedamage != DAMAGE_AIM || pRider->pev->deadflag != DEAD_NO)
		{
			pev->owner = NULL;
			Detonate();
			return;
		}
	}

	if (!m_fAttachedToWorld)
	{
		// ride along
		entvars_t* pevRider = VARS(pev->owner);
		pev->angles = m_vecAttachAngles + pevRider->angles;
		UTIL_MakeVectors(pevRider->angles);
		UTIL_SetOrigin(pev, pevRider->origin + gpGlobals->v_forward * m_vecAttachOffset.x + gpGlobals->v_right * m_vecAttachOffset.y + gpGlobals->v_up * m_vecAttachOffset.z);
		m_vecSurfaceNormal = gpGlobals->v_forward * m_vecLocalNormal.x + gpGlobals->v_right * m_vecLocalNormal.y + gpGlobals->v_up * m_vecLocalNormal.z;
	}
	else
	{
		pev->angles = m_vecAttachAngles;
		UTIL_SetOrigin(pev, m_vecAttachOffset);
	}

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 800, 0.1);
}

void CConcCharge::Detonate() // orig 0xbcb40
{
	const float flRadius = RC2Pref(RC2P_Satchel_ConcBlastRadius).f;
	const float flPunchDistance = RC2Pref(RC2P_Satchel_ConcBlastPunchDistance).f;
	const float flMaxPush = RC2Pref(RC2P_Satchel_ConcBlastMaxPush).f;
	const float flMaxArmorDamage = RC2Pref(RC2P_Satchel_ConcBlastMaxArmorDamage).f;
	const float flMaxVelocity = CVAR_GET_FLOAT("sv_maxvelocity") - 1.0; // the original reads its cvar pointer
	const Vector vecOrigin = pev->origin;

	CBaseEntity* pThrower = NULL;
	entvars_t* pevAttacker = pev;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		pThrower = CBaseEntity::Instance(pev->dmg_inflictor);
		pevAttacker = VARS(pev->dmg_inflictor);
	}

	UTIL_PushEntityMarks();

	for (CBaseEntity* pEntity = UTIL_FindEntityInSphere(NULL, vecOrigin, flRadius); pEntity != NULL; pEntity = UTIL_FindEntityInSphere(pEntity, vecOrigin, flRadius))
	{
		if (pEntity->GetEffectedEnt()->pev->takedamage == DAMAGE_NO)
			continue;
		// VERIFY: the original passes pThrower (a CBaseEntity*) as the first argument, which cbase.h types int
		if ((pEntity->GetEffectedEnt()->GetRCCaps2(0, this) & 2) != 0)
			continue;
		if (pEntity->GetEffectedEnt()->pev->movetype == MOVETYPE_NONE || pEntity->IsBSPModel())
			continue;
		if (UTIL_IsEntityMarked(ENTINDEX(pEntity->GetEffectedEnt()->edict())))
			continue;

		float flPunch = flPunchDistance;
		const Vector vecCenter = pEntity->Center();
		Vector vecDir = vecCenter - vecOrigin;
		float flDist = vecDir.Length();
		if (flDist >= flRadius)
			continue;
		if (flDist <= 0)
		{
			vecDir = Vector(0, 0, 1);
			flDist = 1;
		}
		vecDir = vecDir / flDist;

		TraceResult tr;
		if (!UTIL_BlastReaches(vecOrigin, vecCenter, vecDir, &flPunch, edict(), pEntity->edict(), &tr, 16))
			continue;

		const float flShieldLeft = ShieldDeflect(pEntity->GetEffectedEnt(), RC2Pref(RC2P_Satchel_ConcBlastShieldDamage).f, 1.0, vecCenter, vecDir, NULL, NULL, 15);
		if (flShieldLeft <= 0)
			continue;

		const float flNear = std::clamp(1.0f - flDist / flRadius, 0.0f, 1.0f);
		const float flOpen = std::clamp(flPunch / flPunchDistance, 0.0f, 1.0f);
		const float flUnshielded = std::clamp(flShieldLeft / RC2Pref(RC2P_Satchel_ConcBlastShieldDamage).f, 0.0f, 1.0f);
		const float flScale = (flNear * 0.7 + flOpen * 0.1) * flUnshielded + 0.2;
		const float flArmorDamage = flScale * flMaxArmorDamage;
		float flPush = flScale * flMaxPush;

		CBaseEntity* pPushed = pEntity->GetEffectedEnt();
		if ((pPushed->pev->flags & FL_ONGROUND) != 0 && vecDir.z < 0)
		{
			// blown down onto the ground: skim along it instead
			flPush = flPush * 0.9;
			vecDir.z = 0.1;
			vecDir = vecDir.Normalize();
		}

		pPushed->pev->velocity = vecDir * flPush + pPushed->pev->velocity;
		const float flVel = pPushed->pev->velocity.Length();
		if (flMaxVelocity < flVel && flVel != 0)
			pPushed->pev->velocity = pPushed->pev->velocity * (flMaxVelocity / flVel);
		pPushed->pev->flags &= ~FL_ONGROUND;

		if (pPushed->IsActivePlayer())
			static_cast<CBasePlayer*>(pPushed)->SetPhysFlag10For(0.5);

		if (pEntity->IsActivePlayer() && 0 == static_cast<CBasePlayer*>(pEntity)->m_iTransformType)
		{
			pEntity->pev->armorvalue -= flArmorDamage;
			if (pEntity->pev->armorvalue < 0)
				pEntity->pev->armorvalue = 0;
		}

		pEntity->GetEffectedEnt()->TakeDamage(pev, pevAttacker, RC2Pref(RC2P_Satchel_ConcBlastDamage).f * flUnshielded, DMG_ALWAYSGIB | DMG_BLAST);
		UTIL_MarkEntity(ENTINDEX(pEntity->GetEffectedEnt()->edict()));
	}

	UTIL_PopEntityMarks();

	PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), m_usExplode, 0.0, pev->origin, g_vecZero, 0, 0, UTIL_PackDirection(m_vecSurfaceNormal), 0, false, false);

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_KILLEFFECT);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(CLTE_CONCCHARGE);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_KILLEFFECT);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(CLTE_ATTACHED);
	MESSAGE_END();

	SetThink(NULL);
	SetTouch(NULL);
	UTIL_Remove(this);
}

#endif // !CLIENT_DLL


LINK_ENTITY_TO_CLASS(weapon_satchel, CSatchel); // orig 0xbd410

void CSatchel::AddToPlayer(CBasePlayer* pPlayer)
{
	m_chargeReady = 0; // this satchel charge weapon now forgets that any satchels are deployed by it.
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

void CSatchel::Spawn() // orig 0xbd4d0
{
	Precache();
	m_iId = WEAPON_SATCHEL;
	SET_MODEL(ENT(pev), "models/w_satchel.mdl");

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_SatchelPickupGive).s;
	m_iRoachCount = 0;
	m_flNextRoachCount = gpGlobals->time;

	FallInit(); // get ready to fall down.
}

void CSatchel::Precache() // orig 0xbd530
{
	PRECACHE_MODEL("models/v_satchel.mdl");
	PRECACHE_MODEL("models/v_satchel_radio.mdl");
	PRECACHE_MODEL("models/w_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel_radio.mdl");

	UTIL_PrecacheOther("monster_satchel");
	UTIL_PrecacheOther("roach_charge");
	UTIL_PrecacheOther("conc_charge");
	UTIL_PrecacheOther("control_forklift");
}

bool CSatchel::GetItemInfo(ItemInfo* p) // orig 0xbd5a0
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Satchel Charge";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarrySatchels).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 1;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->iId = m_iId = WEAPON_SATCHEL;
	p->iWeight = 0;

	return true;
}

bool CSatchel::IsUseable() // orig 0xbd610
{
	if (m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] > 0)
		return true; // player is carrying some satchels

	WeaponFrame();
	if (0 == m_chargeReady && m_iRoachCount > 0)
		m_chargeReady = 1; // roaches are out: the radio works

	return 0 != m_chargeReady;
}

bool CSatchel::CanDeploy() // orig 0xbd670
{
	if (m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] > 0)
		return true;

	WeaponFrame();
	if (0 == m_chargeReady && m_iRoachCount > 0)
		m_chargeReady = 1;

	return 0 != m_chargeReady;
}

bool CSatchel::Deploy() // orig 0xbd6d0
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	WeaponFrame();
	if (0 == m_chargeReady && m_iRoachCount > 0)
		m_chargeReady = 1;

	if (0 != m_chargeReady)
		return DefaultDeploy("models/v_satchel_radio.mdl", "models/p_satchel_radio.mdl", SATCHEL_RADIO_DRAW, "hive");
	else
		return DefaultDeploy("models/v_satchel.mdl", "models/p_satchel.mdl", SATCHEL_DRAW, "trip");
}

void CSatchel::Holster() // orig 0xbd790
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (0 != m_chargeReady)
		SendWeaponAnim(SATCHEL_RADIO_HOLSTER);
	else
		SendWeaponAnim(SATCHEL_DROP);

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM, 0, 100);

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && 0 == m_chargeReady)
	{
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CSatchel::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

// Throws a roach charge; with roaches out, arms every one this player threw.
void CSatchel::PrimaryAttack() // orig 0xbd860
{
	switch (m_chargeReady)
	{
	case 0:
		Throw();
		break;

	case 1:
	{
		SendWeaponAnim(SATCHEL_RADIO_FIRE);

#ifndef CLIENT_DLL
		edict_t* pPlayer = m_pPlayer->edict();
		for (CBaseEntity* pRoach = UTIL_FindEntityByClassname(NULL, "roach_charge"); pRoach != NULL; pRoach = UTIL_FindEntityByClassname(pRoach, "roach_charge"))
		{
			if (0 == pRoach->pev->impulse && pRoach->pev->dmg_inflictor == pPlayer)
			{
				pRoach->Use(m_pPlayer, m_pPlayer, USE_ON, 0);
				m_chargeReady = 2;
			}
		}
#endif

		m_chargeReady = 2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_DetonateRefirePrimary).f);
		UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_DetonateRefireSecondary).f);
		UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_DetonateRefireTertiary).f);
		UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_DetonateRefireQuaternary).f);
		break;
	}

	case 2:
		// we're reloading, don't allow fire
		break;
	}
}

void CSatchel::SecondaryAttack() // orig 0xbd990
{
	if (m_chargeReady != 2)
		Throw();
}

// name ours: the body the tertiary and quaternary attacks share, a thrown entity of szClassname
static CBaseEntity* SatchelLaunch(CSatchel* pSatchel, const char* szClassname, float flVelocity)
{
	CBasePlayer* pPlayer = pSatchel->m_pPlayer;
	CBaseEntity* pLaunched = NULL;

#ifndef CLIENT_DLL
	UTIL_MakeVectors(pPlayer->pev->v_angle + pPlayer->pev->punchangle);
	Vector vecSrc = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	vecSrc.z -= 8;
	const Vector vecThrow = gpGlobals->v_forward * flVelocity + pPlayer->pev->velocity;

	pLaunched = CBaseEntity::Create(szClassname, vecSrc, Vector(0, 0, 0), pPlayer->edict());
	pLaunched->pev->velocity = vecThrow;
#endif
	return pLaunched;
}

static void SatchelLaunchOwner(CBasePlayer* pPlayer, CBaseEntity* pLaunched) // name ours
{
#ifndef CLIENT_DLL
	if (0 != pPlayer->m_iRidingIndex)
	{
		edict_t* pentOwner = INDEXENT(pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pentOwner))
			pLaunched->pev->owner = pentOwner;
	}
#endif
}

// The conc charge.
void CSatchel::TertiaryAttack() // orig 0xbd9a0
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= RC2Pref(RC2P_Satchel_ConcAmmoCost).s)
	{
		CBaseEntity* pCharge = SatchelLaunch(this, "conc_charge", RC2Pref(RC2P_Satchel_ConcThrowVel).f);
#ifndef CLIENT_DLL
		pCharge->pev->angles.y = m_pPlayer->pev->v_angle.y;
#endif
		SatchelLaunchOwner(m_pPlayer, pCharge);

		SendWeaponAnim(SATCHEL_DRAW);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2Pref(RC2P_Satchel_ConcAmmoCost).s;

		UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_TertiaryRefirePrimary).f);
		UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_TertiaryRefireSecondary).f);
		UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_TertiaryRefireTertiary).f);
		UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_TertiaryRefireQuaternary).f);
	}
}

// The control forklift (shotgun_forklift's sibling, reconstructed with the forklift).
void CSatchel::QuaternaryAttack() // orig 0xbdc80
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= RC2Pref(RC2P_Satchel_ForkliftAmmoCost).s)
	{
		CBaseEntity* pForklift = SatchelLaunch(this, "control_forklift", RC2Pref(RC2P_Satchel_ForkliftLaunchVel).f);
		SatchelLaunchOwner(m_pPlayer, pForklift);

		SendWeaponAnim(SATCHEL_DRAW);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2Pref(RC2P_Satchel_ForkliftAmmoCost).s;

		UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_QuaternaryRefirePrimary).f);
		UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_QuaternaryRefireSecondary).f);
		UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_QuaternaryRefireTertiary).f);
		UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_QuaternaryRefireQuaternary).f);
	}
}

// Throws a roach charge, up to [Satchel] RoachMaxDeployed of them out.
void CSatchel::Throw() // orig 0xbdf50
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= RC2Pref(RC2P_Satchel_RoachAmmoCost).s && m_iRoachCount < RC2Pref(RC2P_Satchel_RoachMaxDeployed).s)
	{
#ifndef CLIENT_DLL
		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;
		vecSrc.z -= 8;
		Vector vecThrow = gpGlobals->v_forward * RC2Pref(RC2P_Satchel_RoachThrowVel).f + m_pPlayer->pev->velocity;
		vecThrow.z += RC2Pref(RC2P_Satchel_RoachZBoost).f;

		CBaseEntity* pRoach = CBaseEntity::Create("roach_charge", vecSrc, Vector(0, 0, 0), m_pPlayer->edict());
		pRoach->pev->velocity = vecThrow;
		SatchelLaunchOwner(m_pPlayer, pRoach);
#endif

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel_radio.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel_radio.mdl");
		m_flNextRoachCount = gpGlobals->time - 1.0; // recount now

		SendWeaponAnim(SATCHEL_RADIO_DRAW);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		m_chargeReady = 1;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2Pref(RC2P_Satchel_RoachAmmoCost).s;

		UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_ThrowRefirePrimary).f);
		UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_ThrowRefireSecondary).f);
		UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_ThrowRefireTertiary).f);
		UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_ThrowRefireQuaternary).f);
	}
}

void CSatchel::WeaponIdle() // orig 0xbe290
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	switch (m_chargeReady)
	{
	case 0:
		SendWeaponAnim(SATCHEL_FIDGET1);
		// use tripmine animations
		strcpy(m_pPlayer->m_szAnimExtention, "trip");
		break;
	case 1:
		SendWeaponAnim(SATCHEL_RADIO_FIDGET1);
		// use hivehand animations
		strcpy(m_pPlayer->m_szAnimExtention, "hive");
		break;
	case 2:
		if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			m_chargeReady = 0;
			RetireWeapon();
			return;
		}

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel.mdl");
		SendWeaponAnim(SATCHEL_DRAW);

		// use tripmine animations
		strcpy(m_pPlayer->m_szAnimExtention, "trip");

		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_chargeReady = 0;
		break;
	}
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15); // how long till we do this again.
}

// Every frame: counts this player's roaches; twice a second sends the count to the HUD.
void CSatchel::WeaponFrame() // orig 0xbe440
{
	m_iRoachCount = 0;
#ifndef CLIENT_DLL
	for (CBaseEntity* pRoach = UTIL_FindEntityByClassname(NULL, "roach_charge"); pRoach != NULL; pRoach = UTIL_FindEntityByClassname(pRoach, "roach_charge"))
	{
		if (pRoach->pev->dmg_inflictor == m_pPlayer->pev->pContainingEntity)
			m_iRoachCount++;
	}

	if (m_flNextRoachCount < gpGlobals->time)
	{
		m_flNextRoachCount = gpGlobals->time + 0.5;
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_SATCHEL);
		WRITE_BYTE(std::clamp(m_iRoachCount, 0, 255));
		MESSAGE_END();
	}
#endif
}

void CSatchel::ReadWeaponState(void* pData, int iSize) // orig 0xbe530
{
	m_iRoachCount = 0; // VERIFY: as CRpg's: the original assigns 0xfb780's constant false
}

int CSatchel::iItemSlot() { return 5; } // orig 0xbe560

bool CSatchel::UseDecrement() { return true; } // orig 0xbe570

#ifndef CLIENT_DLL
// TEMP STUB (p7): RC2 has no DeactivateSatchels; halflife-updated's multiplay_gamerules.cpp
// still calls it until its package reconstructs PlayerKilled.
void DeactivateSatchels(CBasePlayer* pOwner)
{
}
#endif

// The client's g_rc2PrefValues (all zero) is defined once, in pm_shared.cpp (p6).
