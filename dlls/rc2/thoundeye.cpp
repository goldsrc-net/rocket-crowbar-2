/***
*
*	Rocket Crowbar 2: the houndeye transform (m_iTransformType 2).
*
*	File name ours, from its events (events/thoundeyesonic.sc, thoundeyebeam.sc) and its place
*	in the original's link order (after theadcrab). Primary charges and lets off the sonic
*	blast; secondary charges a beam that hits through walls (punching up to BeamPunchDistance
*	through solid) and recoils the houndeye; IN_ALT1 toggles the locator on energy and
*	IN_CANCEL lies down to sleep, which regains health and energy faster.
*	rc2.ini [TransformHoundeye].
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "p8_needs_transform.h"

// orig 0xdf620
void CBasePlayer::THoundeye_SetAnimation(PLAYER_ANIM playerAnim)
{
	int iSeq;
	pev->gaitsequence = 0;
	float flSpeed = pev->velocity.Length2D();

	if (playerAnim == PLAYER_DIE)
	{
		int iDir = Transform_DeathDirection(this);
		if (iDir == 3)
			pev->sequence = 7;
		else if (iDir == 4)
			pev->sequence = RANDOM_LONG(8, 9);
		else
			pev->sequence = (pev->health >= -30) ? 6 : 13;
		pev->frame = 0;
		ResetSequenceInfo();
		m_flTransformVar1 = pev->v_angle.y;
		return;
	}

	if (pev->deadflag != DEAD_NO)
		return;
	if (0 != m_iTransformState4) // sleeping: its own animations
		return;

	if (m_bFrozen)
	{
		if (pev->sequence != 15)
		{
			pev->sequence = 15;
			pev->frame = 0;
			ResetSequenceInfo();
		}
		m_flTransformVar1 = pev->v_angle.y;
		return;
	}

	if (0 != m_iTransformState1)
	{
		// charging the beam
		if (pev->sequence != 21)
		{
			pev->sequence = 21;
			pev->frame = 0;
			ResetSequenceInfo();
			m_fSequenceLoops = true;
		}
		pev->framerate = 0.8;
		return;
	}
	if (0 != m_iTransformState3)
		return; // the sonic animation plays out

	if (playerAnim == PLAYER_JUMP)
	{
		pev->sequence = 3;
		pev->frame = 0;
		ResetSequenceInfo();
		m_flTransformVar1 = pev->v_angle.y;
		return;
	}

	if (IsOnLadder())
	{
		iSeq = 15;
	}
	else if (pev->waterlevel < 2 && (pev->waterlevel < 1 || (pev->flags & FL_ONGROUND) != 0) &&
			 (m_bAntiFriction || (pev->flags & FL_ONGROUND) != 0))
	{
		if (m_bAntiFriction)
		{
			iSeq = 4;
		}
		else if (flSpeed > 75)
		{
			iSeq = 4;
		}
		else if (flSpeed > 0)
		{
			iSeq = 15;
		}
		else
		{
			int iCur = pev->sequence;
			iSeq = iCur;
			if ((iCur != 0 && iCur != 2 && iCur != 25 && iCur != 26) || m_fSequenceLoops || m_fSequenceFinished)
				iSeq = 1;

			if (m_flTransformLastAttack != -1 && gpGlobals->time - 3 < m_flTransformLastAttack)
			{
				// just attacked: an alert animation
				if (iCur > 19 && iCur < 23 && !m_fSequenceLoops && !m_fSequenceFinished)
					return;
				pev->sequence = RANDOM_LONG(20, 22);
				int iAct = LookupActivity(47); // VERIFY: activity 47 (by its number)
				if (iAct != -1)
					pev->sequence = iAct;
				pev->frame = 0;
				ResetSequenceInfo();
				m_fSequenceLoops = false;
				return;
			}

			// turning in place
			float flDiff = UTIL_AngleDiff(UTIL_AngleMod(pev->v_angle.y), UTIL_AngleMod(m_flTransformVar1));
			if (fabs(flDiff) > 0 && fabs(flDiff) < m_flFrameInterval * 1200 && 0 == pev->fixangle)
			{
				if (flDiff < 0 || (iSeq == 25 && flDiff < m_flFrameInterval * 100))
					iSeq = 25;
				else if (flDiff > 0 || (iSeq == 26 && m_flFrameInterval * -100 < flDiff))
					iSeq = 26;

				if (pev->sequence == iSeq && m_fSequenceFinished)
				{
					pev->frame = 0;
					ResetSequenceInfo();
				}
			}
		}
	}
	else
	{
		iSeq = 3;
	}

	m_flTransformVar1 = pev->v_angle.y;
	if (pev->sequence != iSeq)
	{
		pev->sequence = iSeq;
		pev->frame = 0;
		ResetSequenceInfo();
		if (pev->sequence > 19 && pev->sequence < 23)
			m_fSequenceLoops = false;
	}
}

// orig 0xdfa30
void CBasePlayer::THoundeye_SonicStart()
{
	if (pev->waterlevel >= 2 || (pev->waterlevel >= 1 && (pev->flags & FL_ONGROUND) == 0))
		return;
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;
	if (0 != m_iTransformState1 || 0 != m_iTransformState3 || m_flTransformNextAttack1 > gpGlobals->time)
		return;

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 512, 0.4);
	AddMoveLock(4);
	pev->sequence = 10;
	pev->frame = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = false;
	pev->framerate = RC2Pref(RC2P_TransformHoundeye_SonicFramerate).f;
	m_fSequenceFinished = false;
	m_iTransformState3 = 1;
	m_flTransformTimer2 = 2.4 / pev->framerate + gpGlobals->time; // the blast frame

	m_iTransformState3 = RANDOM_LONG(1, 2); // which charge sound (the blast stops it)
	if (m_iTransformState3 == 1)
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "houndeye/he_attack1.wav", 0.7, ATTN_NORM, 0, 100);
	else if (m_iTransformState3 == 2)
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "houndeye/he_attack3.wav", 0.7, ATTN_NORM, 0, 100);
}

// orig 0xdfbd0
void CBasePlayer::THoundeye_BeamStart()
{
	if (pev->waterlevel >= 2 || (pev->waterlevel >= 1 && (pev->flags & FL_ONGROUND) == 0))
		return;
	if (IsOnLadder() || m_bFrozen || 0 != m_iShrunk || m_pTank != NULL)
		return;
	if (0 != m_iTransformState1 || 0 != m_iTransformState3)
		return;
	if (m_flEnergy < RC2Pref(RC2P_TransformHoundeye_BeamEnergyCost).f || m_flTransformNextAttack2 > gpGlobals->time)
		return;

	CSoundEnt::InsertSound(bits_SOUND_PLAYER, pev->origin, 512, RC2Pref(RC2P_TransformHoundeye_BeamChargeTime).f);
	m_flEnergy -= RC2Pref(RC2P_TransformHoundeye_BeamEnergyCost).f;

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0x41);
	WRITE_BYTE(ENTINDEX(edict()));
	MESSAGE_END();

	AddMoveLock(4);
	m_iTransformState1 = 1;
	m_flTransformTimer1 = RC2Pref(RC2P_TransformHoundeye_BeamChargeTime).f + gpGlobals->time;
}

// As SDK 2.3's CBasePlayer::Jump, for the houndeye.
// orig 0xdfd10
void CBasePlayer::THoundeye_Jump()
{
	if ((pev->flags & FL_WATERJUMP) != 0)
		return;
	if (pev->waterlevel >= 2)
		return;
	if ((m_afButtonPressed & IN_JUMP) == 0)
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

// The sonic blast: everything in a cone ahead (SonicMinDot, SonicMaxDist, the aim's pitch held
// to SonicMaxXAngDev) that answers to RC cap 0x1000 (or RC cap-2 0x10) takes damage and is
// pushed, both falling off with distance (full inside SonicFullThresholdDist and dot), less
// without a line of sight.
// orig 0xdfdd0
void CBasePlayer::THoundeye_SonicBlast()
{
	if (m_iTransformState3 == 1)
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "houndeye/he_attack1.wav", 0, 0, SND_STOP, 100);
	else if (m_iTransformState3 == 2)
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "houndeye/he_attack3.wav", 0, 0, SND_STOP, 100);

	m_iTransformState3 = 0;
	m_flTransformTimer2 = -1;
	RemoveMoveLock(4);
	m_flTransformNextAttack1 = RC2Pref(RC2P_TransformHoundeye_SonicRefireTime).f + gpGlobals->time;
	m_flTransformLastAttack = gpGlobals->time;
	PLAYBACK_EVENT_FULL(0, edict(), g_usTHoundeyeSonic, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	float flMaxDev = RC2Pref(RC2P_TransformHoundeye_SonicMaxXAngDev).f;
	Vector vecAngles = pev->v_angle;
	if (vecAngles.x < -flMaxDev)
		vecAngles.x = -flMaxDev;
	else if (vecAngles.x > flMaxDev)
		vecAngles.x = flMaxDev;
	UTIL_MakeVectors(vecAngles);
	Vector vecAim = gpGlobals->v_forward;

	float flMaxVel = RC2Pref(RC2P_TransformHoundeye_SonicMaxVel).f;
	float flFullDot = RC2Pref(RC2P_TransformHoundeye_SonicFullThresholdDot).f;
	float flNoLOS = RC2Pref(RC2P_TransformHoundeye_SonicNoLOSScaler).f;
	float flMaxDist = RC2Pref(RC2P_TransformHoundeye_SonicMaxDist).f;
	float flMinDot = RC2Pref(RC2P_TransformHoundeye_SonicMinDot).f;
	float flMaxDamage = RC2Pref(RC2P_TransformHoundeye_SonicMaxDamage).f;
	float flFullDist = RC2Pref(RC2P_TransformHoundeye_SonicFullThresholdDist).f;
	float flVelExp = RC2Pref(RC2P_TransformHoundeye_SonicVelFalloffExp).f;
	float flDmgExp = RC2Pref(RC2P_TransformHoundeye_SonicDmgFalloffExp).f;
	Vector vecSrc = pev->origin + pev->view_ofs;

	UTIL_PushEntFlags();
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEnt = CBaseEntity::Instance(pent);
		if (!pEnt)
			continue;
		if (pEnt->GetEffectedEnt()->edict() == edict())
			continue;

		// VERIFY: GetRCCaps / GetRCCaps2's first argument; the original passes the player
		if ((pEnt->GetEffectedEnt()->GetRCCaps(this, this) & 0x1000) == 0 &&
			(pEnt->GetEffectedEnt()->GetRCCaps2(this, this) & 0x10) == 0)
			continue;
		if (UTIL_TestEntFlag(ENTINDEX(pEnt->GetEffectedEnt()->edict())))
			continue;

		Vector vecDir = pEnt->Center() - vecSrc;
		float flDist = vecDir.Length();
		if (flDist != 0)
			vecDir = vecDir / flDist;
		if (!(flDist <= flMaxDist))
			continue;

		float flDot = DotProduct(vecDir, vecAim);
		if (flDot < flMinDot)
			continue;

		float flDamage, flVel;
		if (flDot > flFullDot && flDist < flFullDist)
		{
			flDamage = flMaxDamage;
			flVel = flMaxVel;
		}
		else
		{
			float flFrac = flDist / flMaxDist;
			flDamage = flMaxDamage - pow(flFrac, flDmgExp) * flMaxDamage;
			flVel = flMaxVel - pow(flFrac, flVelExp) * flMaxVel;
		}
		if (flDamage <= 0 && flVel <= 0)
			continue;

		UTIL_SetEntFlag(ENTINDEX(pEnt->GetEffectedEnt()->edict()));

		TraceResult tr;
		UTIL_TraceLine(vecSrc, pEnt->Center(), ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
		{
			flDamage *= flNoLOS;
			flVel *= flNoLOS;
		}

		// pushed a quarter away from the houndeye, three quarters along its aim
		Vector vecPush = (vecDir * 0.25 + vecAim * 0.75).Normalize();

		float flBefore = flDamage;
		flDamage = RC2_ShieldFilterDamage(pEnt->GetEffectedEnt(), flDamage, 1.5, tr.vecEndPos, vecPush, NULL, NULL, 10);
		if (flDamage != flBefore && flBefore != 0)
			flVel = flDamage / flBefore * flVel;
		if (flDamage <= 0 && flVel <= 0)
			continue;

		if ((pEnt->GetEffectedEnt()->GetRCCaps2(this, this) & 0x10) == 0)
		{
			CBaseEntity* pEffected = pEnt->GetEffectedEnt();
			pEffected->pev->velocity = vecPush * flVel + pEffected->pev->velocity;
			pEnt->GetEffectedEnt()->pev->velocity.z += 50;
			pEnt->GetEffectedEnt()->pev->flags &= ~FL_ONGROUND;
		}

		// original: damages even when the shield left nothing but a push
		pEnt->GetEffectedEnt()->TakeDamage(UTIL_NamedInflictor("houndeye_sonic"), pev, flDamage, DMG_SONIC | DMG_ALWAYSGIB);
	}
	UTIL_PopEntFlags();
}

// One sample point of an entity against the beam (from vecStart along vecDir for flLength):
// within BeamStartBlastRadius of the start, or BeamCoreWidth of the line, it takes the core
// damage; within BeamRadialWidth, radial damage falling off with the distance ^ 1.3. The most
// an entity (by the one it stands for) gets is kept in pflTable, by entity index.
// orig 0xe0690
static void THoundeye_BeamDamageAt(Vector vecStart, Vector vecDir, float flLength, Vector vecPoint, CBaseEntity* pEnt, float* pflTable)
{
	float flCore = RC2Pref(RC2P_TransformHoundeye_BeamCoreWidth).f;
	float flStartRadius = RC2Pref(RC2P_TransformHoundeye_BeamStartBlastRadius).f;
	float flRadial = RC2Pref(RC2P_TransformHoundeye_BeamRadialWidth).f;

	Vector vecDelta = vecPoint - vecStart;
	if (vecDelta.Length() < flStartRadius)
	{
		int i = ENTINDEX(pEnt->GetEffectedEnt()->edict());
		if (pflTable[i] < pEnt->GetEffectedEnt()->BeamCoreDamage())
			pflTable[ENTINDEX(pEnt->GetEffectedEnt()->edict())] = pEnt->GetEffectedEnt()->BeamCoreDamage();
	}

	float flAlong = DotProduct(vecDelta, vecDir);
	if (flAlong > 0 && flAlong < flLength)
	{
		Vector vecOnLine = vecStart + vecDir * flAlong;
		float flOff = (vecPoint - vecOnLine).Length();

		if (flOff < flCore)
		{
			int i = ENTINDEX(pEnt->GetEffectedEnt()->edict());
			if (pflTable[i] < pEnt->GetEffectedEnt()->BeamCoreDamage())
				pflTable[ENTINDEX(pEnt->GetEffectedEnt()->edict())] = pEnt->GetEffectedEnt()->BeamCoreDamage();
		}
		if (flOff < flRadial)
		{
			double dMax = pEnt->GetEffectedEnt()->BeamRadialMaxDamage();
			float flDamage = dMax - pow(flOff / flRadial, 1.3) * dMax;
			if (pflTable[ENTINDEX(pEnt->GetEffectedEnt()->edict())] < flDamage)
				pflTable[ENTINDEX(pEnt->GetEffectedEnt()->edict())] = flDamage;
		}
	}
}

// orig 0xe09b0
void CBasePlayer::THoundeye_BeamFire()
{
	m_iTransformState1 = 0;
	m_flTransformTimer1 = -1;
	RemoveMoveLock(4);

	UTIL_MakeVectors(pev->v_angle);
	Vector vecFlat = Vector(gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0).Normalize();

	// recoil
	pev->velocity = pev->velocity - vecFlat * RC2Pref(RC2P_TransformHoundeye_BeamRecoilVel).f;
	pev->velocity.z += RC2Pref(RC2P_TransformHoundeye_BeamRecoilZBoost).f;
	pev->flags &= ~FL_ONGROUND;

	m_flTransformNextAttack2 = RC2Pref(RC2P_TransformHoundeye_BeamRefireTime).f + gpGlobals->time;
	m_flTransformNextAttack1 = RC2Pref(RC2P_TransformHoundeye_SonicRefireTime).f + gpGlobals->time;
	m_flTransformLastAttack = gpGlobals->time;

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x41);
	MESSAGE_END();
	PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), g_usTHoundeyeBeam, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	float flCore = RC2Pref(RC2P_TransformHoundeye_BeamCoreWidth).f - 1.0;
	float* pflTable = (float*)malloc(gpGlobals->maxEntities * sizeof(float));

	Vector vecStart = pev->origin + vecFlat * 80;
	Vector vecAim = gpGlobals->v_forward;
	if (flCore < 1.0)
		flCore = 1.0;

	// the beam's reach: on until it has punched BeamPunchDistance of solid
	float flPunch = RC2Pref(RC2P_TransformHoundeye_BeamPunchDistance).f;
	TraceResult tr;
	UTIL_PunchTrace(vecStart, vecStart + vecAim * 8192, vecAim, &flPunch, edict(), NULL, &tr, 32);
	float flLength = (tr.vecEndPos - vecStart).Length();

	memset(pflTable, 0, gpGlobals->maxEntities * sizeof(float));

	int i;
	for (i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEnt = CBaseEntity::Instance(pent);
		if (!pEnt)
			continue;
		if (pEnt->GetEffectedEnt()->pev->takedamage == DAMAGE_NO)
			continue;
		if (pEnt->GetEffectedEnt()->edict() == edict())
			continue;

		pEnt->SetObjectCollisionBox();
		THoundeye_BeamDamageAt(vecStart, vecAim, flLength, pEnt->Center(), pEnt, pflTable);

		// tall entities: sample their height in core-width steps
		if (flCore < fabs(pEnt->pev->absmax.z - pEnt->pev->absmin.z))
		{
			Vector vecPoint(pEnt->pev->origin.x, pEnt->pev->origin.y, pEnt->pev->absmin.z);
			if (vecPoint.z <= pEnt->pev->absmax.z)
			{
				do
				{
					THoundeye_BeamDamageAt(vecStart, vecAim, flLength, vecPoint, pEnt, pflTable);
					vecPoint.z = vecPoint.z + flCore;
				} while (vecPoint.z <= pEnt->pev->absmax.z);
			}
		}

		// in view of the beam anywhere along it: at least the view damage
		int iIndex = ENTINDEX(pEnt->GetEffectedEnt()->edict());
		if (pEnt->GetEffectedEnt()->BeamViewDamage() > pflTable[iIndex] && 0 < flLength)
		{
			float flAlong = 0;
			do
			{
				Vector vecPoint = vecStart + vecAim * flAlong;
				if (UTIL_PointContents(vecPoint) != CONTENTS_SOLID)
				{
					TraceResult trView;
					UTIL_TraceLine(vecStart + vecAim * flAlong, pEnt->Center(), ignore_monsters, edict(), &trView);
					if (trView.flFraction >= 1.0 || trView.pHit == pEnt->edict() || trView.pHit == pEnt->GetEffectedEnt()->edict())
					{
						pflTable[ENTINDEX(pEnt->GetEffectedEnt()->edict())] = pEnt->GetEffectedEnt()->BeamViewDamage();
						break;
					}
				}
				flAlong += 64;
			} while (flAlong < flLength);
		}
	}

	for (i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pBase = CBaseEntity::Instance(pent);
		if (!pBase)
			continue;
		CBaseEntity* pEnt = pBase->GetEffectedEnt();
		if (pEnt->pev->takedamage == DAMAGE_NO)
			continue;
		if (!(pflTable[ENTINDEX(pEnt->edict())] > 0))
			continue;

		int iBits = (pflTable[ENTINDEX(pEnt->edict())] > 100) ? 15 : 10;
		float flDamage = RC2_ShieldFilterDamage(pEnt, pflTable[ENTINDEX(pEnt->edict())], 0.6, g_vecZero, g_vecZero, NULL, NULL, iBits);
		pflTable[ENTINDEX(pEnt->edict())] = 0;
		if (flDamage > 0)
			pEnt->TakeDamage(UTIL_NamedInflictor("houndeye_beam"), pev, flDamage, DMG_ENERGYBEAM | DMG_RADIATION);
	}

	free(pflTable);
}

// The HUD's TformStat byte: bit 0 the locator, bit 1 asleep (lying down or asleep).
// orig 0xe12f0
void CBasePlayer::THoundeye_SendStatus()
{
	int iStatus = (0 != m_iTransformState2) ? 1 : 0;
	if (0 != m_iTransformState4 && m_iTransformState4 != 3)
		iStatus |= 2;

	MESSAGE_BEGIN(MSG_ONE, gmsgTformStat, NULL, edict());
	WRITE_BYTE(iStatus);
	MESSAGE_END();
}

// orig 0xe1350
void CBasePlayer::THoundeye_Think()
{
	if (m_flTransformVar1 == 0)
		m_flTransformVar1 = pev->v_angle.y;

	bool bLocatorOff = false;
	if (0 == m_iShrunk && m_bFrozen)
	{
		bLocatorOff = true;
	}
	else
	{
		if (0 == m_iShrunk)
		{
			m_flTransformVar2 = RC2Pref(RC2P_TransformHoundeye_HealthRegenRate).f * m_flFrameInterval + m_flTransformVar2;
			if (m_iTransformState4 == 2)
				m_flTransformVar2 = RC2Pref(RC2P_TransformHoundeye_SleepHealthRegenRateExtra).f * m_flFrameInterval + m_flTransformVar2;
			float flHeal = floor(m_flTransformVar2);
			if (flHeal >= 1)
			{
				TakeHealth(flHeal, DMG_GENERIC);
				m_flTransformVar2 -= flHeal;
			}
		}

		if (m_bFrozen || 0 != m_iTransformState4)
		{
			bLocatorOff = true;
		}
		else if ((m_afButtonPressed & IN_ALT1) != 0)
		{
			// the locator. Original oddity: turning it off is gated by m_flTransformNextAttack3,
			// which only the start of the transform sets; on, by m_flTransformNextAttack4
			if (0 != m_iTransformState2 && m_flTransformNextAttack3 < gpGlobals->time)
			{
				m_iTransformState2 = 0;
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.2, ATTN_NORM, 0, RANDOM_LONG(150, 160));
				m_flTransformNextAttack4 = gpGlobals->time + 0.2;
				THoundeye_SendStatus();
			}
			else if (m_flEnergy >= 2.0 && m_flTransformNextAttack4 < gpGlobals->time)
			{
				m_iTransformState2 = 1;
				m_flEnergy -= RC2Pref(RC2P_TransformHoundeye_LocatorTurnOnEnergyCost).f;
				UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_TransformHoundeye_LocatorEnergyRegenDelay).f);
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.2, ATTN_NORM, 0, RANDOM_LONG(150, 160));
				m_flTransformNextAttack4 = gpGlobals->time + 0.2;
				THoundeye_SendStatus();
			}
		}
	}

	if (bLocatorOff)
	{
		if (0 != m_iTransformState2)
			THoundeye_SendStatus();
		m_iTransformState2 = 0;
	}

	if (0 != m_iTransformState2)
	{
		m_flEnergy -= RC2Pref(RC2P_TransformHoundeye_LocatorRunEnergyUse).f * m_flFrameInterval;
		UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_TransformHoundeye_LocatorEnergyRegenDelay).f);
		if (m_flEnergy <= 0)
		{
			m_iTransformState2 = 0;
			m_flEnergy = 0;
			THoundeye_SendStatus();
		}
		m_iRC2Flags |= 0x20; // the locator is on
	}
	else
	{
		m_iRC2Flags &= ~0x20;
	}

	if (0 != m_iTransformState1)
	{
		// charging the beam
		pev->skin = 0;
		if (m_flTransformTimer1 < gpGlobals->time)
		{
			THoundeye_BeamFire();
			return;
		}
		if (0 == m_iShrunk)
			return;
		m_iTransformState1 = 0;
		m_flTransformTimer1 = -1;
		RemoveMoveLock(4);
		m_flTransformNextAttack2 = RC2Pref(RC2P_TransformHoundeye_BeamRefireTime).f + gpGlobals->time;
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfe);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(0x41);
		MESSAGE_END();
		return;
	}

	if (0 != m_iTransformState3)
	{
		// charging the sonic
		pev->skin = 0;
		if (m_flTransformTimer2 < gpGlobals->time)
		{
			THoundeye_SonicBlast();
			return;
		}
		if (0 == m_iShrunk)
			return;
		m_iTransformState3 = 0;
		m_flTransformTimer2 = -1;
		RemoveMoveLock(4);
		m_flTransformNextAttack1 = RC2Pref(RC2P_TransformHoundeye_BeamRefireTime).f + gpGlobals->time; // original: the beam's refire, and the beam's CLTE kill below
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfe);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(0x41);
		MESSAGE_END();
		return;
	}

	if (0 == m_iTransformState4)
	{
		if ((pev->button & IN_ATTACK2) != 0)
			THoundeye_BeamStart();
		else if ((pev->button & IN_ATTACK) != 0)
			THoundeye_SonicStart();
		else
			THoundeye_Jump();
	}

	if (m_bFrozen)
		return;

	if ((m_afButtonPressed & IN_CANCEL) != 0)
	{
		if (0 == m_iTransformState4)
		{
			if ((pev->flags & FL_ONGROUND) != 0)
			{
				// lie down
				AddMoveLock(4);
				pev->sequence = 18;
				pev->frame = 0;
				ResetSequenceInfo();
				m_fSequenceLoops = false;
				pev->framerate = RC2Pref(RC2P_TransformHoundeye_SleepGotoSleepFramerate).f;
				m_fSequenceFinished = false;
				m_iTransformState4 = 1;
				m_flTransformTimer3 = 10.0 / pev->framerate + gpGlobals->time;
				if (0 != m_iTransformState2)
				{
					m_iTransformState2 = 0;
					EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.2, ATTN_NORM, 0, RANDOM_LONG(150, 160));
				}
				THoundeye_SendStatus();
			}
		}
		else if (m_iTransformState4 == 2)
		{
			// get up
			pev->sequence = 19;
			pev->frame = 0;
			ResetSequenceInfo();
			m_fSequenceLoops = false;
			pev->framerate = RC2Pref(RC2P_TransformHoundeye_SleepGetUpFramerate).f;
			m_fSequenceFinished = false;
			m_iTransformState4 = 3;
			m_flTransformTimer3 = 0.8 / pev->framerate + gpGlobals->time;
			THoundeye_SendStatus();
		}
	}

	if (m_iTransformState4 == 1)
	{
		if (m_flTransformTimer3 != -1 && m_flTransformTimer3 < gpGlobals->time)
		{
			m_iTransformState4 = 2;
			m_flTransformTimer3 = -1;
			pev->sequence = 17;
			pev->frame = 0;
			ResetSequenceInfo();
			m_fSequenceLoops = true;
			pev->framerate = 1.0;
			m_fSequenceFinished = false;
		}
		// the eyes close
		if (pev->skin < 2)
			pev->skin++;
		else
			pev->skin = 2;
	}
	else if (m_iTransformState4 == 2)
	{
		if (m_flEnergyRegenTime != -1 && m_flEnergyRegenTime < gpGlobals->time)
		{
			float flEnergy = RC2Pref(RC2P_TransformHoundeye_SleepEnergyRegenRateExtra).f * m_flFrameInterval + m_flEnergy;
			if (flEnergy < 0)
			{
				m_flEnergy = 0;
				pev->skin = 2;
				goto awake;
			}
			if (flEnergy > 100)
				flEnergy = 100;
			m_flEnergy = flEnergy;
		}
		pev->skin = 2;
	}
	else if (m_iTransformState4 == 3)
	{
		if (m_flTransformTimer3 != -1 && m_flTransformTimer3 < gpGlobals->time)
		{
			m_flTransformTimer3 = -1;
			m_iTransformState4 = 0;
			SetAnimation(PLAYER_IDLE);
			pev->skin = 0;
			RemoveMoveLock(4);
			THoundeye_SendStatus();
		}
		// the eyes open
		if (pev->skin < 1)
			pev->skin = 0;
		else
			pev->skin--;
	}

awake:
	if (!m_bFrozen && 0 == m_iTransformState4)
	{
		if ((pev->button & IN_RELOAD) != 0)
			m_flTransformLastAttack = gpGlobals->time; // the alert animation on demand
		if (pev->skin > 0)
		{
			pev->skin--;
			return;
		}
		pev->skin = 0;
	}
}

// orig 0xe1ac0
void CBasePlayer::THoundeye_FixAngle()
{
	if (0 != pev->fixangle)
		m_flTransformVar1 = pev->v_angle.y;
}

// orig 0xe1ae0
void CBasePlayer::THoundeye_DeathSound()
{
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_die1.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_die2.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_die3.wav", 1, ATTN_NORM, 0, 100);
		break;
	}

	// stop a sonic charge
	if (m_iTransformState3 == 1)
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "houndeye/he_attack1.wav", 0, 0, SND_STOP, 100);
	else if (m_iTransformState3 == 2)
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "houndeye/he_attack3.wav", 0, 0, SND_STOP, 100);

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1, ATTN_NORM, 0, 100);
}

// Hurt in its sleep, the houndeye gets up at a cost in energy.
// orig 0xe1c30
void CBasePlayer::THoundeye_PainSound()
{
	if (0 != m_iTransformState4 && m_iTransformState4 != 3)
	{
		pev->sequence = 28;
		pev->frame = 0;
		ResetSequenceInfo();
		m_fSequenceLoops = false;
		pev->framerate = RC2Pref(RC2P_TransformHoundeye_SleepDamageGetUpFramerate).f;
		m_fSequenceFinished = false;
		m_iTransformState4 = 3;
		m_flTransformTimer3 = 0.8 / pev->framerate + gpGlobals->time;
		THoundeye_SendStatus();

		float flEnergy = m_flEnergy - RC2Pref(RC2P_TransformHoundeye_SleepDamageGetUpEnergyCost).f;
		if (flEnergy < 0)
			flEnergy = 0;
		else if (flEnergy > 100)
			flEnergy = 100;
		m_flEnergy = flEnergy;
		UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_TransformHoundeye_SleepDamageGetUpEnergyRegenDelay).f);
		m_flTransformLastAttack = gpGlobals->time;
	}

	if (m_flTransformTimer4 < gpGlobals->time)
	{
		m_flTransformTimer4 = RANDOM_FLOAT(0.2, 0.3) + gpGlobals->time;
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain3.wav", 1, ATTN_NORM, 0, 100);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain4.wav", 1, ATTN_NORM, 0, 100);
			break;
		case 2:
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain5.wav", 1, ATTN_NORM, 0, 100);
			break;
		}
	}
}

// orig 0xe1e00
void CBasePlayer::THoundeye_HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
		if (0 == m_iTransformState1 && !m_bFrozen)
		{
			switch (RANDOM_LONG(0, 2))
			{
			case 0:
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_hunt1.wav", 1, ATTN_NORM, 0, 100);
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_hunt2.wav", 1, ATTN_NORM, 0, 100);
				break;
			case 2:
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_hunt3.wav", 1, ATTN_NORM, 0, 100);
				break;
			}
		}
		break;
	case 2:
	case 3:
	case 6:
		break;
	case 4:
		if (0 == m_iTransformState1 && !m_bFrozen)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain3.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 5:
		if (0 == m_iTransformState1 && !m_bFrozen)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain1.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 7:
		if (0 == m_iTransformState1 && !m_bFrozen)
			pev->skin = 2; // a blink
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
	}
}
