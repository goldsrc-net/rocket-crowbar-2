/***
*
*	Rocket Crowbar 2: ultima_grenade. The hand grenade precaches it but never throws it (its
*	quaternary attack is empty); whatever spawns one gets this.
*
*	It bounces for [Grenade] SharedMaxTimer seconds, rises off the ground in its last 0.75 s,
*	then arms: it hangs in the air for UltimaGrenArmTime seconds, shootable (1 health) and
*	drawn by the client from RC2's CLTE message (type 55), and explodes in a blast of
*	UltimaGrenBlastDamage at every damageable entity in the level that a line from it reaches,
*	punching through up to UltimaGrenBlastMaxPunch units of solid. Killed or touched while
*	armed, or armed while another one is, it aborts instead: a small blast of
*	UltimaGrenAbortBlastDamage (and UltimaGrenAbortTouchDamage to what touched it). Arming one
*	while another is armed aborts both.
*
*	pev->impulse is 1 until it arms.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "rc2grenades.h"

extern unsigned short g_usExplodeEvent; // weapons.cpp

LINK_ENTITY_TO_CLASS(ultima_grenade, CUltimaGrenade); // orig 0x5ac10

// orig 0x5ac90: 2 while it hangs in the air (MOVETYPE_FLY), 0 before
int CUltimaGrenade::GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker)
{
	return pev->movetype == MOVETYPE_FLY ? 2 : 0;
}

// orig 0x5acc0
void CUltimaGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("sprites/animglow01.spr");
	PRECACHE_MODEL("sprites/xbeam1.spr");
	PRECACHE_MODEL("sprites/glow02.spr");
	PRECACHE_MODEL("sprites/glow03.spr");
	PRECACHE_SOUND("weapons/grenade_hit1.wav");
	PRECACHE_SOUND("weapons/grenade_hit2.wav");
	PRECACHE_SOUND("weapons/grenade_hit3.wav");
	PRECACHE_SOUND("ambience/alien_minddrill.wav");
	m_usDetonate = PRECACHE_EVENT(1, "events/ultgrendet.sc");
}

// orig 0x5ad40
void CUltimaGrenade::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->classname = MAKE_STRING("ultima_grenade");
	pev->dmg_inflictor = pev->owner;

	SetTouch(&CGrenade::BounceTouch);
	SetThink(&CUltimaGrenade::PreDetonate);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Grenade_SharedMaxTimer).f + gpGlobals->time;

	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector(0, 0, 127);
	pev->renderamt = 16;

	pev->sequence = RANDOM_LONG(3, 6);
	ResetSequenceInfo();
	pev->framerate = 1.0;
	pev->gravity = RC2Pref(RC2P_Grenade_UltimaGrenGravityScaler).f;
	pev->friction = RC2Pref(RC2P_Grenade_UltimaGrenFrictionScaler).f;
	pev->impulse = 1; // not armed yet
	m_fRegisteredSound = false;
}

// orig 0x5aeb0
void CUltimaGrenade::PreDetonate()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (pev->dmgtime - 2.0 < gpGlobals->time)
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 1024, 0.1);

	if (pev->dmgtime - 0.75 < gpGlobals->time && (pev->flags & FL_ONGROUND) != 0)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector(0, 0, RC2Pref(RC2P_Grenade_UltimaGrenArmRiseHeight).f * 1.3333334f);
		pev->flags &= ~FL_ONGROUND;
		pev->origin.z += 1.0;
		UTIL_SetOrigin(pev, pev->origin);
		SetTouch(NULL);
		pev->sequence = RANDOM_LONG(3, 5);
		ResetSequenceInfo();
		pev->framerate = 3.0;
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		Arm();
		return;
	}

	if (pev->waterlevel != 0 && pev->movetype != MOVETYPE_FLY)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

// orig 0x5b0d0
void CUltimaGrenade::Arm()
{
	bool bAborted = false;

	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_FLY;
	SetTouch(&CUltimaGrenade::KillTouch);
	SetThink(&CUltimaGrenade::DetonateWarmup);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Grenade_UltimaGrenArmTime).f + gpGlobals->time;
	pev->owner = NULL;

	// only one may be armed at a time: arming next to an armed one aborts them all
	CBaseEntity* pOther = NULL;
	while ((pOther = UTIL_FindEntityByClassname(pOther, "ultima_grenade")) != NULL)
	{
		if (pOther->edict() != edict() && pOther->pev->impulse == 0)
		{
			static_cast<CUltimaGrenade*>(pOther)->Abort();
			bAborted = true;
		}
	}
	if (bAborted)
	{
		Abort();
		return;
	}

	// keep 18 units clear of floor and ceiling
	TraceResult trDown, trUp;
	UTIL_TraceLine(pev->origin, Vector(pev->origin.x, pev->origin.y, pev->origin.z - 18.0), ignore_monsters, ENT(pev), &trDown);
	UTIL_TraceLine(pev->origin, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 18.0), ignore_monsters, ENT(pev), &trUp);
	if (trDown.flFraction < 1.0 && trUp.flFraction < 1.0)
		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, (1.0 - trDown.flFraction) * 18.0) - Vector(0, 0, (1.0 - trUp.flFraction) * 18.0));
	else if (trDown.flFraction < 1.0)
		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, (1.0 - trDown.flFraction) * 18.0));
	else if (trUp.flFraction < 1.0)
		UTIL_SetOrigin(pev, pev->origin - Vector(0, 0, (1.0 - trUp.flFraction) * 18.0));

	pev->impulse = 0;
	pev->solid = SOLID_TRIGGER;
	pev->takedamage = DAMAGE_YES;
	pev->health = 1;
	UTIL_SetSize(pev, Vector(-12, -12, -12), Vector(12, 12, 12));
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 1024, 10.0);

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(55); // the ultima grenade's effect
	WRITE_SHORT(ENTINDEX(ENT(pev)));
	MESSAGE_END();
}

// orig 0x5b560: the small blast instead of the big one
void CUltimaGrenade::Abort()
{
	pev->dmg = RC2Pref(RC2P_Grenade_UltimaGrenAbortBlastDamage).f;
	pev->takedamage = DAMAGE_NO;
	if (pev->impulse != 0)
		UTIL_SetOrigin(pev, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 32.0));

	PLAYBACK_EVENT_FULL(0, edict(), g_usExplodeEvent, 0, pev->origin, g_vecZero, pev->dmg, 0, UTIL_PackNormal(Vector(0, 0, -1)), 0, 0, 0);
	pev->owner = NULL;
	RadiusDamage(pev, RC2GrenadeAttacker(pev), pev->dmg, CLASS_NONE, DMG_BLAST);

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(254); // end the effect
	WRITE_SHORT(ENTINDEX(ENT(pev)));
	WRITE_BYTE(55);
	MESSAGE_END();

	SetThink(&CBaseEntity::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

// orig 0x5b710: killed once armed (it only takes damage then), it aborts
void CUltimaGrenade::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->impulse == 0)
		Abort();
}

// orig 0x5b730: anything but the world touching it while it is armed hurts that and aborts
void CUltimaGrenade::KillTouch(CBaseEntity* pOther)
{
	if ((pev->effects & EF_NODRAW) != 0)
		return;
	if (pOther->IsBSPModel())
		return;

	if (pOther->pev->takedamage != DAMAGE_NO)
		pOther->TakeDamage(pev, RC2GrenadeAttacker(pev), RC2Pref(RC2P_Grenade_UltimaGrenAbortTouchDamage).f, DMG_ENERGYBEAM);
	DispatchTouch(pOther->edict(), edict());
	Abort();
}

// orig 0x5b800
void CUltimaGrenade::DetonateWarmup()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (pev->dmgtime < gpGlobals->time)
		DetonateExplode();
}

// orig 0x5b840: does a line from vecStart along vecDir reach pentTarget at vecEnd? Monsters and
// other non-BSP entities in the way are passed through; solid is punched through 8 units at a
// time while *pflPunch lasts. iDepth bounds the recursion.
bool CUltimaGrenade::BlastLineClear(Vector vecStart, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iDepth)
{
	if (iDepth < 0)
		return false;
	iDepth--;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pentIgnore, ptr);
	if (ptr->flFraction >= 1.0 || ptr->pHit == pentTarget)
		return true;

	if (ptr->pHit && CBaseEntity::Instance(ptr->pHit) && !CBaseEntity::Instance(ptr->pHit)->IsBSPModel())
		return BlastLineClear(ptr->vecEndPos, vecEnd, vecDir, pflPunch, ptr->pHit, pentTarget, ptr, iDepth);

	Vector vecPos = ptr->vecEndPos;
	if (*pflPunch <= 0)
		return false;

	do
	{
		float flStep = 8.0;
		if (*pflPunch < 8.0)
			flStep = *pflPunch;
		*pflPunch -= flStep;
		vecPos = vecPos + vecDir * flStep;
		if (UTIL_PointContents(vecPos) != CONTENTS_SOLID)
			return BlastLineClear(vecPos, vecEnd, vecDir, pflPunch, ptr->pHit, pentTarget, ptr, iDepth);
	} while (*pflPunch > 0);

	return false;
}

// orig 0x5ba70: every damageable entity in the level the blast line reaches
void CUltimaGrenade::Blast()
{
	const Vector vecSrc = pev->origin;
	const float flMaxPunch = RC2Pref(RC2P_Grenade_UltimaGrenBlastMaxPunch).f;
	entvars_t* pevAttacker = RC2GrenadeAttacker(pev);

	UTIL_PushEntityMarks();
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent) || pent == edict())
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity || pEntity->pev->takedamage == DAMAGE_NO)
			continue;
		if (UTIL_IsEntityMarked(ENTINDEX(pEntity->GetEffectedEnt()->edict())))
			continue;

		Vector vecSpot = pEntity->Center();
		Vector vecDelta = vecSpot - vecSrc;
		float flPunch = flMaxPunch;
		const float flLength = vecDelta.Length();
		if (flLength > 0)
		{
			TraceResult tr;
			if (!BlastLineClear(vecSrc, vecSpot, vecDelta / flLength, &flPunch, edict(), pent, &tr, 32))
				continue;
		}

		pEntity->GetEffectedEnt()->TakeDamage(pev, pevAttacker, RC2Pref(RC2P_Grenade_UltimaGrenBlastDamage).f, DMG_BLAST | DMG_ENERGYBEAM);
		UTIL_MarkEntity(ENTINDEX(pEntity->GetEffectedEnt()->edict()));
	}
	UTIL_PopEntityMarks();
}

// orig 0x5bd30
void CUltimaGrenade::DetonateExplode()
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(254); // end the effect
	WRITE_SHORT(ENTINDEX(ENT(pev)));
	WRITE_BYTE(55);
	MESSAGE_END();

	PLAYBACK_EVENT_FULL(FEV_GLOBAL, ENT(pev), m_usDetonate, 0, pev->origin, g_vecZero, 0, 0, 0, 0, 0, 0);
	Blast();
	SetThink(NULL);
	SetTouch(NULL);
	UTIL_Remove(this);
}
