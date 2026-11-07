/***
*
*	Rocket Crowbar 2: the headcrab transform (m_iTransformType 5).
*
*	File name ours, by its place in the original's link order (after tentacle) and the other
*	transforms' files. The leap itself is movement code (pm_shared, another package's): it sets
*	and reads the leaping flag here by player index and reports what it lands on to
*	PM_HeadcrabLeapHit. rc2.ini [TransformHeadcrab].
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "shield.h"
#include "rc2prefs.h"

// orig 0xded30
void CBasePlayer::THeadcrab_SetAnimation(PLAYER_ANIM playerAnim)
{
	int iSeq = pev->sequence;
	pev->gaitsequence = 0;
	float flSpeed = pev->velocity.Length2D();

	if (playerAnim == PLAYER_DIE)
	{
		pev->sequence = 7;
	}
	else
	{
		if (pev->deadflag != DEAD_NO)
			return;

		if (!m_bFrozen)
		{
			if (playerAnim == PLAYER_JUMP)
			{
				pev->sequence = RANDOM_LONG(10, 12);
				pev->frame = 0;
				ResetSequenceInfo();
				m_fSequenceLoops = false;
				m_flTransformVar1 = pev->v_angle.y;
				return;
			}

			if (!IsOnLadder() && pev->waterlevel < 2 && (pev->waterlevel < 1 || (pev->flags & FL_ONGROUND) != 0))
			{
				if (m_bAntiFriction)
				{
					iSeq = 4;
				}
				else if ((pev->flags & FL_ONGROUND) == 0)
				{
					// in the air: keep a leap animation going
					if (pev->sequence < 10 || pev->sequence > 12)
						iSeq = RANDOM_LONG(10, 12);
				}
				else if (flSpeed > 40)
				{
					iSeq = 4;
				}
				else if (flSpeed > 0)
				{
					iSeq = 3;
				}
				else
				{
					iSeq = pev->sequence;
					if ((iSeq != 1 && iSeq != 2 && iSeq != 8 && iSeq != 9) || m_fSequenceLoops || m_fSequenceFinished)
						iSeq = 0;

					// turning in place
					float flDiff = UTIL_AngleDiff(UTIL_AngleMod(pev->v_angle.y), UTIL_AngleMod(m_flTransformVar1));
					if (fabs(flDiff) > 0 && fabs(flDiff) < m_flFrameInterval * 1200 && 0 == pev->fixangle)
					{
						// original oddity: 25 and 26 are the houndeye's turn sequences; iSeq is never either here
						if (flDiff < 0 || (iSeq == 25 && flDiff < m_flFrameInterval * 100))
							iSeq = 8;
						else if (flDiff > 0 || (iSeq == 26 && m_flFrameInterval * -100 < flDiff))
							iSeq = 9;

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
			}
			return;
		}

		// frozen
		if (pev->sequence == 0)
		{
			m_flTransformVar1 = pev->v_angle.y;
			return;
		}
		pev->sequence = 0;
	}

	pev->frame = 0;
	ResetSequenceInfo();
	m_flTransformVar1 = pev->v_angle.y;
}

// orig 0xdf000
void CBasePlayer::THeadcrab_Think()
{
	if (m_flTransformVar1 == 0)
		m_flTransformVar1 = pev->v_angle.y;

	if (0 == m_iShrunk && !m_bFrozen)
	{
		m_flTransformVar2 = RC2Pref(RC2P_TransformHeadcrab_HealthRegenRate).f * m_flFrameInterval + m_flTransformVar2;
		float flHeal = floor(m_flTransformVar2);
		if (flHeal >= 1)
		{
			TakeHealth(flHeal, DMG_GENERIC);
			m_flTransformVar2 -= flHeal;
		}
	}
}

// orig 0xdf090
void CBasePlayer::THeadcrab_FixAngle()
{
	if (0 != pev->fixangle)
		m_flTransformVar1 = pev->v_angle.y;
}

// orig 0xdf0b0
void CBasePlayer::THeadcrab_DeathSound()
{
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "headcrab/hc_die1.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "headcrab/hc_die2.wav", 1, ATTN_NORM, 0, 100);
		break;
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1, ATTN_NORM, 0, 100);
}

// orig 0xdf190
void CBasePlayer::THeadcrab_PainSound()
{
	if (m_flTransformTimer1 >= gpGlobals->time)
		return;
	m_flTransformTimer1 = RANDOM_FLOAT(0.2, 0.3) + gpGlobals->time;

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "headcrab/hc_pain1.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "headcrab/hc_pain2.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "headcrab/hc_pain3.wav", 1, ATTN_NORM, 0, 100);
		break;
	}
}

static CBasePlayer* THeadcrab_Player(int iPlayer) // ours: the original repeats this lookup inline
{
	if (iPlayer <= 0 || iPlayer > gpGlobals->maxClients)
		return NULL;
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return NULL;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer || pPlayer->m_iTransformType != 5)
		return NULL;
	return pPlayer;
}

// orig 0xdf260
void PM_SetHeadcrabLeap(int iPlayer, int iLeaping)
{
	CBasePlayer* pPlayer = THeadcrab_Player(iPlayer);
	if (pPlayer)
		pPlayer->m_iTransformState1 = iLeaping;
}

// orig 0xdf2c0
int PM_GetHeadcrabLeap(int iPlayer)
{
	CBasePlayer* pPlayer = THeadcrab_Player(iPlayer);
	if (pPlayer)
		return pPlayer->m_iTransformState1;
	return 0;
}

// The leaping headcrab iPlayer ran into entity iVictim at flSpeed: damage from JumpMinDamage to
// JumpMaxDamage by the speed above JumpBaseVel (a negative result heals the victim). Returns 1
// when the leap should go on through (the victim is gone, hidden, modelless or a non-solid
// brush), 0 when it stops.
// orig 0xdf320
int PM_HeadcrabLeapHit(int iPlayer, int iVictim, float flSpeed)
{
	CBasePlayer* pPlayer = THeadcrab_Player(iPlayer);
	if (!pPlayer || 0 == pPlayer->m_iTransformState1)
		return 0;
	if (iVictim <= 0 || iVictim >= gpGlobals->maxEntities)
		return 0;

	edict_t* pentVictim = INDEXENT(iVictim);
	if (!UTIL_IsValidEntity(pentVictim))
		return 0;
	CBaseEntity* pVictim = CBaseEntity::Instance(pentVictim);
	if (!pVictim)
		return 0;
	if (pVictim->pev->takedamage == DAMAGE_NO)
		return 0;

	float flOver = flSpeed - RC2Pref(RC2P_TransformHeadcrab_JumpBaseVel).f;
	if (flOver == 0)
		return 0;

	float flFrac = flOver / RC2Pref(RC2P_TransformHeadcrab_JumpMaxAddVel).f;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;
	float flDamage = flFrac * (RC2Pref(RC2P_TransformHeadcrab_JumpMaxDamage).f - RC2Pref(RC2P_TransformHeadcrab_JumpMinDamage).f) + RC2Pref(RC2P_TransformHeadcrab_JumpMinDamage).f;

	if (flDamage <= 0)
	{
		pVictim->TakeHealth(-flDamage, DMG_GENERIC);
		return 0;
	}

	flDamage = ShieldDeflect(pVictim, flDamage, RC2Pref(RC2P_TransformHeadcrab_JumpShieldFraction).f, pPlayer->pev->origin, pPlayer->pev->velocity.Normalize(), NULL, NULL, 12);
	if (flDamage <= 0)
		return 0;

	pVictim->TakeDamage(UTIL_RC2DamageInflictor("headcrab_leap"), pPlayer->pev, flDamage, DMG_SLASH | ((flDamage > 120) ? DMG_ALWAYSGIB : 0));
	EMIT_SOUND_DYN(pPlayer->edict(), CHAN_ITEM, "headcrab/hc_headbite.wav", ATTN_NORM, ATTN_NORM, 0, RANDOM_LONG(85, 105)); // volume 0.8

	if (!UTIL_IsValidEntity(pentVictim))
		return 1;
	if ((pentVictim->v.flags & FL_KILLME) != 0)
		return 1;
	if ((pentVictim->v.effects & EF_NODRAW) != 0)
		return 1;
	if (0 == pentVictim->v.modelindex)
		return 1;

	CBaseEntity* pEnt = CBaseEntity::Instance(pentVictim);
	if (!pEnt || !pEnt->IsBSPModel())
		return 0;
	if (pEnt->pev->solid != SOLID_NOT && pEnt->pev->solid != SOLID_TRIGGER)
		return 0;
	return 1;
}
