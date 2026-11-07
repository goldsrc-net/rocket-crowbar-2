/***
*
*	Rocket Crowbar 2: the egon's nuclear missile (nuclear_missile, CNuke).
*
*	The player who fires it steers it with his view (the missile turns its velocity toward
*	his aim), sees through it, and can't fire anything else while it flies. After its first
*	second, pressing attack again detonates it; it also goes off on touch, or when its
*	controller dies, freezes or leaves. The blast is client-side (events/egonnukeexplode.sc);
*	the damage reaches every entity within the radius through up to 256 units of wall.
*
*	The original's functions between nodes.cpp and osprey.cpp, 0x8b380-0x8c600.
*
****/

#include <algorithm>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "client.h"
#include "rc2prefs.h"
#include "UserMessages.h"

// From elsewhere in rc2.dll (see the TEMP STUB block at the bottom)
extern void UTIL_PushEntityMarks();				   // name ours; util, orig 0xf4c70
extern bool UTIL_EntityMarked(int iEntIndex);	   // name ours; util, orig 0xf4dd0
extern void UTIL_MarkEntity(int iEntIndex);		   // name ours; util, orig 0xf4d90
extern void UTIL_PopEntityMarks();				   // name ours; util, orig 0xf4d40
extern void UTIL_DelayAttack(float* pflNextAttack, float flBase, float flDelay); // name ours; util, orig 0xf5900
extern Vector UTIL_Accelerate(const Vector& vecVelocity, const Vector& vecWishDir, float flWishSpeed, float flAccel, float flFrameTime); // name ours; util, orig 0xf50a0
extern int UTIL_PackDirection(const Vector& vecDir); // name ours; util, orig 0xf5920
extern float ShieldAbsorbDamage(CBaseEntity* pEntity, float flDamage, float flShieldFract, Vector vecSrc, Vector vecDir, Vector* pvecHit, Vector* pvecHitDir, int iBits); // name ours; orig 0xc45b0

class CNuke : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override { return 2; } // orig 0x8b3f0

	void EXPORT NukeThink();
	void EXPORT NukeDetonateTouch(CBaseEntity* pOther);

	// RC 1.9's names (CNukeM in its nm.cpp does the same two things)
	bool PunchTrace(Vector vecStart, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iDepth);
	void NukeRadus();

	// Not saved: the original has no save table for CNuke (its Save/Restore are CBaseEntity's).
	// Its own state rides in entvars: pev->dmg_inflictor is the player steering it (pev->owner
	// is the launcher until the missile is 100 units clear), pev->dmgtime the end of the first
	// second, pev->speed and pev->armorvalue the top speed and the acceleration.
	float m_flLastThink;			// orig +0x70, name ours
	unsigned short m_usNukeExplode; // orig +0x74, name ours
};

LINK_ENTITY_TO_CLASS(nuclear_missile, CNuke); // orig 0x8b380

// orig 0x8b400
void CNuke::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/alt_nuke.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	SetTouch(&CNuke::NukeDetonateTouch);
	SetThink(&CNuke::NukeThink);
	pev->nextthink = gpGlobals->time;
	m_flLastThink = gpGlobals->time;

	pev->scale = 1.3;
	pev->dmg_inflictor = pev->owner;
	pev->dmgtime = gpGlobals->time + 1;
	pev->speed = RC2Pref(RC2P_Egon_NukeFlySpeed).f;
	pev->armorvalue = RC2Pref(RC2P_Egon_NukeFlyAccel).f;

	// the controller looks through the missile
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		unsigned char *pvs, *pas;
		SetupVisibility(edict(), pev->dmg_inflictor, &pvs, &pas); // VERIFY: the original passes the same pointer for pvs and pas
		SET_VIEW(pev->dmg_inflictor, edict());

		MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, pev->dmg_inflictor);
		WRITE_BYTE(1);
		MESSAGE_END();
	}

	// the client's trail on this entity, 42 its type; the last byte the launcher
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(42);
	WRITE_SHORT(entindex());
	if (UTIL_IsValidEntity(pev->owner))
		WRITE_BYTE(ENTINDEX(pev->owner));
	else
		WRITE_BYTE(0);
	MESSAGE_END();
}

// orig 0x8b610
void CNuke::Precache()
{
	PRECACHE_MODEL("models/hvr.mdl");
	PRECACHE_MODEL("models/alt_nuke.mdl");
	PRECACHE_MODEL("sprites/bigflare1.spr");
	PRECACHE_MODEL("sprites/animglow01.spr");
	PRECACHE_MODEL("sprites/nukef1.spr");
	PRECACHE_MODEL("sprites/fexplo.spr");
	PRECACHE_MODEL("sprites/noise2.spr");
	PRECACHE_SOUND("weapons/nuker.wav");
	PRECACHE_SOUND("weapons/mortarhit.wav");
	m_usNukeExplode = PRECACHE_EVENT(1, "events/egonnukeexplode.sc");
}

// orig 0x8b690
void CNuke::NukeThink()
{
	CBaseEntity* pPlayer = NULL;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pPlayer = CBaseEntity::Instance(pev->dmg_inflictor);

	// detonate when the controller is gone, dead, frozen, or presses attack after the first second
	if (!pPlayer || pPlayer->pev->deadflag != DEAD_NO || !pPlayer->IsAlive() || pPlayer->RC2Frozen() || (gpGlobals->time > pev->dmgtime && (pPlayer->pev->button & IN_ATTACK) != 0))
	{
		NukeDetonateTouch(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.05;

	// the launcher stops being ignored once the missile is clear of him
	if (pev->owner)
	{
		if (!UTIL_IsValidEntity(pev->owner))
			pev->owner = NULL;

		if (pev->owner && (VARS(pev->owner)->origin - pev->origin).Length() > 100)
			pev->owner = NULL;
	}

	// the controller can't fire anything else while he steers. VERIFY: the original writes
	// CBaseMonster's m_flNextAttack of whatever entity owns the missile without asking.
	CBaseMonster* pSteerer = static_cast<CBaseMonster*>(pPlayer);
	UTIL_DelayAttack(&pSteerer->m_flNextAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_NukeAnythingNextFireTime).f);

	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;

	UTIL_MakeVectors(pPlayer->pev->v_angle);
	Vector vecDir = gpGlobals->v_forward;

	// more than a right angle from where it flies: turn through the half-way direction
	Vector vecFlight = pev->velocity.Normalize();
	float flDot = DotProduct(vecFlight, vecDir);
	if (flDot < 0)
	{
		Vector vecHalf;
		if (flDot == -1)
			vecHalf = Vector(0, 0, 1);
		else
			vecHalf = vecFlight + vecDir;

		vecDir = (vecHalf.Normalize() + vecDir).Normalize();
	}

	pev->velocity = pev->velocity + vecDir * pev->armorvalue * flInterval;

	// always at full speed (the original scales up as well as down)
	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0)
		pev->velocity = pev->velocity / flSpeed * pev->speed;
	else
		pev->velocity = vecDir * pev->speed;

	pev->angles.y = pPlayer->pev->v_angle.y;
	pev->angles.x = -pPlayer->pev->v_angle.x;

	pev->velocity = UTIL_Accelerate(pev->velocity, vecDir, pev->speed, pev->armorvalue, flInterval);
}

// orig 0x8bd80. A line from vecStart to vecEnd that may go through pflPunch units of solid in
// all (in 8-unit steps) and through anything that isn't a brush model; true when it reaches
// pentTarget or the end.
bool CNuke::PunchTrace(Vector vecStart, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iDepth)
{
	if (iDepth < 0)
		return false;
	iDepth--;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pentIgnore, ptr);

	if (ptr->flFraction >= 1.0 || ptr->pHit == pentTarget)
		return true;

	if (ptr->pHit)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(ptr->pHit);
		if (pHit && !pHit->IsBSPModel())
			return PunchTrace(ptr->vecEndPos, vecEnd, vecDir, pflPunch, ptr->pHit, pentTarget, ptr, iDepth);
	}

	Vector vecPos = ptr->vecEndPos;

	if (*pflPunch <= 0)
		return false;

	do
	{
		float flStep = 8;
		if (*pflPunch < 8)
			flStep = *pflPunch;
		*pflPunch -= flStep;

		vecPos = vecPos + vecDir * flStep;

		if (UTIL_PointContents(vecPos) != CONTENTS_SOLID)
			return PunchTrace(vecPos, vecEnd, vecDir, pflPunch, ptr->pHit, pentTarget, ptr, iDepth);
	} while (*pflPunch > 0);

	return false;
}

// orig 0x8bfb0. The blast's damage: every entity that takes damage within the radius, once
// (through GetEffectedEnt), by distance and by how much wall the blast had to go through; a
// player's shield takes its share first.
void CNuke::NukeRadus()
{
	Vector vecSrc = pev->origin;
	const float flPunchDist = RC2Pref(RC2P_Egon_NukeExplodePunchDist).f;
	const float flRadius = RC2Pref(RC2P_Egon_NukeExplodeRadius).f;
	const float flMaxDamage = RC2Pref(RC2P_Egon_NukeExplodeMaxDamage).f;

	entvars_t* pevAttacker = pev;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pevAttacker = VARS(pev->dmg_inflictor);

	UTIL_PushEntityMarks();

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent) || pent == edict())
			continue;

		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity || pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		if (UTIL_EntityMarked(ENTINDEX(pEntity->GetEffectedEnt()->edict())))
			continue;

		Vector vecTarget = pEntity->Center();
		Vector vecDelta = vecTarget - vecSrc;
		float flDist = vecDelta.Length();
		if (flDist > flRadius)
			continue;

		float flPunch = flPunchDist;
		if (flDist > 0)
		{
			Vector vecDir = vecDelta / flDist;
			TraceResult tr;
			if (!PunchTrace(vecSrc, vecTarget, vecDir, &flPunch, edict(), pent, &tr, 32))
				continue;
		}

		UTIL_MarkEntity(ENTINDEX(pEntity->GetEffectedEnt()->edict()));

		float flPunchFrac = flPunch / flPunchDist;
		float flDistFrac = 1.0 - flDist / flRadius;
		flPunchFrac = std::clamp(flPunchFrac, 0.0f, 1.0f);
		flDistFrac = std::clamp(flDistFrac, 0.0f, 1.0f);

		const float flFract = RC2Pref(RC2P_Egon_NukeExplodeDistDamageFract).f;
		float flDamage = (flFract * flDistFrac + (1.0 - flFract) * flPunchFrac) * flMaxDamage;

		flDamage = ShieldAbsorbDamage(pEntity, flDamage, RC2Pref(RC2P_Egon_NukeExplodeShieldDamageFract).f, g_vecZero, g_vecZero, NULL, NULL, 15);
		if (flDamage > 0)
			pEntity->TakeDamage(pev, pevAttacker, flDamage, DMG_BLAST | DMG_RADIATION);
	}

	UTIL_PopEntityMarks();
}

// orig 0x8c380. Also called by NukeThink with this as pOther, which plays the blast where the
// missile is instead of off the surface it hit.
void CNuke::NukeDetonateTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	// give the controller his view back
	int iController = 0;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		unsigned char *pvs, *pas;
		SetupVisibility(pev->dmg_inflictor, pev->dmg_inflictor, &pvs, &pas); // VERIFY: as in Spawn
		SET_VIEW(pev->dmg_inflictor, pev->dmg_inflictor);
		iController = ENTINDEX(pev->dmg_inflictor);

		MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, pev->dmg_inflictor);
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	SetTouch(NULL);
	SetThink(NULL);
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	// end the client's trail (254 ends the effect of the type that follows)
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(254);
	WRITE_SHORT(entindex());
	WRITE_BYTE(42);
	MESSAGE_END();

	TraceResult tr = UTIL_GetGlobalTrace();

	if (pOther->edict() == edict())
	{
		PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), m_usNukeExplode, 0, pev->origin, g_vecZero, 0, 0, 0, iController, 1, 0);
	}
	else
	{
		UTIL_SetOrigin(pev, tr.vecEndPos + tr.vecPlaneNormal * 16);
		PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), m_usNukeExplode, 0, pev->origin, g_vecZero, 0, 0, UTIL_PackDirection(-tr.vecPlaneNormal), iController, 0, 0);
	}

	NukeRadus();
	UTIL_Remove(this);
}

// TEMP STUB (p5): real ones belong in util.cpp (0xf4c70, 0xf4dd0, 0xf4d90, 0xf4d40, 0xf5900,
// 0xf50a0, 0xf5920). Bodies follow the original's reading so the nuke works until then.
static std::vector<std::vector<unsigned char>> s_EntityMarks;

void UTIL_PushEntityMarks()
{
	s_EntityMarks.emplace_back((gpGlobals->maxEntities + 7) / 8, 0);
}

bool UTIL_EntityMarked(int iEntIndex)
{
	if (s_EntityMarks.empty() || iEntIndex < 0 || iEntIndex / 8 >= (int)s_EntityMarks.back().size())
		return false;
	return (s_EntityMarks.back()[iEntIndex / 8] & (1 << (iEntIndex % 8))) != 0;
}

void UTIL_MarkEntity(int iEntIndex)
{
	if (s_EntityMarks.empty() || iEntIndex < 0 || iEntIndex / 8 >= (int)s_EntityMarks.back().size())
		return;
	s_EntityMarks.back()[iEntIndex / 8] |= 1 << (iEntIndex % 8);
}

void UTIL_PopEntityMarks()
{
	if (!s_EntityMarks.empty())
		s_EntityMarks.pop_back();
}

void UTIL_DelayAttack(float* pflNextAttack, float flBase, float flDelay)
{
	if (*pflNextAttack < flBase + flDelay)
		*pflNextAttack = flBase + flDelay;
}

Vector UTIL_Accelerate(const Vector& vecVelocity, const Vector& vecWishDir, float flWishSpeed, float flAccel, float flFrameTime)
{
	float flAddSpeed = flWishSpeed - DotProduct(vecVelocity, vecWishDir);
	if (flAddSpeed <= 0)
		return vecVelocity;
	float flAccelSpeed = flWishSpeed * flAccel * flFrameTime;
	if (flAccelSpeed > flAddSpeed)
		flAccelSpeed = flAddSpeed;
	return vecVelocity + vecWishDir * flAccelSpeed;
}

int UTIL_PackDirection(const Vector& vecDir)
{
	Vector v = vecDir.Normalize();
	int x = (int)(fabs(v.x) * 511.0);
	int y = (int)(fabs(v.y) * 511.0);
	int z = (int)(fabs(v.z) * 511.0);
	if (v.x < 0)
		x |= 0x200;
	if (v.y < 0)
		y |= 0x200;
	if (v.z < 0)
		z |= 0x200;
	return x | (y << 10) | (z << 20);
}

// TEMP STUB (p5): real one belongs in the shield's file (orig 0xc45b0, p7's range): a player's
// shield takes its share of the damage and the rest comes back.
float ShieldAbsorbDamage(CBaseEntity* pEntity, float flDamage, float flShieldFract, Vector vecSrc, Vector vecDir, Vector* pvecHit, Vector* pvecHitDir, int iBits)
{
	return flDamage;
}
