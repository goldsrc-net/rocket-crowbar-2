/***
*
*	Rocket Crowbar 2: the egon's shrinker shot and its imploder's homing implode timer.
*	Reconstructed from rc2.dll, the run between the SDK's effects.cpp and egon.cpp
*	(0x3c1b0-0x3cec0); reference/dossiers/p2.md. The file name is ours.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"
#include "rc2prefs.h"
#include "p2shared.h"

// from other files (TEMP STUBs in dlls/rc2/p2stubs_forkC.cpp)
extern int UTIL_PackNormal(Vector vecNormal);	 // orig 0xf5920 (util): a unit vector in an int, 10 bits and a sign per axis
extern entvars_t* UTIL_NamedInflictor(const char* pszName); // orig 0xf58a0 (util): a shared entity whose classname is pszName (the killer weapon's name), or the world

// gmsgCLTE types (names ours)
#define RC2_CLTE_SHRINKER 0x27		 // short entity: the shot's glow
#define RC2_CLTE_IMPLODE 0x2e		 // short entity: the implosion around it
#define RC2_CLTE_IMPLODE_DONE 0x2d	 // (killed with RC2_CLTE_KILL, a second effect)

//=========================================================
// CShrinkerProject: the egon's shrinker shot. What it hits (if it can be shrunk and its shield
// lets it through) gets a shrink_timer, or a longer one.
//
// OVERLOADS SOME ENTVARS:
//   dmg_inflictor     who fired it; owner again on impact
//=========================================================
class CShrinkerProject : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override { return 2; } // orig 0x3c220. VERIFY: bit 2's meaning

	void EXPORT ShrinkTouch(CBaseEntity* pOther);

	void Impact(const Vector& vecNormal, int iVictim); // name ours: the client's impact effect

	unsigned short m_usImpact; // orig +0x70, events/egonshrinkerimpact.sc
};

LINK_ENTITY_TO_CLASS(shrinker_project, CShrinkerProject); // orig 0x3c1b0

// orig 0x3c230
void CShrinkerProject::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	pev->dmg_inflictor = pev->owner;

	SetTouch(&CShrinkerProject::ShrinkTouch);
	SetThink(&CShrinkerProject::SUB_Remove);
	pev->nextthink = RC2Pref(RC2P_Egon_ShrinkerMaxLife).f + gpGlobals->time;

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(RC2_CLTE_SHRINKER);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0x3c350
void CShrinkerProject::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/shrinkf.spr");
	PRECACHE_MODEL("sprites/sqflare.spr");
	PRECACHE_SOUND("ambience/crtnoise.wav");
	UTIL_PrecacheOther("shrink_timer");
	m_usImpact = PRECACHE_EVENT(1, "events/egonshrinkerimpact.sc");
}

void CShrinkerProject::Impact(const Vector& vecNormal, int iVictim)
{
	PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0, g_vecZero, g_vecZero, 0, 0, UTIL_PackNormal(-vecNormal), iVictim, 0, 0);
}

// orig 0x3c3a0
void CShrinkerProject::ShrinkTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	pev->owner = pev->dmg_inflictor;
	SetTouch(NULL);
	SetThink(NULL);
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_KILL);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(RC2_CLTE_SHRINKER);
	MESSAGE_END();

	TraceResult tr = UTIL_GetGlobalTrace();

	CBaseEntity* pAttacker = NULL;
	if (UTIL_IsValidEntity(pev->owner))
		pAttacker = CBaseEntity::Instance(pev->owner);

	// VERIFY: slot 75's first argument is the attacker here (cbase.h has an int)
	if (!pOther->IsBSPModel() && (pOther->GetRCCaps((int)(intptr_t)pAttacker, this) & 0x40) != 0 && pOther->pev->deadflag == DEAD_NO && pOther->pev->health > 0)
	{
		CBasePlayer* pPlayer = pOther->IsPlayer() ? (CBasePlayer*)pOther : NULL;
		if (pPlayer && pPlayer->m_bRC2State10fc)
		{
			Impact(tr.vecPlaneNormal, 0);
			goto remove;
		}

		if (RC2ShieldDamage(pOther, RC2Pref(RC2P_Egon_ShrinkerShieldDamage).f, 1.0, tr.vecEndPos, pev->velocity.Normalize(), NULL, NULL, 10) == 0)
		{
			Impact(tr.vecPlaneNormal, 0);
			goto remove;
		}

		if (pAttacker && pAttacker->IsPlayer() && !g_pGameRules->CanAffect(pAttacker, pOther))
		{
			Impact(tr.vecPlaneNormal, 0);
			goto remove;
		}

		if (pOther->IsPlayer() && ((CBasePlayer*)pOther)->m_bRC2State10fc)
		{
			Impact(tr.vecPlaneNormal, 0);
			goto remove;
		}

		Impact(tr.vecPlaneNormal, ENTINDEX(pOther->edict()));

		// its shrink timer, or a new one
		CBaseEntity* pTimer;
		for (pTimer = UTIL_FindEntityByClassname(NULL, "shrink_timer"); pTimer; pTimer = UTIL_FindEntityByClassname(pTimer, "shrink_timer"))
		{
			if (pTimer->pev->owner == pOther->edict())
				break;
		}
		if (!pTimer)
			pTimer = CBaseEntity::Create("shrink_timer", g_vecZero, g_vecZero, pOther->edict());

		if (pTimer)
		{
			pTimer->pev->owner = pOther->edict();
			// -1 marks the egon's shrink-yourself timer, which this leaves alone
			if (pTimer->pev->dmg != -1)
				pTimer->pev->dmg = RC2Pref(RC2P_Egon_ShrinkerShrinkTime).f;
			if (pTimer->pev->dmgtime != -1)
			{
				float flEnd = pTimer->pev->dmg + gpGlobals->time;
				if (pTimer->pev->dmgtime < flEnd)
					pTimer->pev->dmgtime = flEnd;
			}
		}
	}
	else
		Impact(tr.vecPlaneNormal, 0);

remove:
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CShrinkerProject::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.01;
}


//=========================================================
// CImplodeTimer: the egon's charged imploder. Invisible, it homes on its target (owner) and
// once close shrinks it to nothing over a second and kills it. dmg_inflictor is who fired it.
//
// OVERLOADS SOME ENTVARS:
//   dmgtime       homing: when it gives up; imploding: when the target dies
//   dmg           how close it must get
//   speed         its speed
//   armorvalue    its turn rate
//=========================================================
class CImplodeTimer : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT PreImplodeThink();
	void EXPORT BeginImplode();
	void EXPORT ImplodeEnt();

	bool TargetGone(); // name ours

	Vector m_vecMins;	  // orig +0x70, name ours: the target's size and eyes before it shrinks
	Vector m_vecMaxs;	  // orig +0x7c, name ours
	Vector m_vecViewOfs;  // orig +0x88, name ours
	float m_flLastThink;  // orig +0x94, name ours
};

LINK_ENTITY_TO_CLASS(implode_timer, CImplodeTimer); // orig 0x3c850

// orig 0x3c8c0
void CImplodeTimer::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);

	SetThink(&CImplodeTimer::PreImplodeThink);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Egon_ImploderTrackingLife).f + gpGlobals->time;
	pev->velocity = g_vecZero;
	m_flLastThink = gpGlobals->time;
	pev->speed = RC2Pref(RC2P_Egon_ImploderTrackSpeed).f;
	pev->dmg = RC2Pref(RC2P_Egon_ImploderImplodeHitDist).f;
	pev->armorvalue = RC2Pref(RC2P_Egon_ImploderTrackAccel).f;
}

// orig 0x3c990
void CImplodeTimer::Precache()
{
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_MODEL("sprites/glow01.spr");
	PRECACHE_MODEL("sprites/sqflare.spr");
}

// orig 0x3c9c0: remove this when the target is gone or dead
bool CImplodeTimer::TargetGone()
{
	if (UTIL_IsValidEntity(pev->owner))
	{
		entvars_t* pevTarget = VARS(pev->owner);
		if (pevTarget->deadflag == DEAD_NO && pevTarget->health > 0)
			return false;
	}
	UTIL_Remove(this);
	return true;
}

// orig 0x3ca30: home on the target
void CImplodeTimer::PreImplodeThink()
{
	if (TargetGone())
		return;

	if (pev->dmgtime < gpGlobals->time)
	{
		UTIL_Remove(this);
		return;
	}

	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.01;

	CBaseEntity* pTarget = CBaseEntity::Instance(pev->owner);
	Vector vecTo = pTarget->Center() - pev->origin;
	float flDist = vecTo.Length();

	if (flDist >= pev->dmg)
	{
		// turn towards it at armorvalue, at a constant speed
		Vector vecDir = vecTo / flDist;
		pev->velocity = pev->velocity + (vecDir * pev->armorvalue) * flInterval; // VERIFY: the turn's scale by the interval, read off a register the decompile lost
		pev->velocity = pev->velocity.Normalize() * pev->speed;
		pev->origin = pev->origin + pev->velocity * flInterval;
		return;
	}

	SetThink(&CImplodeTimer::BeginImplode);
	pev->nextthink = gpGlobals->time;
}

// orig 0x3cd30: caught it; remember its size and start the implosion
void CImplodeTimer::BeginImplode()
{
	if (TargetGone())
		return;

	CBaseEntity* pTarget = CBaseEntity::Instance(pev->owner);
	(void)pTarget->IsPlayer(); // the original asks and drops the answer

	// a monster forgets its enemies
	if ((pTarget->pev->flags & FL_MONSTER) != 0)
	{
		CBaseMonster* pMonster = (CBaseMonster*)pTarget;
		pMonster->m_hEnemy = NULL;
		for (int i = 0; i < MAX_OLD_ENEMIES; i++)
			pMonster->m_hOldEnemy[i] = NULL;
	}

	m_vecMins = pTarget->pev->mins;
	m_vecMaxs = pTarget->pev->maxs;
	m_vecViewOfs = pTarget->pev->view_ofs;

	Vector vecOrigin = VARS(pev->owner)->origin + Vector(0, 0, 1);
	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, vecOrigin);
	WRITE_BYTE(RC2_CLTE_IMPLODE);
	WRITE_SHORT(ENTINDEX(pev->owner));
	MESSAGE_END();

	SetThink(&CImplodeTimer::ImplodeEnt);
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = gpGlobals->time + 1.0;
}

// orig 0x3cec0: shrink the target, then kill it
void CImplodeTimer::ImplodeEnt()
{
	if (TargetGone())
		return;

	CBaseEntity* pTarget = CBaseEntity::Instance(pev->owner);

	float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;

	pTarget->pev->scale -= flInterval;
	if (pTarget->pev->scale < 0.05)
		pTarget->pev->scale = 0.05;

	pTarget->pev->maxs = m_vecMaxs * pTarget->pev->scale;
	pTarget->pev->mins = m_vecMins * pTarget->pev->scale;
	pTarget->pev->view_ofs = m_vecViewOfs * pTarget->pev->scale;
	if (pTarget->IsPlayer())
		pTarget->pev->view_ofs.z += pTarget->pev->scale * 36.0;

	// keep it on the floor as it shrinks
	if ((pTarget->pev->flags & FL_ONGROUND) != 0)
	{
		Vector vecEnd(pTarget->pev->origin.x, pTarget->pev->origin.y, (pTarget->pev->mins.z - 8) + pTarget->pev->origin.z);
		Vector vecStart(pTarget->pev->origin.x, pTarget->pev->origin.y, pTarget->pev->mins.z + pTarget->pev->origin.z);
		TraceResult tr;
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pTarget->edict(), &tr);
		if (tr.flFraction < 1.0)
			UTIL_SetOrigin(pTarget->pev, Vector(tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z + -pTarget->pev->mins.z));
	}

	UTIL_SetSize(pTarget->pev, pTarget->pev->mins, pTarget->pev->maxs);

	if (pev->dmgtime <= gpGlobals->time)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(RC2_CLTE_KILL);
		WRITE_SHORT(ENTINDEX(pev->owner));
		WRITE_BYTE(RC2_CLTE_IMPLODE);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(RC2_CLTE_KILL);
		WRITE_SHORT(ENTINDEX(pev->owner));
		WRITE_BYTE(RC2_CLTE_IMPLODE_DONE);
		MESSAGE_END();

		entvars_t* pevAttacker;
		if (!UTIL_IsValidEntity(pev->dmg_inflictor))
			pevAttacker = pev;
		else
			pevAttacker = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;

		pTarget->TakeDamage(UTIL_NamedInflictor("imploder"), pevAttacker, 50000, DMG_ALWAYSGIB | DMG_ENERGYBEAM);
		UTIL_Remove(this);
	}
}
