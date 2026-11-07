/***
*
*	Rocket Crowbar 2: the hornet gun's freeze grenade (freeze_grenade).
*
*	Sticks to what it hits (a player's shield bounces it off), then goes off after five
*	seconds or as soon as something it can freeze comes within 64 units: damage and a
*	freeze_timer on everything in [HornetGun] GrenadeBlastRadius, falling off with distance.
*
*	In the original this code sits between hornet.cpp and hornetgun.cpp in link order;
*	the file name is ours.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "rc2prefs.h"
#include "shield.h"
#include "UserMessages.h"
#include "freezegrenade.h"

LINK_ENTITY_TO_CLASS(freeze_grenade, CFreezeGrenade); // orig 0x63860

// orig 0x63980
void CFreezeGrenade::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.5;
	pev->friction = 0.3;
	SET_MODEL(ENT(pev), "models/freezegren.mdl");
	pev->skin = 3;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	SetTouch(&CFreezeGrenade::AttachTouch);
	SetThink(&CFreezeGrenade::FlyingThink);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = gpGlobals->time + 5.0;
	pev->angles.z = RANDOM_FLOAT(0, 360);

	pev->rendercolor = Vector(180, 180, 255);
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 24;
	pev->dmg_inflictor = pev->owner;

	// the client's trail on this entity (CLTE type 49)
	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(49);
	WRITE_SHORT(entindex());
	MESSAGE_END();
}

// orig 0x63910
void CFreezeGrenade::Precache()
{
	PRECACHE_MODEL("models/freezegren.mdl");
	PRECACHE_MODEL("sprites/freezedet.spr");
	PRECACHE_MODEL("sprites/gradsmoke.spr");
	PRECACHE_SOUND("weapons/gauss2.wav");
	PRECACHE_SOUND("weapons/grenade_hit1.wav");
	PRECACHE_SOUND("weapons/grenade_hit2.wav");
	PRECACHE_SOUND("weapons/grenade_hit3.wav");
	m_usFreezeDet = PRECACHE_EVENT(1, "events/hgungrenadedet.sc");
}

// orig 0x63af0
void CFreezeGrenade::FlyingThink()
{
	const float flRoll = pev->angles.z;
	const Vector vecAngles = UTIL_VecToAngles(pev->velocity.Normalize());
	pev->angles = Vector(vecAngles.x, vecAngles.y, flRoll + vecAngles.z);
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime < gpGlobals->time)
	{
		Detonate();
		return;
	}

	ApplyExternalForce(0);
}

// The entity the grenade's damage is credited to, as Detonate and AttachedThink find it: the
// thrower kept in dmg_inflictor, when that is still a live entity. Name ours.
static CBaseEntity* FreezeGrenadeAttacker(entvars_t* pev)
{
	if (0 != UTIL_IsValidEntity(pev->dmg_inflictor))
		return CBaseEntity::Instance(pev->dmg_inflictor);
	return NULL;
}

// orig 0x63c20
void CFreezeGrenade::AttachTouch(CBaseEntity* pOther)
{
	// don't hit the thrower (or what it bounced off last)
	if (pOther->edict() == pev->owner)
		return;

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecDir = pev->velocity.Normalize();

	CBaseEntity* pAttacker = FreezeGrenadeAttacker(pev);

	// a player's shield bounces it back, and it becomes the shield owner's grenade
	Vector vecHit, vecNormal;
	if (ShieldDeflect(pOther, 10, 1, tr.vecEndPos, vecDir, &vecHit, &vecNormal, 8) < 10)
	{
		const float flSpeed = pev->velocity.Length();
		pev->velocity = UTIL_ReflectVector(vecDir, vecNormal) * flSpeed;
		pev->origin = vecHit + vecNormal * 2;
		UTIL_SetOrigin(pev, pev->origin);
		pev->owner = pOther->edict();
		return;
	}

	// VERIFY: GetRCCaps' first argument is an entity here (cbase.h types it int)
	if ((pOther->GetRCCaps(pAttacker, this) & 0x200) != 0)
	{
		Detonate();
		return;
	}

	BounceSound();

	m_fOnWorld = ENTINDEX(pOther->edict()) == 0;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->owner = pOther->edict();

	const float flRoll = pev->angles.z;
	if (m_fOnWorld)
	{
		const Vector vecAngles = UTIL_VecToAngles(tr.vecPlaneNormal);
		m_vecAttachAngles = Vector(vecAngles.x, vecAngles.y, flRoll + vecAngles.z);
		m_vecAttachOffset = tr.vecEndPos + tr.vecPlaneNormal * 8;
	}
	else
	{
		// angles and position relative to what it stuck on, in that entity's frame
		const Vector vecAngles = UTIL_VecToAngles(tr.vecPlaneNormal);
		m_vecAttachAngles = Vector(vecAngles.x, vecAngles.y, flRoll + vecAngles.z) - pOther->pev->angles;
		UTIL_MakeVectors(pOther->pev->angles);
		const Vector vecLocal = tr.vecEndPos + tr.vecPlaneNormal * 8 - pOther->pev->origin;
		m_vecAttachOffset.x = DotProduct(vecLocal, gpGlobals->v_forward);
		m_vecAttachOffset.y = DotProduct(vecLocal, gpGlobals->v_right);
		m_vecAttachOffset.z = DotProduct(vecLocal, gpGlobals->v_up);
	}

	// the client sticks its copy on too (CLTE type 25)
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(25);
	WRITE_SHORT(entindex());
	WRITE_SHORT(ENTINDEX(pOther->edict()));
	WRITE_COORD(m_vecAttachOffset.x);
	WRITE_COORD(m_vecAttachOffset.y);
	WRITE_COORD(m_vecAttachOffset.z);
	WRITE_COORD(m_vecAttachAngles.x);
	WRITE_COORD(m_vecAttachAngles.y);
	WRITE_COORD(m_vecAttachAngles.z);
	MESSAGE_END();

	SetThink(&CFreezeGrenade::AttachedThink);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time;
}

// orig 0x64310
void CFreezeGrenade::AttachedThink()
{
	pev->nextthink = gpGlobals->time + 0.05;

	if (pev->dmgtime < gpGlobals->time)
	{
		Detonate();
		return;
	}

	pev->velocity = g_vecZero;

	if (!m_fOnWorld)
	{
		// ride along with what it stuck on. The original reads the owner's entvars without a
		// check (kept): once that entity is removed it follows the freed or reused edict.
		// pev->owner is never NULL here (AttachTouch sets it), so this doesn't crash.
		entvars_t* pevOwner = VARS(pev->owner);
		pev->angles = m_vecAttachAngles + pevOwner->angles;
		UTIL_MakeVectors(pevOwner->angles);
		const Vector vecOrigin = gpGlobals->v_up * m_vecAttachOffset.z + (gpGlobals->v_right * m_vecAttachOffset.y + (gpGlobals->v_forward * m_vecAttachOffset.x + pevOwner->origin));
		UTIL_SetOrigin(pev, vecOrigin);
	}
	else
	{
		pev->angles = m_vecAttachAngles;
		UTIL_SetOrigin(pev, m_vecAttachOffset);
	}

	CBaseEntity* pAttacker = FreezeGrenadeAttacker(pev);

	// go off when something it can freeze comes near and in sight
	UTIL_MakeVectors(pev->angles);
	Vector vecCenter = pev->origin + gpGlobals->v_forward * 8;
	vecCenter = Vector(vecCenter.x, vecCenter.y, vecCenter.z + 1);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecCenter, 64)) != NULL)
	{
		// VERIFY: GetRCCaps' first argument is an entity here (cbase.h types it int)
		if ((pEntity->GetRCCaps(pAttacker, this) & 0x200) != 0 && (pEntity->GetRCCaps2(0, this) & 0x20) == 0)
		{
			TraceResult tr;
			UTIL_TraceLine(vecCenter, pEntity->Center(), ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
			{
				Detonate();
				return;
			}
		}
	}
}

// orig 0x64790
void CFreezeGrenade::Detonate()
{
	UTIL_MakeAimVectors(pev->angles);
	PLAYBACK_EVENT_FULL(0, edict(), m_usFreezeDet, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, UTIL_PackNormal(gpGlobals->v_forward), 0, 0, 0);

	const float flRadius = RC2Pref(RC2P_HornetGun_GrenadeBlastRadius).f;

	// the damage is the thrower's when it is still around, the grenade's own otherwise
	entvars_t* pevAttacker = pev;
	CBaseEntity* pAttacker = NULL;
	if (0 != UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		pAttacker = CBaseEntity::Instance(pev->dmg_inflictor);
		pevAttacker = VARS(pev->dmg_inflictor);
	}

	const Vector vecSrc = pev->origin + gpGlobals->v_forward * 24;

	// each entity the blast lands on (GetEffectedEnt) is hit once
	UTIL_PushEntityMarks();
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRadius)) != NULL)
	{
		CBaseEntity* pTarget = pEntity->GetEffectedEnt();
		if (UTIL_IsEntityMarked(ENTINDEX(pTarget->edict())))
			continue;
		// VERIFY: GetRCCaps' first argument is an entity here (cbase.h types it int)
		if ((pTarget->GetRCCaps(pAttacker, this) & 0x200) == 0)
			continue;
		if (!pTarget->IsAlive() || pTarget->pev->deadflag != DEAD_NO || !(pTarget->pev->health > 0))
			continue;
		if (pAttacker && pAttacker->IsPlayer() && !g_pGameRules->PlayerCanAffect(pAttacker, pTarget))
			continue;

		const Vector vecSpot = pEntity->Center();
		Vector vecDir = vecSpot - vecSrc;
		const float flDist = vecDir.Length();
		if (flDist > flRadius)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSpot, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		if (flDist != 0)
			vecDir = vecDir / flDist;

		float flFrac = 1.0 - pow(flDist / flRadius, RC2Pref(RC2P_HornetGun_GrenadeFalloffExponent).f);
		if (flFrac < 0.0)
			flFrac = 0.0;
		else if (flFrac > 1.0)
			flFrac = 1.0;

		const float flDamage = RC2Pref(RC2P_HornetGun_GrenadeAddDamage).f * flFrac + RC2Pref(RC2P_HornetGun_GrenadeBaseDamage).f;
		const float flFreezeDamage = RC2Pref(RC2P_HornetGun_GrenadeAddFreezeDamage).f * flFrac + RC2Pref(RC2P_HornetGun_GrenadeBaseFreezeDamage).f;
		const float flFreezeTime = RC2Pref(RC2P_HornetGun_GrenadeAddFreezeTime).f * flFrac + RC2Pref(RC2P_HornetGun_GrenadeBaseFreezeTime).f;

		UTIL_MarkEntity(ENTINDEX(pTarget->edict()));
		FreezeDamage(pTarget, pevAttacker, pev, flDamage, flFreezeDamage, flFreezeTime, &tr, vecDir, true);
	}
	UTIL_PopEntityMarks();

	SetTouch(NULL);
	SetThink(NULL);
	UTIL_Remove(this);
}

// orig 0x64be0
void FreezeEntity(CBaseEntity* pEntity, float flTime)
{
	// a player whose linked-entity index (+0x69c) is set can't be frozen
	if (pEntity->IsPlayer() && 0 != static_cast<CBasePlayer*>(pEntity)->m_iRidingIndex)
		return;

	// one freeze_timer per frozen entity; more freezing makes it last longer
	CBaseEntity* pTimer = NULL;
	while ((pTimer = UTIL_FindEntityByClassname(pTimer, "freeze_timer")) != NULL)
	{
		if (pTimer->pev->owner == pEntity->edict())
			break;
	}

	if (!pTimer)
		pTimer = CBaseEntity::Create("freeze_timer", g_vecZero, g_vecZero, pEntity->edict());

	if (pTimer)
	{
		if (pTimer->pev->dmgtime == -1.0f)
			pTimer->pev->dmg += flTime;
		pTimer->pev->dmgtime += flTime;
	}
}

// orig 0x64ca0
void FreezeDamage(CBaseEntity* pEntity, entvars_t* pevAttacker, entvars_t* pevInflictor, float flDamage, float flFreezeDamage, float flFreezeTime, TraceResult* ptr, Vector vecDir, bool bShield)
{
	EHANDLE hEntity;
	hEntity = pEntity;

	CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);
	// VERIFY: GetRCCaps' first argument is an entity here (cbase.h types it int)
	if ((hEntity->GetRCCaps(pAttacker, NULL) & 0x200) == 0)
		return;

	if (hEntity->IsPlayer() && static_cast<CBasePlayer*>(pEntity)->m_bTransforming)
		return;

	bool bWasAlive = hEntity->IsAlive() && hEntity->pev->deadflag == DEAD_NO && hEntity->pev->health > 0;
	if ((hEntity->pev->flags & FL_MONSTER) != 0)
		bWasAlive = bWasAlive && (((CBaseMonster*)GET_PRIVATE(hEntity.Get()))->m_afMemory & bits_MEMORY_KILLED) == 0;

	// a player's shield takes its share ([HornetGun] SharedShieldFraction); what gets through
	// scales the freeze the same way
	const float flFull = flDamage;
	if (bShield)
	{
		flDamage = ShieldDeflect(pEntity, flDamage, RC2Pref(RC2P_HornetGun_SharedShieldFraction).f, ptr ? ptr->vecEndPos : g_vecZero, vecDir, NULL, NULL, 4);
		if (flDamage <= 0)
			return;
	}
	const float flKept = flDamage / flFull;
	flFreezeDamage *= flKept;
	flFreezeTime *= flKept;

	hEntity->TakeDamage(pevInflictor, pevAttacker, flDamage, DMG_FREEZE | DMG_NEVERGIB);

	if (hEntity == NULL)
		return;
	if (0 == UTIL_IsValidEntity(hEntity.Get()))
		return;

	// what was alive must still be; what wasn't (players aside) is frozen whatever its state
	if (bWasAlive || hEntity->IsActivePlayer())
	{
		if (!hEntity->IsAlive() || hEntity->pev->deadflag != DEAD_NO || !(hEntity->pev->health > 0))
			return;
		if ((hEntity->pev->flags & FL_MONSTER) != 0 && (((CBaseMonster*)GET_PRIVATE(hEntity.Get()))->m_afMemory & bits_MEMORY_KILLED) != 0)
			return;
	}

	// frozen when the hit leaves it within the freeze damage
	if (hEntity->pev->health <= flFreezeDamage)
		FreezeEntity(hEntity, flFreezeTime);
}
