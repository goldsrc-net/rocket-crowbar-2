/***
*
*	Rocket Crowbar 2: the stukabat transform (m_iTransform 4).
*
*	File name ours, from its event (events/tbatbite.sc) and its place in the original's link
*	order. Primary bites; IN_ALT1 lands on the floor below or hangs from the ceiling above
*	(from a moving entity too, which the client is told through TformStat), and hanging regains
*	health and energy faster. rc2.ini [TransformBat].
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "p8_needs_transform.h"

extern void FindHullIntersection(const Vector& vecSrc, TraceResult& tr, const Vector& mins, const Vector& maxs, edict_t* pEntity);

static void TBat_SendRoost(CBasePlayer* pPlayer) // ours: the TformStat message the bat sends when it stops roosting
{
	MESSAGE_BEGIN(MSG_ONE, gmsgTformStat, NULL, pPlayer->edict());
	WRITE_BYTE(pPlayer->m_iTransformState1);
	MESSAGE_END();
}

// orig 0xd8840
void CBasePlayer::TBat_SetAnimation(PLAYER_ANIM playerAnim)
{
	int iSeq;

	pev->gaitsequence = 0;
	if (playerAnim == PLAYER_DIE)
	{
		pev->sequence = (m_iTransformState1 == 1) ? 14 : 7;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}
	if (pev->deadflag != DEAD_NO)
		return;

	if (m_bFrozen)
	{
		if (m_iTransformState1 == 1)
		{
			if (pev->sequence != 18)
			{
				pev->sequence = 18;
				ResetSequenceInfo();
			}
		}
		else if (pev->sequence != 10)
		{
			pev->sequence = 10;
			ResetSequenceInfo();
		}
		return;
	}

	// let a bite, a landing or a take-off play out
	int iCur = pev->sequence;
	if ((iCur == 1 || iCur == 23 || iCur == 3) && !m_fSequenceLoops && !m_fSequenceFinished)
		return;
	if (iCur == 0 && pev->framerate > 0 && !m_fSequenceLoops && !m_fSequenceFinished)
		return;

	pev->framerate = 1.0;
	if (pev->sequence == 10)
		pev->framerate = (pev->waterlevel == 0) ? 1.5 : 0.5;
	else if (pev->sequence == 13 && pev->waterlevel != 0)
		pev->framerate = 0.25;

	if (m_iTransformState1 == 1)
	{
		// on the floor
		if (pev->velocity.Length() <= 10)
		{
			if (gpGlobals->time <= m_flTransformTimer2)
			{
				if (pev->sequence == 17 && !m_fSequenceLoops && !m_fSequenceFinished)
					iSeq = 17;
				else
					iSeq = 18;
			}
			else
			{
				iSeq = 17;
				m_flTransformTimer2 = RANDOM_FLOAT(5, 8) + gpGlobals->time;
			}
		}
		else
		{
			iSeq = 19;
			m_flTransformTimer2 = RANDOM_FLOAT(5, 8) + gpGlobals->time;
		}
	}
	else if (m_iTransformState1 == 2)
	{
		// hanging
		if (gpGlobals->time <= m_flTransformTimer2)
		{
			iSeq = pev->sequence;
			if ((iSeq != 21 && iSeq != 22) || m_fSequenceLoops || m_fSequenceFinished)
				iSeq = 20;
		}
		else
		{
			iSeq = RANDOM_LONG(21, 22);
			m_flTransformTimer2 = RANDOM_FLOAT(5, 8) + gpGlobals->time;
		}
	}
	else
	{
		// flying
		if (pev->sequence == 0 && !m_fSequenceLoops && !m_fSequenceFinished)
			iSeq = 0;
		else
			iSeq = 13;

		if (pev->velocity.Length() > 40)
		{
			UTIL_MakeVectors(pev->v_angle);
			iSeq = 10;
			if (DotProduct(gpGlobals->v_forward, pev->velocity) <= 40)
				iSeq = 13;
		}
	}

	if (pev->sequence != iSeq)
	{
		pev->sequence = iSeq;
		pev->frame = 0;
		ResetSequenceInfo();
		if (pev->sequence == 19 || pev->sequence == 18 || pev->sequence == 20)
			m_fSequenceLoops = true;
		if (pev->sequence == 17 || pev->sequence == 21 || pev->sequence == 22)
			m_fSequenceLoops = false;
	}
}

// Roost where the trace hit, if it hit the world or a brush entity flat enough: a floor
// (normal z > 0.8) to land on, a ceiling (normal z < -0.8) to hang from. Hanging remembers
// the spot, in the entity's own frame when it isn't the world, and tells the client.
// orig 0xd8c00
bool CBasePlayer::TBat_TryRoost(TraceResult* ptr)
{
	if (ptr->flFraction >= 1.0)
		return false;

	CBaseEntity* pEnt;
	if (!ptr->pHit)
	{
		pEnt = CBaseEntity::Instance(INDEXENT(0));
	}
	else
	{
		pEnt = (CBaseEntity*)GET_PRIVATE(ptr->pHit);
		if (!pEnt || !pEnt->IsBSPModel())
			return false;
	}

	if (ptr->vecPlaneNormal.z >= -0.8)
	{
		if (ptr->vecPlaneNormal.z <= 0.8)
			return false;

		m_iTransformState1 = 1;
		pev->sequence = 1;
		pev->frame = 0;
		ResetSequenceInfo();
		pev->view_ofs = Vector(0, 0, -4);
	}
	else
	{
		m_iTransformState1 = 2;
		pev->sequence = 0;
		pev->frame = 0;
		ResetSequenceInfo();
		pev->view_ofs = Vector(0, 0, -8);
	}

	m_fSequenceLoops = false;
	m_flTransformTimer3 = -1;
	m_flTransformTimer2 = RANDOM_FLOAT(8, 10) + gpGlobals->time;

	if (m_iTransformState1 == 2)
	{
		m_iTransformState2 = ENTINDEX(pEnt->edict());
		Vector vecSpot(ptr->vecEndPos.x, ptr->vecEndPos.y, ptr->vecEndPos.z - 18);
		if (0 == m_iTransformState2)
		{
			m_vecTransformVec = vecSpot;
		}
		else
		{
			Vector vecLocal = vecSpot - pEnt->pev->origin;
			UTIL_MakeVectors(pEnt->pev->angles);
			m_vecTransformVec.x = DotProduct(vecLocal, gpGlobals->v_forward);
			m_vecTransformVec.y = DotProduct(vecLocal, gpGlobals->v_right);
			m_vecTransformVec.z = DotProduct(vecLocal, gpGlobals->v_up);
		}
	}

	MESSAGE_BEGIN(MSG_ONE, gmsgTformStat, NULL, edict());
	WRITE_BYTE(m_iTransformState1);
	if (m_iTransformState1 == 2)
	{
		WRITE_SHORT(m_iTransformState2);
		WRITE_COORD(m_vecTransformVec.x);
		WRITE_COORD(m_vecTransformVec.y);
		WRITE_COORD(m_vecTransformVec.z);
	}
	MESSAGE_END();
	return true;
}

// orig 0xd8f40
void CBasePlayer::TBat_BiteStart()
{
	if (IsOnLadder() || m_bFrozen || 0 != m_iRC2Player68c || m_pTank != NULL)
		return;
	if (0 != m_iTransformState1 || m_flTransformNextAttack1 > gpGlobals->time)
		return;
	if (pev->sequence == 3 && !m_fSequenceFinished)
		return;

	m_flTransformLastAttack = gpGlobals->time;
	m_flTransformNextAttack1 = RC2Pref(RC2P_TransformBat_BiteRefireBite).f + gpGlobals->time;
	UTIL_SetMaxTime(m_flTransformNextAttack2, gpGlobals->time, RC2Pref(RC2P_TransformBat_BiteRefireOther).f);
	UTIL_SetMaxTime(m_flTransformNextAttack3, gpGlobals->time, RC2Pref(RC2P_TransformBat_BiteRefireOther).f);
	UTIL_SetMaxTime(m_flTransformNextAttack4, gpGlobals->time, RC2Pref(RC2P_TransformBat_BiteRefireOther).f);
	pev->sequence = 3;
	pev->frame = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = false;
	pev->framerate = RC2Pref(RC2P_TransformBat_BiteFramerate).f;
	m_fSequenceFinished = false;
	m_flTransformTimer3 = 0.4167 / pev->framerate + gpGlobals->time; // the bite frame
}

// The bite lands as the crowbar does: a line 32 units out, then the duck-sized hull.
// orig 0xd9090
void CBasePlayer::TBat_BiteHit()
{
	UTIL_MakeVectors(pev->v_angle);
	Vector vecSrc = pev->origin + pev->view_ofs;
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0)
	{
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (tr.flFraction < 1.0)
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (!pHit || pHit->IsBSPModel())
			{
				FindHullIntersection(vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, ENT(pev));
			}
			vecEnd = tr.vecEndPos; // This is the point on the actual surface (the hull could have hit space)
		}
	}

	if (tr.flFraction < 1.0 && UTIL_IsValidEntity(tr.pHit))
	{
		CBaseEntity* pEnt = CBaseEntity::Instance(tr.pHit)->GetEffectedEnt();
		if (pEnt->pev->takedamage != DAMAGE_NO)
		{
			float flDamage = RC2_ShieldFilterDamage(pEnt, RC2Pref(RC2P_TransformBat_BiteDamage).f, RC2Pref(RC2P_TransformBat_BiteShieldFraction).f, vecEnd, gpGlobals->v_forward, NULL, NULL, 6);
			if (flDamage <= 0)
				return;

			ClearMultiDamage();
			pEnt->TraceAttack(pev, flDamage, gpGlobals->v_forward, &tr, DMG_SLASH);
			ApplyMultiDamage(UTIL_NamedInflictor("stukabat_bite"), pev);
			PLAYBACK_EVENT_FULL(0, edict(), g_usTBatBite, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
			return;
		}
	}

	PLAYBACK_EVENT_FULL(0, edict(), g_usTBatBite, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
}

// orig 0xd9420
void CBasePlayer::TBat_Think()
{
	TraceResult tr;

	if (0 == m_iRC2Player68c && !m_bFrozen)
	{
		m_flTransformVar1 = RC2Pref(RC2P_TransformBat_HealthRegenRate).f * m_flFrameInterval + m_flTransformVar1;
		if (m_iTransformState1 == 2)
		{
			m_flTransformVar1 = RC2Pref(RC2P_TransformBat_RoostHealthRegenRateExtra).f * m_flFrameInterval + m_flTransformVar1;
			if (m_flEnergyRegenTime != -1 && m_flEnergyRegenTime < gpGlobals->time)
			{
				float flEnergy = RC2Pref(RC2P_TransformBat_RoostEnergyRegenRateExtra).f * m_flFrameInterval + m_flEnergy;
				if (flEnergy < 0)
					flEnergy = 0;
				else if (flEnergy > 100)
					flEnergy = 100;
				m_flEnergy = flEnergy;
			}
		}

		float flHeal = floor(m_flTransformVar1);
		if (flHeal >= 1)
		{
			TakeHealth(flHeal, DMG_GENERIC);
			m_flTransformVar1 -= flHeal;
		}

		// knocked off the floor
		if (m_iTransformState1 == 1 && (pev->flags & FL_ONGROUND) == 0)
		{
			m_iTransformState1 = 0;
			pev->view_ofs = Vector(0, 0, 8);
			TBat_SendRoost(this);
			TBat_SetAnimation(PLAYER_WALK);
		}

		if ((m_afButtonPressed & IN_ALT1) != 0)
		{
			if (0 == m_iTransformState1)
			{
				if (pev->velocity.Length() < 10)
				{
					UTIL_TraceLine(pev->origin, Vector(pev->origin.x, pev->origin.y, pev->origin.z - 24), dont_ignore_monsters, ENT(pev), &tr);
					if (!TBat_TryRoost(&tr))
					{
						UTIL_TraceLine(pev->origin, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 19), dont_ignore_monsters, ENT(pev), &tr);
						TBat_TryRoost(&tr);
					}
				}
			}
			else
			{
				// take off, if there's room and (hanging) it isn't the thing it hangs from
				UTIL_TraceHull(pev->origin, pev->origin, dont_ignore_monsters, head_hull, ENT(pev), &tr);
				if (tr.flFraction >= 1.0 && 0 == tr.fAllSolid && 0 == tr.fStartSolid &&
					(m_iTransformState1 != 2 || !tr.pHit || ENTINDEX(tr.pHit) != m_iTransformState2))
				{
					if (m_iTransformState1 == 1)
					{
						pev->sequence = 23;
						pev->frame = 0;
						ResetSequenceInfo();
					}
					else
					{
						pev->sequence = 0;
						pev->frame = 0;
						ResetSequenceInfo();
						pev->framerate = -3.0; // the hang-up animation backwards
						pev->frame = 255;
					}
					m_iTransformState1 = 0;
					pev->view_ofs = Vector(0, 0, 8);
					TBat_SendRoost(this);
				}
			}
		}
	}
	else if (m_iTransformState1 == 2)
	{
		m_iTransformState1 = 0;
		TBat_SendRoost(this);
		pev->view_ofs = Vector(0, 0, 8);
	}

	if (m_flTransformTimer3 == -1)
	{
		if ((pev->button & IN_ATTACK) != 0)
			TBat_BiteStart();
	}
	else
	{
		if (m_bFrozen)
		{
			m_flTransformTimer3 = -1;
			return;
		}
		if (m_flTransformTimer3 <= gpGlobals->time)
		{
			TBat_BiteHit();
			m_flTransformTimer3 = -1;
		}
	}
}

// orig 0xd98f0
void CBasePlayer::TBat_DeathSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "headcrab/hc_die2.wav", 1, ATTN_NORM, 0, RANDOM_LONG(110, 120));
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1, ATTN_NORM, 0, 100);
}

// orig 0xd99a0
void CBasePlayer::TBat_PainSound()
{
	if (m_flTransformTimer1 >= gpGlobals->time)
		return;
	m_flTransformTimer1 = RANDOM_FLOAT(0.4, 0.5) + gpGlobals->time;

	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "zombie/zo_pain1.wav", 0.7, ATTN_NORM, 0, RANDOM_LONG(110, 120));
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "zombie/zo_pain2.wav", 0.7, ATTN_NORM, 0, RANDOM_LONG(110, 120));
		break;
	}
}

// For the movement code (by player index): the roost state, and when hanging the entity and
// the spot.
// orig 0xd9a60
int TBat_GetRoost(int iPlayer, int* piEntIndex, Vector* pvecOffset)
{
	if (iPlayer <= 0 || iPlayer > gpGlobals->maxClients)
		return 0;
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return 0;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer || pPlayer->m_iTransform != 4)
		return 0;

	if (piEntIndex)
		*piEntIndex = pPlayer->m_iTransformState2;
	if (pvecOffset)
		*pvecOffset = pPlayer->m_vecTransformVec;
	return pPlayer->m_iTransformState1;
}

// For the movement code: drop out of the roost.
// orig 0xd9b00
void TBat_Unroost(int iPlayer)
{
	if (iPlayer <= 0 || iPlayer > gpGlobals->maxClients)
		return;
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer || pPlayer->m_iTransform != 4)
		return;

	if (pPlayer->m_iTransformState1 == 1)
	{
		pPlayer->pev->sequence = 23;
		pPlayer->pev->frame = 0;
		pPlayer->ResetSequenceInfo();
	}
	else
	{
		pPlayer->pev->sequence = 0;
		pPlayer->pev->frame = 0;
		pPlayer->ResetSequenceInfo();
		pPlayer->pev->framerate = -1.0; // as the think's take-off, but -1 here
		pPlayer->pev->frame = 255;
	}
	pPlayer->m_iTransformState1 = 0;
	pPlayer->pev->view_ofs = Vector(0, 0, 8);
	TBat_SendRoost(pPlayer);
}
