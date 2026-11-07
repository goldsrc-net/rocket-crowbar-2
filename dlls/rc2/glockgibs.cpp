/***
*
*	Rocket Crowbar 2: the glock's three projectiles (the original's run between globals and
*	wpn_shared/hl_wpn_glock; file name ours). All three are CBaseEntity with no save table.
*
*	glock_gib		  primary: bounces up to [Glock] GibMaxBounces times, hurts what it hits hard
*					  enough to get past a shield
*	glock_stickygib	  tertiary: sticks to the world or to something alive, gasses what is near
*					  (and its host) every half second until StickyAttachedLife runs out
*	transphasic_gib	  the charged quaternary: invisible, flies at TransphasicVelocity through
*					  solid world (up to TransphasicMaxSolidPunch units of it) and hurts every
*					  entity on its line
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "shield.h"
#include "UserMessages.h"

#define RC2_GIBCAP_STICKY 4 // GetRCCaps bit: the sticky gib may attach to it even past a shield. Name ours

// The attacker an RC2 projectile credits: what fired it (pev->dmg_inflictor, set from the owner
// at spawn) while that is still valid, else the projectile itself. Inline everywhere in the
// original.
static entvars_t* GibAttacker(CBaseEntity* pGib) // name ours
{
	if (UTIL_IsValidEntity(pGib->pev->dmg_inflictor))
		return VARS(pGib->pev->dmg_inflictor);
	return pGib->pev;
}

// The first argument of the GetRCCaps pair as the gibs pass it: nothing while what fired them is
// still valid, else the entity behind pev->dmg_inflictor (the world when it is gone). VERIFY: the
// test reads inverted; kept as read.
static CBaseEntity* GibCapsAsker(CBaseEntity* pGib) // name ours
{
	if (UTIL_IsValidEntity(pGib->pev->dmg_inflictor))
		return NULL;
	return CBaseEntity::Instance(pGib->pev->dmg_inflictor);
}

//=========================================================
// transphasic_gib
//=========================================================
class CTransphasicGib : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT TGibThink();
	bool PunchThrough(Vector vecSrc, Vector vecEnd, Vector vecDir, edict_t* pentIgnore, int iDepth);

	float m_flLastThink; // orig +0x70, ours
	float m_flPunched;	 // orig +0x74, ours: solid world passed through on the current run
};

LINK_ENTITY_TO_CLASS(transphasic_gib, CTransphasicGib); // orig 0x52640

// orig 0x526b0
void CTransphasicGib::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CTransphasicGib::TGibThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flLastThink = gpGlobals->time;
	pev->air_finished = RC2Pref(RC2P_Glock_TransphasicMaxLife).f + gpGlobals->time; // when it gives up
	pev->speed = RC2Pref(RC2P_Glock_TransphasicVelocity).f;
	m_flPunched = 0;
	pev->dmg_inflictor = pev->owner;
}

// orig 0x52770
void CTransphasicGib::Precache()
{
	PRECACHE_MODEL("models/hgibs.mdl");
	PRECACHE_MODEL("sprites/tpgibflare.spr");
	PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_MODEL("sprites/blood.spr");
	PRECACHE_MODEL("sprites/bloodspray.spr");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
}

// orig 0x527c0
// One step of the gib's line from vecSrc to vecEnd. Inside solid world it creeps forward in
// sixtieths of the line until it is out (true when the end is reached first, or the way back
// out is all solid); then it hull-traces on and, on hitting an entity, deals that entity's
// TransphasicImpactDamage and carries on past it, recursing at most iDepth deep. True means it
// was inside solid this step. The original ignores what the recursion returns (it returns false
// after every recursive call); kept.
bool CTransphasicGib::PunchThrough(Vector vecSrc, Vector vecEnd, Vector vecDir, edict_t* pentIgnore, int iDepth)
{
	TraceResult tr;
	bool fInSolid = false;

	if (--iDepth < 1)
		return false;

	float flStep = (vecEnd - vecSrc).Length() * (1.0 / 60.0);

	int i = 0x95;
	while (true)
	{
		if (UTIL_PointContents(vecSrc) != CONTENTS_SOLID)
		{
			if (!fInSolid)
				goto clear;
			break;
		}

		vecSrc = vecSrc + vecDir * flStep;
		fInSolid = true;
		if ((vecEnd - vecSrc).Length() < flStep)
			return true;

		if (i-- <= 0)
			break;
	}

	// back out of the solid to its surface
	UTIL_TraceLine(vecSrc, vecSrc - vecDir * (flStep + 8), ignore_monsters, pentIgnore, &tr);
	vecSrc = tr.vecEndPos;
	if (0 != tr.fStartSolid || 0 != tr.fAllSolid)
		return true;

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, vecSrc, 800, 0.2);

clear:
	vecSrc = vecSrc + vecDir * 8;
	UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, pentIgnore, &tr);
	if (tr.flFraction >= 1.0)
		return false;

	if ((vecEnd - vecSrc).Length() * tr.flFraction < 8)
	{
		// the hull stopped short: unless that was an entity's own body, trace the line instead
		if (tr.pHit == NULL || !UTIL_IsValidEntity(tr.pHit) || CBaseEntity::Instance(tr.pHit)->IsBSPModel())
			UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pentIgnore, &tr);
	}

	edict_t* pentHit = tr.pHit;
	if (pentHit != NULL)
	{
		if (UTIL_IsValidEntity(pentHit) && pentHit != pev->dmg_inflictor)
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(pentHit);
			if (pEntity != NULL)
			{
				if (pEntity->IsBSPModel())
					UTIL_TraceLine(tr.vecEndPos, vecEnd, ignore_monsters, pentIgnore, &tr);

				float flDamage = pEntity->TransphasicImpactDamage();
				pEntity->GetEffectedEnt()->TakeDamage(pev, GibAttacker(this), flDamage, DMG_ENERGYBEAM | DMG_ALWAYSGIB);
			}

			if (UTIL_IsValidEntity(pentHit))
				pev->owner = pentHit; // VERIFY: the gib's owner becomes what it hit, as read
			else
				pentHit = NULL;

			PunchThrough(tr.vecEndPos, vecEnd, vecDir, pentHit, iDepth);
			return false;
		}

		if (pentHit == pev->owner)
		{
			PunchThrough(tr.vecEndPos, vecEnd, vecDir, pentHit, iDepth);
			return false;
		}
	}

	UTIL_TraceLine(tr.vecEndPos, vecEnd, ignore_monsters, pentIgnore, &tr);
	PunchThrough(tr.vecEndPos + vecDir * flStep, vecEnd, vecDir, pentIgnore, iDepth);
	return false;
}

// orig 0x52ef0
void CTransphasicGib::TGibThink()
{
	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;

	// the shooter's ping, once (CGlock::FireTransphasic puts it in armorvalue)
	if (pev->armorvalue > 0)
		flInterval += pev->armorvalue;
	pev->armorvalue = 0;

	Vector vecEnd = pev->origin + pev->movedir * pev->speed * flInterval;

	if (PunchThrough(pev->origin, vecEnd, (vecEnd - pev->origin).Normalize(), pev->owner, 40))
		m_flPunched += (vecEnd - pev->origin).Length();
	else
		m_flPunched = 0;

	if (m_flPunched <= RC2Pref(RC2P_Glock_TransphasicMaxSolidPunch).f && gpGlobals->time <= pev->air_finished)
	{
		UTIL_SetOrigin(pev, vecEnd);
		return;
	}

	UTIL_Remove(this);
}

//=========================================================
// glock_stickygib
//=========================================================
class CGlockStickyGib : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return RC2_GIBCAP_STICKY; } // orig 0x53160

	void EXPORT AttachedThink();
	void EXPORT AttachTouch(CBaseEntity* pOther);
	void FollowHost();
	void Expire(bool fOnWorld);

	Vector m_vecAttachAngles; // orig +0x70, ours: angles, relative to the host's unless on the world
	Vector m_vecAttachOffset; // orig +0x7c, ours: on the world the point; else forward/right/up of the host
	Vector m_vecAttachDir;	  // orig +0x88, ours: forward of the gib's angles (+90 roll), for the events
	bool m_fOnWorld;		  // orig +0x94, ours
	bool m_fOnAlive;		  // orig +0x95, ours: the host was alive when it stuck
	float m_flLastGas;		  // orig +0x98, ours
	unsigned short m_usImpact; // orig +0x9c: events/glockgibimpact.sc
	unsigned short m_usAttach; // orig +0x9e: events/stickygibattach.sc
};

LINK_ENTITY_TO_CLASS(glock_stickygib, CGlockStickyGib); // orig 0x530f0

// orig 0x53170
void CGlockStickyGib::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->gravity = RC2Pref(RC2P_Glock_StickyGravity).f;
	pev->friction = 0.3;
	SET_MODEL(ENT(pev), "models/stickygib.mdl");
	pev->body = RANDOM_LONG(0, 2);
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(&CGlockStickyGib::AttachTouch);
	SetThink(&CGlockStickyGib::SUB_Remove);
	pev->nextthink = gpGlobals->time + RC2Pref(RC2P_Glock_StickyFlightLife).f;
	pev->dmg = 30;
	pev->dmg_inflictor = pev->owner;
	pev->impulse = 2; // bounces left
	pev->angles = Vector(RANDOM_FLOAT(-90, 90), RANDOM_FLOAT(0, 360), RANDOM_FLOAT(0, 360));
	pev->avelocity = Vector(RANDOM_FLOAT(-1000, 1000), RANDOM_FLOAT(-1000, 1000), RANDOM_FLOAT(-1000, 1000));

	// the client's trail on it
	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(10);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0x53360
void CGlockStickyGib::Precache()
{
	PRECACHE_MODEL("models/stickygib.mdl");
	PRECACHE_SOUND("weapons/pwach.wav");
	PRECACHE_MODEL("sprites/stmbal1.spr");
	m_usImpact = PRECACHE_EVENT(1, "events/glockgibimpact.sc");
	m_usAttach = PRECACHE_EVENT(1, "events/stickygibattach.sc");
}

// The impact event and removal, inline in the original's AttachedThink.
void CGlockStickyGib::Expire(bool fOnWorld) // name ours
{
	PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0.0, pev->origin, g_vecZero, 0, 0, UTIL_PackNormal(m_vecAttachDir), 0, fOnWorld ? 1 : 0, 1);
	UTIL_Remove(this);
}

// orig 0x533c0
void CGlockStickyGib::AttachedThink()
{
	float flInterval = gpGlobals->time - m_flLastGas;
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_fOnWorld)
	{
		if (!UTIL_IsValidEntity(pev->owner))
		{
			Expire(false);
			return;
		}
		if (m_fOnAlive)
		{
			if (VARS(pev->owner)->deadflag != DEAD_NO || !CBaseEntity::Instance(pev->owner)->IsAlive())
			{
				Expire(false);
				return;
			}
		}
	}

	if (pev->air_finished < gpGlobals->time)
	{
		Expire(m_fOnWorld);
		return;
	}

	FollowHost();

	if (flInterval <= 0.5)
		return;

	m_flLastGas = gpGlobals->time;
	float flDamage = RC2Pref(RC2P_Glock_StickyGasDamagePerSec).f * flInterval;
	Vector vecNearest = g_vecZero;

	if (pev->waterlevel >= 2)
		return; // no gas under water

	UTIL_PushEntityMarks();
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityNearestInSphere(pEntity, pev->origin, RC2Pref(RC2P_Glock_StickyGasRadius).f, NULL, &vecNearest)) != NULL)
	{
		CBaseEntity* pVictim = pEntity->GetEffectedEnt();
		if (pVictim->pev->deadflag != DEAD_NO)
			continue;
		if (pVictim->pev->takedamage != DAMAGE_AIM)
			continue;
		if (UTIL_IsEntityMarked(ENTINDEX(pVictim->edict())))
			continue;
		if (!pVictim->IsAlive())
			continue;
		if (!m_fOnWorld && pVictim->edict() == pev->owner)
			continue; // the host is gassed below
		if (pEntity->pev->waterlevel >= 2)
			continue;
		if (pVictim->IsActivePlayer() && (PM_GetAbilityFlags(ENTINDEX(pVictim->edict())) & 2) != 0)
			continue;

		TraceResult tr;
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(pev->origin, vecNearest, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		float flAmount = flDamage;
		if (pVictim->pev->velocity.Length() > RC2Pref(RC2P_Glock_StickyMovementThreshold).f)
			flAmount = RC2Pref(RC2P_Glock_StickyMovementDmgFraction).f * flDamage;

		pVictim->TakeDamage(pev, GibAttacker(this), flAmount, DMG_NERVEGAS);
		UTIL_MarkEntity(ENTINDEX(pVictim->edict()));
	}
	UTIL_PopEntityMarks();

	if (m_fOnWorld)
		return;

	// the host
	float flAmount = flDamage;
	if (VARS(pev->owner)->velocity.Length() > RC2Pref(RC2P_Glock_StickyMovementThreshold).f)
		flAmount = RC2Pref(RC2P_Glock_StickyMovementDmgFraction).f * flDamage;

	CBaseEntity* pHost = CBaseEntity::Instance(pev->owner);
	if (pHost == NULL)
		return;
	CBaseEntity* pVictim = pHost->GetEffectedEnt();
	if (pVictim->IsActivePlayer() && (PM_GetAbilityFlags(ENTINDEX(pVictim->edict())) & 2) != 0)
		return;
	if (pVictim->pev->waterlevel >= 2)
		return;

	// the original passes VARS(dmg_inflictor) (NULL when it is) while it is valid, else this
	pVictim->TakeDamage(pev, UTIL_IsValidEntity(pev->dmg_inflictor) ? VARS(pev->dmg_inflictor) : pev, flAmount, DMG_NERVEGAS);
}

// orig 0x53910
void CGlockStickyGib::AttachTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	int iCaps = pOther->GetRCCaps(GibCapsAsker(this), this);

	edict_t* pentSelf = ENT(pev);
	Vector vecDir = pev->velocity.Normalize();
	Vector vecFront = pev->origin + vecDir * 32;
	Vector vecBack = pev->origin - pev->velocity.Normalize() * 8;

	TraceResult tr;
	UTIL_TraceLine(vecBack, vecFront, dont_ignore_monsters, pentSelf, &tr);

	float flLeft = ShieldDeflect(pOther, RC2Pref(RC2P_Glock_StickyShieldDamage).f, 1.0, tr.vecEndPos, pev->velocity.Normalize(), NULL, NULL, 6);

	bool fStick = false;
	if ((iCaps & RC2_GIBCAP_STICKY) != 0 && flLeft >= RC2Pref(RC2P_Glock_StickyShieldDamage).f &&
		(pOther->pev->takedamage == DAMAGE_NO || pOther->IsBSPModel()))
		fStick = true;
	else if (pOther->IsBSPModel())
		fStick = true;

	if (fStick && tr.flFraction <= 1.0 && tr.pHit == pOther->edict())
	{
		PLAYBACK_EVENT_FULL(0, edict(), m_usAttach, 0.0, pev->origin, g_vecZero, 0, 0,
			UTIL_PackNormal(pev->velocity.Normalize()), 0, pOther->IsBSPModel() ? 1 : 0, 0);

		pev->owner = pOther->edict();
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = g_vecZero;
		pev->avelocity = g_vecZero;

		if (ENTINDEX(pOther->edict()) == 0)
			m_fOnWorld = true;

		Vector vecTip; // where the client draws the gas from
		if (m_fOnWorld)
		{
			m_vecAttachAngles = UTIL_VecToAngles(tr.vecPlaneNormal);
			m_vecAttachOffset = tr.vecEndPos;
			m_vecAttachAngles.z -= 90;
			vecTip = tr.vecPlaneNormal * 20 + m_vecAttachOffset;
		}
		else
		{
			m_vecAttachAngles = UTIL_VecToAngles(tr.vecPlaneNormal) - pOther->pev->angles;
			m_vecAttachAngles.z -= 90;

			UTIL_MakeVectors(pOther->pev->angles);
			Vector vecLocal = tr.vecEndPos - pOther->pev->origin;
			m_vecAttachOffset.x = DotProduct(vecLocal, gpGlobals->v_forward);
			m_vecAttachOffset.y = DotProduct(vecLocal, gpGlobals->v_right);
			m_vecAttachOffset.z = DotProduct(vecLocal, gpGlobals->v_up);

			if (pOther->IsBSPModel())
			{
				Vector vecTipLocal = tr.vecEndPos + tr.vecPlaneNormal * 20 - pOther->pev->origin;
				vecTip.x = DotProduct(vecTipLocal, gpGlobals->v_forward);
				vecTip.y = DotProduct(vecTipLocal, gpGlobals->v_right);
				vecTip.z = DotProduct(vecTipLocal, gpGlobals->v_up);
			}
			else
				vecTip = m_vecAttachOffset;

			m_fOnAlive = pOther->pev->deadflag == DEAD_NO && pOther->IsAlive();
			if (m_fOnAlive)
			{
				pOther->TakeDamage(pev, GibAttacker(this), RC2Pref(RC2P_Glock_StickyImpactDamage).f, DMG_ACID);

				CBaseEntity* pVictim = pOther->GetEffectedEnt();
				if (pVictim->pev->deadflag != DEAD_NO || !pOther->GetEffectedEnt()->IsAlive())
				{
					// the hit killed it
					SetThink(NULL);
					SetTouch(NULL);
					UTIL_Remove(this);
					return;
				}
			}
		}

		FollowHost();
		SetThink(&CGlockStickyGib::AttachedThink);
		SetTouch(NULL);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->air_finished = RC2Pref(RC2P_Glock_StickyAttachedLife).f + gpGlobals->time;
		m_flLastGas = gpGlobals->time;

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(11);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_SHORT(ENTINDEX(pOther->edict()));
		WRITE_COORD(m_vecAttachOffset.x);
		WRITE_COORD(m_vecAttachOffset.y);
		WRITE_COORD(m_vecAttachOffset.z);
		WRITE_COORD(m_vecAttachAngles.x);
		WRITE_COORD(m_vecAttachAngles.y);
		WRITE_COORD(m_vecAttachAngles.z);
		WRITE_COORD(vecTip.x);
		WRITE_COORD(vecTip.y);
		WRITE_COORD(vecTip.z);
		MESSAGE_END();
		return;
	}

	// a bounce
	pev->impulse--;
	if (flLeft >= RC2Pref(RC2P_Glock_StickyShieldDamage).f)
		pOther->GetEffectedEnt()->TakeDamage(pev, GibAttacker(this), RC2Pref(RC2P_Glock_StickyImpactDamage).f, DMG_ACID);

	if (pev->impulse > 0)
	{
		PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0.0, pev->origin, g_vecZero, 0, 0,
			UTIL_PackNormal(pev->velocity.Normalize()), 0, pOther->IsBSPModel() ? 1 : 0, 0);
		return;
	}

	PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0.0, pev->origin, g_vecZero, 0, 0,
		UTIL_PackNormal(pev->velocity.Normalize()), 0, pOther->IsBSPModel() ? 1 : 0, 1);
	UTIL_Remove(this);
	SetThink(NULL);
	SetTouch(NULL);
}

// orig 0x54340
// Keep the gib on its host: in place on the world, else at its offset in the host's frame.
// When the host is gone it marks itself expired (AttachedThink removes it).
void CGlockStickyGib::FollowHost() // name ours
{
	if (!UTIL_IsValidEntity(pev->owner) && !m_fOnWorld)
	{
		pev->owner = NULL;
		pev->air_finished = gpGlobals->time - 1.0;
		return;
	}

	pev->velocity = g_vecZero;

	if (m_fOnWorld)
	{
		pev->angles = m_vecAttachAngles;
		UTIL_SetOrigin(pev, m_vecAttachOffset);
		return;
	}

	entvars_t* pevHost = VARS(pev->owner);
	pev->angles = m_vecAttachAngles + pevHost->angles;

	UTIL_MakeVectors(pevHost->angles);
	Vector vecUp = gpGlobals->v_up * m_vecAttachOffset.z;
	Vector vecRight = gpGlobals->v_right * m_vecAttachOffset.y;
	Vector vecForward = gpGlobals->v_forward * m_vecAttachOffset.x;
	UTIL_SetOrigin(pev, vecForward + pevHost->origin + vecRight + vecUp);

	UTIL_MakeVectors(Vector(pev->angles.x, pev->angles.y, pev->angles.z + 90));
	m_vecAttachDir = gpGlobals->v_forward;
}

//=========================================================
// glock_gib
//=========================================================
class CGlockGib : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT BounceTouch(CBaseEntity* pOther);

	unsigned short m_usImpact; // orig +0x70: events/glockgibimpact.sc
};

LINK_ENTITY_TO_CLASS(glock_gib, CGlockGib); // orig 0x54630

// orig 0x546a0
void CGlockGib::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->gravity = RC2Pref(RC2P_Glock_GibGravity).f;
	pev->friction = RC2Pref(RC2P_Glock_GibFriction).f;
	SET_MODEL(ENT(pev), "models/hgibs.mdl");
	pev->body = RANDOM_LONG(0, 10);
	pev->impulse = RC2Pref(RC2P_Glock_GibMaxBounces).i; // bounces left
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(&CGlockGib::BounceTouch);
	SetThink(&CGlockGib::SUB_Remove);
	pev->nextthink = gpGlobals->time + RC2Pref(RC2P_Glock_GibMaxLife).f;
	pev->dmg = RC2Pref(RC2P_Glock_GibDamage).f;
	pev->dmg_inflictor = pev->owner;
	pev->angles = Vector(RANDOM_FLOAT(-90, 90), RANDOM_FLOAT(0, 360), RANDOM_FLOAT(0, 360));
	pev->avelocity = Vector(RANDOM_FLOAT(-1000, 1000), RANDOM_FLOAT(-1000, 1000), RANDOM_FLOAT(-1000, 1000));

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(10);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0x54890
void CGlockGib::Precache()
{
	PRECACHE_MODEL("models/hgibs.mdl");
	PRECACHE_MODEL("sprites/blood.spr");
	PRECACHE_MODEL("sprites/bloodspray.spr");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	m_usImpact = PRECACHE_EVENT(1, "events/glockgibimpact.sc");
}

// orig 0x548e0
void CGlockGib::BounceTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	pev->owner = NULL;

	int iCaps = pOther->GetRCCaps2(GibCapsAsker(this), this);

	float flLeft = ShieldDeflect(pOther, RC2Pref(RC2P_Glock_GibShieldDamage).f, 1.0, pev->origin, pev->velocity.Normalize(), NULL, NULL, 7);
	TraceResult tr = UTIL_GetGlobalTrace();

	if (flLeft == RC2Pref(RC2P_Glock_GibShieldDamage).f && pOther->pev->takedamage != DAMAGE_NO && !pOther->IsBSPModel() && (iCaps & 1) == 0)
	{
		pOther->TakeDamage(pev, GibAttacker(this), pev->dmg, DMG_ACID | DMG_NEVERGIB);
	}
	else
	{
		if (tr.pHit != pOther->edict())
			return;

		if (pev->impulse > 0)
		{
			PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0.0, pev->origin, g_vecZero, 0, 0,
				UTIL_PackNormal(pev->velocity.Normalize()), 0, pOther->IsBSPModel() ? 1 : 0, 0);
			pev->impulse--;
			return;
		}
	}

	PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0.0, pev->origin, g_vecZero, 0, 0,
		UTIL_PackNormal(pev->velocity.Normalize()), 0, pOther->IsBSPModel() ? 1 : 0, 1);
	SetTouch(NULL);
	UTIL_Remove(this);
}
