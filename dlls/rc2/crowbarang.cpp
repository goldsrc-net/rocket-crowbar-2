/***
*
*	Rocket Crowbar 2: the crowbarang (the crowbar thrown by its tertiary attack) and the crowbar
*	rocket (its secondary). Reconstructed from rc2.dll, the run between the SDK's crossbow.cpp
*	and crowbar.cpp (0x2d1a0-0x30380); reference/dossiers/p2.md. The file name is ours.
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
#include "p2shared.h"

//=========================================================
// CCrowbarang: flies out spinning, turns back when it is about to hit a wall, homes on players
// while its thrower holds the control button, hits what it touches and sticks where it can't
// bounce. The thrower (or anyone, once it is stuck) picks it up as a weapon_crowbar.
//
// OVERLOADS SOME ENTVARS:
//   dmg_inflictor     the thrower; owner is cleared after air_finished so it can hit him
//   impulse           0 going out, 1 turning back, 2 coming back
//   idealpitch        the last think's time (the frame interval)
//   ideal_yaw         when it turns solid again after a touch
//   air_finished      when the thrower can be hit; once stuck, when it fades
//   armorvalue        set at spawn (time + 0.4), never read here
//   health            when it gives up flying
//   dmg               the whack's damage; once stuck, the next pickup check
//   movedir           its wanted velocity
//=========================================================
class CCrowbarang : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT CrowbarangFlyingThink();
	void EXPORT CrowbarangFlyingTouch(CBaseEntity* pOther);
	void EXPORT CrowbarangPickupTouch(CBaseEntity* pOther);
	void EXPORT CrowbarangEmbededThink();

	void Embed(CBaseEntity* pOther, TraceResult* ptr); // name ours
	void Whack(CBaseEntity* pOther);					// name ours

	float m_flNextSightCheck; // orig +0x70, name ours
	bool m_bInSight;		  // orig +0x74, name ours: the thrower (coming back) or the target is in sight
	EHANDLE m_hHomeTarget;	  // orig +0x78, name ours
	bool m_bInWorld;		  // orig +0x80, name ours: stuck in the world, not in an entity
	Vector m_vecEmbedAngles;  // orig +0x84, name ours: angles, relative to what it is stuck in
	Vector m_vecEmbedOffset;  // orig +0x90, name ours: origin, or offset along its forward/right/up
};

LINK_ENTITY_TO_CLASS(crowbarang, CCrowbarang); // orig 0x2d1a0

// orig 0x2d210
void CCrowbarang::Precache()
{
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_SOUND("weapons/cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("fans/fan4.wav");
	PRECACHE_SOUND("buttons/blip2.wav");
}

// orig 0x2d270
void CCrowbarang::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	UTIL_SetSize(pev, Vector(-6, -6, -4), Vector(6, 6, 4));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("crowbarang");

	SetThink(&CCrowbarang::CrowbarangFlyingThink);
	SetTouch(&CCrowbarang::CrowbarangFlyingTouch);

	pev->nextthink = gpGlobals->time + 0.05;
	pev->air_finished = gpGlobals->time + 0.15;
	pev->armorvalue = gpGlobals->time + 0.4;
	pev->ideal_yaw = gpGlobals->time + 0.01;
	pev->idealpitch = gpGlobals->time;
	pev->health = gpGlobals->time + RC2Pref(RC2P_Crowbar_CrowbarangMaxFlyingLife).f;
	pev->impulse = 0;
	pev->dmg = RC2Pref(RC2P_Crowbar_CrowbarangWhackDamage).f;
	pev->avelocity.y = RANDOM_LONG(0, 1) ? 1000 : -1000;

	m_bInSight = false;
	m_flNextSightCheck = gpGlobals->time;

	if (!UTIL_IsValidEntity(pev->owner))
		pev->owner = NULL;
	pev->dmg_inflictor = pev->owner;

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "fans/fan4.wav", 1.0, ATTN_NORM, 0, 150);
}

// orig 0x2d460
void CCrowbarang::CrowbarangFlyingThink()
{
	float flInterval = gpGlobals->time - pev->idealpitch;
	pev->nextthink = gpGlobals->time + 0.05;
	pev->idealpitch = gpGlobals->time;

	if (!UTIL_IsValidEntity(pev->owner))
		pev->owner = NULL;

	// the thrower, while he lives
	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
		pev->dmg_inflictor = NULL;
	else
	{
		CBaseEntity* pThrower = CBaseEntity::Instance(pev->dmg_inflictor);
		if (!pThrower || !pThrower->IsAlive() || pThrower->pev->deadflag != DEAD_NO)
			pev->dmg_inflictor = NULL;
	}

	if (pev->air_finished < gpGlobals->time)
		pev->owner = NULL;
	if (pev->ideal_yaw < gpGlobals->time)
		pev->solid = SOLID_BBOX;

	// steered while the thrower holds the control button
	if (pev->dmg_inflictor && (RC2PlayerButtons(ENTINDEX(pev->dmg_inflictor)) & RC2_IN_CROWBARANG) != 0)
	{
		if (pev->impulse == 2)
		{
			// coming back: towards the thrower while he is in sight
			if (m_flNextSightCheck < gpGlobals->time)
			{
				m_flNextSightCheck = gpGlobals->time + 0.2;
				TraceResult tr;
				UTIL_TraceLine(pev->origin, VARS(pev->dmg_inflictor)->origin, ignore_monsters, ENT(pev), &tr);
				m_bInSight = tr.flFraction >= 1.0 || tr.pHit == pev->dmg_inflictor;
			}

			if (m_bInSight)
			{
				float flSpeed = pev->movedir.Length();
				// Original oddity, kept: the pull is along the unnormalized offset (the normalized
				// one is computed and dropped), so it weighs by distance until the speed clamp
				Vector vecToThrower = VARS(pev->dmg_inflictor)->origin - pev->origin;
				(void)vecToThrower.Normalize();
				if (flSpeed <= 0)
					pev->movedir = vecToThrower * 100;
				else
				{
					Vector vecDir = pev->movedir / flSpeed;
					float flFrac = (1.0 / RC2Pref(RC2P_Crowbar_CrowbarangControlledReturnFract).f) * flInterval;
					pev->movedir = vecToThrower * (1.0 - flFrac) + vecDir * flFrac;
					pev->movedir = pev->movedir * flSpeed;
				}
			}
		}

		bool bSeek = true;
		if (gpGlobals->time <= m_flNextSightCheck)
		{
			if (!m_bInSight)
				bSeek = false; // to the shared flight below
		}
		else
		{
			m_flNextSightCheck = gpGlobals->time + 0.2;

			// lose the target when it goes out of sight
			if (m_bInSight && m_hHomeTarget)
			{
				CBaseEntity* pTarget = m_hHomeTarget;
				TraceResult tr;
				UTIL_TraceLine(pev->origin, pTarget->Center(), ignore_monsters, ENT(pev), &tr);
				if (tr.flFraction < 1.0 || tr.pHit != pTarget->edict())
				{
					m_hHomeTarget = NULL;
					m_bInSight = false;
				}
			}

			// pick the best player ahead
			if (!m_hHomeTarget || !m_bInSight)
			{
				float flMinDot = RC2Pref(RC2P_Crowbar_CrowbarangControlledHomeMinDot).f;
				float flMaxDist = RC2Pref(RC2P_Crowbar_CrowbarangControlledHomeMaxDist).f;
				CBaseEntity* pBest = NULL;
				float flBest = -9999;

				// every entity but the world and the last slot (gpGlobals->maxEntities)
				for (int i = 1; i < gpGlobals->maxEntities; i++)
				{
					edict_t* pent = INDEXENT(i);
					if (!UTIL_IsValidEntity(pent))
						continue;
					CBaseEntity* pEnt = CBaseEntity::Instance(pent);
					if (!pEnt || pEnt->pev->takedamage != DAMAGE_AIM)
						continue;
					if ((pEnt->GetRCCaps2((int)(intptr_t)this, this) & RC2_CAP_NOHOME) != 0) // VERIFY: slot 76's first argument is this CBaseEntity*
						continue;

					Vector vecTo = pEnt->Center() - pev->origin;
					Vector vecDir = pev->movedir.Normalize();
					float flDist = vecTo.Length();
					Vector vecToN = vecTo.Normalize();
					float flDot = DotProduct(vecDir, vecToN);

					if (flDot >= flMinDot && flDist <= flMaxDist)
					{
						float flAim = 1.0 - (1.0 - flDot) / (1.0 - flMinDot);
						if (flAim < 0)
							flAim = 0;
						else if (flAim > 1)
							flAim = 1;
						float flNear = 1.0 - flDist / flMaxDist;
						if (flNear < 0)
							flNear = 0;
						else if (flNear > 1)
							flNear = 1;
						float flScore = flAim * 3.0 + flNear + flNear;

						if (flBest <= flScore)
						{
							TraceResult tr;
							UTIL_TraceLine(pev->origin, pEnt->Center(), ignore_monsters, ENT(pev), &tr);
							if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
							{
								flBest = flScore;
								pBest = pEnt;
							}
						}
					}
				}

				if (pBest)
				{
					m_bInSight = true;
					m_hHomeTarget = pBest;
				}
			}
		}

		if (bSeek && m_hHomeTarget)
		{
			float flSpeed = pev->movedir.Length();
			CBaseEntity* pTarget = m_hHomeTarget;
			// Original oddity, kept: as on the way back, the unnormalized offset
			Vector vecToTarget = pTarget->Center() - pev->origin;
			(void)vecToTarget.Normalize();
			if (flSpeed > 0)
			{
				Vector vecDir = pev->movedir / flSpeed;
				float flFrac = (1.0 / RC2Pref(RC2P_Crowbar_CrowbarangControlledHomeFract).f) * flInterval;
				pev->movedir = vecToTarget * (1.0 - flFrac) + vecDir * flFrac;
				pev->movedir = pev->movedir * flSpeed;
			}
		}
	}

	// turn back short of a wall, speed up again when clear
	Vector vecDir = pev->movedir.Normalize();
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + vecDir * RC2Pref(RC2P_Crowbar_CrowbarangTurnCheckDist).f, ignore_monsters, ENT(pev), &tr);

	if (tr.flFraction >= 1.0 || pev->impulse == 2)
	{
		if (pev->movedir.Length() < RC2Pref(RC2P_Crowbar_CrowbarangSpeed).f)
			pev->movedir = pev->movedir + (vecDir * RC2Pref(RC2P_Crowbar_CrowbarangTurnReAccel).f) * flInterval;

		if (pev->impulse == 1)
		{
			pev->impulse = 2;
			m_bInSight = false;
			m_flNextSightCheck = gpGlobals->time;
		}
	}
	else
	{
		pev->impulse = 1;
		pev->movedir = pev->movedir - (vecDir * RC2Pref(RC2P_Crowbar_CrowbarangTurnBackAccel).f) * flInterval;
	}

	if (RC2Pref(RC2P_Crowbar_CrowbarangSpeed).f < pev->movedir.Length())
		pev->movedir = pev->movedir.Normalize() * RC2Pref(RC2P_Crowbar_CrowbarangSpeed).f;

	if (pev->health < gpGlobals->time)
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, "fans/fan4.wav");
		SetThink(&CCrowbarang::SUB_Remove);
	}

	pev->velocity = pev->movedir;
}

// orig 0x2e310
void CCrowbarang::CrowbarangFlyingTouch(CBaseEntity* pOther)
{
	TraceResult trTouch = UTIL_GetGlobalTrace();

	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		pev->dmg_inflictor = NULL;
		STOP_SOUND(ENT(pev), CHAN_STATIC, "fans/fan4.wav");
		SetThink(&CCrowbarang::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.01;
		pev->effects |= EF_NODRAW;
		return;
	}

	if (!pOther->IsBSPModel())
	{
		// caught by the thrower (or by what he sees through, for him)
		if (pOther->IsActivePlayer())
		{
			if (pOther->edict() == pev->dmg_inflictor || IsPlayersViewEnt(pOther->edict(), pev->dmg_inflictor))
			{
				CBaseEntity* pCatcher = pOther;
				if (IsPlayersViewEnt(pOther->edict(), pev->dmg_inflictor))
					pCatcher = CBaseEntity::Instance(pev->dmg_inflictor);

				g_fItemTimeout = false;
				CBaseEntity* pCrowbar = CBaseEntity::Create("weapon_crowbar", g_vecZero, g_vecZero, NULL);
				pCrowbar->pev->spawnflags |= SF_NORESPAWN;
				pev->dmg_inflictor = NULL;

				if (g_pGameRules->CanHavePlayerItem((CBasePlayer*)pCatcher, (CBasePlayerItem*)pCrowbar))
				{
					g_fGivingItem = true;
					pCrowbar->Touch(pCatcher);
					g_fGivingItem = false;
				}
				else
					pCrowbar->pev->flags |= FL_KILLME;

				SetThink(&CCrowbarang::SUB_Remove);
				SetTouch(NULL);
				pev->nextthink = gpGlobals->time + 0.01;
				pev->effects |= EF_NODRAW;
				STOP_SOUND(ENT(pev), CHAN_STATIC, "fans/fan4.wav");
				g_fItemTimeout = true;
				return;
			}
		}

		// SDK 2.2's spectator director message (svc 50, event 3), priority 6
		MESSAGE_BEGIN(MSG_SPEC, 50);
		WRITE_BYTE(3);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_SHORT(ENTINDEX(pOther->edict()));
		WRITE_LONG(6);
		MESSAGE_END();

		Whack(pOther);

		// on through it, not solid against it for a moment
		pev->velocity = pev->movedir;
		pev->owner = pOther->edict();
		pev->air_finished = gpGlobals->time + 0.3;
		pev->ideal_yaw = gpGlobals->time + 0.1;
		return;
	}

	// the world: bounce off unless the blade bites (a trace along the flight, or one of four
	// beside it, hits something solid that isn't sky)
	Vector vecDir = pev->velocity.Normalize();
	Vector vecStart = pev->origin - vecDir * 16;
	Vector vecEnd = pev->origin + vecDir * 128;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);

	bool bBounce = false;
	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
		bBounce = true;
	else if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
	{
		Embed(pOther, &tr);
		return;
	}
	else
	{
		UTIL_MakeVectors(UTIL_VecToAngles(vecDir));
		Vector vecOffsets[4] = {gpGlobals->v_right * 16, gpGlobals->v_right * -16, gpGlobals->v_up * 16, gpGlobals->v_up * -16};
		bBounce = true;
		for (int i = 0; i < 4; i++)
		{
			UTIL_TraceLine(vecStart + vecOffsets[i], vecEnd + vecOffsets[i], ignore_monsters, ENT(pev), &tr);
			if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
				continue;
			if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
				continue;
			bBounce = false;
			break;
		}
		if (!bBounce)
		{
			Embed(pOther, &tr);
			return;
		}
	}

	// bounce: mirror off the touched plane, jiggle a little, same speed
	float flSpeed = pev->velocity.Length();
	pev->velocity = UTIL_Reflect(pev->velocity.Normalize(), trTouch.vecPlaneNormal);
	float r1 = RANDOM_FLOAT(-0.1, 0.1);
	float r2 = RANDOM_FLOAT(-0.1, 0.1);
	float r3 = RANDOM_FLOAT(-0.1, 0.1);
	pev->velocity = Vector(r3 + pev->velocity.x, r2 + pev->velocity.y, r1 + pev->velocity.z);
	pev->velocity = pev->velocity.Normalize() * flSpeed;
	pev->movedir = pev->velocity;

	if (RANDOM_LONG(0, 1) == 0)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cbar_hit2.wav", 1.0, 1.0, 0, 100);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cbar_hit1.wav", 1.0, 1.0, 0, 100);
}

// orig 0x2f0b0: stick into pOther where ptr hit it
void CCrowbarang::Embed(CBaseEntity* pOther, TraceResult* ptr)
{
	while (pev->angles.x < -90)
		pev->angles.x += 180;
	while (pev->angles.x > 90)
		pev->angles.x -= 180;
	if (pev->angles.x < -70)
		pev->angles.x = -70;
	else if (pev->angles.x > 70)
		pev->angles.x = 70;
	pev->angles.y = UTIL_AngleMod(pev->angles.y);

	if (ENTINDEX(pOther->edict()) == 0)
	{
		m_vecEmbedAngles = pev->angles;
		m_vecEmbedOffset = ptr->vecEndPos + ptr->vecPlaneNormal * 6;
		UTIL_SetOrigin(pev, m_vecEmbedOffset);
		m_bInWorld = true;
	}
	else
	{
		m_vecEmbedAngles = pev->angles - pOther->pev->angles;
		UTIL_MakeVectors(pOther->pev->angles);
		Vector vecPos = ptr->vecEndPos + ptr->vecPlaneNormal * 6;
		Vector vecRel = vecPos - pOther->pev->origin;
		m_vecEmbedOffset.x = DotProduct(vecRel, gpGlobals->v_forward);
		m_vecEmbedOffset.y = DotProduct(vecRel, gpGlobals->v_right);
		m_bInWorld = false;
		m_vecEmbedOffset.z = DotProduct(vecRel, gpGlobals->v_up);
		pev->owner = pOther->edict();

		Vector vecUp = gpGlobals->v_up * m_vecEmbedOffset.z;
		Vector vecRight = gpGlobals->v_right * m_vecEmbedOffset.y;
		Vector vecForward = gpGlobals->v_forward * m_vecEmbedOffset.x;
		UTIL_SetOrigin(pev, vecUp + (vecRight + (vecForward + VARS(pev->owner)->origin)));
	}

	// the client draws it stuck
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_CROWBARANG_EMBED);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_SHORT(ENTINDEX(pOther->edict()));
	WRITE_COORD(m_vecEmbedOffset.x);
	WRITE_COORD(m_vecEmbedOffset.y);
	WRITE_COORD(m_vecEmbedOffset.z);
	WRITE_COORD(m_vecEmbedAngles.x);
	WRITE_COORD(m_vecEmbedAngles.y);
	WRITE_COORD(m_vecEmbedAngles.z);
	MESSAGE_END();

	if (RANDOM_LONG(0, 1) == 0)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cbar_hit2.wav", 1.0, 1.0, 0, 100);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cbar_hit1.wav", 1.0, 1.0, 0, 100);

	pev->dmg = gpGlobals->time;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-24, -24, -24), Vector(24, 24, 24));

	SetTouch(&CCrowbarang::CrowbarangPickupTouch);
	SetThink(&CCrowbarang::CrowbarangEmbededThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->air_finished = RC2Pref(RC2P_Crowbar_CrowbarangInWallLife).f + gpGlobals->time;

	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	STOP_SOUND(ENT(pev), CHAN_STATIC, "fans/fan4.wav");

	// Original bug, kept: this also drops what it is stuck in (set just above), so in anything but
	// the world CrowbarangEmbededThink removes it on its first think (dossier)
	pev->owner = NULL;
	pev->dmg_inflictor = NULL;
	pev->dmgtime = gpGlobals->time + 2.0;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
	pev->rendercolor = g_vecZero;
}

// orig 0x2f780
void CCrowbarang::CrowbarangPickupTouch(CBaseEntity* pOther)
{
	if (!pOther->IsActivePlayer())
		return;
	if (gpGlobals->time < pev->dmg)
		return;

	pev->dmg = gpGlobals->time + 0.2;

	g_fItemTimeout = false;
	CBaseEntity* pCrowbar = CBaseEntity::Create("weapon_crowbar", g_vecZero, g_vecZero, NULL);
	pCrowbar->pev->spawnflags |= SF_NORESPAWN;
	pev->dmg_inflictor = NULL;

	if (g_pGameRules->CanHavePlayerItem((CBasePlayer*)pOther, (CBasePlayerItem*)pCrowbar))
	{
		g_fGivingItem = true;
		pCrowbar->Touch(pOther);
		g_fGivingItem = false;

		SetThink(&CCrowbarang::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.01;
		pev->effects |= EF_NODRAW;
		g_fItemTimeout = true;
		return;
	}

	pCrowbar->pev->flags |= FL_KILLME;
	g_fItemTimeout = true;
}

// orig 0x2f8a0
void CCrowbarang::CrowbarangEmbededThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_bInWorld)
	{
		// ride along with what it is stuck in
		if (!UTIL_IsValidEntity(pev->owner))
		{
			UTIL_Remove(this);
			return;
		}

		entvars_t* pevOwner = VARS(pev->owner);
		pev->angles = m_vecEmbedAngles + pevOwner->angles;
		UTIL_MakeVectors(pevOwner->angles);

		Vector vecUp = gpGlobals->v_up * m_vecEmbedOffset.z;
		Vector vecRight = gpGlobals->v_right * m_vecEmbedOffset.y;
		Vector vecForward = gpGlobals->v_forward * m_vecEmbedOffset.x;
		UTIL_SetOrigin(pev, vecUp + (vecRight + (vecForward + pevOwner->origin)));
	}
	else
	{
		pev->angles = m_vecEmbedAngles;
		UTIL_SetOrigin(pev, m_vecEmbedOffset);
	}

	if (pev->dmgtime < gpGlobals->time)
	{
		pev->dmgtime = gpGlobals->time + 2.0;
		MESSAGE_BEGIN(MSG_PAS, gmsgCLTE, pev->origin);
		WRITE_BYTE(RC2_CLTE_CROWBARANG_GLINT);
		WRITE_SHORT(ENTINDEX(edict()));
		MESSAGE_END();
	}

	if (pev->air_finished < gpGlobals->time)
	{
		pev->velocity = g_vecZero;
		pev->avelocity = g_vecZero;
		SUB_StartFadeOut();
	}
}

// orig 0x2fbc0: hit pOther in flight
void CCrowbarang::Whack(CBaseEntity* pOther)
{
	CBaseEntity* pThrower = NULL;
	if (pev->dmg_inflictor)
		pThrower = CBaseEntity::Instance(pev->dmg_inflictor);

	// VERIFY: slot 75's first argument is a CBaseEntity* here (cbase.h has an int)
	if ((pOther->GetRCCaps((int)(intptr_t)pThrower, this) & RC2_CAP_SHOVE) != 0 && (!pThrower || g_pGameRules->CanAffect(pThrower, pOther)) && (pOther->GetRCCaps((int)(intptr_t)pThrower, this) & RC2_CAP_SHOVE) != 0)
	{
		pOther->pev->velocity = pOther->pev->velocity + pev->movedir * RC2Pref(RC2P_Crowbar_CrowbarangWhackVelFraction).f;
		pOther->pev->velocity.z += RC2Pref(RC2P_Crowbar_CrowbarangWhackVelZBoost).f;
		if ((pOther->pev->flags & FL_ONGROUND) != 0)
			UTIL_SetOrigin(pOther->pev, pOther->pev->origin + Vector(0, 0, 1));
		pOther->pev->flags &= ~FL_ONGROUND;
	}

	if (pOther->Classify() != CLASS_MACHINE)
	{
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/cbar_hitbod1.wav", 1.0, 1.0, 0, 100);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/cbar_hitbod2.wav", 1.0, 1.0, 0, 100);
			break;
		case 2:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/cbar_hitbod3.wav", 1.0, 1.0, 0, 100);
			break;
		}
	}

	TraceResult tr = UTIL_GetGlobalTrace();
	float flDamage = RC2ShieldDamage(pOther, pev->dmg, RC2Pref(RC2P_Crowbar_CrowbarangWhackShieldFraction).f, tr.vecEndPos, pev->velocity.Normalize(), NULL, NULL, 12);
	if (flDamage > 0)
	{
		if (pOther->BloodColor() != DONT_BLEED)
			UTIL_BloodDrips(tr.vecEndPos, g_vecZero, pOther->BloodColor(), 40);

		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			pOther->TakeDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, flDamage, DMG_CLUB);
		else
			pOther->TakeDamage(pev, pev, flDamage, DMG_CLUB);
	}
}


//=========================================================
// CCrowbarRocket: the crowbar's secondary attack. Unlike RC 1.9's, a plain wandering rocket:
// every so often it takes a new random heading inside a cone and a new random speed, and blows
// up on touch or when its life runs out.
//
// OVERLOADS SOME ENTVARS:
//   dmg_inflictor     who fired it (owner again at the explosion)
//   pain_finished     when it blows up by itself
//   armorvalue        its base speed
//=========================================================
class CCrowbarRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT DrunkenThink();
	void EXPORT DrunkenTouch(CBaseEntity* pOther);
};

LINK_ENTITY_TO_CLASS(crowbar_rocket, CCrowbarRocket); // orig 0x2ff40

// orig 0x2ffc0
void CCrowbarRocket::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// orig 0x2fff0
void CCrowbarRocket::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("crowbar_rocket");

	SetThink(&CCrowbarRocket::DrunkenThink);
	SetTouch(&CCrowbarRocket::DrunkenTouch);

	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(RC2Pref(RC2P_Crowbar_RocketMinChangeDirTime).f, RC2Pref(RC2P_Crowbar_RocketMaxChangeDirTime).f);
	pev->pain_finished = gpGlobals->time + RC2Pref(RC2P_Crowbar_RocketMaxLife).f;
	pev->dmg = RC2Pref(RC2P_Crowbar_RocketDamage).f;
	pev->armorvalue = RANDOM_FLOAT(RC2Pref(RC2P_Crowbar_RocketMinSpeed).f, RC2Pref(RC2P_Crowbar_RocketMaxSpeed).f);
	pev->impulse = 0;
	pev->dmg_inflictor = pev->owner;
	pev->effects |= EF_LIGHT;

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.7, 1.0, 0, 100);

	// the client draws its trail
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_ROCKET_TRAIL);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0x301a0
void CCrowbarRocket::DrunkenThink()
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(RC2Pref(RC2P_Crowbar_RocketMinChangeDirTime).f, RC2Pref(RC2P_Crowbar_RocketMaxChangeDirTime).f);

	if (pev->pain_finished < gpGlobals->time)
	{
		SetTouch(NULL);
		DrunkenTouch(this);
		return;
	}

	UTIL_MakeAimVectors(UTIL_VecToAngles(pev->velocity.Normalize()));

	float flSpeed = RANDOM_FLOAT(-RC2Pref(RC2P_Crowbar_RocketSpeedVariance).f, RC2Pref(RC2P_Crowbar_RocketSpeedVariance).f) + pev->armorvalue;
	Vector vecDir = UTIL_RandomSpreadDir(gpGlobals->v_forward, gpGlobals->v_right, gpGlobals->v_up, RC2Pref(RC2P_Crowbar_RocketDirectionSpread).f);

	pev->velocity = vecDir * flSpeed;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

// orig 0x30380
void CCrowbarRocket::DrunkenTouch(CBaseEntity* pOther)
{
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
	UTIL_KillTrail(ENT(pev));
	pev->owner = pev->dmg_inflictor;
	ExplodeTouch(pOther);
}
