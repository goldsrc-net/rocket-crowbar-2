/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== player.cpp ========================================================

  functions dealing with the player

*/

#include <limits>
#include <algorithm>

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "player.h"
#include "rc2/virtualhull.h"
#include "trains.h"
#include "nodes.h"
#include "weapons.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "pm_shared.h"
#include "hltv.h"
#include "UserMessages.h"
#include "client.h"
#include "rc2prefs.h"
#include "rc2/freezetimer.h"
#include "rc2/antifrictimer.h"
#include "rc2/maintainent.h"
#include "rc2/racecontroller.h"
#include "rc2/transform.h"

extern int g_iRC2ScoreMode; // orig .bss 0x1afd38 (world.cpp): mp_scoremode at map start

// #define DUCKFIX

extern void CopyToBodyQue(entvars_t* pev);
extern edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer);

extern bool IsBustingGame();

#define TRAIN_ACTIVE 0x80
#define TRAIN_NEW 0xc0
#define TRAIN_OFF 0x00
#define TRAIN_NEUTRAL 0x01
#define TRAIN_SLOW 0x02
#define TRAIN_MEDIUM 0x03
#define TRAIN_FAST 0x04
#define TRAIN_BACK 0x05

#define FLASH_DRAIN_TIME 1.2  //100 units/3 minutes
#define FLASH_CHARGE_TIME 0.2 // 100 units/20 seconds  (seconds per unit)

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] =
	{
		DEFINE_FIELD(CBasePlayer, m_flFlashLightTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_iFlashBattery, FIELD_INTEGER),

		DEFINE_FIELD(CBasePlayer, m_afButtonLast, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_afButtonPressed, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_afButtonReleased, FIELD_INTEGER),

		DEFINE_ARRAY(CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS),
		DEFINE_FIELD(CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER),

		DEFINE_FIELD(CBasePlayer, m_flTimeStepSound, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flSwimTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flDuckTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flWallJumpTime, FIELD_TIME),

		DEFINE_FIELD(CBasePlayer, m_flSuitUpdate, FIELD_TIME),
		DEFINE_ARRAY(CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST),
		DEFINE_FIELD(CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER),
		DEFINE_ARRAY(CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT),
		DEFINE_ARRAY(CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT),
		DEFINE_FIELD(CBasePlayer, m_lastDamageAmount, FIELD_INTEGER),

		DEFINE_ARRAY(CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES),
		DEFINE_FIELD(CBasePlayer, m_pActiveItem, FIELD_CLASSPTR),
		DEFINE_FIELD(CBasePlayer, m_pLastItem, FIELD_CLASSPTR),
		DEFINE_FIELD(CBasePlayer, m_WeaponBits, FIELD_INT64),

		DEFINE_ARRAY(CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS),
		DEFINE_FIELD(CBasePlayer, m_idrowndmg, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_idrownrestored, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_tSneaking, FIELD_TIME),

		DEFINE_FIELD(CBasePlayer, m_iTrain, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_flFallVelocity, FIELD_FLOAT),
		DEFINE_FIELD(CBasePlayer, m_iTargetVolume, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iWeaponVolume, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iWeaponFlash, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_fLongJump, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_fInitHUD, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_tbdPrev, FIELD_TIME),

		DEFINE_FIELD(CBasePlayer, m_pTank, FIELD_EHANDLE),
		DEFINE_FIELD(CBasePlayer, m_hViewEntity, FIELD_EHANDLE),
		DEFINE_FIELD(CBasePlayer, m_iHideHUD, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iFOV, FIELD_INTEGER),

		DEFINE_FIELD(CBasePlayer, m_SndRoomtype, FIELD_INTEGER),
		// Don't save these. Let the game recalculate the closest env_sound, and continue to use the last room type like it always has.
		//DEFINE_FIELD(CBasePlayer, m_SndLast, FIELD_EHANDLE),
		//DEFINE_FIELD(CBasePlayer, m_flSndRange, FIELD_FLOAT),

		DEFINE_FIELD(CBasePlayer, m_flStartCharge, FIELD_TIME),

		//DEFINE_FIELD( CBasePlayer, m_fDeadTime, FIELD_FLOAT ), // only used in multiplayer games
		//DEFINE_FIELD( CBasePlayer, m_fGameHUDInitialized, FIELD_INTEGER ), // only used in multiplayer games
		//DEFINE_FIELD( CBasePlayer, m_flStopExtraSoundTime, FIELD_TIME ),
		//DEFINE_FIELD( CBasePlayer, m_fKnownItem, FIELD_BOOLEAN ), // reset to zero on load
		//DEFINE_FIELD( CBasePlayer, m_iPlayerSound, FIELD_INTEGER ),	// Don't restore, set in Precache()
		//DEFINE_FIELD( CBasePlayer, m_fNewAmmo, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_flgeigerRange, FIELD_FLOAT ),	// Don't restore, reset in Precache()
		//DEFINE_FIELD( CBasePlayer, m_flgeigerDelay, FIELD_FLOAT ),	// Don't restore, reset in Precache()
		//DEFINE_FIELD( CBasePlayer, m_igeigerRangePrev, FIELD_FLOAT ),	// Don't restore, reset in Precache()
		//DEFINE_FIELD( CBasePlayer, m_iStepLeft, FIELD_INTEGER ), // Don't need to restore
		//DEFINE_ARRAY( CBasePlayer, m_szTextureName, FIELD_CHARACTER, CBTEXTURENAMEMAX ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_chTextureType, FIELD_CHARACTER ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_fNoPlayerSound, FIELD_BOOLEAN ), // Don't need to restore, debug
		//DEFINE_FIELD( CBasePlayer, m_iUpdateTime, FIELD_INTEGER ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_iClientHealth, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_iClientBattery, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_iClientHideHUD, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_fWeapon, FIELD_BOOLEAN ),  // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't restore, depends on server message after spawning and only matters in multiplayer
		//DEFINE_FIELD( CBasePlayer, m_vecAutoAim, FIELD_VECTOR ), // Don't save/restore - this is recomputed
		//DEFINE_ARRAY( CBasePlayer, m_rgAmmoLast, FIELD_INTEGER, MAX_AMMO_SLOTS ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_fOnTarget, FIELD_BOOLEAN ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't need to restore

};

LINK_ENTITY_TO_CLASS(player, CBasePlayer); // orig 0x94610



// orig 0x946a0 (not in files-rc2.tsv). Rocket Crowbar 2: empty; nothing calls it
void CBasePlayer::Pain()
{
}

/* 
 *
 */
// orig 0x946b0
Vector VecVelocityForDamage(float flDamage)
{
	Vector vec(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(200, 300));

	if (flDamage > -50)
		vec = vec * 0.7;
	else if (flDamage > -200)
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

#if 0 /*                                                                 \
static void ThrowGib(entvars_t *pev, char *szGibModel, float flDamage)   \
{                                                                        \
	edict_t *pentNew = CREATE_ENTITY();                                  \
	entvars_t *pevNew = VARS(pentNew);                                   \
																	   \ \
	pevNew->origin = pev->origin;                                        \
	SET_MODEL(ENT(pevNew), szGibModel);                                  \
	UTIL_SetSize(pevNew, g_vecZero, g_vecZero);                          \
																	   \ \
	pevNew->velocity		= VecVelocityForDamage(flDamage);                  \
	pevNew->movetype		= MOVETYPE_BOUNCE;                                 \
	pevNew->solid			= SOLID_NOT;                                         \
	pevNew->avelocity.x		= RANDOM_FLOAT(0,600);                          \
	pevNew->avelocity.y		= RANDOM_FLOAT(0,600);                          \
	pevNew->avelocity.z		= RANDOM_FLOAT(0,600);                          \
	CHANGE_METHOD(ENT(pevNew), em_think, SUB_Remove);                    \
	pevNew->ltime		= gpGlobals->time;                                    \
	pevNew->nextthink	= gpGlobals->time + RANDOM_FLOAT(10,20);           \
	pevNew->frame		= 0;                                                  \
	pevNew->flags		= 0;                                                  \
}                                                                        \
																	   \ \
																	   \ \
static void ThrowHead(entvars_t *pev, char *szGibModel, floatflDamage)   \
{                                                                        \
	SET_MODEL(ENT(pev), szGibModel);                                     \
	pev->frame			= 0;                                                    \
	pev->nextthink		= -1;                                                \
	pev->movetype		= MOVETYPE_BOUNCE;                                    \
	pev->takedamage		= DAMAGE_NO;                                        \
	pev->solid			= SOLID_NOT;                                            \
	pev->view_ofs		= Vector(0,0,8);                                      \
	UTIL_SetSize(pev, Vector(-16,-16,0), Vector(16,16,56));              \
	pev->velocity		= VecVelocityForDamage(flDamage);                     \
	pev->avelocity		= RANDOM_FLOAT(-1,1) * Vector(0,600,0);              \
	pev->origin.z -= 24;                                                 \
	ClearBits(pev->flags, FL_ONGROUND);                                  \
}                                                                        \
																	   \ \
																	   \ \
*/
#endif

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed / fMax;

	if (iSpeed < 0)
		iRet = TRAIN_BACK;
	else if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

// orig 0x94860. RC2: only the suit's death alarm, none of SDK 2.3's pain sounds.
void CBasePlayer::DeathSound()
{
	// play one of the suit death alarms
	EMIT_GROUPNAME_SUIT(ENT(pev), "HEV_DEAD");
}

// override takehealth
// bitsDamageType indicates type of damage healed.

// orig 0x94880
bool CBasePlayer::TakeHealth(float flHealth, int bitsDamageType)
{
	return CBaseMonster::TakeHealth(flHealth, bitsDamageType);
}

// orig 0x948a0
Vector CBasePlayer::GetGunPosition()
{
	//	UTIL_MakeVectors(pev->v_angle);
	//	m_HackedGunPos = pev->view_ofs;
	Vector origin;

	origin = pev->origin + pev->view_ofs;

	return origin;
}

//=========================================================
// TraceAttack
//=========================================================
// orig 0x948e0. RC2: a head shot does 2.5 times the damage, every other hit group the damage
// as it comes (SDK 2.3 scaled each by its skill value).
void CBasePlayer::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (0 != pev->takedamage)
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch (ptr->iHitgroup)
		{
		case HITGROUP_HEAD:
			flDamage *= 2.5;
			break;
		default:
			break;
		}

		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage); // a little surface blood.
		TraceBleed(flDamage, vecDir, ptr, bitsDamageType);
		AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
	}
}

/*
	Take some damage.  
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO 0.2 // Armor Takes 80% of the damage
#define ARMOR_BONUS 0.5 // Each Point of Armor is work 1/x points of health

// Rocket Crowbar 2: set while TakeDamage hands damage back to the attacker (damage reflection), so
// the reflection isn't reflected again; CWorld::Precache clears it. orig .data 0x18f4dc, name ours
bool g_fRC2InDamageReflect = false;

// orig 0x949b0. RC2: a shoulder rider takes only the world's own damage; transform 6 takes none
// and a player who can't drown no drowning; the attacker's and victim's attributes scale the
// damage, and so does the victim's pev->scale; transformed players' armor stays out of it; the
// damage reflection attribute sends some of what was taken back at a player attacker for
// energy; mp_scoremode 1 counts the damage into the damage-based scores; the punch adds -2.
bool CBasePlayer::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	bool ffound = true;
	bool fmajor;
	bool fcritical;
	bool fTookDamage;
	bool ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	// on someone's shoulders only the world's own damage gets through
	if (m_iRidingIndex != 0 && (!pevAttacker || !pevInflictor || pevAttacker != pevInflictor || ENTINDEX(ENT(pevInflictor)) != 0))
		return false;

	if (m_iTransformType == 6)
		return false;

	if ((bitsDamageType & DMG_DROWN) != 0 && (PM_GetAbilityFlags(ENTINDEX(edict())) & RC2ATTRF_NODROWN) != 0)
		return false;

	CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);

	flDamage *= PM_GetAttributeScale(RC2ATTR_DEFEND, ENTINDEX(edict()));
	if (pAttacker && pAttacker->IsActivePlayer())
		flDamage *= PM_GetAttributeScale(RC2ATTR_ATTACK, ENTINDEX(ENT(pevAttacker)));

	if ((bitsDamageType & DMG_BLAST) != 0 && g_pGameRules->IsMultiplayer())
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	if (pev->scale > 0)
		flDamage /= pev->scale;

	// Already dead
	if (!IsAlive())
		return false;
	// go take the damage first

	if (!g_pGameRules->FPlayerCanTakeDamage(this, pAttacker))
	{
		// Refuse the damage
		return false;
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Armor.
	if ((m_iTransformType == 0 || m_bTransforming) && 0 != pev->armorvalue && (bitsDamageType & (DMG_FALL | DMG_DROWN)) == 0) // armor doesn't protect against fall or drown damage!
	{
		float flNew = flDamage * flRatio;

		float flArmor;

		flArmor = (flDamage - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1 / flBonus);
			flNew = flDamage - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;

		flDamage = flNew;
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	fTookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, (int)flDamage, bitsDamageType);

	// RC2: damage reflection, for energy
	if (!g_fRC2InDamageReflect && pevAttacker && ENT(pevAttacker) != edict())
	{
		const float flReflect = PM_GetAttributeScale(RC2ATTR_DMGREFLECT, ENTINDEX(edict()));
		CBaseEntity* pReflectTo = flReflect > 0 ? CBaseEntity::Instance(pevAttacker) : NULL;
		if (pReflectTo)
		{
			float flTaken = flHealthPrev - (pev->health > 0 ? pev->health : 0);
			flTaken = std::clamp(flTaken, 0.0f, pev->max_health);

			float flReflected = flTaken * flReflect;
			const float flEnergy = RC2Pref(RC2P_Attributes_DmgReflectEnergyUseFraction).f * flReflected;
			if (flEnergy != 0)
			{
				if (flEnergy <= m_flEnergy)
					m_flEnergy -= flEnergy;
				else
				{
					flReflected = m_flEnergy / RC2Pref(RC2P_Attributes_DmgReflectEnergyUseFraction).f;
					m_flEnergy = 0;
				}
			}

			if ((int)flReflected > 0)
			{
				g_fRC2InDamageReflect = true;
				pReflectTo->TakeDamage(UTIL_RC2DamageInflictor("dmg_reflect"), pev, flReflected, DMG_FALL);
				g_fRC2InDamageReflect = false;
			}
		}
	}

	// RC2: the damage-based score, by the fraction of the maximum health taken
	if (g_iRC2ScoreMode == 1)
	{
		float flFrac = flHealthPrev - (pev->health > 0 ? pev->health : 0);
		if (pev->max_health != 0)
			flFrac /= pev->max_health;

		if (flFrac > 0)
		{
			float flMulti = RC2Pref(RC2P_Score_DmgBased_DamageByWorldMulti).f;
			if (pevAttacker)
			{
				if (ENT(pevAttacker) == edict())
					flMulti = RC2Pref(RC2P_Score_DmgBased_DamageOnSelfMulti).f;
				else
				{
					// VERIFY: the original calls IsPlayer through a NULL instance (crashes) when the
					// attacker's edict has no entity
					CBaseEntity* pScorer = CBaseEntity::Instance(pevAttacker);
					if (pScorer->IsPlayer())
						static_cast<CBasePlayer*>(pScorer)->m_flDmgScore += RC2Pref(RC2P_Score_DmgBased_DamageOnPlayerMulti).f * flFrac;
					flMulti = RC2Pref(RC2P_Score_DmgBased_DamageByPlayerMulti).f;
				}
			}
			m_flDmgScore += flMulti * flFrac;
		}
	}

	Transform_PainSound();

	// reset damage time countdown for each type of time based damage player just sustained

	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if ((bitsDamageType & (DMG_PARALYZE << i)) != 0)
				m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN(MSG_SPEC, SVC_DIRECTOR);
	WRITE_BYTE(9);							  // command length in bytes
	WRITE_BYTE(DRC_CMD_EVENT);				  // take damage event
	WRITE_SHORT(ENTINDEX(this->edict()));	  // index number of primary entity
	WRITE_SHORT(ENTINDEX(ENT(pevInflictor))); // index number of secondary entity
	WRITE_LONG(5);							  // eventflags (priority and flags)
	MESSAGE_END();


	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

	// DMG_BURN
	// DMG_FREEZE
	// DMG_BLAST
	// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;			// make sure the damage bits get resent

	while (fTookDamage && (!ftrivial || (bitsDamage & DMG_TIMEBASED) != 0) && ffound && 0 != bitsDamage)
	{
		ffound = false;

		if ((bitsDamage & DMG_CLUB) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", false, SUIT_NEXT_IN_30SEC); // minor fracture
			bitsDamage &= ~DMG_CLUB;
			ffound = true;
		}
		if ((bitsDamage & (DMG_FALL | DMG_CRUSH)) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", false, SUIT_NEXT_IN_30SEC); // major fracture
			else
				SetSuitUpdate("!HEV_DMG4", false, SUIT_NEXT_IN_30SEC); // minor fracture

			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			ffound = true;
		}

		if ((bitsDamage & DMG_BULLET) != 0)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", false, SUIT_NEXT_IN_30SEC); // blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", false, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_BULLET;
			ffound = true;
		}

		if ((bitsDamage & DMG_SLASH) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", false, SUIT_NEXT_IN_30SEC); // major laceration
			else
				SetSuitUpdate("!HEV_DMG0", false, SUIT_NEXT_IN_30SEC); // minor laceration

			bitsDamage &= ~DMG_SLASH;
			ffound = true;
		}

		if ((bitsDamage & DMG_SONIC) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", false, SUIT_NEXT_IN_1MIN); // internal bleeding
			bitsDamage &= ~DMG_SONIC;
			ffound = true;
		}

		if ((bitsDamage & (DMG_POISON | DMG_PARALYZE)) != 0)
		{
			SetSuitUpdate("!HEV_DMG3", false, SUIT_NEXT_IN_1MIN); // blood toxins detected
			bitsDamage &= ~(DMG_POISON | DMG_PARALYZE);
			ffound = true;
		}

		if ((bitsDamage & DMG_ACID) != 0)
		{
			SetSuitUpdate("!HEV_DET1", false, SUIT_NEXT_IN_1MIN); // hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			ffound = true;
		}

		if ((bitsDamage & DMG_NERVEGAS) != 0)
		{
			SetSuitUpdate("!HEV_DET0", false, SUIT_NEXT_IN_1MIN); // biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			ffound = true;
		}

		if ((bitsDamage & DMG_RADIATION) != 0)
		{
			SetSuitUpdate("!HEV_DET2", false, SUIT_NEXT_IN_1MIN); // radiation detected
			bitsDamage &= ~DMG_RADIATION;
			ffound = true;
		}
		if ((bitsDamage & DMG_SHOCK) != 0)
		{
			bitsDamage &= ~DMG_SHOCK;
			ffound = true;
		}
	}

	pev->punchangle.x -= 2; // RC2: SDK 2.3 set it to -2

	if (fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75)
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", false, SUIT_NEXT_IN_30MIN); // automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", false, SUIT_NEXT_IN_30MIN); // morphine shot
	}

	if (fTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (pev->health < 6)
			SetSuitUpdate("!HEV_HLTH3", false, SUIT_NEXT_IN_10MIN); // near death
		else if (pev->health < 20)
			SetSuitUpdate("!HEV_HLTH2", false, SUIT_NEXT_IN_10MIN); // health critical

		// give critical health warnings
		if (!RANDOM_LONG(0, 3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", false, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if (fTookDamage && (bitsDamageType & DMG_TIMEBASED) != 0 && flHealthPrev < 75)
	{
		if (flHealthPrev < 50)
		{
			if (!RANDOM_LONG(0, 3))
				SetSuitUpdate("!HEV_DMG7", false, SUIT_NEXT_IN_5MIN); //seek medical attention
		}
		else
			SetSuitUpdate("!HEV_HLTH1", false, SUIT_NEXT_IN_10MIN); // health dropping
	}

	return fTookDamage;
}

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
// orig 0x95280
void CBasePlayer::PackDeadPlayerItems()
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon* rgpPackWeapons[MAX_WEAPONS];
	int iPackAmmo[MAX_AMMO_SLOTS + 1];
	int iPW = 0; // index into packweapons array
	int iPA = 0; // index into packammo array

	memset(rgpPackWeapons, 0, sizeof(rgpPackWeapons));
	memset(iPackAmmo, -1, sizeof(iPackAmmo));

	// get the game rules
	iWeaponRules = g_pGameRules->DeadPlayerWeapons(this);
	iAmmoRules = g_pGameRules->DeadPlayerAmmo(this);

	if (iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO)
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems(true);
		return;
	}

	// go through all of the weapons and make a list of the ones to pack
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem* pPlayerItem = m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				switch (iWeaponRules)
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if (m_pActiveItem && pPlayerItem == m_pActiveItem)
					{
						CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)pPlayerItem;
						int nIndex = iPW++;

						// this is the active item. Pack it.
						rgpPackWeapons[nIndex] = pWeapon;

						// Reload the weapon before dropping it if we have ammo
						int j = V_min(pWeapon->iMaxClip() - pWeapon->m_iClip, m_rgAmmo[pWeapon->m_iPrimaryAmmoType]);

						// Add them to the clip
						pWeapon->m_iClip += j;
						m_rgAmmo[pWeapon->m_iPrimaryAmmoType] -= j;

						TabulateAmmo();
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					rgpPackWeapons[iPW++] = (CBasePlayerWeapon*)pPlayerItem;
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	// now go through ammo and make a list of which types to pack.
	if (iAmmoRules != GR_PLR_DROP_AMMO_NO)
	{
		for (i = 0; i < MAX_AMMO_SLOTS; i++)
		{
			if (m_rgAmmo[i] > 0)
			{
				// player has some ammo of this type.
				switch (iAmmoRules)
				{
				case GR_PLR_DROP_AMMO_ALL:
					iPackAmmo[iPA++] = i;
					break;

				case GR_PLR_DROP_AMMO_ACTIVE:
					if (m_pActiveItem && i == m_pActiveItem->PrimaryAmmoIndex())
					{
						// this is the primary ammo type for the active weapon
						iPackAmmo[iPA++] = i;
					}
					else if (m_pActiveItem && i == m_pActiveItem->SecondaryAmmoIndex())
					{
						// this is the secondary ammo type for the active weapon
						iPackAmmo[iPA++] = i;
					}
					break;

				default:
					break;
				}
			}
		}
	}

	// create a box to pack the stuff into.
	CWeaponBox* pWeaponBox = (CWeaponBox*)CBaseEntity::Create("weaponbox", pev->origin, pev->angles, edict());

	pWeaponBox->pev->angles.x = 0; // don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	pWeaponBox->SetThink(&CWeaponBox::Kill);
	pWeaponBox->pev->nextthink = gpGlobals->time + 120;

	// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

	if (IsBustingGame())
	{
		if (HasNamedPlayerItem("weapon_egon"))
		{
			for (i = 0; i < MAX_ITEM_TYPES; i++)
			{
				CBasePlayerItem* pItem = m_rgpPlayerItems[i];

				if (pItem)
				{
					if (!strcmp("weapon_egon", STRING(pItem->pev->classname)))
					{
						pWeaponBox->PackWeapon(pItem);

						SET_MODEL(ENT(pWeaponBox->pev), "models/w_egon.mdl");

						pWeaponBox->pev->velocity = Vector(0, 0, 0);
						pWeaponBox->pev->renderfx = kRenderFxGlowShell;
						pWeaponBox->pev->renderamt = 25;
						pWeaponBox->pev->rendercolor = Vector(0, 75, 250);

						break;
					}
				}
			}
		}
	}
	else
	{
		bool bPackItems = true;

		if (iAmmoRules == GR_PLR_DROP_AMMO_ACTIVE && iWeaponRules == GR_PLR_DROP_GUN_ACTIVE)
		{
			if (rgpPackWeapons[0] && FClassnameIs(rgpPackWeapons[0]->pev, "weapon_satchel") && (iPackAmmo[0] == -1 || (m_rgAmmo[iPackAmmo[0]] == 0)))
			{
				bPackItems = false;
			}
		}

		if (bPackItems)
		{
			// pack the ammo
			while (iPackAmmo[iPA] != -1)
			{
				pWeaponBox->PackAmmo(MAKE_STRING(CBasePlayerItem::AmmoInfoArray[iPackAmmo[iPA]].pszName), m_rgAmmo[iPackAmmo[iPA]]);
				iPA++;
			}

			// now pack all of the items in the lists
			while (rgpPackWeapons[iPW])
			{
				// weapon unhooked from the player. Pack it into der box.
				pWeaponBox->PackWeapon(rgpPackWeapons[iPW]);

				iPW++;
			}
		}

		pWeaponBox->pev->velocity = pev->velocity * 1.2; // weaponbox has player's velocity, then some.
	}

	RemoveAllItems(true); // now strip off everything that wasn't handled by the code above.
}

// orig 0x954c0. RC2: SDK 2.3's, ending with an empty CurWeapon; halflife-updated's m_pTank release
// and weapon bits stay.
void CBasePlayer::RemoveAllItems(bool removeSuit)
{
	if (m_pActiveItem)
	{
		ResetAutoaim();
		m_pActiveItem->Holster();
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}

	int i;
	CBasePlayerItem* pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while (m_pActiveItem)
		{
			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop();
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel = 0;
	pev->weaponmodel = 0;

	m_WeaponBits = 0ULL;
	// Mirror m_WeaponBits = 0 onto the documented pev->weapons mask so
	// server-side consumers (Metamod plugins, the engine's
	// cdata->iWeaponBits delivery to vanilla clients) see the wipe too.
	// SetHasSuit() below goes through SetWeaponBit() / ClearWeaponBit(),
	// which are paired to maintain pev->weapons in lockstep with
	// m_WeaponBits.
	pev->weapons = 0;

	//Re-add suit bit if needed.
	SetHasSuit(!removeSuit);

	for (i = 0; i < MAX_AMMO_SLOTS; i++)
		m_rgAmmo[i] = 0;

	UpdateClientData();

	// RC2: tell the client no weapon is selected (CurWeapon is RC2's four bytes)
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_SHORT(0);
	MESSAGE_END();
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */

//=========================================================
// Rocket Crowbar 2: per-player attributes by entity index (orig 0x95690-0x95970). Each public
// function is a wrapper the rest of rc2.dll calls around a static inner one, as in the
// original; the names are ours.
//=========================================================

// orig 0x95690. Attribute iAttr of player iPlayer, 1 without one
static float AttrOf(int iAttr, int iPlayer)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (!pPlayer)
		return 1;
	return pPlayer->m_flAttributes[iAttr];
}

// orig 0x956c0
static int AttrFlagsOf(int iPlayer)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (!pPlayer)
		return 0;
	return pPlayer->m_iAbilities;
}

// orig 0x956e0
static void SetAttrIntOf(int iPlayer, int i, int iValue)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (pPlayer)
		pPlayer->m_rgiAttrInt[i] = iValue;
}

// orig 0x95710
static int GetAttrIntOf(int iPlayer, int i)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (!pPlayer)
		return 0;
	return pPlayer->m_rgiAttrInt[i];
}

// orig 0x95730 (not in files-rc2.tsv)
static void SetAttrFloatOf(int iPlayer, int i, float flValue)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (pPlayer)
		pPlayer->m_rgflAttrValue[i] = flValue;
}

// orig 0x95760 (not in files-rc2.tsv)
static float GetAttrFloatOf(int iPlayer, int i)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (!pPlayer)
		return 0;
	return pPlayer->m_rgflAttrValue[i];
}

// orig 0x95790. The player's energy, 100 without one
static float EnergyOf(int iPlayer)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (!pPlayer)
		return 100;
	return pPlayer->m_flEnergy;
}

// orig 0x957b0. Spend energy (not below 0) and hold its regeneration off for flRegenDelay
static void UseEnergyOf(int iPlayer, float flAmount, float flRegenDelay)
{
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(iPlayer));
	if (!pPlayer)
		return;

	pPlayer->m_flEnergy -= flAmount;
	if (pPlayer->m_flEnergy < 0)
		pPlayer->m_flEnergy = 0;

	UTIL_ExtendTime(&pPlayer->m_flEnergyRegenTime, gpGlobals->time, flRegenDelay);
}

// orig 0x95810. Mode 0 shows the client effect CLTE 2 on the player to those near him; the
// vectors go unused
static void EnergyEffectOf(int iPlayer, int iMode, float* vecA, float* vecB)
{
	CBaseEntity* pPlayer = UTIL_PlayerByIndex(iPlayer);
	if (!pPlayer || iMode != 0)
		return;

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pPlayer->pev->origin);
	WRITE_BYTE(2);
	WRITE_BYTE(iPlayer);
	MESSAGE_END();
}

// orig 0x95860. Attribute flag 0x200 of a player entity
bool PlayerHasAttrFlag200(CBaseEntity* pPlayer)
{
	return ((PM_GetAbilityFlags(ENTINDEX(pPlayer->edict())) >> 9) & 1) != 0;
}

float PM_GetAttributeScale(int iAttr, int iPlayer) { return AttrOf(iAttr, iPlayer); }									 // orig 0x95890
int PM_GetAbilityFlags(int iPlayer) { return AttrFlagsOf(iPlayer); }												 // orig 0x958b0
void PM_SetPlayerTimer(int iPlayer, int i, int iValue) { SetAttrIntOf(iPlayer, i, iValue); }						 // orig 0x958c0
int PM_GetPlayerTimer(int iPlayer, int i) { return GetAttrIntOf(iPlayer, i); }									 // orig 0x958e0
void SetPlayerAttrFloat(int iPlayer, int i, float flValue) { SetAttrFloatOf(iPlayer, i, flValue); }				 // orig 0x95900 (not in files-rc2.tsv)
float GetPlayerAttrFloat(int iPlayer, int i) { return GetAttrFloatOf(iPlayer, i); }								 // orig 0x95920 (not in files-rc2.tsv)
float PM_GetEnergy(int iPlayer) { return EnergyOf(iPlayer); }													 // orig 0x95940
void PM_UseEnergy(int iPlayer, float flAmount, float flRegenDelay) { UseEnergyOf(iPlayer, flAmount, flRegenDelay); } // orig 0x95950
void PM_JetboostEffect(int iPlayer, int iMode, float* vecA, float* vecB) { EnergyEffectOf(iPlayer, iMode, vecA, vecB); } // orig 0x95970

// orig 0x955e0. Players riding each other see each other (orig slot 77)
bool CBasePlayer::IsViewOfPlayer(CBaseEntity* pPlayer)
{
	if (!pPlayer->IsPlayer())
		return false;

	const int iOther = ENTINDEX(pPlayer->edict());
	const int iMe = ENTINDEX(edict());
	if (iOther == 0 || iMe == 0 || iOther == iMe)
		return false;

	if (m_iRidingIndex == iOther)
		return true;

	CBasePlayer* pOther = static_cast<CBasePlayer*>(pPlayer);
	if (m_iShoulderRider[0] == iOther || m_iShoulderRider[1] == iOther || pOther->m_iRidingIndex == iMe || pOther->m_iShoulderRider[0] == iMe || pOther->m_iShoulderRider[1] == iMe)
		return true;

	return CBaseEntity::IsViewOfPlayer(pPlayer);
}

// orig 0x959b0. Before player iPlayer moves, everything that is a view of him (riders and
// the like, IsViewOfPlayer) goes non-solid and far away for the move; the callback (from
// pm_shared) takes entity index, solid, origin. Its solid is kept in the entity's
// m_iRC2Base6c.
void PM_HideViewers(int iPlayer, void (*pfnSetSolid)(int iEnt, int iSolid, float* vecOrigin))
{
	edict_t* pentPlayer = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pentPlayer))
		return;

	CBaseEntity* pPlayer = CBaseEntity::Instance(pentPlayer);
	if (!pPlayer)
		return;

	Vector vecFar(8192, 8192, 8192);
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;

		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity || !pEntity->IsViewOfPlayer(pPlayer))
			continue;

		pEntity->m_iRC2Base6c = pEntity->pev->solid;
		pEntity->pev->solid = SOLID_NOT;
		pfnSetSolid(i, SOLID_NOT, vecFar);
	}
}

// orig 0x95ab0. And back after the move
void PM_RestoreViewers(int iPlayer, void (*pfnSetSolid)(int iEnt, int iSolid, float* vecOrigin))
{
	edict_t* pentPlayer = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pentPlayer))
		return;

	CBaseEntity* pPlayer = CBaseEntity::Instance(pentPlayer);
	if (!pPlayer)
		return;

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;

		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity || !pEntity->IsViewOfPlayer(pPlayer))
			continue;

		pEntity->pev->solid = pEntity->m_iRC2Base6c;
		pfnSetSolid(i, pEntity->pev->solid, pEntity->pev->origin);
	}
}

// orig 0x95b80. RC2's dying camera: m_pCam goes up to 200 units (times a scale from the speed,
// or from the health when gibbed) out at a random yaw and a 10-80 degree pitch, in the open,
// looking back at the body, and the player sees through it. Five tries, else straight above.
void CBasePlayer::StartDyingCam(bool bGibbed)
{
	Vector vecCam = pev->origin + Vector(0, 0, 48);
	Vector vecAngles(0, 90, 0);

	float flScale;
	if (bGibbed)
		flScale = fabs(pev->health) * 0.005;
	else
		flScale = pev->velocity.Length() * 0.003;
	flScale = std::clamp(flScale, 1.0f, 2.5f);

	TraceResult tr;
	Vector vecTry;
	int iTries = 5;
	int iLeft;
	bool bFound = true;
	while (true)
	{
		vecTry = Vector(RANDOM_FLOAT(-10, -80), RANDOM_FLOAT(0, 360), 0);
		UTIL_MakeVectors(vecTry);
		Vector vecEnd = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT(100, 200) * flScale;
		UTIL_TraceLine(pev->origin, vecEnd, ignore_monsters, edict(), &tr);

		iLeft = iTries - 1;
		if (iTries < 1)
		{
			bFound = false;
			break;
		}
		iTries = iLeft;

		if ((tr.vecEndPos - pev->origin).Length() >= 64 && 0 == tr.fAllSolid && UTIL_PointContents(tr.vecEndPos) != CONTENTS_SOLID)
			break;
	}

	// VERIFY: a spot found on the last allowed try (iLeft 0) is not used, as in the original
	if (bFound && iLeft > 0)
	{
		vecAngles.x = vecTry.x;
		vecAngles.y = UTIL_AngleMod(vecTry.y - 180);
		vecCam = tr.vecEndPos;
		if (tr.flFraction < 1.0)
			vecCam = vecCam - tr.vecPlaneNormal * 8;
	}

	UTIL_SetOrigin(m_pCam->pev, vecCam);
	m_pCam->pev->origin = vecCam;
	m_pCam->pev->v_angle = vecAngles;
	m_pCam->pev->angles = m_pCam->pev->v_angle;

	unsigned char *pvs, *pas;
	SetupVisibility(m_pCam->edict(), edict(), &pvs, &pas);
	SET_VIEW(edict(), m_pCam->edict());
}

// orig 0x96020. RC2: the riders on this player's shoulders die with him and he gets off the one
// he rides; his freeze and antifriction timers go; a transformation in progress is finished or
// undone; the jetpack, energy draw, phys flags, view mode, render effects, player stat and
// virtual hulls are reset; transformed players don't jump on death; the dying camera
// (StartDyingCam) takes the view. A rider who dies just vanishes.
void CBasePlayer::Killed(entvars_t* pevAttacker, int iGib)
{
	CSound* pSound;

	if (pev->deadflag != DEAD_NO)
		return;

	for (int i = 0; i < 2; i++)
	{
		if (m_iShoulderRider[i] == 0)
			continue;
		CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[i]));
		entvars_t* pevSavedInflictor = g_pevLastInflictor;
		if (pRider)
		{
			pRider->pev->health = pev->health;
			pRider->Killed(pevAttacker, iGib);
		}
		g_pevLastInflictor = pevSavedInflictor;
	}

	if (m_iRidingIndex != 0)
	{
		CBasePlayer* pRidden = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iRidingIndex));
		if (pRidden)
			pRidden->m_iShoulderRider[m_iRideShoulder != 0 ? 1 : 0] = 0;
	}

	FreezeTimer_Thaw(edict());
	StopAntiFriction(edict());

	// Holster weapon immediately, to allow it to cleanup
	if (m_pActiveItem && 0 == m_iShrunk && 0 == m_iTransformType && !m_bTransforming)
		m_pActiveItem->Holster();

	if (m_bTransforming)
	{
		if (m_iTransformType == 0)
			FinishUntransform();
		else
			FinishTransform();
		m_bTransforming = false;
	}

	SetTouch(NULL);

	g_pGameRules->PlayerKilled(this, pevAttacker, g_pevLastInflictor);

	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}

	if (m_fJetpackOn)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(4);
		WRITE_BYTE(ENTINDEX(edict()));
		WRITE_BYTE(0);
		MESSAGE_END();
		m_fJetpackOn = false;
	}

	if (m_fDrawingEnergy)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(6);
		WRITE_BYTE(ENTINDEX(edict()));
		MESSAGE_END();
		m_fDrawingEnergy = false;
	}

	// end the client's effect 64 on this player
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(254);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(64);
	MESSAGE_END();

	m_pCam2->m_iRC2Flags = 0;
	ResetPhysFlags();
	m_fDrawingEnergy = false;
	m_fCreatingHealth = false;
	m_iShrunk = 0;
	UTIL_KillEntityEffects(edict());

	MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	m_ucRideInfo = 0;
	m_iClientPlayerStat = 0;
	MESSAGE_BEGIN(MSG_ALL, gmsgPlrStat);
	WRITE_BYTE(ENTINDEX(edict()));
	WRITE_BYTE(m_iClientPlayerStat);
	MESSAGE_END();

	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
	pev->rendercolor = g_vecZero;

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(edict()));
	{
		if (pSound)
		{
			pSound->Reset();
		}
	}

	for (int i = 0; i < 16; i++)
	{
		if (m_rgpVirtualHulls[i])
		{
			m_rgpVirtualHulls[i]->pev->solid = SOLID_NOT;
			UTIL_SetSize(m_rgpVirtualHulls[i]->pev, g_vecZero, g_vecZero);
			m_rgpVirtualHulls[i]->pev->flags |= FL_KILLME;
			m_rgpVirtualHulls[i] = NULL;
		}
	}

	SetAnimation(PLAYER_DIE);

	if (m_iTransformType != 0)
		Transform_Killed();

	m_flRespawnTimer = 0.0f;

	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	ClearBits(pev->flags, FL_ONGROUND);
	if (m_iTransformType == 0 && pev->velocity.z < 10)
		pev->velocity.z += RANDOM_FLOAT(0, 300);

	if (FlashlightIsOn())
		FlashlightTurnOff();
	pev->effects &= ~EF_DIMLIGHT;

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, false, 0);

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, pev);
	WRITE_SHORT(m_iClientHealth);
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead (CurWeapon is RC2's four bytes)
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0XFF);
	WRITE_SHORT(-1);
	MESSAGE_END();

	// reset FOV
	m_iFOV = m_iClientFOV = 0;
	pev->fov = 0;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
	WRITE_BYTE(0);
	MESSAGE_END();

	if ((pev->health < -40 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		pev->solid = SOLID_NOT;
		GibMonster(); // This clears pev->model
		pev->effects |= EF_NODRAW;
		StartDyingCam(true);
		m_iShoulderRider[1] = m_iShoulderRider[0] = m_iRidingIndex = 0;
		pev->owner = NULL;
		return;
	}

	if (m_iRidingIndex != 0)
	{
		// a rider vanishes. VERIFY: and doesn't think again until something respawns him
		pev->solid = SOLID_NOT;
		pev->effects |= EF_NODRAW;
		m_iShoulderRider[1] = m_iShoulderRider[0] = m_iRidingIndex = 0;
		pev->owner = NULL;
		return;
	}

	m_iShoulderRider[1] = m_iShoulderRider[0] = m_iRidingIndex = 0;
	pev->owner = NULL;

	if (m_iTransformType == 0)
		DeathSound();
	else
		Transform_DeathSound();

	pev->angles.x = 0;
	pev->angles.z = 0;

	SetThink(&CBasePlayer::PlayerDeathThink);
	pev->nextthink = gpGlobals->time + 0.1;

	StartDyingCam(false);
}


// Set the activity based on an event or current state
// orig 0x96670. RC2: a transformed player animates as what he became (and transform 6 holds
// sequence 0); a frozen one stands in ref_aim; one riding a shoulder or slipping on
// antifriction walks in place.
void CBasePlayer::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;
	float speed;
	char szAnim[64];

	if (m_iTransformType == 6)
	{
		if (pev->sequence == 0)
			return;
		pev->sequence = 0;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

	if (m_iTransformType != 0)
	{
		Transform_SetAnimation(playerAnim);
		return;
	}

	speed = pev->velocity.Length2D();

	if ((pev->flags & FL_FROZEN) != 0)
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	if (RC2Frozen())
	{
		pev->gaitsequence = LookupSequence("deep_idle");
		strcpy(szAnim, "ref_aim_");
		strcat(szAnim, m_szAnimExtention);
		animDesired = LookupSequence(szAnim);
		if (animDesired == -1)
			animDesired = 0;
		if (pev->sequence == animDesired)
			return;
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

	switch (playerAnim)
	{
	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		break;

	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		break;

	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity();
		break;

	case PLAYER_ATTACK1:
		switch (m_Activity)
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if (!FBitSet(pev->flags, FL_ONGROUND) && (m_Activity == ACT_HOP || m_Activity == ACT_LEAP)) // Still jumping
		{
			m_IdealActivity = m_Activity;
		}
		else if (pev->waterlevel > 1)
		{
			if (speed == 0)
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
		{
			m_IdealActivity = ACT_WALK;
		}
		break;
	}

	if (m_iRidingIndex != 0)
	{
		m_Activity = m_IdealActivity = ACT_WALK;
		speed = 0;
	}

	if (m_bAntiFriction)
	{
		m_Activity = m_IdealActivity = ACT_WALK;
	}

	switch (m_IdealActivity)
	{
	case ACT_HOVER:
	case ACT_LEAP:
	case ACT_SWIM:
	case ACT_HOP:
	case ACT_DIESIMPLE:
	default:
		if (m_Activity == m_IdealActivity)
			return;
		m_Activity = m_IdealActivity;

		animDesired = LookupActivity(m_Activity);
		// Already using the desired animation?
		if (pev->sequence == animDesired)
			return;

		//ALERT(at_console, "Set die animation to %d\n", animDesired);

		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
		return;

	case ACT_RANGE_ATTACK1:
		if (FBitSet(pev->flags, FL_DUCKING)) // crouching
			strcpy(szAnim, "crouch_shoot_");
		else
			strcpy(szAnim, "ref_shoot_");
		strcat(szAnim, m_szAnimExtention);
		animDesired = LookupSequence(szAnim);
		if (animDesired == -1)
			animDesired = 0;

		if (pev->sequence != animDesired || !m_fSequenceLoops)
		{
			pev->frame = 0;
		}

		if (!m_fSequenceLoops)
		{
			pev->effects |= EF_NOINTERP;
		}

		m_Activity = m_IdealActivity;

		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_WALK:
		if (m_Activity != ACT_RANGE_ATTACK1 || m_fSequenceFinished)
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				strcpy(szAnim, "crouch_aim_");
			else
				strcpy(szAnim, "ref_aim_");
			strcat(szAnim, m_szAnimExtention);
			animDesired = LookupSequence(szAnim);
			if (animDesired == -1)
				animDesired = 0;
			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
	}

	if (FBitSet(pev->flags, FL_DUCKING))
	{
		if (speed == 0)
		{
			pev->gaitsequence = LookupActivity(ACT_CROUCHIDLE);
			// pev->gaitsequence	= LookupActivity( ACT_CROUCH );
		}
		else
		{
			pev->gaitsequence = LookupActivity(ACT_CROUCH);
		}
	}
	else if (speed > 220)
	{
		pev->gaitsequence = LookupActivity(ACT_RUN);
	}
	else if (speed > 0)
	{
		pev->gaitsequence = LookupActivity(ACT_WALK);
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence = LookupSequence("deep_idle");
	}


	// Already using the desired animation?
	if (pev->sequence == animDesired)
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	pev->sequence = animDesired;
	pev->frame = 0;
	ResetSequenceInfo();
}

/*
===========
TabulateAmmo
This function is used to find and store 
all the ammo we have into the ammo vars.
============
*/
void CBasePlayer::TabulateAmmo()
{
	ammo_9mm = AmmoInventory(GetAmmoIndex("9mm"));
	ammo_357 = AmmoInventory(GetAmmoIndex("357"));
	ammo_argrens = AmmoInventory(GetAmmoIndex("ARgrenades"));
	ammo_bolts = AmmoInventory(GetAmmoIndex("bolts"));
	ammo_buckshot = AmmoInventory(GetAmmoIndex("buckshot"));
	ammo_rockets = AmmoInventory(GetAmmoIndex("rockets"));
	ammo_uranium = AmmoInventory(GetAmmoIndex("uranium"));
	ammo_hornets = AmmoInventory(GetAmmoIndex("Hornets"));
}


/*
===========
WaterMove
============
*/
#define AIRTIME 12 // lung full of air lasts this many seconds

// orig 0x96cc0. RC2: a player whose attributes keep him from drowning only makes bubbles and
// swim sounds under water; the drowning damage comes from a "water" inflictor with the player as
// attacker; the energy draw and health creation silence the water sounds; no lava or slime
// damage. halflife-updated's count of the drowning damage really taken stays.
void CBasePlayer::WaterMove()
{
	int air;

	if (pev->movetype == MOVETYPE_NOCLIP)
		return;

	if (pev->health < 0)
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	const bool bNoDrown = (PM_GetAbilityFlags(ENTINDEX(edict())) & RC2ATTRF_NODROWN) != 0;

	if (!bNoDrown)
	{
		if (pev->waterlevel != 3)
		{
			// not underwater

			// play 'up for air' sound
			if (!m_fCreatingHealth && !m_fDrawingEnergy)
			{
				if (pev->air_finished < gpGlobals->time)
					EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade1.wav", 1, ATTN_NORM);
				else if (pev->air_finished < gpGlobals->time + 9)
					EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade2.wav", 1, ATTN_NORM);
			}

			pev->air_finished = gpGlobals->time + AIRTIME;
			pev->dmg = 2;

			// if we took drowning damage, give it back slowly
			if (m_idrowndmg > m_idrownrestored)
			{
				m_bitsDamageType |= DMG_DROWNRECOVER;
				m_bitsDamageType &= ~DMG_DROWN;
				m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
			}
		}
		else
		{ // fully under water
			// stop restoring damage while underwater
			m_bitsDamageType &= ~DMG_DROWNRECOVER;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;

			if (pev->air_finished < gpGlobals->time) // drown!
			{
				if (pev->pain_finished < gpGlobals->time)
				{
					// take drowning damage
					pev->dmg += 1;
					if (pev->dmg > 5)
						pev->dmg = 5;

					const float oldHealth = pev->health;

					TakeDamage(UTIL_RC2DamageInflictor("water"), pev, pev->dmg, DMG_DROWN);
					pev->pain_finished = gpGlobals->time + 1;

					// Account for god mode, the unkillable flag, potential damage mitigation
					// and gaining health from drowning to avoid counting damage not actually taken.
					const float drownDamageTaken = std::max(0.f, std::floor(oldHealth - pev->health));

					m_idrowndmg += drownDamageTaken;
				}
			}
			else
			{
				m_bitsDamageType &= ~DMG_DROWN;
			}
		}
	}
	else if (pev->waterlevel != 3)
	{
		pev->air_finished = gpGlobals->time + RANDOM_FLOAT(2, 3);
	}
	else if (pev->air_finished < gpGlobals->time)
	{
		// can't drown: bubbles now and then
		if (!m_fCreatingHealth && !m_fDrawingEnergy)
		{
			switch (RANDOM_LONG(0, 3))
			{
			case 0:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM);
				break;
			case 2:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM);
				break;
			case 3:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM);
				break;
			}
		}

		UTIL_MakeVectors(pev->v_angle);
		Vector vecMouth = pev->origin + pev->view_ofs - gpGlobals->v_forward * 8;
		UTIL_Bubbles(vecMouth - Vector(5, 5, 5), vecMouth + Vector(5, 5, 5), RANDOM_LONG(4, 8));

		pev->air_finished = gpGlobals->time + RANDOM_FLOAT(2, 3);
	}

	if (0 == pev->waterlevel)
	{
		if (FBitSet(pev->flags, FL_INWATER))
		{
			ClearBits(pev->flags, FL_INWATER);
		}
		return;
	}

	// make bubbles

	if (!bNoDrown)
	{
		air = (int)(pev->air_finished - gpGlobals->time);
		if (!RANDOM_LONG(0, 0x1f) && RANDOM_LONG(0, AIRTIME - 1) >= air)
		{
			switch (RANDOM_LONG(0, 3))
			{
			case 0:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM);
				break;
			case 2:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM);
				break;
			case 3:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM);
				break;
			}
		}
	}

	if (!FBitSet(pev->flags, FL_INWATER))
	{
		SetBits(pev->flags, FL_INWATER);
		pev->dmgtime = 0;
	}
}


// true if the player is attached to a ladder
bool CBasePlayer::IsOnLadder()
{
	return (pev->movetype == MOVETYPE_FLY);
}

// orig 0x97290. RC2: the body is non-solid; transformed players play their own falls and
// last frames and wait longer (or for ever) before they can respawn; a transformed player's
// weapons are removed, not packed; no body copy as soon as the death animation ends; the
// forcespawn command respawns too. halflife-updated's time-based wait (the original counts
// frames: 120, or 200 for transforms 3 and 4) and its spectator check stay.
void CBasePlayer::PlayerDeathThink()
{
	float flForward;

	pev->solid = SOLID_NOT;

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		if (m_iTransformType == 3)
		{
			if (pev->sequence == 8)
			{
				pev->sequence = 18;
				pev->frame = 0;
				ResetSequenceInfo();
			}
		}
		else if (m_iTransformType == 4 && pev->sequence == 7)
		{
			pev->sequence = 5;
			pev->frame = 0;
			ResetSequenceInfo();
		}

		flForward = pev->velocity.Length() - 20;
		if (flForward <= 0)
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalize();
	}

	if (HasWeapons())
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		if (m_iTransformType == 0)
			PackDeadPlayerItems();
		else
			RemoveAllItems(true);
	}

	if (0 != pev->modelindex && (pev->effects & EF_NODRAW) == 0 && (!m_fSequenceFinished) && (pev->deadflag == DEAD_DYING))
	{
		const float flInterval = StudioFrameAdvance(0.0);
		if (m_iTransformType != 0)
			DispatchAnimEvents(flInterval);

		switch (m_iTransformType)
		{
		case 0:
			m_flRespawnTimer += gpGlobals->frametime;
			if (m_flRespawnTimer < 4.0f) // 120 frames at 30 fps -- animations should be no longer than this
				return;
			break;
		case 1:
		case 2:
		case 5:
			return;
		case 3:
		case 4:
			if (m_iTransformType == 4 && pev->sequence == 14)
				return;
			m_flRespawnTimer += gpGlobals->frametime;
			if (m_flRespawnTimer < 200.0f / 30.0f) // VERIFY: the original counts 200 frames
				return;
			break;
		}
	}

	if (0 != pev->modelindex && (pev->effects & EF_NODRAW) == 0)
	{
		switch (m_iTransformType)
		{
		case 1:
		case 2:
		case 5:
			pev->frame = 255;
			break;
		case 3:
			if (pev->sequence != 18)
			{
				pev->sequence = 18;
				ResetSequenceInfo();
			}
			pev->frame = 255;
			break;
		case 4:
			if (pev->sequence != 5 && pev->sequence != 14)
			{
				pev->sequence = 5;
				ResetSequenceInfo();
			}
			pev->frame = 255;
			break;
		}
	}

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if (pev->movetype != MOVETYPE_NONE && FBitSet(pev->flags, FL_ONGROUND))
		pev->movetype = MOVETYPE_NONE;

	if (pev->deadflag == DEAD_DYING)
		pev->deadflag = DEAD_DEAD;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;

	bool fAnyButtonDown = (pev->button & ~IN_SCORE) != 0;

	// wait for all buttons released
	if (pev->deadflag == DEAD_DEAD)
	{
		if (fAnyButtonDown)
			return;

		if (g_pGameRules->FPlayerCanRespawn(this))
		{
			m_fDeadTime = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}

		return;
	}

	// if the player has been dead for one second longer than allowed by forcerespawn,
	// forcerespawn isn't on. Send the player off to an intermission camera until they
	// choose to respawn.
	if (g_pGameRules->IsMultiplayer() && (gpGlobals->time > (m_fDeadTime + 6)) && (m_afPhysicsFlags & PFLAG_OBSERVER) == 0)
	{
		// go to dead camera.
		StartDeathCam();
	}

	if (0 != pev->iuser1) // player is in spectator mode
		return;

	// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown && !m_fForceSpawn && !(g_pGameRules->IsMultiplayer() && forcerespawn.value > 0 && (gpGlobals->time > (m_fDeadTime + 5))))
		return;

	pev->button = 0;
	m_flRespawnTimer = 0.0f;

	//ALERT(at_console, "Respawn\n");

	respawn(pev, (m_afPhysicsFlags & PFLAG_OBSERVER) == 0); // don't copy a corpse if we're in deathcam.
	pev->nextthink = -1;
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
// orig 0x97720. SDK 2.3's, which hands over to StartObserver (halflife-updated inlined it)
void CBasePlayer::StartDeathCam()
{
	edict_t *pSpot, *pNewSpot;
	int iRand;

	if (pev->view_ofs == g_vecZero)
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	pSpot = FIND_ENTITY_BY_CLASSNAME(NULL, "info_intermission");

	if (!FNullEnt(pSpot))
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG(0, 3);

		while (iRand > 0)
		{
			pNewSpot = FIND_ENTITY_BY_CLASSNAME(pSpot, "info_intermission");

			if (pNewSpot)
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		CopyToBodyQue(pev);
		StartObserver(pSpot->v.origin, pSpot->v.v_angle);
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		CopyToBodyQue(pev);
		UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, 128), ignore_monsters, edict(), &tr);
		StartObserver(tr.vecEndPos, UTIL_VecToAngles(tr.vecEndPos - pev->origin));
	}
}

// orig 0x978b0. RC2's is SDK 2.3's, which only parks the view, plus taking the view back from
// whatever the player saw through. halflife-updated's spectator modes (Observer_*) are not
// entered from here any more: RC2 has none.
void CBasePlayer::StartObserver(Vector vecPosition, Vector vecViewAngle)
{
	unsigned char *pvs, *pas;
	SetupVisibility(edict(), edict(), &pvs, &pas);
	SET_VIEW(edict(), edict());

	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->view_ofs = g_vecZero;
	pev->angles = pev->v_angle = vecViewAngle;
	pev->fixangle = 1;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	pev->modelindex = 0;
	UTIL_SetOrigin(pev, vecPosition);
}

//
// PlayerUse - handles USE keypress
//
#define PLAYER_SEARCH_RADIUS (float)64

// orig 0x97990
void CBasePlayer::PlayerUse()
{
	if (IsObserver())
		return;

	// Was use pressed or released?
	if (((pev->button | m_afButtonPressed | m_afButtonReleased) & IN_USE) == 0)
		return;

	// RC2: a player with transform kind 6 can't use anything
	if (m_iChosenTransform == 6)
		return;

	// Hit Use on a train?
	if ((m_afButtonPressed & IN_USE) != 0)
	{
		if (m_pTank != NULL)
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use(this, this, USE_OFF, 0);
			m_pTank = NULL;
			return;
		}
		else
		{
			if ((m_afPhysicsFlags & PFLAG_ONTRAIN) != 0)
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				CBaseEntity* pTrain = CBaseEntity::Instance(pev->groundentity);
				if (pTrain && (pTrain->Classify() == CLASS_VEHICLE))
					((CFuncVehicle*)pTrain)->m_pDriver = NULL;
				return;
			}
			else
			{ // Start controlling the train!
				CBaseEntity* pTrain = CBaseEntity::Instance(pev->groundentity);

				if (pTrain && (pev->button & IN_JUMP) == 0 && FBitSet(pev->flags, FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) != 0 && pTrain->OnControls(pev))
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
					m_iTrain |= TRAIN_NEW;
					if (pTrain->Classify() == CLASS_VEHICLE)
					{
						EMIT_SOUND(ENT(pev), CHAN_ITEM, "plats/vehicle_ignition.wav", 0.8, ATTN_NORM);
						((CFuncVehicle*)pTrain)->m_pDriver = this;
					}
					else
						EMIT_SOUND(ENT(pev), CHAN_ITEM, "plats/train_use1.wav", 0.8, ATTN_NORM);
					return;
				}
			}
		}
	}

	CBaseEntity* pObject = NULL;
	CBaseEntity* pClosest = NULL;
	Vector vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;

	UTIL_MakeVectors(pev->v_angle); // so we know which way we are facing

	while ((pObject = UTIL_FindEntityInSphere(pObject, pev->origin, PLAYER_SEARCH_RADIUS)) != NULL)
	{

		if ((pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE)) != 0)
		{
			// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
			// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
			// when player hits the use key. How many objects can be in that area, anyway? (sjb)
			vecLOS = (VecBModelOrigin(pObject->pev) - (pev->origin + pev->view_ofs));

			// This essentially moves the origin of the target to the corner nearest the player to test to see
			// if it's "hull" is in the view cone
			vecLOS = UTIL_ClampVectorToBox(vecLOS, pObject->pev->size * 0.5);

			flDot = DotProduct(vecLOS, gpGlobals->v_forward);
			if (flDot > flMaxDot)
			{ // only if the item is in front of the user
				pClosest = pObject;
				flMaxDot = flDot;
				//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
			//			ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
		}
	}
	pObject = pClosest;

	// Found an object
	if (pObject)
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls
		int caps = pObject->ObjectCaps();

		if ((m_afButtonPressed & IN_USE) != 0)
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_select.wav", 0.4, ATTN_NORM);

		if (((pev->button & IN_USE) != 0 && (caps & FCAP_CONTINUOUS_USE) != 0) ||
			((m_afButtonPressed & IN_USE) != 0 && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE)) != 0))
		{
			if ((caps & FCAP_CONTINUOUS_USE) != 0)
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use(this, this, USE_SET, 1);
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ((m_afButtonReleased & IN_USE) != 0 && (pObject->ObjectCaps() & FCAP_ONOFF_USE) != 0) // BUGBUG This is an "off" use
		{
			pObject->Use(this, this, USE_SET, 0);
		}
	}
	else
	{
		if ((m_afButtonPressed & IN_USE) != 0)
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4, ATTN_NORM);
	}
}



void CBasePlayer::Jump()
{
	Vector vecWallCheckDir; // direction we're tracing a line to find a wall when walljumping
	Vector vecAdjustedVelocity;
	Vector vecSpot;
	TraceResult tr;

	if (FBitSet(pev->flags, FL_WATERJUMP))
		return;

	if (pev->waterlevel >= 2)
	{
		return;
	}

	// jump velocity is sqrt( height * gravity * 2)

	// If this isn't the first frame pressing the jump button, break out.
	if (!FBitSet(m_afButtonPressed, IN_JUMP))
		return; // don't pogo stick

	if ((pev->flags & FL_ONGROUND) == 0 || !pev->groundentity)
	{
		return;
	}

	// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors(pev->angles);

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk

	SetAnimation(PLAYER_JUMP);

	if (m_fLongJump &&
		(pev->button & IN_DUCK) != 0 &&
		(pev->flDuckTime > 0) &&
		pev->velocity.Length() > 50)
	{
		SetAnimation(PLAYER_SUPERJUMP);
	}

	// If you're standing on a conveyor, add it's velocity to yours (for momentum)
	entvars_t* pevGround = VARS(pev->groundentity);
	if (pevGround && (pevGround->flags & FL_CONVEYOR) != 0)
	{
		pev->velocity = pev->velocity + pev->basevelocity;
	}

	// JoshA: CS behaviour does this for tracktrain + train as well,
	// but let's just do this for func_vehicle to avoid breaking existing content.
	//
	// If you're standing on a moving train... then add the velocity of the train to yours.
	if (pevGround && (/*(!strcmp( "func_tracktrain", STRING(pevGround->classname))) ||
			(!strcmp( "func_train", STRING(pevGround->classname))) ) ||*/
			(!strcmp("func_vehicle", STRING(pevGround->classname)))))
		pev->velocity = pev->velocity + pevGround->velocity;
}



// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck(edict_t* pPlayer)
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for (int i = 0; i < 18; i++)
	{
		UTIL_TraceHull(pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, head_hull, pPlayer, &trace);
		if (0 != trace.fStartSolid)
			pPlayer->v.origin.z++;
		else
			break;
	}
}

void CBasePlayer::Duck()
{
	if ((pev->button & IN_DUCK) != 0)
	{
		if (m_IdealActivity != ACT_LEAP)
		{
			SetAnimation(PLAYER_WALK);
		}
	}
}

//
// ID's player as such.
//
// orig 0x97ef0. RC2: nothing before joining; a transformed player is classed as what he became
int CBasePlayer::Classify()
{
	if (!IsActivePlayer())
		return CLASS_NONE;

	if (m_iTransformType != 0)
		return Transform_Classify(m_iTransformType);

	return CLASS_PLAYER;
}


// orig 0x97f20
void CBasePlayer::AddPoints(int score, bool bAllowNegativeScore)
{
	// Positive score always adds
	if (score < 0)
	{
		if (!bAllowNegativeScore)
		{
			if (pev->frags < 0) // Can't go more negative
				return;

			if (-score > pev->frags) // Will this go negative?
			{
				score = -pev->frags; // Sum will be 0
			}
		}
	}

	pev->frags += score;

	// RC2's ScoreInfo: six bytes
	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(ENTINDEX(edict()));
	WRITE_SHORT(pev->frags);
	WRITE_SHORT(m_iDeaths);
	WRITE_BYTE(g_pGameRules->GetTeamIndex(m_szTeamName) + 1);
	MESSAGE_END();
}


// orig 0x98000
void CBasePlayer::AddPointsToTeam(int score, bool bAllowNegativeScore)
{
	int index = entindex();

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer && i != index)
		{
			if (g_pGameRules->PlayerRelationship(this, pPlayer) == GR_TEAMMATE)
			{
				pPlayer->AddPoints(score, bAllowNegativeScore);
			}
		}
	}
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0;
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[SBAR_END];
	char sbuf0[SBAR_STRING_SIZE];
	char sbuf1[SBAR_STRING_SIZE];

	memset(newSBarState, 0, sizeof(newSBarState));
	strcpy(sbuf0, m_SbarString0);
	strcpy(sbuf1, m_SbarString1);

	// Find an ID Target
	TraceResult tr;
	UTIL_MakeVectors(pev->v_angle + pev->punchangle);
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * MAX_ID_RANGE);
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	if (tr.flFraction != 1.0)
	{
		if (!FNullEnt(tr.pHit))
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

			if (pEntity->Classify() == CLASS_PLAYER)
			{
				newSBarState[SBAR_ID_TARGETNAME] = ENTINDEX(pEntity->edict());
				strcpy(sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%");

				// allies and medics get to see the targets health
				if (g_pGameRules->PlayerRelationship(this, pEntity) == GR_TEAMMATE)
				{
					newSBarState[SBAR_ID_TARGETHEALTH] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
					newSBarState[SBAR_ID_TARGETARMOR] = pEntity->pev->armorvalue; //No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
			}
		}
		else if (m_flStatusBarDisappearDelay > gpGlobals->time)
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[SBAR_ID_TARGETNAME] = m_izSBarState[SBAR_ID_TARGETNAME];
			newSBarState[SBAR_ID_TARGETHEALTH] = m_izSBarState[SBAR_ID_TARGETHEALTH];
			newSBarState[SBAR_ID_TARGETARMOR] = m_izSBarState[SBAR_ID_TARGETARMOR];
		}
	}

	bool bForceResend = false;

	if (0 != strcmp(sbuf0, m_SbarString0))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusText, NULL, pev);
		WRITE_BYTE(0);
		WRITE_STRING(sbuf0);
		MESSAGE_END();

		strcpy(m_SbarString0, sbuf0);

		// make sure everything's resent
		bForceResend = true;
	}

	if (0 != strcmp(sbuf1, m_SbarString1))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusText, NULL, pev);
		WRITE_BYTE(1);
		WRITE_STRING(sbuf1);
		MESSAGE_END();

		strcpy(m_SbarString1, sbuf1);

		// make sure everything's resent
		bForceResend = true;
	}

	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if (newSBarState[i] != m_izSBarState[i] || bForceResend)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgStatusValue, NULL, pev);
			WRITE_BYTE(i);
			WRITE_SHORT(newSBarState[i]);
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}









#define CLIMB_SHAKE_FREQUENCY 22 // how many frames in between screen shakes when climbing
#define MAX_CLIMB_SPEED 200		 // fastest vertical climbing speed possible
#define CLIMB_SPEED_DEC 15		 // climbing deceleration rate
#define CLIMB_PUNCH_X -7		 // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z 7			 // how far to 'punch' client Z axis when climbing

// orig 0x98080. The debug HUD (m_bDebugCounts): the eight most common classnames among the
// valid entities, at most every 0.2-0.75 s by the client's ping (always for the host), and what
// the watched entity (m_pentDebug) is doing.
void CBasePlayer::SendDebugCounts()
{
	if (!m_bDebugCounts)
		return;

	if (!Admin_IsListenHost(edict()))
	{
		if (gpGlobals->time < m_flNextDebugCounts)
			return;

		int iPing = 0, iLoss = 0;
		g_engfuncs.pfnGetPlayerStats(edict(), &iPing, &iLoss);
		if (iPing < 50)
			m_flNextDebugCounts = gpGlobals->time + 0.2;
		else if (iPing < 300)
			m_flNextDebugCounts = gpGlobals->time + 0.5;
		else
			m_flNextDebugCounts = gpGlobals->time + 0.75;
	}

	static char s_szTop[8][128]; // name ours (orig .bss 0x18efc8)
	int rgiName[384] = {};
	int rgiCount[384] = {};
	int rgiTop[8] = {};
	for (auto& szTop : s_szTop)
		szTop[0] = 0;

	int iEntities = 0;
	edict_t* pent = INDEXENT(1);
	for (int e = 1; e < gpGlobals->maxEntities; e++, pent++)
	{
		if (!UTIL_IsValidEntity(pent))
			continue;
		iEntities++;

		bool bCounted = false;
		for (int i = 0; i < 384; i++)
		{
			if (rgiCount[i] > 0 && 0 == strcmp(STRING(rgiName[i]), STRING(pent->v.classname)))
			{
				rgiCount[i]++;
				bCounted = true;
				break;
			}
		}
		if (bCounted)
			continue;

		for (int i = 0; i < 384; i++)
		{
			if (rgiCount[i] < 1)
			{
				rgiCount[i] = 1;
				rgiName[i] = pent->v.classname;
				break;
			}
		}
	}

	for (int t = 0; t < 8; t++)
	{
		int iBest = 0;
		for (int i = 0; i < 384; i++)
		{
			if (rgiCount[iBest] < rgiCount[i])
				iBest = i;
		}
		rgiTop[t] = rgiCount[iBest];
		strcpy(s_szTop[t], STRING(rgiName[iBest]));
		rgiCount[iBest] = -1;
	}

	MESSAGE_BEGIN(MSG_ONE, gmsgDBGCounts, NULL, edict());
	WRITE_SHORT(iEntities);
	WRITE_SHORT(gpGlobals->maxEntities);
	for (int t = 0; t < 8; t++)
	{
		if (rgiTop[t] < 1)
		{
			WRITE_SHORT(0);
			WRITE_STRING("");
		}
		else
		{
			WRITE_SHORT(rgiTop[t]);
			WRITE_STRING(s_szTop[t]);
		}
	}
	MESSAGE_END();

	if (!m_pentDebug)
		return;

	// say why the watched entity is gone
	int iGone = 0;
	if (m_pentDebug->serialnumber != m_iDebugSerial)
		iGone = 1;
	else if (0 != m_pentDebug->free)
		iGone = 2;
	else if (!m_pentDebug->pvPrivateData)
		iGone = 3;
	else if (0 == g_engfuncs.pfnEntOffsetOfPEntity(m_pentDebug))
		iGone = 4;
	else if ((m_pentDebug->v.flags & FL_KILLME) != 0)
		iGone = 5;

	if (iGone != 0)
	{
		m_pentDebug = NULL;
		MESSAGE_BEGIN(MSG_ONE, gmsgDBGEntInfo, NULL, edict());
		WRITE_BYTE(iGone);
		MESSAGE_END();
		return;
	}

	CBaseEntity* pEntity = static_cast<CBaseEntity*>(m_pentDebug->pvPrivateData);
	MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgDBGEntInfo, NULL, edict());
	WRITE_BYTE(0);
	WRITE_LONG((int)(intptr_t)pEntity); // VERIFY: the original sends the object's address
	WRITE_SHORT(ENTINDEX(pEntity->edict()));
	WRITE_STRING(STRING(pEntity->pev->classname));
	WRITE_STRING(STRING(pEntity->pev->netname));
	WRITE_STRING(STRING(pEntity->pev->target));
	WRITE_STRING(STRING(pEntity->pev->targetname));
	WRITE_STRING(STRING(pEntity->pev->model));
	WRITE_SHORT((int)pEntity->pev->health);
	WRITE_SHORT((int)pEntity->pev->armorvalue);
	WRITE_BYTE(pEntity->pev->waterlevel);
	WRITE_LONG(pEntity->pev->flags);
	if (pEntity->pev->nextthink == 0 || fabs(pEntity->pev->nextthink - gpGlobals->time) >= 16)
		WRITE_SHORT(0);
	else
		WRITE_SHORT((int)((pEntity->pev->nextthink - gpGlobals->time) * 1000 + 0.5)); // ms to the next think
	WRITE_STRING(pEntity->m_pfnThink ? "N/A" : "none"); // the original has no name for the function
	WRITE_STRING(pEntity->m_pfnTouch ? "N/A" : "none");
	WRITE_COORD(pEntity->pev->origin.x);
	WRITE_COORD(pEntity->pev->origin.y);
	WRITE_COORD(pEntity->pev->origin.z);
	WRITE_COORD(pEntity->pev->velocity.x);
	WRITE_COORD(pEntity->pev->velocity.y);
	WRITE_COORD(pEntity->pev->velocity.z);
	WRITE_COORD(pEntity->pev->angles.x);
	WRITE_COORD(pEntity->pev->angles.y);
	WRITE_COORD(pEntity->pev->angles.z);
	WRITE_COORD(pEntity->pev->v_angle.x);
	WRITE_COORD(pEntity->pev->v_angle.y);
	WRITE_COORD(pEntity->pev->v_angle.z);
	MESSAGE_END();
}

// orig 0x98700. The next (or previous) valid entity with that classname after m_iSpecIndex,
// skipping FL_KILLME and FL_SPECTATOR ones, wrapping around once
CBaseEntity* CBasePlayer::FindNextNamed(bool bReverse, const char* pszClassname)
{
	auto Matches = [pszClassname](int i) -> CBaseEntity*
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			return NULL;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity || (pEntity->pev->flags & (FL_KILLME | FL_SPECTATOR)) != 0)
			return NULL;
		if (0 != strcmp(pszClassname, STRING(pEntity->pev->classname)))
			return NULL;
		return pEntity;
	};

	if (!bReverse)
	{
		for (int i = m_iSpecIndex == -1 ? 1 : m_iSpecIndex + 1; i < gpGlobals->maxEntities; i++)
		{
			if (CBaseEntity* pEntity = Matches(i))
				return pEntity;
		}
	}
	else
	{
		for (int i = m_iSpecIndex == -1 ? gpGlobals->maxEntities - 1 : m_iSpecIndex - 1; i >= 1; i--)
		{
			if (CBaseEntity* pEntity = Matches(i))
				return pEntity;
		}
	}

	if (m_iSpecIndex == -1)
		return NULL;

	if (!bReverse)
	{
		for (int i = 1; i < gpGlobals->maxEntities; i++)
		{
			if (CBaseEntity* pEntity = Matches(i))
				return pEntity;
		}
	}
	else
	{
		for (int i = gpGlobals->maxEntities - 1; i >= 1; i--)
		{
			if (CBaseEntity* pEntity = Matches(i))
				return pEntity;
		}
	}
	return NULL;
}

// orig 0x98a20. RC2 adds to SDK 2.3's: the frame time; the first frames of a new client
// (m_iInitStage) and the damage-based score updates; the debug HUD; the spectator view
// before joining (attack joins, jump switches between spawn points and players, attack2 and
// cancel step through them); getting off a shoulder whose player is gone; the end of a
// transformation; transform 6's own think; the energy draw, health creation, shield, health
// regeneration and jetpack, all paid in energy; transformed players move their own way.
// halflife-updated's view entity reset and spectator (Observer) handling stay; the node graph
// pointers are set in Precache as in RC2.
void CBasePlayer::PreThink()
{
	int buttonsChanged = (m_afButtonLast ^ pev->button); // These buttons have changed this frame

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed = buttonsChanged & pev->button;	  // The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & (~pev->button); // The ones not down are "released"

	m_flFrameInterval = std::clamp(gpGlobals->time - m_flLastPreThink, 0.0001f, 10.0f);
	m_flLastPreThink = gpGlobals->time;

	g_pGameRules->PlayerThink(this);

	if (m_flPhysFlag10Time != -1 && m_flPhysFlag10Time < gpGlobals->time)
	{
		ClearPhysFlags(0x10);
		m_flPhysFlag10Time = -1;
	}

	if (g_fGameOver)
	{
		m_iClientId = 0; // VERIFY: the original clears +0x66c here, which makes the next spawn a new client
		return;			 // intermission or finale
	}

	if (m_iInitStage != 0)
	{
		if (m_iInitStage == 3)
		{
			m_pLastItem = NULL;
			m_fInitHUD = true;
			m_iClientHideHUD = -1;
			m_fWeapon = false;
			m_pClientActiveItem = NULL;
			m_iClientBattery = -1;
			m_iClientGaussBeacons = -1;
			m_iClientPlayerStat = 0xff;
			UpdateClientData();

			m_fInitHUD = true;
			m_iClientHealth = -1;
			m_iClientBattery = -1;
			m_iTrain |= TRAIN_NEW;
			m_fWeapon = false;
			m_fKnownItem = false;
			m_iClientPlayerStat = 0xff;

			MESSAGE_BEGIN(MSG_ONE, gmsgScoreMode, NULL, edict());
			WRITE_BYTE(g_iRC2ScoreMode);
			MESSAGE_END();

			RC2Prefs_SendResetToClient(edict());
			RC2Prefs_SendToClient(edict());
			ResetTethers();
		}
		else if (g_pGameRules->IsMultiplayer() && m_iInitStage == 2)
		{
			static_cast<CHalfLifeMultiplay*>(g_pGameRules)->SendMOTDToClient(edict());
		}
		else if (m_iInitStage == 1)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgAttrConf, NULL, edict());
			MESSAGE_END();
		}

		if (--m_iInitStage < 0)
			m_iInitStage = 0;
	}
	else if (m_flClientDmgScore != m_flDmgScore)
	{
		m_flClientDmgScore = m_flDmgScore;

		int iScoreBits;
		memcpy(&iScoreBits, &m_flDmgScore, sizeof(iScoreBits));

		MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
		WRITE_BYTE(ENTINDEX(edict()) | 0x80);
		WRITE_LONG(iScoreBits);
		WRITE_BYTE(g_pGameRules->GetTeamIndex(m_szTeamName) + 1);
		MESSAGE_END();
	}

	SendDebugCounts();

	if (m_bNotJoined)
	{
		// the spectator view before joining
		if (((m_afButtonPressed & IN_ATTACK) != 0 || m_fForceSpawn) && m_iInitStage <= 0)
		{
			Spawn();
			return;
		}

		if ((m_afButtonPressed & IN_JUMP) != 0)
		{
			if (!m_bSpecChase)
			{
				m_afButtonPressed |= IN_ATTACK2;
				m_iSpecIndex = -1;
				m_bSpecChase = true;
				pev->iuser1 = 2;
				pev->iuser2 = 0;
			}
			else
			{
				m_bSpecChase = false;
				pev->iuser2 = 0;
				pev->iuser1 = 3;
			}
		}

		if ((m_afButtonPressed & (IN_ATTACK2 | IN_CANCEL)) != 0)
		{
			const bool bReverse = (m_afButtonPressed & IN_CANCEL) != 0;
			if (!m_bSpecChase)
			{
				CBaseEntity* pSpot = FindNextNamed(bReverse, "info_player_deathmatch");
				if (pSpot)
				{
					m_iSpecIndex = ENTINDEX(pSpot->edict());
					UTIL_SetOrigin(pev, pSpot->pev->origin);
					pev->v_angle = pSpot->pev->angles;
					pev->angles = pev->v_angle;
					pev->fixangle = 1;
				}
			}
			else
			{
				CBaseEntity* pTarget = FindNextNamed(bReverse, "player");
				if (pTarget)
				{
					m_iSpecIndex = ENTINDEX(pTarget->edict());
					pev->iuser2 = ENTINDEX(pTarget->edict());
				}
			}
		}

		if (m_bSpecChase && pev->iuser2 > 0)
		{
			edict_t* pentTarget = INDEXENT(pev->iuser2);
			if (UTIL_IsValidEntity(pentTarget))
				UTIL_SetOrigin(pev, pentTarget->v.origin);
		}
		return;
	}

	UTIL_MakeVectors(pev->v_angle); // is this still used?

	ItemPreFrame();
	WaterMove();

	if (g_pGameRules && g_pGameRules->FAllowFlashlight())
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;

	if (m_bResetViewEntity)
	{
		m_bResetViewEntity = false;

		CBaseEntity* viewEntity = m_hViewEntity;

		if (viewEntity)
		{
			SET_VIEW(edict(), viewEntity->edict());
		}
	}

	// off a shoulder whose player has gone
	if (m_iRidingIndex != 0 && !UTIL_PlayerByIndex(m_iRidingIndex))
		StopRiding(false);

	if (m_bTransforming && m_flTransformDoneTime < gpGlobals->time)
	{
		if (m_iTransformType != 0)
			FinishUntransform();
		else
			FinishTransform();
		m_bTransforming = false;
	}

	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();

	CheckTimeBasedDamage();

	CheckSuitUpdate();

	UpdateVirtualHulls();

	// Observer Button Handling
	if (IsObserver())
	{
		Observer_HandleButtons();
		Observer_CheckTarget();
		Observer_CheckProperties();
		pev->impulse = 0;
		return;
	}

	if (pev->deadflag >= DEAD_DYING)
	{
		PlayerDeathThink();
		return;
	}

	if (m_iTransformType == 6)
	{
		RaceUseFrame(this);
		return;
	}

	// energy draw: refills the energy over EnergyDrawTimeTaken, then waits EnergyDrawReuseDelay
	if (m_fDrawingEnergy)
	{
		if (m_flEnergyDrawDone < gpGlobals->time)
		{
			m_fDrawingEnergy = false;
			m_flEnergyDrawDone = gpGlobals->time + RC2Pref(RC2P_Attributes_EnergyDrawReuseDelay).f;
			RemovePhysFlag2Source(1);

			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(6);
			WRITE_BYTE(ENTINDEX(edict()));
			MESSAGE_END();
		}
		else if (m_flEnergy < 100)
		{
			m_flEnergy += RC2Pref(RC2P_Attributes_EnergyDrawGive).f / RC2Pref(RC2P_Attributes_EnergyDrawTimeTaken).f * m_flFrameInterval;
			if (m_flEnergy > 100)
				m_flEnergy = 100;
		}
		else
			m_flEnergy = 100;
	}

	// health creation: CreateHealthExpectedToGive over the creation, on a parabola, for energy
	if (m_fCreatingHealth)
	{
		if (m_flCreateHealthDone < gpGlobals->time)
		{
			m_fCreatingHealth = false;
			m_flCreateHealthDone = gpGlobals->time + RC2Pref(RC2P_Attributes_CreateHealthReuseDelay).f;
			if (m_flHealthCreated < RC2Pref(RC2P_Attributes_CreateHealthExpectedToGive).f)
				TakeHealth(RC2Pref(RC2P_Attributes_CreateHealthExpectedToGive).f - m_flHealthCreated, 0);
			m_flHealthCreated = 0;
		}
		else
		{
			// VERIFY: the time elapsed is counted from EnergyDrawTimeTaken, where the parabola
			// uses CreateHealthTimeTaken (an original bug, or the creation starts that long)
			const float flElapsed = RC2Pref(RC2P_Attributes_EnergyDrawTimeTaken).f - (m_flCreateHealthDone - gpGlobals->time);
			const float flHeal = RC2Pref(RC2P_Attributes_CreateHealthGiveConstant).f * flElapsed * (RC2Pref(RC2P_Attributes_CreateHealthTimeTaken).f - flElapsed) * m_flFrameInterval;
			m_flHealthCreated += flHeal;

			float flEnergyUse = RC2Pref(RC2P_Attributes_CreateHealthEnergyUsed).f / RC2Pref(RC2P_Attributes_CreateHealthTimeTaken).f * m_flFrameInterval;
			float flScale = 1;
			if (flEnergyUse != 0 && flEnergyUse > m_flEnergy)
			{
				flScale = m_flEnergy / flEnergyUse;
				flEnergyUse = m_flEnergy;
			}
			if (flEnergyUse > 0)
				m_flEnergy -= flEnergyUse;

			if (flScale > 0 && flHeal > 0)
				TakeHealth(flScale * flHeal, 0);
		}
	}

	// the shield's client effect, and its minimum energy
	if (m_fShieldOn)
	{
		if (!m_bClientShield)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(0x25);
			WRITE_BYTE(ENTINDEX(edict()));
			MESSAGE_END();
		}
		m_bClientShield = true;
		if (m_flEnergy < RC2Pref(RC2P_Shield_MinRunEnergy).f)
			m_fShieldOn = false;
	}
	else
	{
		if (m_bClientShield)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(254);
			WRITE_SHORT(ENTINDEX(edict()));
			WRITE_BYTE(0x25);
			MESSAGE_END();
		}
		m_bClientShield = false;
	}

	// health over the maximum decays; under it, it regenerates (not while transformed)
	if (pev->health > pev->max_health)
		m_flHealthRegen -= RC2Pref(RC2P_Attributes_BaseHealthOverMaxDecayRate).f * m_flFrameInterval;
	else if (m_iTransformType == 0)
		m_flHealthRegen += RC2Pref(RC2P_Attributes_BaseHealthRegenRate).f * m_flFrameInterval;

	while (m_flHealthRegen >= 1)
	{
		TakeHealth(1, 0);
		m_flHealthRegen -= 1;
	}
	while (m_flHealthRegen <= -1)
	{
		if ((int)pev->health > 1)
			pev->health -= 1;
		m_flHealthRegen += 1;
	}

	// the jetpack goes off in water and costs energy, more while moving
	if (pev->waterlevel >= 2 && m_fJetpackOn)
	{
		m_fJetpackOn = false;
		ClearPhysFlags(1);
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(4);
		WRITE_BYTE(ENTINDEX(edict()));
		WRITE_BYTE(1);
		MESSAGE_END();
	}

	if (m_fJetpackOn)
	{
		float flUse = RC2Pref(RC2P_Attributes_JetpackIdleEnergyUse).f * m_flFrameInterval;
		if (pev->velocity.Length() > RC2Pref(RC2P_Attributes_JetpackMovingThreshhold).f)
			flUse = RC2Pref(RC2P_Attributes_JetpackMovingEnergyUse).f * m_flFrameInterval;

		if (flUse <= m_flEnergy)
			m_flEnergy -= flUse;
		else
		{
			m_fJetpackOn = false;
			ClearPhysFlags(1);
			MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
			WRITE_BYTE(4);
			WRITE_BYTE(ENTINDEX(edict()));
			WRITE_BYTE(0);
			MESSAGE_END();
			m_flEnergy = 0;
		}
	}

	// So the correct flags get sent to client asap.
	//
	if ((m_afPhysicsFlags & PFLAG_ONTRAIN) != 0)
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;

	// Train speed control
	if ((m_afPhysicsFlags & PFLAG_ONTRAIN) != 0)
	{
		CBaseEntity* pTrain = CBaseEntity::Instance(pev->groundentity);
		float vel;

		if (!pTrain)
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -38), ignore_monsters, ENT(pev), &trainTrace);

			// HACKHACK - Just look for the func_tracktrain classname
			if (trainTrace.flFraction != 1.0 && trainTrace.pHit)
				pTrain = CBaseEntity::Instance(trainTrace.pHit);


			if (!pTrain || (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) == 0 || !pTrain->OnControls(pev))
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				if (pTrain->Classify() == CLASS_VEHICLE)
					((CFuncVehicle*)pTrain)->m_pDriver = NULL;
				return;
			}
		}
		else if (!FBitSet(pev->flags, FL_ONGROUND) || FBitSet(pTrain->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL) || ((pev->button & (IN_MOVELEFT | IN_MOVERIGHT)) != 0 && pTrain->Classify() != CLASS_VEHICLE))
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			// and it isn't a func_vehicle
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW | TRAIN_OFF;
			return;
		}

		pev->velocity = g_vecZero;
		vel = 0;
		if (pTrain->Classify() == CLASS_VEHICLE)
		{
			if (pev->button & IN_FORWARD)
			{
				vel = 1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_BACK)
			{
				vel = -1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_MOVELEFT)
			{
				vel = 20;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_MOVERIGHT)
			{
				vel = 30;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
		}
		else
		{
			if ((m_afButtonPressed & IN_FORWARD) != 0)
			{
				vel = 1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			else if ((m_afButtonPressed & IN_BACK) != 0)
			{
				vel = -1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
		}

		if (0 != vel)
		{
			m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
			m_iTrain |= TRAIN_ACTIVE | TRAIN_NEW;
		}
	}
	else if ((m_iTrain & TRAIN_ACTIVE) != 0)
		m_iTrain = TRAIN_NEW; // turn off train

	if (m_iTransformType == 0)
	{
		if ((pev->button & IN_JUMP) != 0)
		{
			// If on a ladder, jump off the ladder
			// else Jump
			Jump();
		}


		// If trying to duck, already ducked, or in the process of ducking
		if ((pev->button & IN_DUCK) != 0 || FBitSet(pev->flags, FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) != 0)
			Duck();
	}
	else
		Transform_Think();

	if (!FBitSet(pev->flags, FL_ONGROUND))
	{
		m_flFallVelocity = -pev->velocity.z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if ((m_afPhysicsFlags & PFLAG_ONBARNACLE) != 0)
	{
		pev->velocity = g_vecZero;
	}
}
/* Time based Damage works as follows: 
	1) There are several types of timebased damage:

		#define DMG_PARALYZE		(1 << 14)	// slows affected creature down
		#define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
		#define DMG_POISON			(1 << 16)	// blood poisioning
		#define DMG_RADIATION		(1 << 17)	// radiation exposure
		#define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
		#define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
		#define DMG_SLOWBURN		(1 << 20)	// in an oven
		#define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer

	2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
		per damage type. The counter is decremented every second, so the maximum time 
		an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
		of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

	3) Every second that a tbd counter is running, the player takes damage.  The damage
		is determined by the type of tdb.  
			Paralyze		- 1/2 movement rate, 30 second duration.
			Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
			Poison			- 2 points per second, 25 second duration = 50 points max dose.
			Radiation		- 1 point per second, 50 second duration = 50 points max dose.
			Drown			- 5 points per second, 2 second duration.
			Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
			Burn			- 10 points per second, 2 second duration.
			Freeze			- 3 points per second, 10 second duration = 30 points max.

	4) Certain actions or countermeasures counteract the damaging effects of tbds:

		Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
							- recharged by suit recharger
		Air In Lungs		- drowning damage is done to air in lungs first, then to body
							- recharged by poking head out of water
							- 10 seconds if swiming fast
		Air In SCUBA		- drowning damage is done to air in tanks first, then to body
							- 2 minutes in tanks. Need new tank once empty.
		Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
		Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
		Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
		Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.
		
	
*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

//#define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
//#define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

//#define NERVEGAS_DURATION	16
//#define NERVEGAS_DAMAGE		5.0

//#define POISON_DURATION		25
//#define POISON_DAMAGE		2.0

//#define RADIATION_DURATION	50
//#define RADIATION_DAMAGE	1.0

//#define ACID_DURATION		10
//#define ACID_DAMAGE			5.0

//#define SLOWBURN_DURATION	2
//#define SLOWBURN_DAMAGE		1.0

//#define SLOWFREEZE_DURATION	1.0
//#define SLOWFREEZE_DAMAGE	3.0

/* */


// orig 0x99710
void CBasePlayer::CheckTimeBasedDamage()
{
	int i;
	byte bDuration = 0;

	static float gtbdPrev = 0.0;

	if ((m_bitsDamageType & DMG_TIMEBASED) == 0)
		return;

	// only check for time based damage approx. every 2 seconds
	if (fabs(gpGlobals->time - m_tbdPrev) < 2.0)
		return;

	m_tbdPrev = gpGlobals->time;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// make sure bit is set for damage type
		if ((m_bitsDamageType & (DMG_PARALYZE << i)) != 0)
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
				//				TakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				TakeDamage(pev, pev, POISON_DAMAGE, DMG_GENERIC);
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
				//				TakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = V_min(m_idrowndmg - m_idrownrestored, 10);

					TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4; // get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
				//				TakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
				//				TakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
				//				TakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (0 != m_rgbTimeBasedDamage[i])
			{
				// use up an antitoxin on poison or nervegas after a few seconds of damage
				if (((i == itbd_NerveGas) && (m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION)) ||
					((i == itbd_Poison) && (m_rgbTimeBasedDamage[i] < POISON_DURATION)))
				{
					if (0 != m_rgItems[ITEM_ANTIDOTE])
					{
						m_rgbTimeBasedDamage[i] = 0;
						m_rgItems[ITEM_ANTIDOTE]--;
						SetSuitUpdate("!HEV_HEAL4", false, SUIT_REPEAT_OK);
					}
				}


				// decrement damage duration, detect when done.
				if (0 == m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation. 
Some functions are automatic, some require power. 
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

	Heat/Cold
		When the player enters a hot/cold area, the heating/cooling indicator on the suit 
		will come on and the battery will drain while the player stays in the area. 
		After the battery is dead, the player starts to take damage. 
		This feature is built into the suit and is automatically engaged.
	Radiation Syringe
		This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
	Anti-Toxin Syringe
		This will cure the player from being poisoned. Single use item.
	Health
		Small (1st aid kits, food, etc.)
		Large (boxes on walls)
	Armor
		The armor works using energy to create a protective field that deflects a
		percentage of damage projectile and explosive attacks. After the armor has been deployed,
		it will attempt to recharge itself to full capacity with the energy reserves from the battery.
		It takes the armor N seconds to fully charge. 

Notification (via the HUD)

x	Health
x	Ammo  
x	Automatic Health Care
		Notifies the player when automatic healing has been engaged. 
x	Geiger counter
		Classic Geiger counter sound and status bar at top of HUD 
		alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
	Armor
		Displays the current level of armor. 

Augmentation 

	Reanimation (w/adrenaline)
		Causes the player to come back to life after he has been dead for 3 seconds. 
		Will not work if player was gibbed. Single use.
	Long Jump
		Used by hitting the ??? key(s). Caused the player to further than normal.
	SCUBA	
		Used automatically after picked up and after player enters the water. 
		Works for N seconds. Single use.	
	
Things powered by the battery

	Armor		
		Uses N watts for every M units of damage.
	Heat/Cool	
		Uses N watts for every second in hot/cold area.
	Long Jump	
		Uses N watts for every jump.
	Alien Cloak	
		Uses N watts for each use. Each use lasts M seconds.
	Alien Shield	
		Augments armor. Reduces Armor drain by one half
 
*/

// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer::UpdateGeigerCounter()
{
	byte range;

	// delay per update ie: don't flood net with these msgs
	if (gpGlobals->time < m_flgeigerDelay)
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;

	// send range to radition source to client

	range = (byte)(m_flgeigerRange / 4);

	if (range != m_igeigerRangePrev)
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN(MSG_ONE, gmsgGeigerRange, NULL, pev);
		WRITE_BYTE(range);
		MESSAGE_END();
	}

	// reset counter and semaphore
	if (!RANDOM_LONG(0, 3))
		m_flgeigerRange = 1000;
}

/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME 3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch = m_iSuitPlayNext;

	// Ignore suit updates if no suit
	if (!HasSuit())
		return;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();

	if (g_pGameRules->IsMultiplayer())
	{
		// don't bother updating HEV voice in multiplayer.
		return;
	}

	if (gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0)
	{
		// play a sentence off of the end of the queue
		for (i = 0; i < CSUITPLAYLIST; i++)
		{
			isentence = m_rgSuitPlayList[isearch];
			if (0 != isentence)
				break;

			if (++isearch == CSUITPLAYLIST)
				isearch = 0;
		}

		if (0 != isentence)
		{
			m_rgSuitPlayList[isearch] = 0;
			if (isentence > 0)
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX + 1];
				strcpy(sentence, "!");
				strcat(sentence, gszallsentencenames[isentence]);
				EMIT_SOUND_SUIT(ENT(pev), sentence);
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT(ENT(pev), -isentence);
			}
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check
			m_flSuitUpdate = 0;
	}
}

// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

// orig 0x99a90
void CBasePlayer::SetSuitUpdate(const char* name, bool fgroup, int iNoRepeatTime)
{
	int i;
	int isentence;
	int iempty = -1;


	// Ignore suit updates if no suit
	if (!HasSuit())
		return;

	if (g_pGameRules->IsMultiplayer())
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if (!name)
	{
		for (i = 0; i < CSUITPLAYLIST; i++)
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if (!fgroup)
	{
		isentence = SENTENCEG_Lookup(name, NULL);
		if (isentence < 0)
			return;
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex(name);

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for (i = 0; i < CSUITNOREPEAT; i++)
	{
		if (isentence == m_rgiSuitNoRepeat[i])
		{
			// this sentence or group is already in
			// the norepeat list

			if (m_rgflSuitNoRepeatTime[i] < gpGlobals->time)
			{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i] = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty = i;
				break;
			}
			else
			{
				// don't play, still marked as norepeat
				return;
			}
		}
		// keep track of empty slot
		if (0 == m_rgiSuitNoRepeat[i])
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if (0 != iNoRepeatTime)
	{
		if (iempty < 0)
			iempty = RANDOM_LONG(0, CSUITNOREPEAT - 1); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty] = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if (m_iSuitPlayNext == CSUITPLAYLIST)
		m_iSuitPlayNext = 0;

	if (m_flSuitUpdate <= gpGlobals->time)
	{
		if (m_flSuitUpdate == 0)
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
	}
}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
// orig 0x9a3b0. RC2 emptied it: a transformed player keeps the model he has.
static void
CheckPowerups(entvars_t* pev)
{
}


//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
// orig 0x99c10
void CBasePlayer::UpdatePlayerSound()
{
	int iBodyVolume;
	int iVolume;
	CSound* pSound;

	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(edict()));

	if (!pSound)
	{
		ALERT(at_console, "Client lost reserved sound!\n");
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if (iBodyVolume > 512)
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if ((pev->button & IN_JUMP) != 0)
	{
		iBodyVolume += 100;
	}

	// convert player move speed and actions into sound audible by monsters.
	if (m_iWeaponVolume > iBodyVolume)
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if (m_iWeaponVolume < 0)
	{
		iVolume = 0;
	}


	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if (m_iTargetVolume > iVolume)
	{
		iVolume = m_iTargetVolume;
	}
	else if (iVolume > m_iTargetVolume)
	{
		iVolume -= 250 * gpGlobals->frametime;

		if (iVolume < m_iTargetVolume)
		{
			iVolume = 0;
		}
	}

	if (m_fNoPlayerSound)
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	// RC2: capability bit 8 (GetRCCaps2) quiets the player to a fifth
	if ((GetRCCaps2(0, this) & 8) != 0)
		iVolume = (int)(iVolume * 0.2);

	if (gpGlobals->time > m_flStopExtraSoundTime)
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if (pSound)
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= (bits_SOUND_PLAYER | m_iExtraSoundTypes);
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if (m_iWeaponFlash < 0)
		m_iWeaponFlash = 0;

	//UTIL_MakeVectors ( pev->angles );
	//gpGlobals->v_forward.z = 0;

	// Below are a couple of useful little bits that make it easier to determine just how much noise the
	// player is making.
	// UTIL_ParticleEffect ( pev->origin + gpGlobals->v_forward * iVolume, g_vecZero, 255, 25 );
	//ALERT ( at_console, "%d/%d\n", iVolume, m_iTargetVolume );
}


// orig 0x99dd0. RC2: nothing but the timers before joining or in transform 6; transformed
// players' own post-think and animation events; energy regenerates (by the attribute, slower
// with the shield up); no fall damage in multiplayer (and none for transforms 1 and 5); a
// transformed player in the air still walks. halflife-updated's DecrementTimers stays, which is
// where the weapons package decrements the two floats RC2's weapons gained (orig +0xb0, +0xb4)
// as this loop does in the original.
void CBasePlayer::PostThink()
{
	if (g_fGameOver)
		goto pt_end; // intermission or finale

	if (m_bNotJoined)
		goto pt_end;

	if (!IsAlive())
		goto pt_end;

	if (m_iTransformType == 6)
		goto pt_end;

	// Handle Tank controlling
	if (m_pTank != NULL)
	{ // if they've moved too far from the gun,  or selected a weapon, unuse the gun
		if (m_pTank->OnControls(pev) && 0 == pev->weaponmodel)
		{
			m_pTank->Use(this, this, USE_SET, 2); // try fire the gun
		}
		else
		{ // they've moved off the platform
			m_pTank->Use(this, this, USE_OFF, 0);
			m_pTank = NULL;
		}
	}

	if (m_iTransformType != 0)
		Transform_FixAngle();

	// do weapon stuff
	ItemPostFrame();

	// RC2: energy regeneration
	if (m_flEnergyRegenTime != -1 && m_flEnergyRegenTime < gpGlobals->time)
	{
		if (m_flEnergy < 0)
			m_flEnergy = 0;

		if (m_flEnergy < 100)
		{
			float flRegen = PM_GetAttributeScale(RC2ATTR_ENERGYREGEN, ENTINDEX(edict())) * m_flFrameInterval * RC2Pref(RC2P_Attributes_EnergyBaseRegenerationRate).f;
			if (m_fShieldOn)
				flRegen *= RC2Pref(RC2P_Shield_EnergyRegenerationScale).f;
			m_flEnergy += flRegen;
			if (m_flEnergy > 100)
				m_flEnergy = 100;
		}
		else
			m_flEnergy = 100;
	}

	// check to see if player landed hard enough to make a sound
	// falling farther than half of the maximum safe distance, but not as far a max safe distance will
	// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
	// of maximum safe distance will make no sound. Falling farther than max safe distance will play a
	// fallpain sound, and damage will be inflicted based on how far the player fell

	if ((FBitSet(pev->flags, FL_ONGROUND)) && (pev->health > 0) && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)
	{
		// ALERT ( at_console, "%f\n", m_flFallVelocity );

		if (pev->watertype == CONTENT_WATER)
		{
			// Did he hit the world or a non-moving entity?
			// BUG - this happens all the time in water, especially when
			// BUG - water has current force
			// if ( !pev->groundentity || VARS(pev->groundentity)->velocity.z == 0 )
			// EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM);
		}
		else if (m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED && !((m_iTransformType == 1 || m_iTransformType == 5) && g_pGameRules->IsMultiplayer()))
		{ // after this point, we start doing damage

			float flFallDamage = g_pGameRules->FlPlayerFallDamage(this);

			// RC2: and only in single player
			if (g_pGameRules->IsMultiplayer())
				flFallDamage = 0;

			if (flFallDamage > pev->health)
			{ //splat
				// note: play on item channel because we play footstep landing on body channel
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat.wav", 1, ATTN_NORM);
			}

			if (flFallDamage > 0)
			{
				TakeDamage(CWorld::World->pev, CWorld::World->pev, flFallDamage, DMG_FALL);
				pev->punchangle.x = 0;
			}
		}

		if (IsAlive())
		{
			SetAnimation(PLAYER_WALK);
		}
	}

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		if (m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer())
		{
			CSoundEnt::InsertSound(bits_SOUND_PLAYER, pev->origin, m_flFallVelocity, 0.2);
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	// select the proper animation for the player character
	if (IsAlive())
	{
		if (0 == pev->velocity.x && 0 == pev->velocity.y)
			SetAnimation(PLAYER_IDLE);
		else if ((0 != pev->velocity.x || 0 != pev->velocity.y) && (FBitSet(pev->flags, FL_ONGROUND)))
			SetAnimation(PLAYER_WALK);
		else if (pev->waterlevel > 1)
			SetAnimation(PLAYER_WALK);
		else if (m_iTransformType != 0)
			SetAnimation(PLAYER_WALK);
	}

	{
		const float flInterval = StudioFrameAdvance(0.0);
		if (m_iTransformType != 0)
			DispatchAnimEvents(flInterval);
	}
	CheckPowerups(pev);

	UpdatePlayerSound();

pt_end:
#if defined(CLIENT_WEAPONS)
	// Decay timers on weapons
	// go through all of the weapons and make a list of the ones to pack
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			CBasePlayerItem* pPlayerItem = m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				CBasePlayerWeapon* gun = pPlayerItem->GetWeaponPtr();

				if (gun && gun->UseDecrement())
				{
					gun->m_flNextPrimaryAttack = V_max(gun->m_flNextPrimaryAttack - gpGlobals->frametime, -1.0f);
					gun->m_flNextSecondaryAttack = V_max(gun->m_flNextSecondaryAttack - gpGlobals->frametime, -0.001f);

					if (gun->m_flTimeWeaponIdle != 1000)
					{
						gun->m_flTimeWeaponIdle = V_max(gun->m_flTimeWeaponIdle - gpGlobals->frametime, -0.001f);
					}

					if (gun->pev->fuser1 != 1000)
					{
						gun->pev->fuser1 = V_max(gun->pev->fuser1 - gpGlobals->frametime, -0.001f);
					}

					gun->DecrementTimers();

					// Only decrement if not flagged as NO_DECREMENT
					//					if ( gun->m_flPumpTime != 1000 )
					//	{
					//		gun->m_flPumpTime	= V_max( gun->m_flPumpTime - gpGlobals->frametime, -0.001 );
					//	}
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	m_flNextAttack -= gpGlobals->frametime;
	if (m_flNextAttack < -0.001)
		m_flNextAttack = -0.001;

	if (m_flNextAmmoBurn != 1000)
	{
		m_flNextAmmoBurn -= gpGlobals->frametime;

		if (m_flNextAmmoBurn < -0.001)
			m_flNextAmmoBurn = -0.001;
	}

	if (m_flAmmoStartCharge != 1000)
	{
		m_flAmmoStartCharge -= gpGlobals->frametime;

		if (m_flAmmoStartCharge < -0.001)
			m_flAmmoStartCharge = -0.001;
	}
#endif

	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;
}


// checks if the spot is clear of players
// orig 0x9a3c0. RC2: a spot is taken by an active player (IsActivePlayer), not any player
bool IsSpawnPointValid(CBaseEntity* pPlayer, CBaseEntity* pSpot)
{
	CBaseEntity* ent = NULL;

	if (!pSpot->IsTriggered(pPlayer))
	{
		return false;
	}

	while ((ent = UTIL_FindEntityInSphere(ent, pSpot->pev->origin, 128)) != NULL)
	{
		// if ent is a client, don't spawn on 'em
		if (ent->IsActivePlayer() && ent != pPlayer)
			return false;
	}

	return true;
}

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at

USES AND SETS GLOBAL g_pLastSpawn
============
*/
// orig 0x9a430. RC2: telefrags active players only (IsActivePlayer)
edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer)
{
	CBaseEntity* pSpot;
	edict_t* player;

	int nNumRandomSpawnsToTry = 10;

	player = pPlayer->edict();

	// choose a info_player_deathmatch point
	if (g_pGameRules->IsCoOp())
	{
		pSpot = UTIL_FindEntityByClassname(g_pLastSpawn, "info_player_coop");
		if (!FNullEnt(pSpot))
			goto ReturnSpot;
		pSpot = UTIL_FindEntityByClassname(g_pLastSpawn, "info_player_start");
		if (!FNullEnt(pSpot))
			goto ReturnSpot;
	}
	else if (g_pGameRules->IsDeathmatch())
	{
		if (NULL == g_pLastSpawn)
		{
			int nNumSpawnPoints = 0;
			CBaseEntity* pEnt = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch");
			while (NULL != pEnt)
			{
				nNumSpawnPoints++;
				pEnt = UTIL_FindEntityByClassname(pEnt, "info_player_deathmatch");
			}
			nNumRandomSpawnsToTry = nNumSpawnPoints;
		}

		pSpot = g_pLastSpawn;
		// Randomize the start spot
		for (int i = RANDOM_LONG(1, nNumRandomSpawnsToTry - 1); i > 0; i--)
			pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");
		if (FNullEnt(pSpot)) // skip over the null point
			pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");

		CBaseEntity* pFirstSpot = pSpot;

		do
		{
			if (pSpot)
			{
				// check if pSpot is valid
				if (IsSpawnPointValid(pPlayer, pSpot))
				{
					if (pSpot->pev->origin == Vector(0, 0, 0))
					{
						pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");
						continue;
					}

					// if so, go to pSpot
					goto ReturnSpot;
				}
			}
			// increment pSpot
			pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");
		} while (pSpot != pFirstSpot); // loop if we're not back to the start

		// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
		if (!FNullEnt(pSpot))
		{
			CBaseEntity* ent = NULL;
			while ((ent = UTIL_FindEntityInSphere(ent, pSpot->pev->origin, 128)) != NULL)
			{
				// if ent is a client, kill em (unless they are ourselves)
				if (ent->IsActivePlayer() && !(ent->edict() == player))
					ent->TakeDamage(CWorld::World->pev, CWorld::World->pev, 300, DMG_GENERIC);
			}
			goto ReturnSpot;
		}
	}

	// If startspot is set, (re)spawn there.
	if (FStringNull(gpGlobals->startspot) || 0 == strlen(STRING(gpGlobals->startspot)))
	{
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
		if (!FNullEnt(pSpot))
			goto ReturnSpot;
	}
	else
	{
		pSpot = UTIL_FindEntityByTargetname(NULL, STRING(gpGlobals->startspot));
		if (!FNullEnt(pSpot))
			goto ReturnSpot;
	}

ReturnSpot:
	if (FNullEnt(pSpot))
	{
		ALERT(at_error, "PutClientInServer: no info_player_start on level");
		return CWorld::World->edict();
	}

	g_pLastSpawn = pSpot;
	return pSpot->edict();
}

// orig 0x9a730
void CBasePlayer::SetPhysFlags(int iBits)
{
	m_iPhysKeyFlags |= iBits;
	static char s_szPhf[16]; // name ours (orig .bss 0x18efa8)
	snprintf(s_szPhf, sizeof(s_szPhf), "%02X", m_iPhysKeyFlags);
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "phf", s_szPhf);
}

// orig 0x9a780
void CBasePlayer::ClearPhysFlags(int iBits)
{
	m_iPhysKeyFlags &= ~iBits;
	static char s_szPhf[16]; // name ours (orig .bss 0x18f3d0)
	snprintf(s_szPhf, sizeof(s_szPhf), "%02X", m_iPhysKeyFlags);
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "phf", s_szPhf);
}

// orig 0x9a7d0
void CBasePlayer::ResetPhysFlags()
{
	m_iPhysKeyFlags = 0;
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "phf", "0");
}

// orig 0x9a800. Phys flag 2 stays set while any source holds it
void CBasePlayer::AddPhysFlag2Source(int iBits)
{
	m_iMoveLock |= iBits;
	SetPhysFlags(2);
}

// orig 0x9a820
void CBasePlayer::RemovePhysFlag2Source(int iBits)
{
	m_iMoveLock &= ~iBits;
	if (0 == m_iMoveLock)
		ClearPhysFlags(2);
}

// orig 0x9a840
void CBasePlayer::ClearPhysFlag2Sources()
{
	m_iMoveLock = 0;
	ClearPhysFlags(2);
}

// orig 0x9a860. Phys flag 0x10 for flTime seconds (PreThink takes it off)
void CBasePlayer::SetPhysFlag10For(float flTime)
{
	SetPhysFlags(0x10);
	m_flPhysFlag10Time = flTime + gpGlobals->time;
}

// orig 0x9a880. RC2: a new client in this slot first goes to the spectator view
// (InitNewClient); the attributes chosen are applied and told to everyone (SetAttr), and set the
// health, armor, maximum health and long jump; RC2's per-player state (riding, energy, shield,
// phys flags, view, transform, pull, dying camera) starts over. halflife-updated's spawning flag,
// "hl" and "bj" physinfo keys, sound room type and chat time stay.
void CBasePlayer::Spawn()
{
	m_flStartCharge = gpGlobals->time;
	m_bIsSpawning = true;

	//Make sure this gets reset even if somebody adds an early return or throws an exception.
	const CallOnDestroy resetIsSpawning{[this]()
		{
			//Done spawning; reset.
			m_bIsSpawning = false;
		}};

	MESSAGE_BEGIN(MSG_ONE, gmsgThisClient, NULL, edict());
	WRITE_BYTE(ENTINDEX(edict()));
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	m_iRC2Flags = 0;
	m_iClientGaussBeacons = -1;
	m_iTransformType = 0;
	pev->playerclass = 0;
	m_bTransforming = false;
	m_flNextImpulse = gpGlobals->time;
	SetTouch(NULL);
	m_ucRideInfo = 0;

	// everyone's player stat to this client
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));
		if (pPlayer)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgPlrStat, NULL, edict());
			WRITE_BYTE(ENTINDEX(pPlayer->edict()));
			WRITE_BYTE(pPlayer->m_ucRideInfo);
			MESSAGE_END();
		}
	}

	char szEid[16];
	snprintf(szEid, sizeof(szEid), "%02d", ENTINDEX(edict()));
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "eid", szEid);
	ResetPhysFlags();
	ClearPhysFlag2Sources();
	m_flPhysFlag10Time = -1;
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "shr", "0");

	m_iRidingIndex = 0;
	m_iRideShoulder = 0;
	m_iShoulderRider[1] = 0;
	m_iShoulderRider[0] = 0;

	unsigned char *pvs, *pas;
	SetupVisibility(edict(), edict(), &pvs, &pas);
	SET_VIEW(edict(), edict());

	m_flLastPreThink = gpGlobals->time;
	m_flFrameInterval = 0.0001;
	m_vecRC2Base58 = g_vecZero;
	m_flNextHullUpdate = gpGlobals->time;
	m_bFrozen = false;
	m_flHealthRegen = 0;
	m_fForceSpawn = false;
	pev->owner = NULL;

	if (InitNewClient())
		return;

	// the attributes chosen for this life
	bool bChanged = false;
	if (g_pGameRules->IsMultiplayer())
	{
		for (int i = 0; i < 10; i++)
		{
			if (m_flAttrPending[i] != m_flAttributes[i])
			{
				bChanged = true;
				m_flAttributes[i] = m_flAttrPending[i];
			}
		}
		if (m_iAbilitiesPending != m_iAbilities)
		{
			m_iAbilities = m_iAbilitiesPending;
			bChanged = true;
		}
		m_iAttrWeapons = m_iWeaponsPending;
		m_iChosenTransform = m_iTransformPending;
	}
	else
		bChanged = true; // VERIFY: in single player the original sends SetAttr every spawn without applying anything

	if (bChanged)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgSetAttr);
		WRITE_BYTE(ENTINDEX(edict()));
		for (int i = 0; i < 10; i++)
			WRITE_BYTE(AttrPoints(i, m_flAttributes[i]));
		WRITE_LONG(m_iAbilities);
		MESSAGE_END();
	}

	m_pActiveItem = NULL;
	m_pClientActiveItem = NULL;
	m_pLastItem = NULL;
	for (auto& pItem : m_rgpPlayerItems)
		pItem = NULL;

	pev->classname = MAKE_STRING("player");
	pev->health = PM_GetAttributeScale(RC2ATTR_HEALTH, ENTINDEX(edict()));
	pev->armorvalue = PM_GetAttributeScale(RC2ATTR_ARMOR, ENTINDEX(edict()));
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = PM_GetAttributeScale(RC2ATTR_EXTRAHEALTH, ENTINDEX(edict())) + pev->health;
	pev->flags &= FL_PROXY | FL_FAKECLIENT; // keep proxy and fakeclient flags set by engine
	pev->flags |= FL_CLIENT;
	pev->air_finished = gpGlobals->time + 12;
	pev->dmg = 2; // initial water damage
	pev->effects = 0;
	pev->deadflag = DEAD_NO;
	pev->dmg_take = 0;
	pev->dmg_save = 0;
	pev->friction = 1.0;
	pev->gravity = 1.0;
	m_bitsHUDDamage = -1;
	m_bitsDamageType = 0;
	m_afPhysicsFlags = 0;
	m_fLongJump = (PM_GetAbilityFlags(ENTINDEX(edict())) & RC2ATTRF_LONGJUMP) != 0;
	pev->scale = 1;
	pev->iuser1 = 0;
	pev->iuser2 = 0;
	pev->iuser3 = 0;
	pev->skin = 0;
	m_fJetpackOn = false;
	m_iShrunk = 0;
	m_bNotJoined = false;

	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", m_fLongJump ? "1" : "0");
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "hl", "1");
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "bj", UTIL_dtos1(sv_allowbunnyhopping.value != 0 ? 1 : 0));

	m_flEnergy = 100;
	m_iClientEnergy = -1;
	m_flEnergyRegenTime = gpGlobals->time;
	m_fDrawingEnergy = false;
	m_fCreatingHealth = false;
	m_flEnergyDrawDone = gpGlobals->time;
	m_flHealthCreated = 0;
	m_flCreateHealthDone = gpGlobals->time;
	m_fShieldOn = (PM_GetAbilityFlags(ENTINDEX(edict())) & RC2ATTRF_SHIELD) != 0;
	m_bClientShield = false;
	m_iSpecIndex = -1;
	m_bSpecChase = false;

	m_iFOV = 0; // init field of view.
	pev->fov = 0;
	m_iClientFOV = -1; // make sure fov reset is sent
	m_ClientSndRoomtype = -1;

	m_flNextDecalTime = 0; // let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0; // wait a few seconds until user-defined message registrations
											 // are recieved by all clients

	m_flTimeStepSound = 0;
	m_iStepLeft = 0;
	m_flFieldOfView = 0.5; // some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor = BLOOD_COLOR_RED;
	m_flNextAttack = UTIL_WeaponTimeBase();
	StartSneaking();

	m_iHideHUD = 0;
	m_vecRC2Base58 = g_vecZero;
	m_vecClientPull = Vector(-1, -1, -1);
	m_ucTractorHeld = 0;

	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

	// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam(this);
	g_pGameRules->GetPlayerSpawnSpot(this);

	SET_MODEL(ENT(pev), "models/player.mdl");
	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence = LookupActivity(ACT_IDLE);

	if (FBitSet(pev->flags, FL_DUCKING))
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);

	pev->view_ofs = VEC_VIEW;
	Precache();
	m_HackedGunPos = Vector(0, 32, 0);

	if (m_iPlayerSound == SOUNDLIST_EMPTY)
	{
		ALERT(at_console, "Couldn't alloc player sound slot!\n");
	}

	m_fNoPlayerSound = false; // normal sound behavior.

	m_pLastItem = NULL;
	m_fInitHUD = true;
	m_iClientHideHUD = -1; // force this to be recalculated
	m_fWeapon = false;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;
	m_iClientPlayerStat = 0xff;

	// reset all ammo values to 0
	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0; // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = m_lasty = 0;

	m_flNextChatTime = gpGlobals->time;

	g_pGameRules->PlayerSpawn(this);

	LCS_RunPlayerScript(edict());
}

// orig 0x9b0b0. The points an attribute value costs (what SetAttr sends), from the rc2.ini
// costs; attack and defend are logarithmic
int AttrPoints(int iAttr, float flValue)
{
	float flPoints;
	switch (iAttr)
	{
	case RC2ATTR_JUMP:
		flPoints = RC2Pref(RC2P_Attributes_JumpCost).f * flValue;
		break;
	case RC2ATTR_SPEED:
		flPoints = RC2Pref(RC2P_Attributes_SpeedCost).f * flValue;
		break;
	case RC2ATTR_ATTACK:
		flPoints = log(RC2Pref(RC2P_Attributes_AttackScaler).f * flValue) / RC2Pref(RC2P_Attributes_AttackConstant).f;
		break;
	case RC2ATTR_DEFEND:
		flPoints = log(flValue / RC2Pref(RC2P_Attributes_DefendScaler).f) / -RC2Pref(RC2P_Attributes_DefendConstant).f;
		break;
	case RC2ATTR_HEALTH:
		flPoints = (flValue - RC2Pref(RC2P_Attributes_MinHealth).f) * RC2Pref(RC2P_Attributes_HealthCost).f;
		break;
	case RC2ATTR_EXTRAHEALTH:
		flPoints = RC2Pref(RC2P_Attributes_ExtraHealthCost).f * flValue;
		break;
	case RC2ATTR_ARMOR:
		flPoints = RC2Pref(RC2P_Attributes_ArmorCost).f * flValue;
		break;
	case RC2ATTR_EXTRAARMOR:
		flPoints = RC2Pref(RC2P_Attributes_ExtraArmorCost).f * flValue;
		break;
	case RC2ATTR_ENERGYREGEN:
		flPoints = RC2Pref(RC2P_Attributes_EnergyRegenCost).f * flValue;
		break;
	case RC2ATTR_DMGREFLECT:
		flPoints = RC2Pref(RC2P_Attributes_DamageReflectionCost).f * flValue;
		break;
	default:
		return 0;
	}

	if (flPoints <= 0)
		flPoints = 0;
	return (int)(flPoints + 0.5);
}

// orig 0x9b2c0. When a different client (by WON id, or entity index for a bot) spawns in this
// slot: reset RC2's per-client state, make the dying camera and the second helper entity,
// default the attributes, and in multiplayer put him in the spectator view at a spawn point
// until he joins (returns true then). Returns false for the same client, or in single player.
bool CBasePlayer::InitNewClient()
{
	if (!edict())
		return false;

	const bool bProxy = (pev->flags & FL_PROXY) != 0;

	int iId = g_engfuncs.pfnGetPlayerWONId(edict());
	if ((pev->flags & FL_FAKECLIENT) != 0)
		iId = ENTINDEX(edict());

	if (m_iClientId == iId && !bProxy)
		return false;

	m_iClientId = iId;
	m_iAdminLevel = Admin_GetLevel(edict());
	m_fGiveBan = false;
	m_iSpecIndex = -1;
	m_bSpecChase = false;
	m_flDmgScore = 0;
	m_flClientDmgScore = -1;
	ResetTethers();
	m_fAttributesSet = false;
	m_ucRideInfo = 0;
	m_iClientPlayerStat = 0xff;

	if (bProxy)
	{
		m_iAdminLevel = 0;
		m_iClientId = 1;
		m_fGiveBan = true;
	}

	m_pCam = GetClassPtr((CBaseEntity*)NULL);
	m_pCam->Spawn();
	m_pCam->pev->owner = edict();
	SET_MODEL(m_pCam->edict(), "models/player.mdl");
	m_pCam->pev->renderamt = 0;
	m_pCam->pev->rendermode = kRenderTransColor;
	m_pCam->m_iRC2Flags |= 1;

	m_pCam2 = GetClassPtr((CBaseEntity*)NULL);
	m_pCam2->Spawn();
	m_pCam2->pev->owner = edict();
	SET_MODEL(m_pCam2->edict(), "models/player.mdl");
	m_pCam2->pev->effects |= EF_NODRAW;
	m_pCam2->m_iRC2Flags = 0;

	unsigned char *pvs, *pas;
	SetupVisibility(edict(), edict(), &pvs, &pas);
	SET_VIEW(edict(), edict());

	for (auto& pHull : m_rgpVirtualHulls)
		pHull = NULL;

	if (!g_pGameRules->IsMultiplayer())
	{
		if (!bProxy)
		{
			for (int i = 0; i < 10; i++)
				m_flAttrPending[i] = AttrDefault(i);
			m_iAbilitiesPending = 0;
			m_iWeaponsPending = 1;
			return false;
		}
		m_flNextVoteTime = gpGlobals->time + 1e16;
	}
	else if (bProxy)
		m_flNextVoteTime = gpGlobals->time + 1e16;
	else
		m_flNextVoteTime = gpGlobals->time;

	m_bNotJoined = true;
	m_flJoinTime = gpGlobals->time;

	pev->classname = MAKE_STRING("player");
	pev->health = 100;
	pev->iuser1 = 3;
	pev->iuser2 = 0;
	pev->iuser3 = 0;
	m_bNotJoined = true;
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");
	g_pGameRules->SetDefaultPlayerTeam(this);

	CBaseEntity* pSpot = UTIL_RandomEntityByClassname("info_player_deathmatch");
	if (!pSpot)
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
	if (!pSpot)
		pSpot = CBaseEntity::Instance(INDEXENT(0));

	pev->origin = pSpot->pev->origin;
	pev->v_angle = g_vecZero;
	pev->velocity = g_vecZero;
	pev->angles = pSpot->pev->angles;
	pev->punchangle = g_vecZero;
	pev->fixangle = 1;
	pev->view_ofs = g_vecZero;
	Precache();
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = pev->health;
	pev->flags &= FL_PROXY;
	pev->flags |= FL_SPECTATOR | FL_CLIENT;
	pev->deadflag = DEAD_DEAD;
	pev->effects = EF_NODRAW;
	SET_MODEL(edict(), "models/player.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);

	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0;
	}
	for (auto& pItem : m_rgpPlayerItems)
		pItem = NULL;

	m_flAttributes[0] = -9999; // so that the first Spawn sees a change
	for (int i = 0; i < 10; i++)
		m_flAttrPending[i] = AttrDefault(i);
	m_iAbilitiesPending = 0;
	m_iWeaponsPending = 1;
	m_iTransformPending = -1;

	// everyone's attributes to this client
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));
		if (!pPlayer)
			continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgSetAttr, NULL, edict());
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		for (int a = 0; a < 10; a++)
			WRITE_BYTE(AttrPoints(a, pPlayer->m_flAttributes[a]));
		WRITE_LONG(pPlayer->m_iAbilities);
		MESSAGE_END();
	}

	m_iInitStage = bProxy ? 0 : 3;
	return true;
}

// orig 0x9b940. An attribute's default value
float AttrDefault(int iAttr)
{
	switch (iAttr)
	{
	case RC2ATTR_JUMP:
	case RC2ATTR_SPEED:
	case RC2ATTR_ATTACK:
	case RC2ATTR_DEFEND:
	case RC2ATTR_ENERGYREGEN:
		return 1;
	case RC2ATTR_HEALTH:
		return 100;
	default:
		return 0;
	}
}

// orig 0x9b990. Back to the spectator view (multiplayer): a live player is killed first; at most
// every 5 s while alive; not for an HLTV proxy
void CBasePlayer::GoToSpectatorView()
{
	if (!g_pGameRules->IsMultiplayer())
		return;

	if (gpGlobals->time < m_fNextSuicideTime && pev->deadflag == DEAD_NO)
		return;

	if (m_fGiveBan)
		return;

	m_fNextSuicideTime = gpGlobals->time + 5;
	if (pev->deadflag == DEAD_NO)
	{
		entvars_t* pevWorld = VARS(INDEXENT(0));
		TakeDamage(pevWorld, pevWorld, 50000, DMG_ALWAYSGIB);
	}

	RemoveAllItems(true);
	m_bNotJoined = true;
	pev->classname = MAKE_STRING("player");
	pev->health = 100;
	pev->iuser1 = 3;
	pev->iuser2 = 0;
	ResetTethers();
	m_bNotJoined = true;

	unsigned char *pvs, *pas;
	SetupVisibility(edict(), edict(), &pvs, &pas);
	SET_VIEW(edict(), edict());
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");

	CBaseEntity* pSpot = UTIL_RandomEntityByClassname("info_player_deathmatch");
	if (!pSpot)
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
	if (!pSpot)
		pSpot = CBaseEntity::Instance(INDEXENT(0));

	pev->origin = pSpot->pev->origin;
	pev->v_angle = g_vecZero;
	pev->velocity = g_vecZero;
	pev->angles = pSpot->pev->angles;
	pev->punchangle = g_vecZero;
	pev->fixangle = 1;
	pev->view_ofs = g_vecZero;
	Precache();
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = pev->health;
	pev->flags &= FL_PROXY;
	pev->flags |= FL_SPECTATOR | FL_CLIENT;
	pev->deadflag = DEAD_DEAD;
	pev->effects = EF_NODRAW;
	SET_MODEL(edict(), "models/player.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);

	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0;
	}
	for (auto& pItem : m_rgpPlayerItems)
		pItem = NULL;

	m_pLastItem = NULL;
	m_fInitHUD = true;
	m_iClientHideHUD = -1;
	m_fWeapon = false;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;
	UpdateClientData();
	m_iClientHealth = -1;
	m_iClientBattery = -1;
	m_fInitHUD = true;
	m_iTrain |= TRAIN_NEW;
	m_fWeapon = false;
	m_fKnownItem = false;
}


// orig 0x9bd10. RC2: the transformations' events; and the node graph pointers, as SDK 2.3 did
// here
void CBasePlayer::Precache()
{
	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	g_usTAssassinGun = PRECACHE_EVENT(1, "events/tassassingun.sc");
	g_usTAssassinKick = PRECACHE_EVENT(1, "events/tassassinkick.sc");
	g_usTHoundeyeBeam = PRECACHE_EVENT(1, "events/thoundeyebeam.sc");
	g_usTHoundeyeSonic = PRECACHE_EVENT(1, "events/thoundeyesonic.sc");
	g_usTControllerThrow = PRECACHE_EVENT(1, "events/tcontrollerthrow.sc");
	g_usTBatBite = PRECACHE_EVENT(1, "events/tbatbite.sc");

	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if (0 != WorldGraph.m_fGraphPresent && 0 == WorldGraph.m_fGraphPointersSet)
	{
		if (!WorldGraph.FSetGraphPointers())
		{
			ALERT(at_console, "**Graph pointers were not set!\n");
		}
		else
		{
			ALERT(at_console, "**Graph Pointers Set!\n");
		}
	}

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iClientBattery = -1;

	m_iTrain |= TRAIN_NEW;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();

	m_iUpdateTime = 5; // won't update for 1/2 a second

	if (gInitHUD)
		m_fInitHUD = true;
}


bool CBasePlayer::Save(CSave& save)
{
	if (!CBaseMonster::Save(save))
		return false;

	return save.WriteFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));
}


//
// Marks everything as new so the player will resend this to the hud.
//
// orig 0x9be50
void CBasePlayer::RenewItems()
{
}


// orig 0x9be60
bool CBasePlayer::Restore(CRestore& restore)
{
	if (!CBaseMonster::Restore(restore))
		return false;

	bool status = restore.ReadFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));

	SAVERESTOREDATA* pSaveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;
	// landmark isn't present.
	if (0 == pSaveData->fUseLandmark)
	{
		ALERT(at_console, "No Landmark:%s\n", pSaveData->szLandmarkName);

		// default to normal spawn
		edict_t* pentSpawnSpot = EntSelectSpawnPoint(this);
		pev->origin = VARS(pentSpawnSpot)->origin + Vector(0, 0, 1);
		pev->angles = VARS(pentSpawnSpot)->angles;
	}
	pev->v_angle.z = 0; // Clear out roll
	pev->angles = pev->v_angle;

	pev->fixangle = 1; // turn this way immediately

	m_iClientFOV = -1; // Make sure the client gets the right FOV value.
	m_ClientSndRoomtype = -1;

	// Reset room type on level change.
	if (!FStrEq(restore.GetData().szCurrentMapName, STRING(gpGlobals->mapname)))
	{
		m_SndRoomtype = 0;
	}

	// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	if (FBitSet(pev->flags, FL_DUCKING))
	{
		// Use the crouch HACK
		//FixPlayerCrouchStuck( edict() );
		// Don't need to do this with new player prediction code.
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	}
	else
	{
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	}

	// RC2: the entity index for its pm_shared (halflife-updated's "hl" stays)
	{
		char szEid[16];
		snprintf(szEid, sizeof(szEid), "%02d", ENTINDEX(edict()));
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "eid", szEid);
	}
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "hl", "1");

	if (m_fLongJump)
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "1");
	}
	else
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");
	}

	RenewItems();

	TabulateAmmo();

#if defined(CLIENT_WEAPONS)
	// HACK:	This variable is saved/restored in CBaseMonster as a time variable, but we're using it
	//			as just a counter.  Ideally, this needs its own variable that's saved as a plain float.
	//			Barring that, we clear it out here instead of using the incorrect restored time value.
	m_flNextAttack = UTIL_WeaponTimeBase();
#endif

	m_bResetViewEntity = true;

	m_bRestored = true;

	return status;
}



// orig 0x9c0b0. RC2: not while carrying or transformed
void CBasePlayer::SelectNextItem(int iItem)
{
	CBasePlayerItem* pItem;

	if (0 != m_iShrunk || 0 != m_iTransformType || m_bTransforming)
		return;

	pItem = m_rgpPlayerItems[iItem];

	if (!pItem)
		return;

	if (pItem == m_pActiveItem)
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if (!pItem)
		{
			return;
		}

		CBasePlayerItem* pLast;
		pLast = pItem;
		while (pLast->m_pNext)
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[iItem] = pItem;
	}

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
	{
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->Deploy();
		m_pActiveItem->UpdateItemInfo();
	}
}

// orig 0x9c180. RC2: not while carrying or transformed, nor when the active item can't be put away
void CBasePlayer::SelectItem(const char* pstr)
{
	if (!pstr)
		return;

	if (0 != m_iShrunk || 0 != m_iTransformType || m_bTransforming)
		return;

	if (m_pActiveItem && !m_pActiveItem->CanHolster())
		return;

	CBasePlayerItem* pItem = NULL;

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];

			while (pItem)
			{
				if (FClassnameIs(pItem->pev, pstr))
					break;
				pItem = pItem->m_pNext;
			}
		}

		if (pItem)
			break;
	}

	if (!pItem)
		return;


	if (pItem == m_pActiveItem)
		return;

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster();

	m_pLastItem = m_pActiveItem;
	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->m_ForceSendAnimations = true;
		m_pActiveItem->Deploy();
		m_pActiveItem->m_ForceSendAnimations = false;
		m_pActiveItem->UpdateItemInfo();
	}
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
bool CBasePlayer::HasWeapons()
{
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			return true;
		}
	}

	return false;
}

// orig 0x9c360
void CBasePlayer::SelectPrevItem(int iItem)
{
}


const char* CBasePlayer::TeamID()
{
	if (pev == NULL) // Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void Spawn(entvars_t* pevOwner);
	void Think() override;

	int ObjectCaps() override { return FCAP_DONT_SAVE; } // orig 0x9ce20
};

// orig 0x9c390
void CSprayCan::Spawn(entvars_t* pevOwner)
{
	pev->origin = pevOwner->origin + Vector(0, 0, 32);
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);
	pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.1;
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM);
}

// orig 0x9c460
void CSprayCan::Think()
{
	TraceResult tr;
	int playernum;
	int nFrames;
	CBasePlayer* pPlayer;

	pPlayer = (CBasePlayer*)GET_PRIVATE(pev->owner);

	if (pPlayer)
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX(pev->owner);

	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors(pev->angles);
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

	// No customization present.
	if (nFrames == -1)
	{
		UTIL_DecalTrace(&tr, DECAL_LAMBDA6);
		UTIL_Remove(this);
	}
	else
	{
		UTIL_PlayerDecalTrace(&tr, playernum, pev->frame, true);
		// Just painted last custom frame.
		if (pev->frame++ >= (nFrames - 1))
			UTIL_Remove(this);
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

class CBloodSplat : public CBaseEntity
{
public:
	void Spawn(entvars_t* pevOwner);
	void Spray();
};

// orig 0x9c5b0
void CBloodSplat::Spawn(entvars_t* pevOwner)
{
	pev->origin = pevOwner->origin + Vector(0, 0, 32);
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);

	SetThink(&CBloodSplat::Spray);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x9c650
void CBloodSplat::Spray()
{
	TraceResult tr;

	if (g_Language != LANGUAGE_GERMAN)
	{
		UTIL_MakeVectors(pev->angles);
		UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

		UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_RED);
	}
	SetThink(&CBloodSplat::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
}

//==============================================

static edict_t* GiveNamedItem_Common(entvars_t* pev, const char* pszName)
{
	int istr = MAKE_STRING(pszName);

	edict_t* pent = CREATE_NAMED_ENTITY(istr);
	if (FNullEnt(pent))
	{
		ALERT(at_console, "NULL Ent in GiveNamedItem!\n");
		return nullptr;
	}
	VARS(pent)->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	DispatchSpawn(pent);

	return pent;
}

void CBasePlayer::GiveNamedItem(const char* szName)
{
	auto pent = GiveNamedItem_Common(pev, szName);

	if (!pent)
	{
		return;
	}

	DispatchTouch(pent, ENT(pev));
}

void CBasePlayer::GiveNamedItem(const char* szName, int defaultAmmo)
{
	auto pent = GiveNamedItem_Common(pev, szName);

	if (!pent)
	{
		return;
	}

	auto entity = CBaseEntity::Instance(pent);

	if (!entity)
	{
		return;
	}

	if (auto weapon = dynamic_cast<CBasePlayerWeapon*>(entity); weapon)
	{
		weapon->m_iDefaultAmmo = defaultAmmo;
	}

	DispatchTouch(pent, ENT(pev));
}

// orig 0x9c900
bool CBasePlayer::FlashlightIsOn()
{
	return FBitSet(pev->effects, EF_DIMLIGHT);
}


void CBasePlayer::FlashlightTurnOn()
{
	if (!g_pGameRules->FAllowFlashlight())
	{
		return;
	}

	if (HasSuit())
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM);
		SetBits(pev->effects, EF_DIMLIGHT);
		MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
		WRITE_BYTE(1);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
	}
}


// orig 0x9c9c0. RC2: the click only while alive
void CBasePlayer::FlashlightTurnOff()
{
	if (pev->deadflag == DEAD_NO)
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM);
	ClearBits(pev->effects, EF_DIMLIGHT);
	MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
// orig 0x9ca60
void CBasePlayer::ForceClientDllUpdate()
{
	m_iClientHealth = -1;
	m_iClientBattery = -1;
	m_iClientHideHUD = -1;
	m_iClientFOV = -1;
	m_ClientWeaponBits = 0;
	m_ClientSndRoomtype = -1;

	for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
	{
		m_rgAmmoLast[i] = 0;
	}

	m_iTrain |= TRAIN_NEW; // Force new train message.
	m_fWeapon = false;	   // Force weapon send
	m_fKnownItem = false;  // Force weaponinit messages.
	// RC2 doesn't force the HUD reset (SDK 2.3 set m_fInitHUD here)

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/
// orig 0x9caa0. RC2: nothing while frozen; impulses at most every 0.1 s (204 always), and an
// impulse that comes too early waits instead of being dropped
void CBasePlayer::ImpulseCommands()
{
	TraceResult tr; // UNDONE: kill me! This is temporary for PreAlpha CDs

	if (RC2Frozen())
		return;

	// Handle use events
	PlayerUse();

	if (gpGlobals->time < m_flNextImpulse && pev->impulse != 204)
		return;

	if (0 != pev->impulse)
		m_flNextImpulse = gpGlobals->time + 0.1;

	int iImpulse = (int)pev->impulse;
	switch (iImpulse)
	{
	case 99:
	{

		bool iOn;

		if (0 == gmsgLogo)
		{
			iOn = true;
			gmsgLogo = REG_USER_MSG("Logo", 1);
		}
		else
		{
			iOn = false;
		}

		ASSERT(gmsgLogo > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgLogo, NULL, pev);
		WRITE_BYTE(static_cast<int>(iOn));
		MESSAGE_END();

		if (!iOn)
			gmsgLogo = 0;
		break;
	}
	case 100:
		// temporary flashlight for level designers
		if (FlashlightIsOn())
		{
			FlashlightTurnOff();
		}
		else
		{
			FlashlightTurnOn();
		}
		break;

	case 201: // paint decal

		if (gpGlobals->time < m_flNextDecalTime)
		{
			// too early!
			break;
		}

		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine(pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0)
		{ // line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
			CSprayCan* pCan = GetClassPtr((CSprayCan*)NULL);
			pCan->Spawn(pev);
		}

		break;

	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands(iImpulse);
		break;
	}

	pev->impulse = 0;
}

//=========================================================
//=========================================================
// orig 0x9ce30. RC2 keeps only impulse 101 (every SDK 2.3 weapon and its ammo, the suit and a
// battery), under sv_cheats
void CBasePlayer::CheatImpulseCommands(int iImpulse)
{
	if (0 == g_psv_cheats->value)
	{
		return;
	}

	if (iImpulse != 101)
		return;

	gEvilImpulse101 = true;
	g_fRC2Randomize = false;
	GiveNamedItem("item_suit");
	GiveNamedItem("item_battery");
	GiveNamedItem("weapon_crowbar");
	GiveNamedItem("weapon_9mmhandgun");
	GiveNamedItem("ammo_9mmclip");
	GiveNamedItem("weapon_shotgun");
	GiveNamedItem("ammo_buckshot");
	GiveNamedItem("weapon_9mmAR");
	GiveNamedItem("ammo_9mmAR");
	GiveNamedItem("ammo_ARgrenades");
	GiveNamedItem("weapon_handgrenade");
	GiveNamedItem("weapon_tripmine");
	GiveNamedItem("weapon_357");
	GiveNamedItem("ammo_357");
	GiveNamedItem("weapon_crossbow");
	GiveNamedItem("ammo_crossbow");
	GiveNamedItem("weapon_egon");
	GiveNamedItem("weapon_gauss");
	GiveNamedItem("ammo_gaussclip");
	GiveNamedItem("weapon_rpg");
	GiveNamedItem("ammo_rpgclip");
	GiveNamedItem("weapon_satchel");
	GiveNamedItem("weapon_snark");
	GiveNamedItem("weapon_hornetgun");
	gEvilImpulse101 = false;
	g_fRC2Randomize = true;
}

// orig 0x9cfa0. An item this player can't take goes to a shoulder rider: a random one first,
// the other when the first has it already
bool CBasePlayer::RidersAddPlayerItem(CBasePlayerItem* pItem)
{
	if (0 == m_iShoulderRider[0] && 0 == m_iShoulderRider[1])
		return false;

	int r = RANDOM_LONG(0, 1);
	if (0 == m_iShoulderRider[r])
		r = !r;

	if (0 != m_iShoulderRider[r])
	{
		CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[r]));
		if (pRider)
		{
			if (pRider->HasPlayerItem(pItem) && 0 != m_iShoulderRider[!r])
			{
				CBasePlayer* pOther = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[!r]));
				if (pOther && !pOther->HasPlayerItem(pItem) && pOther->AddPlayerItem(pItem))
					return true;
			}

			if (pRider->AddPlayerItem(pItem))
				return true;
		}
	}

	if (0 != m_iShoulderRider[!r])
	{
		CBasePlayer* pOther = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[!r]));
		if (pOther && pOther->AddPlayerItem(pItem))
			return true;
	}

	return false;
}

// orig 0x9d0c0. True when there are no riders or they all have the item. Original bug, kept:
// with the first rider missing or without it, the second check asks the first rider again
bool CBasePlayer::RidersHavePlayerItem(CBasePlayerItem* pItem)
{
	if (0 == m_iShoulderRider[0] && 0 == m_iShoulderRider[1])
		return true;

	CBasePlayer* pRider;
	if (0 != m_iShoulderRider[0] && (pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[0]))) != NULL && pRider->HasPlayerItem(pItem))
	{
		if (0 == m_iShoulderRider[1])
			return true;
		pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[1]));
		if (!pRider)
			return true;
	}
	else
	{
		if (0 == m_iShoulderRider[1])
			return false;
		pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[0]));
		if (!pRider)
			return false;
	}

	return pRider->HasPlayerItem(pItem);
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
// orig 0x9d150. RC2: what this player can't take, or has and his riders don't, goes to the
// riders on his shoulders (RidersAddPlayerItem)
bool CBasePlayer::AddPlayerItem(CBasePlayerItem* pItem)
{
	CBasePlayerItem* pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while (pInsert)
	{
		if (FClassnameIs(pInsert->pev, STRING(pItem->pev->classname)))
		{
			if (gEvilImpulse101 || RidersHavePlayerItem(pItem))
			{
				if (pItem->AddDuplicate(pInsert))
				{
					g_pGameRules->PlayerGotWeapon(this, pItem);
					pItem->CheckRespawn();

					// ugly hack to update clip w/o an update clip message
					pInsert->UpdateItemInfo();
					if (m_pActiveItem)
						m_pActiveItem->UpdateItemInfo();

					pItem->Kill();
					return false;
				}
				else if (gEvilImpulse101)
				{
					// FIXME: remove anyway for deathmatch testing
					pItem->Kill();
					return false;
				}
			}
			return RidersAddPlayerItem(pItem);
		}
		pInsert = pInsert->m_pNext;
	}


	if (pItem->CanAddToPlayer(this))
	{
		pItem->AddToPlayer(this);

		g_pGameRules->PlayerGotWeapon(this, pItem);
		pItem->CheckRespawn();

		// Add to the list before extracting ammo so weapon ownership checks work properly.
		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		if (auto weapon = pItem->GetWeaponPtr(); weapon)
		{
			weapon->ExtractAmmo(weapon);

			//Immediately update the ammo HUD so weapon pickup isn't sometimes red because the HUD doesn't know about regenerating/free ammo yet.
			if (-1 != weapon->m_iPrimaryAmmoType)
			{
				SendSingleAmmoUpdate(CBasePlayer::GetAmmoIndex(weapon->pszAmmo1()));
			}

			if (-1 != weapon->m_iSecondaryAmmoType)
			{
				SendSingleAmmoUpdate(CBasePlayer::GetAmmoIndex(weapon->pszAmmo2()));
			}

			//Don't show weapon pickup if we're spawning or if it's an exhaustible weapon (will show ammo pickup instead).
			if (!m_bIsSpawning && (weapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE) == 0)
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pev);
				WRITE_BYTE(weapon->m_iId);
				MESSAGE_END();
			}
		}

		// should we switch to this item?
		if (g_pGameRules->FShouldSwitchWeapon(this, pItem))
		{
			SwitchWeapon(pItem);
		}

		return true;
	}
	else if (gEvilImpulse101)
	{
		// FIXME: remove anyway for deathmatch testing
		pItem->Kill();
		return false;
	}
	return RidersAddPlayerItem(pItem);
}



bool CBasePlayer::RemovePlayerItem(CBasePlayerItem* pItem)
{
	if (m_pActiveItem == pItem)
	{
		ResetAutoaim();
		pItem->Holster();
		pItem->pev->nextthink = 0; // crowbar may be trying to swing again, etc.
		pItem->SetThink(NULL);
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}
	if (m_pLastItem == pItem)
		m_pLastItem = NULL;

	CBasePlayerItem* pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if (pPrev == pItem)
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return true;
	}
	else
	{
		while (pPrev && pPrev->m_pNext != pItem)
		{
			pPrev = pPrev->m_pNext;
		}
		if (pPrev)
		{
			pPrev->m_pNext = pItem->m_pNext;
			return true;
		}
	}
	return false;
}


//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer::GiveAmmo(int iCount, const char* szName, int iMax)
{
	return GiveAmmo(iCount, szName, iMax, NULL);
}

// orig 0x9d3d0. Ammo this player can't take goes to his shoulder riders, a random one first
int CBasePlayer::RidersGiveAmmo(int iAmount, const char* szName, int iMax)
{
	if (iAmount < 1)
		return -1;

	if (gEvilImpulse101)
		return -1;

	int iResult = -1;
	int iGiven = -1;
	if (0 == m_iShoulderRider[0] && 0 == m_iShoulderRider[1])
		return iResult;

	int r = RANDOM_LONG(0, 1);
	if (0 == m_iShoulderRider[r])
		r = !r;

	if (0 != m_iShoulderRider[r])
	{
		CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[r]));
		if (pRider)
		{
			iResult = pRider->GiveAmmo(iAmount, szName, iMax, &iGiven);
			if (iAmount <= iGiven)
				return iResult;
			// VERIFY: when the rider took none, iGiven stays -1 and the next one is offered one more
			iAmount -= iGiven;
		}
	}

	if (0 != m_iShoulderRider[!r])
	{
		CBasePlayer* pOther = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(m_iShoulderRider[!r]));
		if (pOther)
			iResult = pOther->GiveAmmo(iAmount, szName, iMax, &iGiven);
	}

	return iResult;
}

// orig 0x9d4c0. RC2's has a fourth argument, where it stores how much it took; what is left
// over goes to the shoulder riders
int CBasePlayer::GiveAmmo(int iCount, const char* szName, int iMax, int* piGiven)
{
	if (!szName)
	{
		// no ammo.
		return -1;
	}

	if (!g_pGameRules->CanHaveAmmo(this, szName, iMax))
	{
		// game rules say I can't have any more of this ammo type.
		return RidersGiveAmmo(iCount, szName, iMax);
	}

	int i = 0;

	i = GetAmmoIndex(szName);

	if (i < 0 || i >= MAX_AMMO_SLOTS)
		return -1;

	int iAdd = V_min(iCount, iMax - m_rgAmmo[i]);
	if (iAdd < 1)
	{
		RidersGiveAmmo(iCount, szName, iMax);
		return i;
	}

	// If this is an exhaustible weapon make sure the player has it.
	if (const auto& ammoType = CBasePlayerItem::AmmoInfoArray[i]; ammoType.WeaponName != nullptr)
	{
		if (!HasNamedPlayerItem(ammoType.WeaponName))
		{
			GiveNamedItem(ammoType.WeaponName, 0);
		}
	}

	m_rgAmmo[i] += iAdd;


	if (0 != gmsgAmmoPickup) // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, pev);
		WRITE_BYTE(GetAmmoIndex(szName));		  // ammo ID
		WRITE_BYTE(std::clamp(iAdd, 0, 255)); // amount
		MESSAGE_END();
	}

	TabulateAmmo();

	if (piGiven)
		*piGiven = iAdd;

	if (iAdd < iCount)
		RidersGiveAmmo(iCount - iAdd, szName, iMax);

	return i;
}


/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/
void CBasePlayer::ItemPreFrame()
{
#if defined(CLIENT_WEAPONS)
	if (m_flNextAttack > 0)
#else
	if (gpGlobals->time < m_flNextAttack)
#endif
	{
		return;
	}

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPreFrame();
}


/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/
// orig 0x9d620. RC2: carrying or transformed, only +use; and SDK 2.3's attack-time check
void CBasePlayer::ItemPostFrame()
{
	// check if the player is using a tank
	if (m_pTank != NULL)
		return;

	if (0 != m_iShrunk || 0 != m_iTransformType || m_bTransforming)
	{
		PlayerUse();
		return;
	}

#if defined(CLIENT_WEAPONS)
	if (m_flNextAttack > 0)
#else
	if (gpGlobals->time < m_flNextAttack)
#endif
	{
		return;
	}

	ImpulseCommands();

	// check again if the player is using a tank if they started using it in PlayerUse
	if (m_pTank != NULL)
		return;

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPostFrame();
}

int CBasePlayer::AmmoInventory(int iAmmoIndex)
{
	if (iAmmoIndex == -1)
	{
		return -1;
	}

	return m_rgAmmo[iAmmoIndex];
}

int CBasePlayer::GetAmmoIndex(const char* psz)
{
	int i;

	if (!psz)
		return -1;

	for (i = 1; i < MAX_AMMO_SLOTS; i++)
	{
		if (!CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

		if (stricmp(psz, CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
			return i;
	}

	return -1;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
// orig 0x9d700
void CBasePlayer::SendAmmoUpdate()
{
	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		InternalSendSingleAmmoUpdate(i);
	}
}

void CBasePlayer::SendSingleAmmoUpdate(int ammoIndex)
{
	if (ammoIndex < 0 || ammoIndex >= MAX_AMMO_SLOTS)
	{
		return;
	}

	InternalSendSingleAmmoUpdate(ammoIndex);
}

void CBasePlayer::InternalSendSingleAmmoUpdate(int ammoIndex)
{
	if (m_rgAmmo[ammoIndex] != m_rgAmmoLast[ammoIndex])
	{
		m_rgAmmoLast[ammoIndex] = m_rgAmmo[ammoIndex];

		ASSERT(m_rgAmmo[ammoIndex] >= 0);
		ASSERT(m_rgAmmo[ammoIndex] < 255);

		// send "Ammo" update message. RC2: the count is a short (AmmoX is three bytes)
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoX, NULL, pev);
		WRITE_BYTE(ammoIndex);
		WRITE_SHORT(V_max(V_min(m_rgAmmo[ammoIndex], 0x7fff), 0));
		MESSAGE_END();
	}
}

/*
=========================================================
	UpdateClientData

resends any changed player HUD info to the client.
Called every frame by PlayerPreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/
// orig 0x9d780. Bit 0 of m_iGaussBeacons while a gauss beacon this player put down is out
void CBasePlayer::UpdateGaussBeacons()
{
	m_iGaussBeacons = 0;
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent) || !pent->pvPrivateData)
			continue;

		if (FClassnameIs(&pent->v, "gauss_becon") && pent->v.dmg_inflictor == edict())
			m_iGaussBeacons |= 1;
	}
}

// orig 0x9d860. RC2's HUD messages: no status bar, Weapons or no-weapon CurWeapon; Energy,
// PlrStat, PullerCtrl, WeaponSync and EIcons; Battery clamped to a short; WeaponList with the
// first maximum ammo as a short. halflife-updated's flashlight resend after a restore, room type
// and one-health rounding stay.
void CBasePlayer::UpdateClientData()
{
	if (m_fInitHUD)
	{
		m_fInitHUD = false;
		gInitHUD = false;

		MESSAGE_BEGIN(MSG_ONE, gmsgResetHUD, NULL, pev);
		WRITE_BYTE(0);
		MESSAGE_END();

		if (!m_fGameHUDInitialized)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgInitHUD, NULL, pev);
			MESSAGE_END();

			g_pGameRules->InitHUD(this);
			m_fGameHUDInitialized = true;

			m_iObserverLastMode = OBS_ROAMING;

			if (g_pGameRules->IsMultiplayer())
			{
				FireTargets("game_playerjoin", this, this, USE_TOGGLE, 0);
			}
		}

		FireTargets("game_playerspawn", this, this, USE_TOGGLE, 0);
	}

	if (m_iHideHUD != m_iClientHideHUD)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, pev);
		WRITE_BYTE(m_iHideHUD);
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if (m_iFOV != m_iClientFOV)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
		WRITE_BYTE(m_iFOV);
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	// HACKHACK -- send the message to display the game title
	//TODO: will not work properly in multiplayer
	if (gDisplayTitle)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgShowGameTitle, NULL, pev);
		WRITE_BYTE(0);
		MESSAGE_END();
		gDisplayTitle = false;
	}

	if ((int)pev->health != m_iClientHealth)
	{
		int iHealth = std::clamp<float>(pev->health, 0.f, (float)(std::numeric_limits<short>::max())); // make sure that no negative health values are sent
		if (pev->health > 0.0f && pev->health <= 1.0f)
			iHealth = 1;

		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, pev);
		WRITE_SHORT(iHealth);
		MESSAGE_END();

		m_iClientHealth = pev->health;
	}

	if ((int)m_flEnergy != m_iClientEnergy)
	{
		m_iClientEnergy = (int)m_flEnergy;

		MESSAGE_BEGIN(MSG_ONE, gmsgEnergy, NULL, pev);
		WRITE_BYTE(std::clamp(m_iClientEnergy, 0, 255));
		MESSAGE_END();
	}

	if (m_iClientPlayerStat != m_ucRideInfo)
	{
		m_iClientPlayerStat = m_ucRideInfo;

		MESSAGE_BEGIN(MSG_ALL, gmsgPlrStat);
		WRITE_BYTE(ENTINDEX(edict()));
		WRITE_BYTE(m_iClientPlayerStat);
		MESSAGE_END();
	}

	if ((int)pev->armorvalue != m_iClientBattery)
	{
		m_iClientBattery = pev->armorvalue;

		ASSERT(gmsgBattery > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, pev);
		WRITE_SHORT(std::clamp((int)pev->armorvalue, -32767, 32767));
		MESSAGE_END();
	}

	// the pull on the player (m_vecRC2Base58) while it is on (RC2PullActive), once more when it ends
	bool bSendPull;
	if (m_vecClientPull != m_vecRC2Base58)
		bSendPull = true;
	else if (0 != m_ucTractorHeld)
		bSendPull = false;
	else
		bSendPull = !(m_vecClientPull == g_vecZero && m_vecRC2Base58 == g_vecZero);

	if (bSendPull)
	{
		if (0 == m_ucTractorHeld)
			m_vecRC2Base58 = g_vecZero;
		m_vecClientPull = m_vecRC2Base58;

		MESSAGE_BEGIN(MSG_ONE, gmsgPullerCtrl, NULL, pev);
		WRITE_BYTE(3);
		WRITE_COORD(m_vecRC2Base58.x);
		WRITE_COORD(m_vecRC2Base58.y);
		WRITE_COORD(m_vecRC2Base58.z);
		MESSAGE_END();
	}

	SendPullPoints();

	UpdateGaussBeacons();
	if (m_iGaussBeacons != m_iClientGaussBeacons)
	{
		m_iClientGaussBeacons = m_iGaussBeacons;

		MESSAGE_BEGIN(MSG_ONE, gmsgEIcons, NULL, pev);
		WRITE_BYTE((byte)m_iGaussBeacons);
		MESSAGE_END();
	}

	if (0 != pev->dmg_take || 0 != pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t* other = pev->dmg_inflictor;
		if (other)
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(other);
			if (pEntity)
				damageOrigin = pEntity->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		MESSAGE_BEGIN(MSG_ONE, gmsgDamage, NULL, pev);
		WRITE_BYTE(pev->dmg_save);
		WRITE_BYTE(pev->dmg_take);
		WRITE_LONG(visibleDamageBits);
		WRITE_COORD(damageOrigin.x);
		WRITE_COORD(damageOrigin.y);
		WRITE_COORD(damageOrigin.z);
		MESSAGE_END();

		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	if (m_bRestored)
	{
		//Always tell client about battery state
		MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, pev);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		//Tell client the flashlight is on
		if (FlashlightIsOn())
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
			WRITE_BYTE(1);
			WRITE_BYTE(m_iFlashBattery);
			MESSAGE_END();
		}
	}

	// Update Flashlight
	if ((0 != m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (0 != m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;

				if (0 == m_iFlashBattery)
					FlashlightTurnOff();
			}
		}
		else
		{
			if (m_iFlashBattery < 100)
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, pev);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}


	if ((m_iTrain & TRAIN_NEW) != 0)
	{
		ASSERT(gmsgTrain > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, pev);
		WRITE_BYTE(m_iTrain & 0xF);
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	//
	// New Weapon?
	//
	if (!m_fKnownItem)
	{
		m_fKnownItem = true;

		// Send ALL the weapon info now
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			ItemInfo& II = CBasePlayerItem::ItemInfoArray[i];

			if (WEAPON_NONE == II.iId)
				continue;

			const char* pszName;
			if (!II.pszName)
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN(MSG_ONE, gmsgWeaponList, NULL, pev);
			WRITE_STRING(pszName);				   // string	weapon name
			WRITE_BYTE(GetAmmoIndex(II.pszAmmo1)); // byte		Ammo Type
			WRITE_SHORT(II.iMaxAmmo1);			   // short    Max Ammo 1 (RC2: SDK 2.3 sent a byte)
			WRITE_BYTE(GetAmmoIndex(II.pszAmmo2)); // byte		Ammo2 Type
			WRITE_BYTE(II.iMaxAmmo2);			   // byte     Max Ammo 2
			WRITE_BYTE(II.iSlot);				   // byte		bucket
			WRITE_BYTE(II.iPosition);			   // byte		bucket pos
			WRITE_BYTE(II.iId);					   // byte		id (bit index into m_WeaponBits)
			WRITE_BYTE(II.iFlags);				   // byte		Flags
			MESSAGE_END();
		}
	}


	SendAmmoUpdate();

	// Update all the items
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i]) // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData(this);
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV = m_iFOV;

	// Send new room type to client.
	if (m_ClientSndRoomtype != m_SndRoomtype)
	{
		m_ClientSndRoomtype = m_SndRoomtype;

		MESSAGE_BEGIN(MSG_ONE, SVC_ROOMTYPE, nullptr, edict());
		WRITE_SHORT((short)m_SndRoomtype); // sequence number
		MESSAGE_END();
	}

	//Handled anything that needs resetting
	m_bRestored = false;
}


//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
bool CBasePlayer::FBecomeProne()
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return true;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer::BarnacleVictimBitten(entvars_t* pevBarnacle)
{
	TakeDamage(pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB);
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns.
//=========================================================
// orig 0x9e0d0
void CBasePlayer::BarnacleVictimReleased()
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}


//=========================================================
// Illumination
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer::Illumination()
{
	int iIllum = CBaseEntity::Illumination();

	iIllum += m_iWeaponFlash;
	if (iIllum > 255)
		return 255;
	return iIllum;
}


void CBasePlayer::EnableControl(bool fControl)
{
	if (!fControl)
		pev->flags |= FL_FROZEN;
	else
		pev->flags &= ~FL_FROZEN;
}


#define DOT_1DEGREE 0.9998476951564
#define DOT_2DEGREE 0.9993908270191
#define DOT_3DEGREE 0.9986295347546
#define DOT_4DEGREE 0.9975640502598
#define DOT_5DEGREE 0.9961946980917
#define DOT_6DEGREE 0.9945218953683
#define DOT_7DEGREE 0.9925461516413
#define DOT_8DEGREE 0.9902680687416
#define DOT_9DEGREE 0.9876883405951
#define DOT_10DEGREE 0.9848077530122
#define DOT_15DEGREE 0.9659258262891
#define DOT_20DEGREE 0.9396926207859
#define DOT_25DEGREE 0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer::GetAutoaimVector(float flDelta)
{
	if (g_iSkillLevel == SKILL_HARD)
	{
		UTIL_MakeVectors(pev->v_angle + pev->punchangle);
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition();
	float flDist = 8192;

	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (true || g_iSkillLevel == SKILL_MEDIUM)
	{
		m_vecAutoAim = Vector(0, 0, 0);
		// flDelta *= 0.5;
	}

	bool m_fOldTargeting = m_fOnTarget;
	Vector angles = AutoaimDeflection(vecSrc, flDist, flDelta);

	// update ontarget if changed
	if (!g_pGameRules->AllowAutoTargetCrosshair())
		m_fOnTarget = false;
	else if (m_fOldTargeting != m_fOnTarget)
	{
		m_pActiveItem->UpdateItemInfo();
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;


	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (false || g_iSkillLevel == SKILL_EASY)
	{
		m_vecAutoAim = m_vecAutoAim * 0.67 + angles * 0.33;
	}
	else
	{
		m_vecAutoAim = angles * 0.9;
	}

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	if (g_psv_aim->value != 0 && g_psv_allow_autoaim != 0)
	{
		if (m_vecAutoAim.x != m_lastx ||
			m_vecAutoAim.y != m_lasty)
		{
			SET_CROSSHAIRANGLE(edict(), -m_vecAutoAim.x, m_vecAutoAim.y);

			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}
	else
	{
		ResetAutoaim();
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors(pev->v_angle + pev->punchangle + m_vecAutoAim);
	return gpGlobals->v_forward;
}


// orig 0x9e530. RC2 took autoaim out: no deflection
Vector CBasePlayer::AutoaimDeflection(Vector& vecSrc, float flDist, float flDelta)
{
	return g_vecZero;
}


void CBasePlayer::ResetAutoaim()
{
	if (m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0)
	{
		m_vecAutoAim = Vector(0, 0, 0);
		SET_CROSSHAIRANGLE(edict(), 0, 0);
	}
	m_fOnTarget = false;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer::SetCustomDecalFrames(int nFrames)
{
	if (nFrames > 0 &&
		nFrames < 8)
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
// orig 0x9e620
int CBasePlayer::GetCustomDecalFrames()
{
	return m_nCustomSprayFrames;
}


//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item.
//=========================================================
// orig 0x9e630. RC2: nothing is dropped while more than 20 weaponboxes lie around
void CBasePlayer::DropPlayerItem(char* pszItemName)
{
	if (!g_pGameRules->IsMultiplayer() || (weaponstay.value > 0))
	{
		// no dropping in single player.
		return;
	}

	if (UTIL_CountEntitiesByClass("weaponbox") > 20)
		return;

	if (0 == strlen(pszItemName))
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		// make the string null to make future operations in this function easier
		pszItemName = NULL;
	}

	CBasePlayerItem* pWeapon;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pWeapon = m_rgpPlayerItems[i];

		while (pWeapon)
		{
			if (pszItemName)
			{
				// try to match by name.
				if (0 == strcmp(pszItemName, STRING(pWeapon->pev->classname)))
				{
					// match!
					break;
				}
			}
			else
			{
				// trying to drop active item
				if (pWeapon == m_pActiveItem)
				{
					// active item!
					break;
				}
			}

			pWeapon = pWeapon->m_pNext;
		}


		// if we land here with a valid pWeapon pointer, that's because we found the
		// item we want to drop and hit a BREAK;  pWeapon is the item.
		if (pWeapon)
		{
			if (!g_pGameRules->GetNextBestWeapon(this, pWeapon))
				return; // can't drop the item they asked for, may be our last item or something we can't holster

			UTIL_MakeVectors(pev->angles);

			ClearWeaponBit(pWeapon->m_iId); // take item off hud

			CWeaponBox* pWeaponBox = (CWeaponBox*)CBaseEntity::Create("weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict());
			pWeaponBox->pev->angles.x = 0;
			pWeaponBox->pev->angles.z = 0;
			pWeaponBox->PackWeapon(pWeapon);
			pWeaponBox->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;

			// drop half of the ammo for this weapon.
			int iAmmoIndex;

			iAmmoIndex = GetAmmoIndex(pWeapon->pszAmmo1()); // ???

			if (iAmmoIndex != -1)
			{
				// this weapon weapon uses ammo, so pack an appropriate amount.
				if ((pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE) != 0)
				{
					// pack up all the ammo, this weapon is its own ammo type
					pWeaponBox->PackAmmo(MAKE_STRING(pWeapon->pszAmmo1()), m_rgAmmo[iAmmoIndex]);
					m_rgAmmo[iAmmoIndex] = 0;
				}
				else
				{
					// pack half of the ammo
					pWeaponBox->PackAmmo(MAKE_STRING(pWeapon->pszAmmo1()), m_rgAmmo[iAmmoIndex] / 2);
					m_rgAmmo[iAmmoIndex] /= 2;
				}
			}

			return; // we're done, so stop searching with the FOR loop.
		}
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
bool CBasePlayer::HasPlayerItem(CBasePlayerItem* pCheckItem)
{
	CBasePlayerItem* pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs(pItem->pev, STRING(pCheckItem->pev->classname)))
		{
			return true;
		}
		pItem = pItem->m_pNext;
	}

	return false;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
bool CBasePlayer::HasNamedPlayerItem(const char* pszItemName)
{
	CBasePlayerItem* pItem;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pItem = m_rgpPlayerItems[i];

		while (pItem)
		{
			if (0 == strcmp(pszItemName, STRING(pItem->pev->classname)))
			{
				return true;
			}
			pItem = pItem->m_pNext;
		}
	}

	return false;
}

//=========================================================
// HasPlayerItemFromID
// Just compare IDs, rather than classnames
//=========================================================
bool CBasePlayer::HasPlayerItemFromID(int nID)
{
	CBasePlayerItem* pItem;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pItem = m_rgpPlayerItems[i];

		while (pItem)
		{
			if (pItem->m_iId == nID)
			{
				return true;
			}

			pItem = pItem->m_pNext;
		}
	}

	return false;
}

//=========================================================
//
//=========================================================
// orig 0x9eac0. RC2: not while carrying or transformed
bool CBasePlayer::SwitchWeapon(CBasePlayerItem* pWeapon)
{
	if (0 != m_iShrunk || 0 != m_iTransformType || m_bTransforming)
		return false;

	if (pWeapon && !pWeapon->CanDeploy())
	{
		return false;
	}

	ResetAutoaim();

	if (m_pActiveItem)
	{
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pWeapon;

	if (pWeapon)
	{
		pWeapon->m_ForceSendAnimations = true;
		pWeapon->Deploy();
		pWeapon->m_ForceSendAnimations = false;
	}

	return true;
}

void CBasePlayer::EquipWeapon()
{
	if (m_pActiveItem)
	{
		if ((!FStringNull(pev->viewmodel) || !FStringNull(pev->weaponmodel)))
		{
			//Already have a weapon equipped and deployed.
			return;
		}

		//Have a weapon equipped, but not deployed.
		if (m_pActiveItem->CanDeploy() && m_pActiveItem->Deploy())
		{
			return;
		}
	}

	//No weapon equipped or couldn't deploy it, find a suitable alternative.
	g_pGameRules->GetNextBestWeapon(this, m_pActiveItem, true);
}

void CBasePlayer::SetPrefsFromUserinfo(char* infobuffer)
{
	const char* value = g_engfuncs.pfnInfoKeyValue(infobuffer, "cl_autowepswitch");

	if ('\0' != *value)
	{
		m_iAutoWepSwitch = atoi(value);
	}
	else
	{
		m_iAutoWepSwitch = 1;
	}
}

//=========================================================
// Dead HEV suit prop
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn() override;
	int Classify() override { return CLASS_HUMAN_MILITARY; } // orig 0x9ec20

	bool KeyValue(KeyValueData* pkvd) override;

	int m_iPose; // which sequence to display	-- temporary, don't need to save
	static const char* m_szPoses[4];
};

const char* CDeadHEV::m_szPoses[] = {"deadback", "deadsitting", "deadstomach", "deadtable"};

// orig 0x9eb30
bool CDeadHEV::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

LINK_ENTITY_TO_CLASS(monster_hevsuit_dead, CDeadHEV); // orig 0x9ebb0

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CDeadHEV::Spawn()
{
	PRECACHE_MODEL("models/player.mdl");
	SET_MODEL(ENT(pev), "models/player.mdl");

	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	pev->body = 1;
	m_bloodColor = BLOOD_COLOR_RED;

	pev->sequence = LookupSequence(m_szPoses[m_iPose]);

	if (pev->sequence == -1)
	{
		ALERT(at_console, "Dead hevsuit with bad pose\n");
		pev->sequence = 0;
		pev->effects = EF_BRIGHTFIELD;
	}

	// Corpses have less health
	pev->health = 8;

	MonsterInitDead();
}


class CStripWeapons : public CPointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

private:
};

LINK_ENTITY_TO_CLASS(player_weaponstrip, CStripWeapons); // orig 0x9ed10

void CStripWeapons::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = NULL;

	if (pActivator && pActivator->IsPlayer())
	{
		pPlayer = (CBasePlayer*)pActivator;
	}
	else if (!g_pGameRules->IsDeathmatch())
	{
		pPlayer = (CBasePlayer*)UTIL_GetLocalPlayer();
	}

	if (pPlayer)
		pPlayer->RemoveAllItems(false);
}


class CRevertSaved : public CPointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT MessageThink();
	void EXPORT LoadThink();
	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	inline float Duration() { return pev->dmg_take; }
	inline float HoldTime() { return pev->dmg_save; }
	inline float MessageTime() { return m_messageTime; }
	inline float LoadTime() { return m_loadTime; }

	inline void SetDuration(float duration) { pev->dmg_take = duration; }
	inline void SetHoldTime(float hold) { pev->dmg_save = hold; }
	inline void SetMessageTime(float time) { m_messageTime = time; }
	inline void SetLoadTime(float time) { m_loadTime = time; }

private:
	float m_messageTime;
	float m_loadTime;
};

LINK_ENTITY_TO_CLASS(player_loadsaved, CRevertSaved); // orig 0x9ede0

TYPEDESCRIPTION CRevertSaved::m_SaveData[] =
	{
		DEFINE_FIELD(CRevertSaved, m_messageTime, FIELD_FLOAT), // These are not actual times, but durations, so save as floats
		DEFINE_FIELD(CRevertSaved, m_loadTime, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CRevertSaved, CPointEntity); // orig 0x9ee50 Save, orig 0x9ee80 Restore

bool CRevertSaved::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "messagetime"))
	{
		SetMessageTime(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "loadtime"))
	{
		SetLoadTime(atof(pkvd->szValue));
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

void CRevertSaved::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT);
	pev->nextthink = gpGlobals->time + MessageTime();
	SetThink(&CRevertSaved::MessageThink);
}


void CRevertSaved::MessageThink()
{
	UTIL_ShowMessageAll(STRING(pev->message));
	float nextThink = LoadTime() - MessageTime();
	if (nextThink > 0)
	{
		pev->nextthink = gpGlobals->time + nextThink;
		SetThink(&CRevertSaved::LoadThink);
	}
	else
		LoadThink();
}


void CRevertSaved::LoadThink()
{
	if (0 == gpGlobals->deathmatch)
	{
		SERVER_COMMAND("reload\n");
	}
}


//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	void Spawn() override;
	void Think() override;
};

void CInfoIntermission::Spawn()
{
	UTIL_SetOrigin(pev, pev->origin);
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	pev->nextthink = gpGlobals->time + 2; // let targets spawn!
}

// orig 0x9f180
void CInfoIntermission::Think()
{
	edict_t* pTarget;

	// find my target
	pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->target));

	if (!FNullEnt(pTarget))
	{
		pev->v_angle = UTIL_VecToAngles((pTarget->v.origin - pev->origin).Normalize());
		pev->v_angle.x = -pev->v_angle.x;
	}
}

LINK_ENTITY_TO_CLASS(info_intermission, CInfoIntermission); // orig 0x9f2b0
