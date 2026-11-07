/***
*
*	Rocket Crowbar 2: the crossbow's special bolts, the run of RC2's own code between SDK 2.3's
*	controller.cpp and crossbow.cpp (orig 0x26ee0-0x2ad30). One file for all three, named after
*	what they are (rc2.dll keeps no file names):
*
*	- impailer: a harpoon. It grabs the first living thing it hits (players get a tether, monsters
*	  are pulled through GetPullOrg), bleeds it, and when it then hits a wall it sticks there and
*	  keeps pulling. It explodes when it lets go.
*	- xbow_teather: a tether. Stuck in a wall it pulls the shooter to it; stuck in a living thing it
*	  pulls the two together.
*	- explode_bolt: sticks in what it hits and explodes after a delay.
*
*	The crossbow (p2's crossbow.cpp) creates them by classname, and calls CXBowTeather::DieThink
*	itself (crossbowbolts.h).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "UserMessages.h"
#include "gamerules.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "pm_rc2.h"
#include "shield.h"
#include "crossbowbolts.h"

// what a bolt stuck in something keeps (the three classes do the same): its angles and its
// position in the struck entity's frame, or in the world's
static void StickPosition(entvars_t* pev, CBaseEntity* pOther, const Vector& vecPos, Vector& vecStuckAngles, Vector& vecStuckPos, bool& fToWorld)
{
	if (0 == ENTINDEX(pOther->edict()))
	{
		vecStuckAngles = pev->angles;
		vecStuckPos = vecPos;
		fToWorld = true;
		return;
	}

	vecStuckAngles = pev->angles - pOther->pev->angles;
	UTIL_MakeVectors(pOther->pev->angles);
	const Vector vecLocal = vecPos - pOther->pev->origin;
	vecStuckPos.x = DotProduct(vecLocal, gpGlobals->v_forward);
	vecStuckPos.y = DotProduct(vecLocal, gpGlobals->v_right);
	vecStuckPos.z = DotProduct(vecLocal, gpGlobals->v_up);
	fToWorld = false;
}

// and back: where it is now on its pev->owner
static void FollowStuck(entvars_t* pev, const Vector& vecStuckAngles, const Vector& vecStuckPos, bool fToWorld)
{
	if (fToWorld)
	{
		pev->angles = vecStuckAngles;
		UTIL_SetOrigin(pev, vecStuckPos);
		return;
	}

	entvars_t* pevOwner = VARS(pev->owner); // the original reads through a null owner here
	pev->angles = vecStuckAngles + pevOwner->angles;
	UTIL_MakeVectors(pevOwner->angles);
	UTIL_SetOrigin(pev, gpGlobals->v_forward * vecStuckPos.x + pevOwner->origin + gpGlobals->v_right * vecStuckPos.y + gpGlobals->v_up * vecStuckPos.z);
}

// the stuck bolt's message: CLTE 0x19, bolt, struck entity, position, angles
static void SendStuck(CBaseEntity* pBolt, CBaseEntity* pOther, const Vector& vecStuckPos, const Vector& vecStuckAngles)
{
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x19);
	WRITE_SHORT(ENTINDEX(pBolt->edict()));
	WRITE_SHORT(ENTINDEX(pOther->edict()));
	WRITE_COORD(vecStuckPos.x);
	WRITE_COORD(vecStuckPos.y);
	WRITE_COORD(vecStuckPos.z);
	WRITE_COORD(vecStuckAngles.x);
	WRITE_COORD(vecStuckAngles.y);
	WRITE_COORD(vecStuckAngles.z);
	MESSAGE_END();
}

// CLTE 0xfe: ends the client effect (byte) on an entity
static void SendKillEffect(CBaseEntity* pEntity, int iEffect)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(pEntity->edict()));
	WRITE_BYTE(iEffect);
	MESSAGE_END();
}

static void BoltExplosion(entvars_t* pev, int iScale)
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	// VERIFY: which of the two sprite globals (orig 0x1af670 / 0x1af684) is which; read as the
	// SDK's: the fireball, the underwater one in water
	WRITE_SHORT(UTIL_PointContents(pev->origin) != CONTENTS_WATER ? g_sModelIndexFireball : g_sModelIndexWExplosion);
	WRITE_BYTE(iScale);
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
}

static bool IsLivingTarget(CBaseEntity* pEntity)
{
	return pEntity->IsAlive() && pEntity->pev->takedamage == DAMAGE_AIM && pEntity->pev->deadflag == DEAD_NO;
}

//=========================================================
// CImpailer
//=========================================================
class CImpailer : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool GetPullOrg(CBaseEntity* pPulled, Vector& vecOrigin, float& flForce, float& flOther) override;
	void ReleasePull(CBaseEntity* pPulled) override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x12; } // orig 0x26f80

	void EXPORT AttachTouch(CBaseEntity* pOther);
	void EXPORT PullingThink();
	void EXPORT DieThink();

	// none saved; names ours
	EHANDLE m_hPulled;		// orig +0x2b8: what it holds
	int m_iTetherSlot;		// orig +0x2c0: the held player's tether, -1 for none
	bool m_fStuck;			// orig +0x2c4: stuck in a wall
	bool m_fStuckToWorld;	// orig +0x2c5: in the world itself, not a brush entity
	Vector m_vecStuckAngles; // orig +0x2c8
	Vector m_vecStuckPos;	// orig +0x2d4
};

LINK_ENTITY_TO_CLASS(impailer, CImpailer); // orig 0x26ee0

// orig 0x26f90
void CImpailer::Precache()
{
	PRECACHE_MODEL("models/hvr.mdl");
	PRECACHE_MODEL("sprites/rope.spr");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// orig 0x26fe0
void CImpailer::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/hvr.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("impailer");
	SetThink(&CImpailer::DieThink);
	SetTouch(&CImpailer::AttachTouch);
	pev->nextthink = gpGlobals->time + 30.0;
	pev->takedamage = DAMAGE_NO;
	pev->dmg_inflictor = pev->owner; // the shooter, as owner changes to what it holds
	pev->speed = RANDOM_FLOAT(RC2Pref(RC2P_Crossbow_ImpailerPullStrengthMin).f, RC2Pref(RC2P_Crossbow_ImpailerPullStrengthMax).f);
	pev->max_health = RC2Pref(RC2P_Crossbow_ImpailerPullMinDist).f;
	m_hPulled = NULL;
	m_iTetherSlot = -1;
	pev->effects |= EF_LIGHT;
	m_fStuck = false;
	m_fStuckToWorld = false;
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/rocket1.wav", 1.0, 0.9, 0, 130);

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x17); // its trail
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0x271a0
void CImpailer::AttachTouch(CBaseEntity* pOther)
{
	pev->velocity = pev->movedir;

	if (pOther->edict() == pev->owner)
		return;
	if (pOther->edict() == pev->dmg_inflictor)
	{
		pev->velocity = pev->movedir;
		return;
	}

	if (m_hPulled != NULL && pOther->edict() == m_hPulled->edict())
	{
		pev->owner = pOther->edict();
		return;
	}

	if (m_hPulled == NULL)
	{
		// first hit: grab it
		CBaseEntity* pShooter = NULL;
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pShooter = CBaseEntity::Instance(pev->dmg_inflictor);

		if ((pOther->GetRCCaps(pShooter, this) & 0x8) == 0 || !IsLivingTarget(pOther))
		{
			DieThink();
			return;
		}

		if (pShooter && pShooter->IsPlayer() && !g_pGameRules->PlayerCanAffect(pShooter, pOther))
		{
			DieThink();
			return;
		}

		// VERIFY: the original passes the hit position from its trace copy before it fills it
		// (an uninitialised stack slot); ours is zero
		TraceResult tr = {};
		if (ShieldDeflect(pOther, RC2Pref(RC2P_Crossbow_ImpailerShieldDeflectDamage).f, 1.0, tr.vecEndPos, pev->velocity.Normalize(), NULL, NULL, 8) < 1.0)
		{
			DieThink();
			return;
		}

		m_hPulled = pOther;
		if (pOther->IsPlayer())
		{
			m_iTetherSlot = static_cast<CBasePlayer*>(pOther)->AddTether(entindex(), g_vecZero, pev->speed, pev->max_health);
			if (m_iTetherSlot < 0)
			{
				DieThink();
				return;
			}
		}

		tr = UTIL_GetGlobalTrace();
		entvars_t* pevAttacker = pev;
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pevAttacker = VARS(pev->dmg_inflictor);

		ClearMultiDamage();
		pOther->GetEffectedEnt()->TraceAttack(pevAttacker, RC2Pref(RC2P_Crossbow_ImpailerImpactDamage).f, -tr.vecPlaneNormal, &tr, DMG_NEVERGIB | DMG_SLASH);
		ApplyMultiDamage(pev, pevAttacker);

		if (m_hPulled == NULL || !IsLivingTarget(m_hPulled))
		{
			DieThink();
			return;
		}

		pev->owner = pOther->edict();
		pev->velocity = pev->movedir;
		SetThink(&CImpailer::PullingThink);
		pev->nextthink = gpGlobals->time + 0.01;
		pev->air_finished = gpGlobals->time + 60;
		pev->dmgtime = RC2Pref(RC2P_Crossbow_ImpailerFirstBleedDelay).f + gpGlobals->time;
		UTIL_SetSize(pev, g_vecZero, g_vecZero);

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(0x18); // the impailer in it
		WRITE_SHORT(entindex());
		WRITE_SHORT(ENTINDEX(pOther->edict()));
		WRITE_BYTE(pOther->BloodColor() == -1 ? 0xff : pOther->BloodColor());
		WRITE_COORD(tr.vecEndPos.z - pOther->pev->origin.z);
		MESSAGE_END();

		// the spectators' director: watch these two
		MESSAGE_BEGIN(MSG_SPEC, 50); // svc_hltv; VERIFY: the command byte 3 and long 0x26
		WRITE_BYTE(3);
		WRITE_SHORT(entindex());
		WRITE_SHORT(ENTINDEX(pOther->edict()));
		WRITE_LONG(0x26);
		MESSAGE_END();
		return;
	}

	// holding something and hit something else: only a brush holds it
	if (!pOther->IsBSPModel())
	{
		DieThink();
		return;
	}

	const Vector vecDir = pev->velocity.Normalize();
	TraceResult tr;
	UTIL_TraceLine(pev->origin - vecDir * 32, pev->origin + vecDir * 128, ignore_monsters, ENT(pev), &tr);
	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
	{
		DieThink();
		return;
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/rocket1.wav", 0, 0, SND_STOP, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, RANDOM_LONG(98, 105));

	StickPosition(pev, pOther, tr.vecEndPos + tr.vecPlaneNormal * 12, m_vecStuckAngles, m_vecStuckPos, m_fStuckToWorld);

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;
	pev->health = RC2Pref(RC2P_Crossbow_ImpailerHealth).f;
	UTIL_SetSize(pev, Vector(-12, -12, -12), Vector(12, 12, 12));
	pev->effects = 0;
	pev->owner = pOther->edict();
	m_fStuck = true;
	pev->air_finished = RC2Pref(RC2P_Crossbow_ImpailerLife).f + gpGlobals->time;
	SetTouch(NULL);

	SendKillEffect(this, 0x17);
	SendStuck(this, pOther, m_vecStuckPos, m_vecStuckAngles);
}

// orig 0x27ce0. The pull itself is GetPullOrg's (monsters) and the tether's (players); this
// bleeds the held one and keeps a stuck impailer where it stuck.
void CImpailer::PullingThink()
{
	if (m_hPulled == NULL || !IsLivingTarget(m_hPulled))
	{
		DieThink();
		return;
	}
	if (m_fStuck && !m_fStuckToWorld && !UTIL_IsValidEntity(pev->owner))
	{
		DieThink();
		return;
	}
	if (gpGlobals->time > pev->air_finished)
	{
		DieThink();
		return;
	}
	if (m_hPulled->IsPlayer() && static_cast<CBasePlayer*>((CBaseEntity*)m_hPulled)->m_iRidingIndex != 0)
	{
		DieThink();
		return;
	}

	if (gpGlobals->time > pev->dmgtime)
	{
		entvars_t* pevAttacker = pev;
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pevAttacker = VARS(pev->dmg_inflictor);

		// bleeding doesn't kick the view
		const Vector vecPunch = m_hPulled->pev->punchangle;
		m_hPulled->TakeDamage(UTIL_RC2DamageInflictor("impailer_bleeding"), pevAttacker, RC2Pref(RC2P_Crossbow_ImpailerBleedDamage).f, DMG_NEVERGIB | DMG_SLASH);
		if (m_hPulled == NULL || !IsLivingTarget(m_hPulled))
		{
			DieThink();
			return;
		}
		m_hPulled->pev->punchangle = vecPunch;
		pev->dmgtime = RANDOM_FLOAT(RC2Pref(RC2P_Crossbow_ImpailerBleedMinDelay).f, RC2Pref(RC2P_Crossbow_ImpailerBleedMaxDelay).f) + gpGlobals->time;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	if (!m_fStuck)
	{
		pev->velocity = pev->movedir;
		return;
	}
	FollowStuck(pev, m_vecStuckAngles, m_vecStuckPos, m_fStuckToWorld);
}

// orig 0x28110
void CImpailer::DieThink()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/rocket1.wav", 0, 0, SND_STOP, 100);
	SetThink(NULL);
	SetTouch(NULL);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->dmg = RC2Pref(RC2P_Crossbow_ImpailerExplodeDamage).f;
	pev->owner = pev->dmg_inflictor;

	UTIL_MakeAimVectors(pev->angles);
	TraceResult tr;
	UTIL_TraceLine(pev->origin - gpGlobals->v_forward * 32, pev->origin + gpGlobals->v_forward * 128, dont_ignore_monsters, ENT(pev), &tr);

	if (m_hPulled != NULL && m_hPulled->IsPlayer() && m_iTetherSlot >= 0)
	{
		static_cast<CBasePlayer*>(CBaseEntity::Instance(m_hPulled->edict()))->RemoveTether(m_iTetherSlot);
		m_iTetherSlot = -1;
	}
	m_hPulled = NULL;

	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	UTIL_KillEntityEffects(ENT(pev));

	if (tr.flFraction != 1.0)
		UTIL_SetOrigin(pev, tr.vecEndPos + tr.vecPlaneNormal * 20);

	BoltExplosion(pev, 16);

	entvars_t* pevOwner = pev->owner ? VARS(pev->owner) : NULL;
	pev->owner = NULL;
	RadiusDamage(pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST);

	SetThink(&CImpailer::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

// orig 0x28100
void CImpailer::Killed(entvars_t* pevAttacker, int iGib)
{
	DieThink();
}

// orig 0x28490. A held monster is pulled to the impailer: force pev->speed, down to pev->max_health
bool CImpailer::GetPullOrg(CBaseEntity* pPulled, Vector& vecOrigin, float& flForce, float& flOther)
{
	if (m_hPulled == NULL)
		return false;
	if (pPulled != (CBaseEntity*)m_hPulled)
		return false;
	vecOrigin = pev->origin;
	flForce = pev->speed;
	flOther = pev->max_health;
	return true;
}

// orig 0x28500
void CImpailer::ReleasePull(CBaseEntity* pPulled)
{
	if (m_hPulled != NULL && pPulled->edict() == m_hPulled->edict())
		DieThink();
}

//=========================================================
// CXBowTeather (crossbowbolts.h)
//=========================================================
LINK_ENTITY_TO_CLASS(xbow_teather, CXBowTeather); // orig 0x28550

// orig 0x28650
void CXBowTeather::Precache()
{
	PRECACHE_MODEL("models/headcrab.mdl");
	PRECACHE_MODEL("sprites/rope.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// orig 0x28680
void CXBowTeather::Spawn()
{
	Precache();
	if (!UTIL_IsValidEntity(pev->owner))
	{
		pev->flags |= FL_KILLME;
		return;
	}

	pev->dmg_inflictor = pev->owner;
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->effects = EF_LIGHT;
	SET_MODEL(ENT(pev), "models/headcrab.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("xbow_teather");
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/rocket1.wav", 1.0, 0.9, 0, 150);
	SetThink(&CXBowTeather::DieThink);
	SetTouch(&CXBowTeather::AttachTouch);
	pev->nextthink = gpGlobals->time + 0.25;
	m_fInLiving = false;
	m_fStuckToWorld = false;
	m_fAttached = false;
	m_iShooterTether = -1;
	m_iTargetTether = -1;
	m_flPullDist = RANDOM_FLOAT(RC2Pref(RC2P_Crossbow_TeatherPullDistMin).f, RC2Pref(RC2P_Crossbow_TeatherPullDistMax).f);
	m_flPullStrength = RANDOM_FLOAT(RC2Pref(RC2P_Crossbow_TeatherPullStrengthMin).f, RC2Pref(RC2P_Crossbow_TeatherPullStrengthMax).f);
	m_flZOffset = 0;
	m_flBackPull = m_flPullStrength * RC2Pref(RC2P_Crossbow_TeatherBackPullFraction).f;
	pev->scale = 0.8;

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x17); // its trail
	WRITE_SHORT(entindex());
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x1a); // the rope, from the shooter
	WRITE_SHORT(entindex());
	WRITE_BYTE(ENTINDEX(pev->dmg_inflictor));
	WRITE_SHORT(0);
	WRITE_COORD(0);
	MESSAGE_END();
}

// orig 0x288d0
void CXBowTeather::AttachTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;
	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		DieThink();
		return;
	}

	TraceResult tr;
	Vector vecDir = pev->velocity.Normalize();
	UTIL_TraceLine(pev->origin - vecDir * 32, pev->origin + vecDir * 128, dont_ignore_monsters, ENT(pev), &tr);

	if (ShieldDeflect(pOther, 1.0, 1.0, tr.vecEndPos, pev->velocity.Normalize(), NULL, NULL, 8) < 1.0)
	{
		DieThink();
		return;
	}

	if (pOther->IsBSPModel())
	{
		// lie flat on the surface, facing the way it flew
		const Vector vecAngles = UTIL_VecToAngles(pev->velocity);
		const float flYaw = vecAngles.y * 0.017453292519943295;
		const float flCos = cos(flYaw);
		const float flSin = sin(flYaw);
		const Vector vecNormal = tr.vecPlaneNormal;
		Vector vecA, vecB;
		if (vecNormal.x == 1.0 && vecNormal.y == 0.0 && vecNormal.z == 0.0)
		{
			vecA = Vector(0, 0, -1);
			vecB = Vector(0, 1, 0);
		}
		else if (vecNormal.x == -1.0 && vecNormal.y == 0.0 && vecNormal.z == 0.0)
		{
			vecA = Vector(0, 0, 1);
			vecB = Vector(0, 1, 0);
		}
		else
		{
			vecB = Vector(0, vecNormal.z, -vecNormal.y); // the normal crossed with +x
			vecA = CrossProduct(vecB, vecNormal);
		}
		const Vector vecForward = vecB * flSin + vecA * flCos;
		const Vector vecRight = vecA * flSin - vecB * flCos;
		VectorAnglesWithRoll(vecForward, vecRight, vecNormal, pev->angles);
	}

	if (0 == ENTINDEX(pOther->edict()))
	{
		m_vecStuckAngles = pev->angles;
		m_vecStuckPos = tr.vecPlaneNormal * -1 + tr.vecEndPos;
		UTIL_SetOrigin(pev, m_vecStuckPos);
		m_fStuckToWorld = true;
	}
	else
	{
		m_vecStuckAngles = pev->angles - pOther->pev->angles;
		UTIL_MakeVectors(pOther->pev->angles);
		UTIL_SetOrigin(pev, tr.vecPlaneNormal * -1 + tr.vecEndPos);
		const Vector vecLocal = pev->origin - pOther->pev->origin;
		m_vecStuckPos.x = DotProduct(vecLocal, gpGlobals->v_forward);
		m_vecStuckPos.y = DotProduct(vecLocal, gpGlobals->v_right);
		m_vecStuckPos.z = DotProduct(vecLocal, gpGlobals->v_up);
		m_fStuckToWorld = false;
	}

	// VERIFY: the shooter is used as a player without a check (the crossbow's owner)
	CBasePlayer* pShooter = static_cast<CBasePlayer*>(CBaseEntity::Instance(pev->dmg_inflictor));

	if (pOther->IsBSPModel())
	{
		// in a wall: the shooter is pulled to it
		if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
		{
			DieThink();
			return;
		}
		// Original bug (crash), guarded: a teather from an LCS spawner has the spawner as its
		// shooter, and AddTether writes player members past its end; ours fails the tether
		m_iShooterTether = pShooter->IsPlayer() ? pShooter->AddTether(entindex(), g_vecZero, m_flPullStrength, m_flPullDist) : -1;
		if (m_iShooterTether < 0)
		{
			m_iShooterTether = -1;
			DieThink();
			return;
		}
		pev->movetype = MOVETYPE_NONE;
		pev->solid = SOLID_BBOX;
		pev->takedamage = DAMAGE_YES;
		pev->health = RC2Pref(RC2P_Crossbow_TeatherHealth).f;
		UTIL_SetSize(pev, Vector(-6, -6, -6), Vector(6, 6, 6));
		pev->effects = 0;
		pev->owner = pOther->edict();
	}
	else
	{
		// in something alive: shooter and target are pulled together
		if ((pOther->GetRCCaps(pShooter, this) & 0x8) == 0 || !IsLivingTarget(pOther))
		{
			DieThink();
			return;
		}

		EHANDLE hOther;
		hOther = pOther;
		pOther->TakeDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, RC2Pref(RC2P_Crossbow_TeatherImpactDamage).f, DMG_NEVERGIB | DMG_SLASH);
		if (hOther == NULL || !IsLivingTarget(hOther))
		{
			DieThink();
			return;
		}

		m_fInLiving = true;
		pev->movetype = MOVETYPE_NONE;
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
		pev->effects = 0;
		pev->owner = pOther->edict();
		pev->renderamt = 0;
		pev->rendermode = kRenderTransTexture;

		const float flZOffset = UTIL_GetGlobalTrace().vecEndPos.z - pOther->pev->origin.z;
		// Original bug (crash), guarded: as above, a shooter that isn't a player
		m_iShooterTether = pShooter->IsPlayer() ? pShooter->AddTether(ENTINDEX(pOther->edict()), Vector(0, 0, flZOffset), m_flPullStrength, m_flPullDist) : -1;
		if (m_iShooterTether < 0)
		{
			m_iShooterTether = -1;
			DieThink();
			return;
		}
		if (pOther->IsPlayer())
		{
			m_iTargetTether = static_cast<CBasePlayer*>(pOther)->AddTether(ENTINDEX(pShooter->edict()), g_vecZero, m_flBackPull, m_flPullDist);
			if (m_iTargetTether < 0)
				m_iTargetTether = -1;
		}

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(0x1a); // the rope, now from the shooter to the target
		WRITE_SHORT(entindex());
		WRITE_BYTE(ENTINDEX(pev->dmg_inflictor));
		WRITE_SHORT(ENTINDEX(pev->owner));
		WRITE_COORD(flZOffset);
		MESSAGE_END();
	}

	m_fAttached = true;
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/rocket1.wav", 0, 0, SND_STOP, 100);
	SendKillEffect(this, 0x17);
	SendStuck(this, pOther, m_vecStuckPos, m_vecStuckAngles);
	SetTouch(NULL);
	SetThink(&CXBowTeather::PullingThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x296d0. Until the shooter (or a living target) dies or rides, or they snap apart.
void CXBowTeather::PullingThink()
{
	CBaseEntity* pShooter = NULL;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pShooter = CBaseEntity::Instance(pev->dmg_inflictor);

	bool fKeep = pShooter && IsLivingTarget(pShooter) && static_cast<CBasePlayer*>(pShooter)->m_iRidingIndex == 0;
	if (fKeep && m_fInLiving)
	{
		CBaseEntity* pTarget = CBaseEntity::Instance(pev->owner);
		fKeep = pTarget && IsLivingTarget(pTarget) && (!pTarget->IsPlayer() || static_cast<CBasePlayer*>(pTarget)->m_iRidingIndex == 0);
	}
	if (!fKeep)
	{
		DieThink();
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	FollowStuck(pev, m_vecStuckAngles, m_vecStuckPos, m_fStuckToWorld);

	if ((pev->origin - pShooter->pev->origin).Length() > RC2Pref(RC2P_Crossbow_TeatherSnapDistance).f)
		DieThink();
}

// orig 0x29aa0
void CXBowTeather::DieThink()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/rocket1.wav", 0, 0, SND_STOP, 100);
	SetThink(NULL);
	SetTouch(NULL);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	if (m_iShooterTether != -1 && UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		// VERIFY: the original doesn't check that the shooter is a player
		static_cast<CBasePlayer*>(CBaseEntity::Instance(pev->dmg_inflictor))->RemoveTether(m_iShooterTether);
		m_iShooterTether = -1;
	}
	if (m_iTargetTether != -1 && UTIL_IsValidEntity(pev->owner))
	{
		CBaseEntity* pTarget = CBaseEntity::Instance(pev->owner);
		if (pTarget && pTarget->IsPlayer())
			static_cast<CBasePlayer*>(pTarget)->RemoveTether(m_iTargetTether);
		m_iTargetTether = -1;
	}

	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	BoltExplosion(pev, 12);

	SetThink(&CXBowTeather::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

// orig 0x29c80
void CXBowTeather::Killed(entvars_t* pevAttacker, int iGib)
{
	DieThink();
}

// orig 0x29c90. The shooter is pulled to the tether's end (to the target's origin when it is
// in a living thing); the target, when it's a living one, back to the shooter.
bool CXBowTeather::GetPullOrg(CBaseEntity* pPulled, Vector& vecOrigin, float& flForce, float& flOther)
{
	if (!m_fAttached)
		return false;

	if (UTIL_IsValidEntity(pev->dmg_inflictor) && pev->dmg_inflictor == pPulled->edict())
	{
		if (m_fInLiving && UTIL_IsValidEntity(pev->owner))
		{
			vecOrigin = VARS(pev->owner)->origin;
			vecOrigin.z += m_flZOffset;
		}
		else
			vecOrigin = pev->origin;
		flForce = m_flPullStrength;
		flOther = m_flPullDist;
		return true;
	}

	if (UTIL_IsValidEntity(pev->owner) && pev->owner == pPulled->edict() && m_fInLiving && UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		vecOrigin = VARS(pev->dmg_inflictor)->origin;
		flForce = m_flBackPull;
		flOther = m_flPullDist;
		return true;
	}
	return false;
}

// orig 0x28600: forgets whichever end it is
void CXBowTeather::ReleasePull(CBaseEntity* pPulled)
{
	if (pev->owner == pPulled->edict())
		pev->owner = NULL;
	if (pev->dmg_inflictor == pPulled->edict())
		pev->dmg_inflictor = NULL;
}

//=========================================================
// CExplodeBolt
//=========================================================
class CExplodeBolt : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 2; } // orig 0x29eb0

	void EXPORT EmbedTouch(CBaseEntity* pOther);
	void EXPORT ArmingThink();
	void EXPORT RemoveThink();
	void EXPORT FlyThink();

	// none saved; names ours. pev->dmg_inflictor is the shooter, pev->owner what it's stuck in.
	bool m_fStuckToWorld;	 // orig +0x2b8
	bool m_fInLiving;		 // orig +0x2b9: it hit something alive (it goes off at once when that dies)
	Vector m_vecStuckAngles; // orig +0x2bc
	Vector m_vecStuckPos;	 // orig +0x2c8
};

LINK_ENTITY_TO_CLASS(explode_bolt, CExplodeBolt); // orig 0x29e10

// orig 0x29ec0
void CExplodeBolt::Precache()
{
	PRECACHE_MODEL("models/crossbow_bolt.mdl");
	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("ambience/alienwind2.wav");
}

// orig 0x29ef0
void CExplodeBolt::Spawn()
{
	Precache();
	pev->dmg_inflictor = pev->owner;
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/crossbow_bolt.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("explode_bolt");
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/xbow_fly1.wav", 0.5, ATTN_NORM, 0, 100);
	SetThink(&CExplodeBolt::FlyThink);
	SetTouch(&CExplodeBolt::EmbedTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time + 30;
	pev->scale = 2.0;
	m_fStuckToWorld = false;
	m_fInLiving = false;

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x1d); // its glow
	WRITE_SHORT(entindex());
	MESSAGE_END();
}

// orig 0x2a030
void CExplodeBolt::EmbedTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;
	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		RemoveThink();
		return;
	}

	TraceResult tr = UTIL_GetGlobalTrace();

	// a shield turns it away
	Vector vecHit, vecShieldNormal;
	if (ShieldDeflect(pOther, RC2Pref(RC2P_Crossbow_ExplodeBoltShieldDeflectDmg).f, 1.0, tr.vecEndPos, pev->velocity.Normalize(), &vecHit, &vecShieldNormal, 8) < 10.0)
	{
		const float flSpeed = pev->velocity.Length();
		pev->velocity = UTIL_ReflectVector(pev->velocity.Normalize(), vecShieldNormal) * flSpeed;
		pev->movedir = pev->velocity;
		pev->origin = vecHit + vecShieldNormal * 8;
		UTIL_SetOrigin(pev, pev->origin);
		pev->owner = pOther->edict();
		return;
	}

	if (0 == ENTINDEX(pOther->edict()))
	{
		m_vecStuckAngles = pev->angles;
		m_vecStuckPos = tr.vecPlaneNormal * 4 + tr.vecEndPos;
		UTIL_SetOrigin(pev, m_vecStuckPos);
		m_fStuckToWorld = true;
	}
	else
	{
		m_vecStuckAngles = pev->angles - pOther->pev->angles;
		UTIL_MakeVectors(pOther->pev->angles);
		UTIL_SetOrigin(pev, tr.vecPlaneNormal * 4 + tr.vecEndPos);
		const Vector vecLocal = pev->origin - pOther->pev->origin;
		m_vecStuckPos.x = DotProduct(vecLocal, gpGlobals->v_forward);
		m_vecStuckPos.y = DotProduct(vecLocal, gpGlobals->v_right);
		m_vecStuckPos.z = DotProduct(vecLocal, gpGlobals->v_up);
		m_fStuckToWorld = false;
	}

	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
	{
		RemoveThink();
		return;
	}

	if (IsLivingTarget(pOther))
	{
		EHANDLE hOther;
		hOther = pOther;
		UTIL_BloodDrips(tr.vecEndPos, tr.vecPlaneNormal, pOther->BloodColor(), 20);
		pOther->TakeDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, RC2Pref(RC2P_Crossbow_ExplodeBoltImpactDamage).f, DMG_NEVERGIB | DMG_SLASH);
		if (hOther == NULL || !IsLivingTarget(hOther))
		{
			RemoveThink();
			return;
		}
		m_fInLiving = true;
	}

	CSoundEnt::InsertSound(bits_SOUND_DANGER, tr.vecPlaneNormal * 8 + tr.vecEndPos, 600, 2.0);
	pev->owner = pOther->edict();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->velocity = g_vecZero;
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/xbow_fly1.wav", 0, 0, SND_STOP, 100);

	SendKillEffect(this, 0x1d);

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x1e); // armed
	WRITE_SHORT(entindex());
	MESSAGE_END();

	SendStuck(this, pOther, m_vecStuckPos, m_vecStuckAngles);

	SetTouch(NULL);
	SetThink(&CExplodeBolt::ArmingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = RC2Pref(RC2P_Crossbow_ExplodeBoltDetonateDelay).f + gpGlobals->time;
}

// orig 0x2a8b0
void CExplodeBolt::ArmingThink()
{
	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		RemoveThink();
		return;
	}
	if (!m_fStuckToWorld && !UTIL_IsValidEntity(pev->owner))
	{
		RemoveThink();
		return;
	}

	if (m_fInLiving)
	{
		// what it's in died: now
		CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
		if (!pOwner || !IsLivingTarget(pOwner))
			pev->dmgtime = gpGlobals->time - 1;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	FollowStuck(pev, m_vecStuckAngles, m_vecStuckPos, m_fStuckToWorld);

	if (pev->dmgtime >= gpGlobals->time)
		return;

	pev->dmg = RC2Pref(RC2P_Crossbow_ExplodeBoltDetonateDamage).f;
	UTIL_MakeAimVectors(pev->angles);
	PLAYBACK_EVENT_FULL(0, edict(), g_usExplodeEvent, 0.0, pev->origin, g_vecZero, pev->dmg, 0.0, UTIL_PackNormal(gpGlobals->v_forward), 0, 0, 0);

	entvars_t* pevAttacker = pev;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pevAttacker = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;
	pev->owner = NULL;
	RadiusDamage(pev, pevAttacker, pev->dmg, CLASS_NONE, DMG_BLAST);

	SetThink(&CExplodeBolt::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

// orig 0x2ac80
void CExplodeBolt::RemoveThink()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/xbow_fly1.wav", 0, 0, SND_STOP, 100);
	SetThink(NULL);
	SetTouch(NULL);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
}

// orig 0x2ace0. Flies straight for 30 seconds.
void CExplodeBolt::FlyThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (pev->dmgtime < gpGlobals->time)
	{
		RemoveThink();
		return;
	}
	pev->velocity = pev->movedir;
}

// Compiler-generated pieces of this run (no source of their own): orig 0x27c90 is Vector's unary
// minus and orig 0x27cc0 CBaseEntity::entindex(), both inline in the headers (stock).


// The weapon COMDATs at the end of this run, emitted here for CCrossbow (crossbow.cpp, p2's).
// Their bodies, as read:
// orig 0x2ada0 CBasePlayerItem::ItemPreFrame: empty (stock)
// orig 0x2adb0 CBasePlayerWeapon::AddWeapon: ExtractAmmo(this) (slot 98), return true (stock)
// orig 0x2adc0 CBasePlayerWeapon::UpdateItemInfo: empty (stock)
// orig 0x2add0 CBasePlayerWeapon slot 112, one of RC2's added weapon virtuals: return 0 (the
//   weapons packages name it; BASE_VIRTUALS.md "CBasePlayerWeapon")
// orig 0x2ade0 CBasePlayerWeapon::GetWeaponPtr: return this (stock)
// orig 0x2adf0 CCrossbow::iItemSlot: return 3 (stock)
// orig 0x2ae00 CCrossbow::UseDecrement (slot 113): return true (stock, client weapons)
