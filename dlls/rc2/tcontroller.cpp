/***
*
*	Rocket Crowbar 2: the controller transform (m_iTransformType 3) and its two balls.
*
*	File name ours, from its event (events/tcontrollerthrow.sc) and class names
*	(CTController_*), and its place in the original's link order (after talkmonster). Primary
*	throws a ball that blasts on impact; secondary, held, charges a ball that leeches health
*	from what it passes and gives it back to the thrower after it hits. rc2.ini
*	[TransformController].
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "p8_needs_transform.h"

//=========================================================
// The thrown ball (primary)
//=========================================================
class CTController_ThrowBall : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 2; } // orig 0xdaae0
	void EXPORT ExplodeTouch(CBaseEntity* pOther);

	unsigned short m_usHitEvent; // orig +0x70, name ours: events/tcontrollertballhit.sc
};

LINK_ENTITY_TO_CLASS(tcontroller_tball, CTController_ThrowBall); // orig 0xdaa70

// orig 0xdaaf0
void CTController_ThrowBall::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	pev->dmg_inflictor = pev->owner; // the thrower, kept past the owner being cleared on impact
	SetTouch(&CTController_ThrowBall::ExplodeTouch);
	SetThink(&CTController_ThrowBall::SUB_Remove);
	pev->nextthink = gpGlobals->time + RC2Pref(RC2P_TransformController_ThrowBallMaxLife).f;

	// the client draws the ball (CLTE 0x46)
	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x46);
	WRITE_SHORT(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0xdac10
void CTController_ThrowBall::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_MODEL("sprites/glow04.spr");
	PRECACHE_SOUND("ambience/crtnoise.wav");
	m_usHitEvent = PRECACHE_EVENT(1, "events/tcontrollertballhit.sc");
}

// The blast both balls do on impact: every damageable entity whose bounds come within flRadius
// of the impact point and that sees it takes flMaxDamage scaled down by
// (dist / radius) ^ flExponent, less what an HEV shield takes, and is pushed away from the
// point by its share of flMaxVelocity. Each entity (by the one it stands for, slot 47) is hit
// once; the thrower never. Ours: the original has this loop twice, inline.
static void TController_BallBlast(CBaseEntity* pBall, const Vector& vecCenter, float flRadius, float flMaxDamage, float flExponent, float flShieldFraction, float flMaxVelocity)
{
	entvars_t* pev = pBall->pev;
	float flDist;
	Vector vecClosest;

	UTIL_PushEntFlags();
	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityNearSphere(pEnt, vecCenter, flRadius, &flDist, &vecClosest)) != NULL)
	{
		if (pEnt->pev->takedamage == DAMAGE_NO)
			continue;
		if (UTIL_TestEntFlag(ENTINDEX(pEnt->GetEffectedEnt()->edict())))
			continue;
		if (pEnt->GetEffectedEnt()->edict() == pev->dmg_inflictor)
			continue;
		if (!(flDist < flRadius))
			continue;

		Vector vecDir = (vecClosest - vecCenter).Normalize();

		TraceResult tr;
		UTIL_TraceLine(vecCenter, vecClosest, ignore_monsters, pBall->edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		float flDamage = flMaxDamage - pow(flDist / flRadius, flExponent) * flMaxDamage;
		if (flDamage < 0)
			flDamage = 0;
		else if (flDamage > flMaxDamage)
			flDamage = flMaxDamage;

		flDamage = RC2_ShieldFilterDamage(pEnt, flDamage, flShieldFraction, tr.vecEndPos, vecDir, NULL, NULL, 8);
		if (flDamage <= 0)
			continue;

		// VERIFY: GetRCCaps2's first argument; the original passes the ball itself
		if ((pEnt->GetEffectedEnt()->GetRCCaps2(pBall, pBall) & 2) == 0 && flMaxDamage != 0)
			pEnt->pev->velocity = pEnt->pev->velocity + vecDir * (flDamage / flMaxDamage) * flMaxVelocity;

		entvars_t* pevAttacker = pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL;
		pEnt->GetEffectedEnt()->TakeDamage(pev, pevAttacker, flDamage, DMG_ENERGYBEAM);
		UTIL_SetEntFlag(ENTINDEX(pEnt->GetEffectedEnt()->edict()));
	}
	UTIL_PopEntFlags();
}

// orig 0xdac60
void CTController_ThrowBall::ExplodeTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	SetTouch(NULL);
	SetThink(NULL);
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe); // kill the ball's effect
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x46);
	MESSAGE_END();

	TraceResult tr = UTIL_GetGlobalTrace();
	PLAYBACK_EVENT_FULL(0, edict(), m_usHitEvent, 0, g_vecZero, g_vecZero, 0, 0, UTIL_PackNormal(-tr.vecPlaneNormal), 0, 0, 0);

	Vector vecCenter = tr.vecEndPos + tr.vecPlaneNormal * 16;

	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
		pev->dmg_inflictor = edict();
	pev->owner = NULL;

	TController_BallBlast(this, vecCenter,
		RC2Pref(RC2P_TransformController_ThrowBallBlastRadius).f,
		RC2Pref(RC2P_TransformController_ThrowBallBlastMaxDamage).f,
		RC2Pref(RC2P_TransformController_ThrowBallBlastFalloffExponent).f,
		RC2Pref(RC2P_TransformController_ThrowBallBlastShieldFraction).f,
		RC2Pref(RC2P_TransformController_ThrowBallBlastMaxVelocity).f);

	UTIL_Remove(this);
}

//=========================================================
// The charged ball (secondary)
//=========================================================
class CTController_ChargeBall : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 2; } // orig 0xdb2c0
	void EXPORT LeechThink();
	void EXPORT ExplodeTouch(CBaseEntity* pOther);

	// names ours; pev->dmg holds the charge (0-1, not clamped)
	float m_flLastThink;  // orig +0x70
	float* m_pflGive;	  // orig +0x74: health still to give the thrower, by half second of travel back
	int m_cGive;		  // orig +0x78: its length
	float m_flGiveTimer;  // orig +0x7c: until the queue moves on
	float m_flLastGive;	  // orig +0x80
	float m_flLeechAccum; // orig +0x84: fractional leech carried between thinks
	bool m_fSentFX;		  // orig +0x88
	bool m_fExploded;	  // orig +0x89
	unsigned short m_usHitEvent; // orig +0x8a
};

LINK_ENTITY_TO_CLASS(tcontroller_cball, CTController_ChargeBall); // orig 0xdb250

// orig 0xdb2d0
void CTController_ChargeBall::Spawn()
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
	SetTouch(&CTController_ChargeBall::ExplodeTouch);
	SetThink(&CTController_ChargeBall::LeechThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flLastThink = gpGlobals->time;

	m_cGive = (int)(RC2Pref(RC2P_TransformController_ChargeBallGiveMaxTime).f * 2);
	if (m_cGive < 1)
		m_cGive = 1;
	else if (m_cGive > 600)
		m_cGive = 600;
	m_pflGive = (float*)malloc(m_cGive * sizeof(float)); // original bug: freed only when the ball removes itself through LeechThink
	memset(m_pflGive, 0, m_cGive * sizeof(float));

	m_flGiveTimer = 0.5;
	m_fExploded = false;
	m_flLastGive = gpGlobals->time;
	m_fSentFX = false;
	m_flLeechAccum = 0;
}

// orig 0xdb440
void CTController_ChargeBall::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_MODEL("sprites/glow04.spr");
	PRECACHE_SOUND("ambience/crtnoise.wav");
	m_usHitEvent = PRECACHE_EVENT(1, "events/tcontrollertballhit.sc");
}

// orig 0xdb490
void CTController_ChargeBall::LeechThink()
{
	float flNow = gpGlobals->time;
	float flLast = m_flLastThink;
	pev->nextthink = gpGlobals->time + 0.1;
	m_flLastThink = gpGlobals->time;
	float flInterval = flNow - flLast;

	if (!m_fSentFX)
	{
		m_fSentFX = true;
		float flCharge = pev->dmg;
		if (flCharge < 0)
			flCharge = 0;
		else if (flCharge > 1)
			flCharge = 1;

		MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
		WRITE_BYTE(0x49);
		WRITE_SHORT(ENTINDEX(edict()));
		if (UTIL_IsValidEntity(pev->dmg_inflictor))
			WRITE_BYTE(ENTINDEX(pev->dmg_inflictor));
		else
			WRITE_BYTE(0);
		WRITE_BYTE((int)(flCharge * 255));
		MESSAGE_END();
	}

	if (!UTIL_IsValidEntity(pev->dmg_inflictor) || !m_pflGive)
	{
		if (m_fExploded)
		{
			SetThink(&CTController_ChargeBall::SUB_Remove);
			pev->nextthink = gpGlobals->time;
			if (m_pflGive)
				free(m_pflGive);
			m_pflGive = NULL;
			return;
		}
	}
	else
	{
		// give the thrower what has come back to him
		CBaseEntity* pThrower = CBaseEntity::Instance(pev->dmg_inflictor);
		if (pThrower)
		{
			if (m_flGiveTimer <= 0)
			{
				if (m_pflGive[0] >= 1)
					pThrower->TakeHealth(m_pflGive[0], DMG_GENERIC);
				m_flGiveTimer = 0.5;
				for (int i = 1; i < m_cGive; i++)
					m_pflGive[i - 1] = m_pflGive[i];
				m_pflGive[m_cGive - 1] = 0;
				m_flLastGive = gpGlobals->time;
			}

			float flGive = (gpGlobals->time - m_flLastGive) * m_pflGive[0] * 2;
			if (flGive >= 1)
			{
				pThrower->TakeHealth(flGive, DMG_GENERIC);
				m_flLastGive = gpGlobals->time;
				m_pflGive[0] -= flGive;
			}
			m_flGiveTimer -= flInterval;
		}

		if (m_fExploded)
		{
			int i;
			for (i = 0; i < m_cGive; i++)
			{
				if (m_pflGive[i] != 0)
					break;
			}
			if (i < m_cGive)
				return;

			SetThink(&CTController_ChargeBall::SUB_Remove);
			pev->nextthink = gpGlobals->time;
			if (m_pflGive)
				free(m_pflGive);
			m_pflGive = NULL;
			return;
		}
	}

	// leech from what is near and ahead of the ball
	m_flLeechAccum += (RC2Pref(RC2P_TransformController_ChargeBallLeechAddRate).f * pev->dmg + RC2Pref(RC2P_TransformController_ChargeBallLeechBaseRate).f) * flInterval;
	int iLeech = (int)m_flLeechAccum;
	if (0 == iLeech)
		return;
	m_flLeechAccum -= (float)iLeech;

	CBaseEntity* pThrower = NULL;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pThrower = CBaseEntity::Instance(pev->dmg_inflictor);

	float flRadius = RC2Pref(RC2P_TransformController_ChargeBallLeechRadius).f;
	float flTotal = 0;
	Vector vecTravel = pev->velocity.Normalize();

	UTIL_PushEntFlags();
	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, flRadius)) != NULL)
	{
		if (UTIL_TestEntFlag(ENTINDEX(pEnt->edict())))
			continue;
		if (pEnt->GetEffectedEnt()->pev->takedamage != DAMAGE_AIM)
			continue;
		// VERIFY: GetRCCaps's first argument; the original passes the thrower
		if ((pEnt->GetEffectedEnt()->GetRCCaps(pThrower, this) & 0x2000) == 0)
			continue;
		if (pThrower && pEnt->GetEffectedEnt()->edict() == pThrower->edict())
			continue;

		Vector vecToEnt = (pEnt->Center() - pev->origin).Normalize();
		if (DotProduct(vecToEnt, vecTravel) < RC2Pref(RC2P_TransformController_ChargeBallLeechMinDot).f)
			continue;

		TraceResult tr;
		UTIL_TraceLine(pev->origin, pEnt->Center(), dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		float flAmount = RC2_ShieldFilterDamage(pEnt->GetEffectedEnt(), (float)iLeech, RC2Pref(RC2P_TransformController_ChargeBallLeechShieldFraction).f, tr.vecEndPos, pEnt->Center() - pev->origin, NULL, NULL, 4);
		if (flAmount == 0)
			continue;

		entvars_t* pevAttacker = pev->owner ? VARS(pev->owner) : NULL; // VERIFY: the leech names pev->owner, the blast pev->dmg_inflictor
		pEnt->GetEffectedEnt()->TakeDamage(pev, pevAttacker, flAmount, DMG_ENERGYBEAM);
		flTotal += flAmount;
		UTIL_SetEntFlag(ENTINDEX(pEnt->edict()));
	}
	UTIL_PopEntFlags();

	if (!pThrower || flTotal == 0)
		return;

	// queue it by how long it takes to travel back to the thrower
	int iSlot = (int)((pev->origin - pThrower->Center()).Length() / RC2Pref(RC2P_TransformController_ChargeBallGiveTravelVel).f * 2);
	if (iSlot < 0)
		iSlot = 0;
	else if (iSlot >= m_cGive)
		iSlot = m_cGive - 1;
	m_pflGive[iSlot] += flTotal;
}

// orig 0xdbc20
void CTController_ChargeBall::ExplodeTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	SetTouch(NULL);
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x49);
	MESSAGE_END();

	TraceResult tr = UTIL_GetGlobalTrace();
	PLAYBACK_EVENT_FULL(0, edict(), m_usHitEvent, 0, g_vecZero, g_vecZero, 0, 0, UTIL_PackNormal(-tr.vecPlaneNormal), 0, 0, 0);

	Vector vecCenter = tr.vecEndPos + tr.vecPlaneNormal * 16;
	float flRadius = RC2Pref(RC2P_TransformController_ChargeBallMaxImpactRadius).f * pev->dmg;
	float flMaxDamage = RC2Pref(RC2P_TransformController_ChargeBallMaxImpactDamage).f * pev->dmg;

	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
		pev->dmg_inflictor = edict();
	pev->owner = NULL;

	TController_BallBlast(this, vecCenter, flRadius, flMaxDamage,
		RC2Pref(RC2P_TransformController_ChargeBallBlastFalloffExponent).f,
		RC2Pref(RC2P_TransformController_ChargeBallBlastShieldFraction).f,
		RC2Pref(RC2P_TransformController_ChargeBallBlastMaxVelocity).f);

	// it stays, thinking, until it has given back all it leeched
	m_fExploded = true;
}

//=========================================================
// The controller transform (CBasePlayer, m_iTransformType 3)
//=========================================================

// orig 0xd9c40
void CBasePlayer::TController_SetAnimation(PLAYER_ANIM playerAnim)
{
	int iSeq;

	pev->gaitsequence = 0;
	if (playerAnim == PLAYER_DIE)
	{
		pev->sequence = 8;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}
	if (pev->deadflag != DEAD_NO)
		return;

	if (!m_bFrozen)
	{
		if (0 != m_iTransformState2) // charging
			return;
		if ((pev->sequence == 2 || pev->sequence == 4) && !m_fSequenceLoops && !m_fSequenceFinished)
			return; // a throw still playing

		if (pev->velocity.Length() <= 40)
		{
			iSeq = 15;
		}
		else
		{
			// the move direction that dominates
			UTIL_MakeVectors(pev->v_angle);
			float flForward = DotProduct(pev->velocity, gpGlobals->v_forward);
			float flRight = DotProduct(gpGlobals->v_right, pev->velocity);
			float flAbsForward = fabs(flForward);
			float flAbsRight = fabs(flRight);
			float flAbsUp = fabs(pev->velocity.z);

			if (flAbsRight < flAbsUp || flAbsRight < flAbsForward)
			{
				if (flAbsUp < flAbsRight || flAbsUp < flAbsForward)
					iSeq = (flForward <= 0) ? 10 : 9;
				else
					iSeq = (pev->velocity.z <= 0) ? 12 : 11;
			}
			else
			{
				iSeq = (flRight <= 0) ? 14 : 13;
			}
		}
		if (pev->sequence == iSeq)
			return;
	}
	else
	{
		iSeq = 15;
		if (pev->sequence == 15)
			return;
	}

	pev->sequence = iSeq;
	pev->frame = 0;
	ResetSequenceInfo();
}

// orig 0xd9e30
void CBasePlayer::TController_ThrowStart()
{
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;
	if (0 != m_iTransformState1 || 0 != m_iTransformState2 || m_flTransformNextAttack1 > gpGlobals->time)
		return;
	if (pev->sequence == 2 && !m_fSequenceFinished)
		return;
	if (m_flEnergy < RC2Pref(RC2P_TransformController_ThrowEnergyCost).f)
		return;

	m_flTransformLastAttack = gpGlobals->time;
	m_flTransformNextAttack1 = RC2Pref(RC2P_TransformController_ThrowRefireThrow).f + gpGlobals->time;
	UTIL_SetMaxTime(m_flTransformNextAttack2, gpGlobals->time, RC2Pref(RC2P_TransformController_ThrowRefireOther).f);
	UTIL_SetMaxTime(m_flTransformNextAttack3, gpGlobals->time, RC2Pref(RC2P_TransformController_ThrowRefireOther).f);
	UTIL_SetMaxTime(m_flTransformNextAttack4, gpGlobals->time, RC2Pref(RC2P_TransformController_ThrowRefireOther).f);
	m_flEnergy -= RC2Pref(RC2P_TransformController_ThrowEnergyCost).f;

	pev->sequence = 2;
	pev->frame = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = false;
	pev->framerate = RC2Pref(RC2P_TransformController_ThrowFramerate).f;
	m_fSequenceFinished = false;
	m_iTransformState1 = 1;
	m_flTransformTimer2 = 1.4 / pev->framerate + gpGlobals->time; // the release frame

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x45);
	WRITE_BYTE(ENTINDEX(edict()));
	MESSAGE_END();
}

// orig 0xda000
void CBasePlayer::TController_ChargeStart()
{
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;
	if (0 != m_iTransformState1 || 0 != m_iTransformState2 || m_flTransformNextAttack2 > gpGlobals->time)
		return;
	if (m_flEnergy < RC2Pref(RC2P_TransformController_ChargeBallStartEnergyCost).f)
		return;

	m_flTransformLastAttack = gpGlobals->time;
	m_flTransformNextAttack2 = RC2Pref(RC2P_TransformController_ChargeBallRefireChargeBall).f + gpGlobals->time;
	UTIL_SetMaxTime(m_flTransformNextAttack1, gpGlobals->time, RC2Pref(RC2P_TransformController_ChargeBallRefireOther).f);
	UTIL_SetMaxTime(m_flTransformNextAttack3, gpGlobals->time, RC2Pref(RC2P_TransformController_ChargeBallRefireOther).f);
	UTIL_SetMaxTime(m_flTransformNextAttack4, gpGlobals->time, RC2Pref(RC2P_TransformController_ChargeBallRefireOther).f);
	m_flEnergy -= RC2Pref(RC2P_TransformController_ChargeBallStartEnergyCost).f;

	pev->sequence = 5;
	pev->frame = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = false;
	pev->framerate = RC2Pref(RC2P_TransformController_ChargeBallFramerate).f;
	m_fSequenceFinished = false;
	m_iTransformState2 = 1;
	m_flTransformTimer2 = 2.3333 / pev->framerate + gpGlobals->time;
	m_flTransformTimer3 = gpGlobals->time;

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pev->origin);
	WRITE_BYTE(0x48);
	WRITE_BYTE(ENTINDEX(edict()));
	MESSAGE_END();

	AddMoveLock(4);
}

// orig 0xda1d0
void CBasePlayer::TController_ChargeLaunch()
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x48);
	MESSAGE_END();

	float flCharge = (gpGlobals->time - m_flTransformTimer3) / RC2Pref(RC2P_TransformController_ChargeBallChargeTime).f;

	UTIL_MakeVectors(pev->punchangle + pev->v_angle);
	CBaseEntity* pBall = CBaseEntity::Create("tcontroller_cball", pev->origin + pev->view_ofs, g_vecZero, edict());
	pBall->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_TransformController_ChargeBallLaunchVelocity).f;
	pBall->pev->dmg = flCharge;

	PLAYBACK_EVENT_FULL(0, edict(), g_usTControllerThrow, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	pev->sequence = 4;
	pev->frame = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = false;
	pev->framerate = 3.0;
	m_fSequenceFinished = false;
	m_iTransformState2 = 0;
	m_flTransformTimer2 = -1;
	RemoveMoveLock(4);
}

// While charging: cycle the charge animations until every attack button is up, then launch.
// Frozen or busy, the charge is dropped.
// orig 0xda3b0
void CBasePlayer::TController_ChargeThink()
{
	if (0 == m_iShrunk && !m_bFrozen)
	{
		if (m_flTransformTimer2 < gpGlobals->time)
		{
			if ((pev->button & (IN_ATTACK | IN_CANCEL | IN_ATTACK2 | IN_ALT1)) == 0)
			{
				TController_ChargeLaunch();
				return;
			}

			float flLength;
			if (pev->sequence == 3)
			{
				pev->sequence = 1;
				flLength = 3.3333;
			}
			else if (pev->sequence == 1)
			{
				pev->sequence = 4;
				flLength = 1.6667;
			}
			else
			{
				pev->sequence = 3;
				flLength = 3.3333;
			}
			pev->frame = 0;
			ResetSequenceInfo();
			m_fSequenceLoops = false;
			pev->framerate = RC2Pref(RC2P_TransformController_ChargeBallFramerate).f;
			m_fSequenceFinished = false;
			m_flTransformTimer2 = flLength / pev->framerate + gpGlobals->time;
		}
		return;
	}

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x48);
	MESSAGE_END();
	m_iTransformState2 = 0;
	m_flTransformTimer2 = -1;
	RemoveMoveLock(4);
}

// orig 0xda580
void CBasePlayer::TController_ThrowRelease()
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x45);
	MESSAGE_END();

	UTIL_MakeVectors(pev->punchangle + pev->v_angle);
	Vector vecRight = gpGlobals->v_right;
	Vector vecSrc = (pev->origin + pev->view_ofs) + vecRight * -16;
	CBaseEntity* pBall = CBaseEntity::Create("tcontroller_tball", vecSrc, g_vecZero, edict());
	pBall->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_TransformController_ThrowBallVelocity).f;

	PLAYBACK_EVENT_FULL(0, edict(), g_usTControllerThrow, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	m_iTransformState1 = 0;
	m_flTransformTimer2 = -1;
}

// orig 0xda780
void CBasePlayer::TController_Think()
{
	if (0 == m_iShrunk && !m_bFrozen)
	{
		m_flTransformVar1 += RC2Pref(RC2P_TransformController_HealthRegenRate).f * m_flFrameInterval;
		float flHeal = floor(m_flTransformVar1);
		if (flHeal >= 1)
		{
			TakeHealth(flHeal, DMG_GENERIC);
			m_flTransformVar1 -= flHeal;
		}
	}

	if (m_iTransformState2 == 1)
	{
		TController_ChargeThink();
		return;
	}

	if (m_flTransformTimer2 == -1 || 0 == m_iTransformState1)
	{
		if ((pev->button & IN_ATTACK) != 0)
		{
			TController_ThrowStart();
			return;
		}
		if ((pev->button & IN_ATTACK2) != 0)
			TController_ChargeStart();
		return;
	}

	if (m_bFrozen)
	{
		m_flTransformTimer2 = -1;
		m_iTransformState1 = 0;
		return;
	}
	if (m_flTransformTimer2 <= gpGlobals->time)
	{
		if (m_iTransformState1 == 1)
			TController_ThrowRelease();
		m_flTransformTimer2 = -1;
		m_iTransformState1 = 0;
	}
}

// orig 0xda890
void CBasePlayer::TController_DeathSound()
{
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "controller/con_die1.wav", 1, ATTN_NORM, 0, RANDOM_LONG(95, 105));
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "controller/con_die2.wav", 1, ATTN_NORM, 0, RANDOM_LONG(95, 105));
		break;
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1, ATTN_NORM, 0, 100);
}

// orig 0xda980
void CBasePlayer::TController_PainSound()
{
	if (m_flTransformTimer1 >= gpGlobals->time)
		return;
	m_flTransformTimer1 = RANDOM_FLOAT(0.4, 0.5) + gpGlobals->time;

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "controller/con_pain1.wav", 1, ATTN_NORM, 0, RANDOM_LONG(95, 105));
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "controller/con_pain2.wav", 1, ATTN_NORM, 0, RANDOM_LONG(95, 105));
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "controller/con_pain3.wav", 1, ATTN_NORM, 0, RANDOM_LONG(95, 105));
		break;
	}
}
