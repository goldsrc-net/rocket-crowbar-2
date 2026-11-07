/***
*
*	Rocket Crowbar 2: the assassin transform (m_iTransformType 1).
*
*	File name ours, from its events (events/tassassingun.sc, tassassinkick.sc) and its place in
*	the original's link order (after talkmonster). Primary fires the pistol (spread that
*	tightens while firing), secondary kicks (a swipe, then a back kick), IN_ALT1 zooms and
*	IN_CANCEL cloaks on energy. rc2.ini [TransformAssassin].
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "p8_needs_transform.h"

// orig 0xd7550
void CBasePlayer::TAssassin_SetAnimation(PLAYER_ANIM playerAnim)
{
	int iSeq = pev->sequence;
	pev->gaitsequence = 0;
	float flSpeed = pev->velocity.Length2D();

	if (playerAnim == PLAYER_DIE)
	{
		int iDir = Transform_DeathDirection(this);
		if (flSpeed <= 200)
			pev->sequence = (iDir != 4) + 10;
		else
			pev->sequence = 9;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

	if (pev->deadflag != DEAD_NO)
		return;

	if (m_bFrozen)
	{
		if (pev->sequence == 4)
			return;
		pev->sequence = 4;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

	if (playerAnim == PLAYER_JUMP)
	{
		pev->sequence = 12;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

	if (playerAnim == PLAYER_ATTACK1)
	{
		pev->sequence = ((pev->flags & FL_ONGROUND) != 0) ? 5 : 15;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

	if (pev->sequence == 7 && !m_fSequenceFinished)
		return; // kicking

	if (!IsOnLadder() && pev->waterlevel < 2 && (pev->waterlevel < 1 || (pev->flags & FL_ONGROUND) != 0))
	{
		if (m_bAntiFriction)
		{
			iSeq = 3;
		}
		else if ((pev->flags & FL_ONGROUND) != 0)
		{
			if (m_flTransformLastAttack == -1 || m_flTransformLastAttack < gpGlobals->time)
			{
				if (flSpeed <= 150)
				{
					if (flSpeed <= 0)
					{
						if (m_flTransformLastAttack == -1 || m_flTransformLastAttack <= gpGlobals->time - 5)
						{
							// idle: keep the picked idle until it has been off for 10 seconds
							if (gpGlobals->time - 10 <= m_flTransformIdleTime)
							{
								iSeq = m_iTransformIdleAnim;
								m_flTransformIdleTime = gpGlobals->time;
							}
							else
							{
								iSeq = RANDOM_LONG(0, 1);
								m_iTransformIdleAnim = iSeq;
								m_flTransformIdleTime = gpGlobals->time;
							}
						}
						else
						{
							iSeq = 2;
						}
					}
					else
					{
						iSeq = 4;
						m_flTransformLastAttack = -1;
					}
				}
				else
				{
					iSeq = 3;
				}
			}
			else
			{
				iSeq = 5;
			}
		}
		else
		{
			// in the air
			bool bTestJump;
			if (pev->velocity.z > 50)
			{
				bTestJump = (pev->sequence == 12);
			}
			else if (pev->velocity.z <= 0 || pev->sequence != 12)
			{
				iSeq = (m_flTransformLastAttack == -1 || m_flTransformLastAttack < gpGlobals->time) ? 14 : 15;
				goto set;
			}
			else
			{
				bTestJump = true;
			}

			if (bTestJump && !m_fSequenceFinished)
				goto set; // the jump still playing
			iSeq = 13;
		}
	}
	else
	{
		iSeq = 4;
	}

set:
	if (pev->sequence != iSeq)
	{
		pev->sequence = iSeq;
		pev->frame = 0;
		ResetSequenceInfo();
		if (pev->sequence == 12)
		{
			pev->framerate = 2.0;
			return;
		}
		if (pev->sequence == 5)
			m_fSequenceLoops = true;
	}
}

// As SDK 2.3's CBasePlayer::Jump, for the assassin.
// orig 0xd7890
void CBasePlayer::TAssassin_Jump()
{
	if ((pev->flags & FL_WATERJUMP) != 0)
		return;
	if (pev->waterlevel >= 2)
		return;
	if ((m_afButtonReleased & IN_JUMP) == 0) // VERIFY: the original tests the released buttons here (the houndeye the pressed ones)
		return;
	if ((pev->flags & FL_ONGROUND) == 0 || !pev->groundentity)
		return;
	if (m_bFrozen)
		return;

	SetAnimation(PLAYER_JUMP);

	entvars_t* pevGround = VARS(pev->groundentity);
	if (pevGround && (pevGround->flags & FL_CONVEYOR) != 0)
		pev->velocity = pev->velocity + pev->basevelocity;
}

// orig 0xd7950
void CBasePlayer::TAssassin_GunAttack()
{
	if (pev->waterlevel >= 2 || (pev->waterlevel >= 1 && (pev->flags & FL_ONGROUND) == 0))
		return;
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;

	float flRefire;
	if ((pev->flags & FL_ONGROUND) != 0)
		flRefire = RC2Pref(RC2P_TransformAssassin_GunRefireOnGround).f;
	else if (pev->velocity.z <= 50)
		flRefire = RC2Pref(RC2P_TransformAssassin_GunRefireInAir).f;
	else
		return;

	// the spread: back to the base after 3 seconds without firing, tighter with every shot
	if (m_flTransformLastAttack == -1 || m_flTransformLastAttack < gpGlobals->time - 3)
		m_flTransformVar1 = (0 == m_iTransformState1) ? RC2Pref(RC2P_TransformAssassin_GunBaseSpread).f : RC2Pref(RC2P_TransformAssassin_GunZoomedSpread).f;
	else
		m_flTransformVar1 = m_flTransformVar1 - RC2Pref(RC2P_TransformAssassin_GunSpreadRefineRate).f * m_flFrameInterval;

	float flMin = (0 != m_iTransformState1) ? RC2Pref(RC2P_TransformAssassin_GunSpreadZoomedMin).f : RC2Pref(RC2P_TransformAssassin_GunSpreadBaseMin).f;
	float flSpread = flMin;
	if (flMin <= m_flTransformVar1)
		flSpread = (m_flTransformVar1 <= 1.0) ? m_flTransformVar1 : 1.0;
	m_flTransformVar1 = flSpread;

	if (m_flTransformNextAttack1 - flRefire - 0.1 <= gpGlobals->time)
		m_flTransformLastAttack = gpGlobals->time; // still firing

	if (m_flTransformNextAttack1 > gpGlobals->time)
		return;

	m_flTransformLastAttack = gpGlobals->time;
	m_flTransformNextAttack1 = flRefire + gpGlobals->time;
	UTIL_SetMaxTime(m_flTransformNextAttack2, gpGlobals->time, RC2Pref(RC2P_TransformAssassin_GunRefireOther).f);
	SetAnimation(PLAYER_ATTACK1);
	UTIL_MakeVectors(pev->v_angle);

	float x, y, z;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		z = x * x + y * y;
	} while (z > 1);

	Vector vecUp = gpGlobals->v_up * (x * m_flTransformVar1);
	Vector vecRight = gpGlobals->v_right * (y * m_flTransformVar1);
	Vector vecDir = (vecUp + gpGlobals->v_forward) + vecRight;
	Vector vecSrc = pev->origin + pev->view_ofs;
	Vector vecEnd = vecSrc + vecDir * 8192;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction < 1.0)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit)
		{
			float flDamage = RC2_ShieldFilterDamage(pHit, RC2Pref(RC2P_TransformAssassin_GunDamage).f, RC2Pref(RC2P_TransformAssassin_GunShieldFraction).f, tr.vecEndPos, vecDir, (float*)&tr.vecEndPos, NULL, 9);
			if (flDamage > 0)
			{
				ClearMultiDamage();
				gMultiDamage.type = DMG_BULLET | DMG_BLAST | DMG_NEVERGIB; // 0x1042
				pHit->TraceAttack(pev, flDamage, vecDir, &tr, DMG_BULLET | DMG_BLAST | DMG_NEVERGIB);
				ApplyMultiDamage(UTIL_NamedInflictor("assassin_gun"), pev);
			}
		}
	}

	PLAYBACK_EVENT_FULL(0, edict(), g_usTAssassinGun, 0, g_vecZero, g_vecZero, y * m_flTransformVar1, x * m_flTransformVar1, 0, 0, 0, 0);
	pev->effects |= EF_MUZZLEFLASH;
}

// orig 0xd7ea0
void CBasePlayer::TAssassin_KickStart()
{
	if (pev->waterlevel >= 2 || (pev->waterlevel >= 1 && (pev->flags & FL_ONGROUND) == 0))
		return;
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;
	if ((pev->flags & FL_ONGROUND) == 0 || m_flTransformNextAttack2 > gpGlobals->time)
		return;
	if (pev->sequence == 7 && !m_fSequenceFinished)
		return;

	m_flTransformLastAttack = gpGlobals->time;
	m_flTransformNextAttack2 = RC2Pref(RC2P_TransformAssassin_KickRefireKick).f + gpGlobals->time;
	UTIL_SetMaxTime(m_flTransformNextAttack1, gpGlobals->time, RC2Pref(RC2P_TransformAssassin_KickRefireOther).f);
	pev->sequence = 7;
	pev->frame = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = false;
	pev->framerate = RC2Pref(RC2P_TransformAssassin_KickFramerate).f;
	m_fSequenceFinished = false;
	m_iTransformState3 = 1;
	m_flTransformTimer1 = 0.1429 / pev->framerate + gpGlobals->time; // the swipe's hit frame
}

// One of the kick's two hits: the damageable entity nearest the facing (within KickMinDot)
// that comes within KickCheckDist and KickMaxZDelta and is in sight. The swipe (phase 1)
// knocks it left, the back kick up; both lift it, and a player's view is kicked.
// orig 0xd7ff0
void CBasePlayer::TAssassin_KickHit()
{
	if (pev->waterlevel > 1 || (pev->waterlevel >= 1 && (pev->flags & FL_ONGROUND) == 0))
		return;
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;

	float flBestDot = RC2Pref(RC2P_TransformAssassin_KickMinDot).f;
	CBaseEntity* pBest = NULL;
	CBaseEntity* pEnt = NULL;

	UTIL_MakeVectors(pev->v_angle);
	float flFwdX, flFwdY;
	float flLen = sqrt(gpGlobals->v_forward.x * gpGlobals->v_forward.x + gpGlobals->v_forward.y * gpGlobals->v_forward.y);
	if (flLen == 0)
	{
		flFwdX = 0;
		flFwdY = 0;
	}
	else
	{
		flLen = 1 / flLen;
		flFwdX = gpGlobals->v_forward.x * flLen;
		flFwdY = gpGlobals->v_forward.y * flLen;
	}
	Vector vecRight = gpGlobals->v_right;

	Vector vecClosest;
	while ((pEnt = UTIL_FindEntityNearSphere(pEnt, Center(), RC2Pref(RC2P_TransformAssassin_KickCheckDist).f, NULL, &vecClosest)) != NULL)
	{
		if (pEnt->pev->takedamage == DAMAGE_NO)
			continue;

		Vector vecCenter = Center();
		float dx = vecClosest.x - vecCenter.x;
		float dy = vecClosest.y - vecCenter.y;
		if (RC2Pref(RC2P_TransformAssassin_KickMaxZDelta).f < fabs(vecClosest.z - vecCenter.z))
			continue;

		float flDLen = sqrt(dx * dx + dy * dy);
		if (flDLen == 0)
		{
			dx = 0;
			dy = 0;
		}
		else
		{
			flDLen = 1 / flDLen;
			dx = dx * flDLen;
			dy = dy * flDLen;
		}

		float flDot = flFwdX * dx + flFwdY * dy;
		if (flDot < flBestDot)
			continue;

		TraceResult tr;
		UTIL_TraceLine(Center(), vecClosest, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		pBest = pEnt;
		flBestDot = flDot;
	}

	PLAYBACK_EVENT_FULL(0, edict(), g_usTAssassinKick, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, m_iTransformState3 != 1, pBest != NULL);

	if (!pBest)
		return;

	if ((pBest->GetRCCaps(0, this) & 1) != 0 && RC2_GameRulesCanKnock(this, pBest))
	{
		pBest->pev->flags &= ~FL_ONGROUND;
		pBest->pev->velocity.z += RC2Pref(RC2P_TransformAssassin_KickHitZBoost).f;
		if (m_iTransformState3 == 1)
			pBest->pev->velocity = pBest->pev->velocity - vecRight * RC2Pref(RC2P_TransformAssassin_KickHitSwipeLeftVel).f;
		else
			pBest->pev->velocity.z += RC2Pref(RC2P_TransformAssassin_KickHitUpVel).f;

		if (pBest->IsActivePlayer())
			pBest->pev->punchangle.x += 10;
	}

	float flDamage = (m_iTransformState3 == 1) ? RC2Pref(RC2P_TransformAssassin_KickSwipeDamage).f : RC2Pref(RC2P_TransformAssassin_KickBackKickDamage).f;
	pBest->TakeDamage(UTIL_NamedInflictor("assassin_kick"), pev, flDamage, DMG_CLUB);
}

// orig 0xd8410
void CBasePlayer::TAssassin_Think()
{
	if (0 == m_iShrunk && !m_bFrozen)
	{
		m_flTransformVar2 = RC2Pref(RC2P_TransformAssassin_HealthRegenRate).f * m_flFrameInterval + m_flTransformVar2;
		float flHeal = floor(m_flTransformVar2);
		if (flHeal >= 1)
		{
			TakeHealth(flHeal, DMG_GENERIC);
			m_flTransformVar2 -= flHeal;
		}
	}

	if (0 == m_iTransformState3)
	{
		TAssassin_Jump();
		if ((pev->button & IN_ATTACK) != 0)
			TAssassin_GunAttack();
		else if ((pev->button & IN_ATTACK2) != 0)
			TAssassin_KickStart();
	}
	else if (!m_bFrozen)
	{
		if (m_flTransformTimer1 <= gpGlobals->time)
		{
			if (m_iTransformState3 == 1)
			{
				TAssassin_KickHit();
				m_iTransformState3 = 2;
				m_flTransformTimer1 = 0.4571 / pev->framerate + gpGlobals->time; // the back kick's frame
			}
			else
			{
				TAssassin_KickHit();
				m_iTransformState3 = 0;
			}
		}
	}
	else
	{
		m_iTransformState3 = 0;
	}

	bool bFrozenCloak = false;
	if ((m_afButtonPressed & IN_ALT1) != 0)
	{
		if (m_bFrozen)
		{
			bFrozenCloak = true;
		}
		else
		{
			// zoom
			int iOld = m_iTransformState1;
			m_iTransformState1 = (0 == iOld) ? 1 : 0;
			MESSAGE_BEGIN(MSG_ONE, gmsgTformStat, NULL, edict());
			WRITE_BYTE(0 == iOld);
			MESSAGE_END();
		}
	}
	if (!bFrozenCloak && m_bFrozen)
		bFrozenCloak = true;

	if (bFrozenCloak)
	{
		m_iTransformState2 = 0;
	}
	else if ((m_afButtonPressed & IN_CANCEL) != 0)
	{
		// the cloak: on for CloakTurnOnEnergy, off again on the next press
		if (0 == m_iTransformState2 || gpGlobals->time <= m_flTransformNextAttack3)
		{
			if (RC2Pref(RC2P_TransformAssassin_CloakTurnOnEnergy).f <= m_flEnergy && m_flTransformNextAttack3 < gpGlobals->time)
			{
				m_iTransformState2 = 1;
				m_flEnergy -= RC2Pref(RC2P_TransformAssassin_CloakTurnOnEnergy).f;
				UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_TransformAssassin_CloakTurnOnEnergyRegenDelay).f);
				pev->rendermode = kRenderTransTexture;
				pev->renderamt = 255;
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.2, ATTN_NORM, 0, RANDOM_LONG(95, 105));
				m_flTransformNextAttack3 = gpGlobals->time + 0.2;
			}
		}
		else
		{
			m_iTransformState2 = 0;
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.15, ATTN_NORM, 0, RANDOM_LONG(80, 90));
			m_flTransformNextAttack3 = gpGlobals->time + 0.2;
		}
	}

	if (0 != m_iTransformState2)
	{
		m_flEnergy -= RC2Pref(RC2P_TransformAssassin_CloakEnergyPerSecond).f * m_flFrameInterval;
		UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_TransformAssassin_CloakEnergyRegenDelay).f);
		if (m_flEnergy <= 0)
		{
			m_iTransformState2 = 0;
			m_flEnergy = 0;
		}

		// how visible: more when kicking, firing, moving fast or off the ground
		float flTarget = 20;
		pev->rendermode = kRenderTransTexture;
		if (pev->sequence == 7)
			flTarget = 40;
		if (m_flTransformLastAttack != -1 && gpGlobals->time <= m_flTransformLastAttack)
			flTarget = 60;
		if (pev->velocity.Length() > 200 || (pev->flags & FL_ONGROUND) == 0)
			flTarget = 100;
		pev->renderamt = UTIL_Approach(flTarget, pev->renderamt, m_flFrameInterval * 400);
		return;
	}

	if (pev->rendermode != kRenderNormal)
	{
		pev->renderamt = UTIL_Approach(255, pev->renderamt, m_flFrameInterval * 500);
		if (pev->renderamt >= 255)
			pev->rendermode = kRenderNormal;
	}
}
