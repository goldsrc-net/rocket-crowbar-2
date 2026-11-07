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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"



#include "rc2prefs.h"
#include "soundent.h"
#ifndef CLIENT_DLL
#include "rc2/shield.h"
#include "UserMessages.h"

#define RC2_CAP_SHOVE 0x1 // GetRCCaps: may be knocked about (name ours)
#endif

#define CROWBAR_BODYHIT_VOLUME 128
#define CROWBAR_WALLHIT_VOLUME 512

#ifdef CLIENT_DLL

// halflife-updated's crowbar, kept for the client's weapon prediction until RC2's client.dll is
// reconstructed. The server's crowbar is Rocket Crowbar 2's, below.


LINK_ENTITY_TO_CLASS(weapon_crowbar, CCrowbar);

void CCrowbar::Spawn()
{
	Precache();
	m_iId = WEAPON_CROWBAR;
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit(); // get ready to fall down.
}


void CCrowbar::Precache()
{
	PRECACHE_MODEL("models/v_crowbar.mdl");
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/p_crowbar.mdl");
	PRECACHE_SOUND("weapons/cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");

	m_usCrowbar = PRECACHE_EVENT(1, "events/crowbar.sc");
}

bool CCrowbar::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_CROWBAR;
	p->iWeight = CROWBAR_WEIGHT;
	return true;
}



bool CCrowbar::Deploy()
{
	return DefaultDeploy("models/v_crowbar.mdl", "models/p_crowbar.mdl", CROWBAR_DRAW, "crowbar");
}

void CCrowbar::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(CROWBAR_HOLSTER);
}


void FindHullIntersection(const Vector& vecSrc, TraceResult& tr, const Vector& mins, const Vector& maxs, edict_t* pEntity)
{
	int i, j, k;
	float distance;
	const Vector* minmaxs[2] = {&mins, &maxs};
	TraceResult tmpTrace;
	Vector vecHullEnd = tr.vecEndPos;
	Vector vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc) * 2);
	UTIL_TraceLine(vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace);
	if (tmpTrace.flFraction < 1.0)
	{
		tr = tmpTrace;
		return;
	}

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i]->x;
				vecEnd.y = vecHullEnd.y + minmaxs[j]->y;
				vecEnd.z = vecHullEnd.z + minmaxs[k]->z;

				UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace);
				if (tmpTrace.flFraction < 1.0)
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if (thisDistance < distance)
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


void CCrowbar::PrimaryAttack()
{
	if (!Swing(true))
	{
		SetThink(&CCrowbar::SwingAgain);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}


void CCrowbar::Smack()
{
	DecalGunshot(&m_trHit, BULLET_PLAYER_CROWBAR);
}


void CCrowbar::SwingAgain()
{
	Swing(false);
}


bool CCrowbar::Swing(bool fFirst)
{
	bool fDidHit = false;

	TraceResult tr;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

#ifndef CLIENT_DLL
	if (tr.flFraction >= 1.0)
	{
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(m_pPlayer->pev), &tr);
		if (tr.flFraction < 1.0)
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (!pHit || pHit->IsBSPModel())
				FindHullIntersection(vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict());
			vecEnd = tr.vecEndPos; // This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	if (fFirst)
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCrowbar,
			0.0, g_vecZero, g_vecZero, 0, 0, 0,
			0.0, 0, 0.0);
	}


	if (tr.flFraction >= 1.0)
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = GetNextAttackDelay(0.5);

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		}
	}
	else
	{
		switch (((m_iSwing++) % 2) + 1)
		{
		case 0:
			SendWeaponAnim(CROWBAR_ATTACK1HIT);
			break;
		case 1:
			SendWeaponAnim(CROWBAR_ATTACK2HIT);
			break;
		case 2:
			SendWeaponAnim(CROWBAR_ATTACK3HIT);
			break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL

		// hit
		fDidHit = true;
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

		ClearMultiDamage();

		// JoshA: Changed from < -> <= to fix the full swing logic since client weapon prediction.
		// -1.0f + 1.0f = 0.0f. UTIL_WeaponTimeBase is always 0 with client weapon prediction (0 time base vs curtime base)
		if ((m_flNextPrimaryAttack + 1.0f <= UTIL_WeaponTimeBase()) || g_pGameRules->IsMultiplayer())
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_CLUB);
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgCrowbar / 2, gpGlobals->v_forward, &tr, DMG_CLUB);
		}
		ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

#endif

		m_flNextPrimaryAttack = GetNextAttackDelay(0.25);

#ifndef CLIENT_DLL
		// play thwack, smack, or dong sound
		float flVol = 1.0;
		bool fHitWorld = true;

		if (pEntity)
		{
			if (pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
			{
				// play thwack or smack sound
				switch (RANDOM_LONG(0, 2))
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM);
					break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM);
					break;
				case 2:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM);
					break;
				}
				m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
				if (!pEntity->IsAlive())
					return true;
				else
					flVol = 0.1;

				fHitWorld = false;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_PLAYER_CROWBAR);

			if (g_pGameRules->IsMultiplayer())
			{
				// override the volume here, cause we don't play texture sounds in multiplayer,
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play crowbar strike
			switch (RANDOM_LONG(0, 1))
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;
#endif
		SetThink(&CCrowbar::Smack);
		pev->nextthink = gpGlobals->time + 0.2;
	}
	return fDidHit;
}

#else // !CLIENT_DLL

//=========================================================
// Rocket Crowbar 2's crowbar. Holding the fourth attack charges it (spending the player's RC2
// energy); the charge powers the next swing (more damage and a shove, of the victim or of the
// player off what can't move), the rocket (secondary) or the crowbarang (tertiary, which gives
// the crowbar away until it comes back). A charge drains away a while after it is released.
// Every number is rc2.ini [Crowbar]. Reconstructed from rc2.dll (reference/dossiers/p2.md).
//
// Inline defaults the original emitted here, from weapons.h: CBasePlayerItem::CanHolster
// (orig 0x30450, true), CBasePlayerWeapon::Reload and WeaponIdle (orig 0x30460, orig 0x30470,
// empty).
//=========================================================

LINK_ENTITY_TO_CLASS(weapon_crowbar, CCrowbar); // orig 0x303e0

// orig 0x304a0. RC2's AddToPlayer refuses the crowbar (returns 0) while a crowbarang this
// player threw is still out; halflife-updated asks that before AddToPlayer, so it is here.
bool CCrowbar::CanAddToPlayer(CBasePlayer* pPlayer)
{
	for (CBaseEntity* pBang = UTIL_FindEntityByClassname(NULL, "crowbarang"); pBang; pBang = UTIL_FindEntityByClassname(pBang, "crowbarang"))
	{
		if (UTIL_IsValidEntity(pBang->pev->dmg_inflictor) && pBang->pev->dmg_inflictor == pPlayer->edict())
			return false;
	}
	return true;
}

// orig 0x304a0 (the rest of it): the base's
void CCrowbar::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

// orig 0x30520: end a charge; the client hears it end unless bSilent or there was none
void CCrowbar::ChargeStop(bool bSilent)
{
	edict_t* pPlayer = m_pPlayer->edict();
	PLAYBACK_EVENT_FULL(FEV_RELIABLE | FEV_GLOBAL, pPlayer, m_usCbarCharge, 0, m_pPlayer->pev->origin, g_vecZero, 0, 0, ENTINDEX(pPlayer), 0, !bSilent && m_flCharge > 0, 0);
	m_bCharging = false;
	m_flCharge = 0;
	m_flNextChargeEvent = gpGlobals->time + 0.25;
	m_flNextSync = gpGlobals->time;
}

// orig 0x305d0
void CCrowbar::Spawn()
{
	Precache();
	m_iId = WEAPON_CROWBAR;
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	m_bCharging = false;
	m_flCharge = 0;
	m_flLastFrame = gpGlobals->time;
	m_flDischargeStart = gpGlobals->time;
	m_flNextChargeEvent = gpGlobals->time;
	m_flNextSync = gpGlobals->time;

	FallInit(); // get ready to fall down.
}

// orig 0x30660
void CCrowbar::Precache()
{
	PRECACHE_MODEL("models/v_crowbar.mdl");
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/p_crowbar.mdl");
	PRECACHE_SOUND("weapons/cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");
	PRECACHE_SOUND("ambience/alien_cycletone.wav");
	PRECACHE_SOUND("debris/zap4.wav");

	UTIL_PrecacheOther("crowbarang");
	UTIL_PrecacheOther("crowbar_rocket");

	m_usCrowbar = PRECACHE_EVENT(1, "events/crowbar.sc");
	m_usCrowbarSwing = PRECACHE_EVENT(1, "events/crowbarswing.sc");
	m_usCbarCharge = PRECACHE_EVENT(1, "events/cbarcharge.sc");
}

// orig 0x30740. Unlike SDK 2.3's, the weight is 1 and iFlags is left as the caller had it.
bool CCrowbar::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_CROWBAR;
	p->iWeight = 1;
	return true;
}

// orig 0x30780
bool CCrowbar::Deploy()
{
	ChargeStop(false);
	m_flNextChargeEvent = gpGlobals->time;
	return DefaultDeploy("models/v_crowbar.mdl", "models/p_crowbar.mdl", CROWBAR_DRAW, "crowbar");
}

// orig 0x307c0
void CCrowbar::Holster()
{
	ChargeStop(false);
	m_flNextChargeEvent = gpGlobals->time;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(CROWBAR_HOLSTER);
}

// stock (SDK 2.3's, matched; orig 0x30810), kept for the server
void FindHullIntersection(const Vector& vecSrc, TraceResult& tr, const Vector& mins, const Vector& maxs, edict_t* pEntity)
{
	int i, j, k;
	float distance;
	const Vector* minmaxs[2] = {&mins, &maxs};
	TraceResult tmpTrace;
	Vector vecHullEnd = tr.vecEndPos;
	Vector vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc) * 2);
	UTIL_TraceLine(vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace);
	if (tmpTrace.flFraction < 1.0)
	{
		tr = tmpTrace;
		return;
	}

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i]->x;
				vecEnd.y = vecHullEnd.y + minmaxs[j]->y;
				vecEnd.z = vecHullEnd.z + minmaxs[k]->z;

				UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace);
				if (tmpTrace.flFraction < 1.0)
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if (thisDistance < distance)
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}



// orig 0x30a50
void CCrowbar::PrimaryAttack()
{
	if (!Swing(true))
	{
		SetThink(&CCrowbar::SwingAgain);
		pev->nextthink = RC2Pref(RC2P_Crowbar_PrimaryNoHitSwingAgain).f + gpGlobals->time;
	}
}

// orig 0x30a80: the rocket, powered by the charge, for RocketEnergyCost of the player's energy
void CCrowbar::SecondaryAttack()
{
	float flCharge = m_flCharge;

	if (m_pPlayer->m_flEnergy < RC2Pref(RC2P_Crowbar_RocketEnergyCost).f)
	{
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.5);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.5);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.5);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.5);
		return;
	}

	ChargeStop(true);

	// Original oddity, kept: bparam1 is 4
	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCrowbarSwing, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 4, 0);

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Crowbar_RocketEnergyCost).f;

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefireQuaternary).f);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecAngles(-m_pPlayer->pev->v_angle.x, m_pPlayer->pev->v_angle.y, 0);
	Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;

	CBaseEntity* pRocket = CBaseEntity::Create("crowbar_rocket", vecSrc, vecAngles, m_pPlayer->edict());
	pRocket->pev->velocity = gpGlobals->v_forward * pRocket->pev->armorvalue;
	pRocket->pev->dmg = (RC2Pref(RC2P_Crowbar_RocketChargeDamageFraction).f * flCharge + 1.0) * pRocket->pev->dmg;
	pRocket->pev->armorvalue = (RC2Pref(RC2P_Crowbar_RocketChargeSpeedFraction).f * flCharge + 1.0) * pRocket->pev->armorvalue;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pRocket->pev->owner = pControlled;
	}
}

// orig 0x30d60: throw the crowbar (it leaves the player's inventory)
void CCrowbar::TertiaryAttack()
{
	float flCharge = m_flCharge;

	// Original oddity, kept: the secondary attack's refire keys
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_SecondaryRefireQuaternary).f);

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);

	Vector vecSrc = m_pPlayer->pev->view_ofs + m_pPlayer->pev->origin;
	CBaseEntity* pBang = CBaseEntity::Create("crowbarang", vecSrc, g_vecZero, m_pPlayer->edict());
	pBang->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Crowbar_CrowbarangSpeed).f;
	pBang->pev->movedir = pBang->pev->velocity;
	pBang->pev->owner = m_pPlayer->edict();
	pBang->pev->dmg_inflictor = pBang->pev->owner;
	pBang->pev->dmg = (RC2Pref(RC2P_Crowbar_CrowbarangChargeDamageFraction).f * flCharge + 1.0) * pBang->pev->dmg;

	// a charged one glows
	if (flCharge > 0)
	{
		pBang->pev->renderfx = kRenderFxGlowShell;
		pBang->pev->renderamt = flCharge / RC2Pref(RC2P_Crowbar_MaxCharge).f * 32.0;
		float flFrac = flCharge / RC2Pref(RC2P_Crowbar_MaxCharge).f;
		pBang->pev->rendercolor = Vector(flFrac * 190.0, flFrac * 190.0, flFrac * 255.0);
	}

	pBang->pev->angles.y = RANDOM_FLOAT(0, 360);

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pBang->pev->owner = pControlled;
	}

	ChargeStop(true);

	m_pPlayer->RemovePlayerItem(this);
	m_pPlayer->pev->weapons &= ~(1 << m_iId);
	Drop();
}

// orig 0x31030: start charging
void CCrowbar::QuaternaryAttack()
{
	if (m_flCharge > 0 || m_bCharging)
		return;

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_QuaternaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_QuaternaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_QuaternaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_QuaternaryRefireQuaternary).f);

	m_bCharging = true;
	m_flCharge = 0;
	m_flLastFrame = gpGlobals->time;

	edict_t* pPlayer = m_pPlayer->edict();
	PLAYBACK_EVENT_FULL(0, pPlayer, m_usCbarCharge, 0, g_vecZero, g_vecZero, m_flCharge, 0, ENTINDEX(pPlayer), 0, 0, 1);

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, m_pPlayer->Center(), 400, 0.9);
}

// orig 0x31170
void CCrowbar::Smack()
{
	DecalGunshot(&m_trHit, BULLET_PLAYER_CROWBAR);
}

// orig 0x31190
void CCrowbar::SwingAgain()
{
	Swing(false);
}

// orig 0x311a0. SDK 2.3's swing with the charge: what it hits gets ChargeDamageFraction more
// damage per unit of charge (and is gibbed above half a unit), and the charge's push, of what it
// hit when that can be shoved, otherwise of the player back off it.
bool CCrowbar::Swing(bool fFirst)
{
	bool fDidHit = false;

	TraceResult tr;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (tr.flFraction >= 1.0)
	{
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(m_pPlayer->pev), &tr);
		if (tr.flFraction < 1.0)
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (!pHit || pHit->IsBSPModel())
				FindHullIntersection(vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict());
			vecEnd = tr.vecEndPos; // This is the point on the actual surface (the hull could have hit space)
		}
	}

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCrowbar, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_PrimaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_PrimaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_PrimaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crowbar_PrimaryRefireQuaternary).f);

	if (tr.flFraction >= 1.0)
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
			UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.5);

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		}
		return fDidHit;
	}

	// hit
	switch (((m_iSwing++) % 2) + 1)
	{
	case 0:
		SendWeaponAnim(CROWBAR_ATTACK1HIT);
		break;
	case 1:
		SendWeaponAnim(CROWBAR_ATTACK2HIT);
		break;
	case 2:
		SendWeaponAnim(CROWBAR_ATTACK3HIT);
		break;
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	float flCharge = m_flCharge;
	ChargeStop(true);
	fDidHit = true;

	// Original bug, kept: no check that the hit entity has an instance (dossier)
	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit)->GetEffectedEnt();

	// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
	if ((pEntity->GetRCCaps(m_pPlayer, this) & RC2_CAP_SHOVE) != 0)
	{
		if (g_pGameRules->PlayerCanAffect(m_pPlayer, pEntity))
		{
			// shove it
			UTIL_MakeVectors(m_pPlayer->pev->v_angle);
			float flScale = RC2Pref(RC2P_Crowbar_PrimaryChargeVelocityFraction).f * flCharge + 1.0;
			pEntity->pev->velocity = pEntity->pev->velocity + gpGlobals->v_forward * RC2Pref(RC2P_Crowbar_PrimaryOtherVelocityScaler).f * flScale;
			pEntity->pev->flags &= ~FL_ONGROUND;
		}
	}
	else if (pEntity->pev->takedamage == DAMAGE_NO || pEntity->IsBSPModel())
	{
		// push the player back off the wall
		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		Vector vecPush = gpGlobals->v_forward * RC2Pref(RC2P_Crowbar_PrimaryHitBSPVelocityScaler).f;
		vecPush = vecPush * (RC2Pref(RC2P_Crowbar_PrimaryChargeVelocityFraction).f * flCharge + 1.0);
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - vecPush;
		m_pPlayer->pev->flags &= ~FL_ONGROUND;
	}
	else
	{
		// push the player back off what won't move
		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		float flScale = RC2Pref(RC2P_Crowbar_PrimaryChargeVelocityFraction).f * flCharge + 1.0;
		Vector vecPush = (gpGlobals->v_forward * RC2Pref(RC2P_Crowbar_PrimaryNoMoveVelocityScaler).f) * flScale;
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - vecPush;
		m_pPlayer->pev->flags &= ~FL_ONGROUND;
	}

	float flDamage = ShieldDeflect(pEntity, (RC2Pref(RC2P_Crowbar_PrimaryChargeDamageFraction).f * flCharge + 1.0) * RC2Pref(RC2P_Crowbar_PrimaryDamage).f,
		RC2Pref(RC2P_Crowbar_PrimaryShieldFraction).f, vecEnd, gpGlobals->v_forward, NULL, NULL, (int)flCharge + 10);
	if (flDamage > 0)
	{
		ClearMultiDamage();
		pEntity->TraceAttack(m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB | (flCharge > 0.5 ? DMG_ALWAYSGIB : 0));
		ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
	}

	// play thwack, smack, or dong sound
	float flVol = 1.0;

	if (pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE && flDamage > 0)
	{
		// play thwack or smack sound
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM);
			break;
		}
		m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
		if (!pEntity->IsAlive())
			return true;

		// Original bug, kept: a body hit doesn't update m_trHit, so Smack decals the last
		// world hit's spot (dossier)
		flVol = 0.1;
	}
	else
	{
		// play texture hit sound
		float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_PLAYER_CROWBAR);

		if (g_pGameRules->IsMultiplayer())
		{
			// override the volume here, cause we don't play texture sounds in multiplayer,
			// and fvolbar is going to be 0 from the above call.
			fvolbar = 1;
		}

		// also play crowbar strike
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		}

		// delay the decal a bit
		m_trHit = tr;
	}

	m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;

	SetThink(&CCrowbar::Smack);
	// Original oddity, kept: the weapon time base (0 with client weapons), not gpGlobals->time,
	// so Smack runs at the next frame
	pev->nextthink = UTIL_WeaponTimeBase() + 0.2;

	return fDidHit;
}

// orig 0x31b60: the client's half of WeaponSync (on the server the reader returns 0)
void CCrowbar::ReadWeaponState(void* pData, int iSize)
{
	m_flCharge = RC2ReadStateFloat(&pData, &iSize);
}

// orig 0x31b80: the charge, every frame
void CCrowbar::WeaponFrame()
{
	float flInterval = gpGlobals->time - m_flLastFrame;
	m_flLastFrame = gpGlobals->time;

	if (m_flCharge < 0)
		m_flCharge = 0;
	else if (m_flCharge > RC2Pref(RC2P_Crowbar_MaxCharge).f)
		m_flCharge = RC2Pref(RC2P_Crowbar_MaxCharge).f;

	if (m_flNextSync < gpGlobals->time)
	{
		// the charge, as the float's bits
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->pev);
		WRITE_BYTE(WEAPON_CROWBAR);
		WRITE_LONG(*reinterpret_cast<int*>(&m_flCharge));
		MESSAGE_END();
		m_flNextSync = gpGlobals->time + 0.1;
	}

	if (g_fRC2PlayWeaponEvents && m_flNextChargeEvent < gpGlobals->time)
	{
		edict_t* pPlayer = m_pPlayer->edict();
		PLAYBACK_EVENT_FULL(FEV_NOTHOST | FEV_UPDATE, pPlayer, m_usCbarCharge, 0, g_vecZero, g_vecZero, m_flCharge, 0, ENTINDEX(pPlayer), 0, 0, 0);
		m_flNextChargeEvent = gpGlobals->time + (m_flCharge > 0 ? 0.1 : 0.25);
	}

	if (m_flCharge > 0 && !m_bCharging)
	{
		// a released charge drains away
		if (m_flDischargeStart < gpGlobals->time)
		{
			m_flCharge -= (RC2Pref(RC2P_Crowbar_MaxCharge).f / RC2Pref(RC2P_Crowbar_ChargeDischargeTime).f) * flInterval;
			if (m_flCharge <= 0)
			{
				m_flCharge = 0.1;
				ChargeStop(true);
			}
			return;
		}
	}

	if (!m_bCharging)
		return;

	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crowbar_QuaternaryRefireQuaternary).f;

	// let go
	if (m_bCharging && (m_pPlayer->pev->button & IN_ALT1) == 0)
	{
		m_bCharging = false;
		m_flDischargeStart = RC2Pref(RC2P_Crowbar_ChargeReleaseStartDischargeTime).f + gpGlobals->time;
		return;
	}

	float flAdd = (RC2Pref(RC2P_Crowbar_MaxCharge).f / RC2Pref(RC2P_Crowbar_ChargeTime).f) * flInterval;

	// out of energy
	if (flAdd * RC2Pref(RC2P_Crowbar_EnergyPerUnitCharge).f > m_pPlayer->m_flEnergy)
	{
		if (g_fRC2PlayWeaponEvents)
		{
			edict_t* pPlayer = m_pPlayer->edict();
			PLAYBACK_EVENT_FULL(FEV_NOTHOST | FEV_UPDATE, pPlayer, m_usCbarCharge, 0, g_vecZero, g_vecZero, m_flCharge, 0, ENTINDEX(pPlayer), 0, 0, 0);
		}
		m_bCharging = false;
		m_flDischargeStart = RC2Pref(RC2P_Crowbar_ChargeEmptyStartDischargeTime).f + gpGlobals->time;
		return;
	}

	float flCharge = flAdd + m_flCharge;
	if (flCharge < 0)
		flCharge = 0;
	else if (flCharge > RC2Pref(RC2P_Crowbar_MaxCharge).f)
		flCharge = RC2Pref(RC2P_Crowbar_MaxCharge).f;
	m_flCharge = flCharge;
	m_flDischargeStart = RC2Pref(RC2P_Crowbar_ChargeReleaseStartDischargeTime).f + gpGlobals->time;

	// full
	if (m_flCharge >= RC2Pref(RC2P_Crowbar_MaxCharge).f)
	{
		m_flDischargeStart = RC2Pref(RC2P_Crowbar_ChargeFullStartDischargeTime).f + gpGlobals->time;
		m_bCharging = false;
	}

	// Original bug, kept: the cost is the charge before this step, not the step (dossier)
	m_pPlayer->m_flEnergy -= (m_flCharge - flAdd) * RC2Pref(RC2P_Crowbar_EnergyPerUnitCharge).f;
	UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_Crowbar_ChargeEnergyRechargeDelay).f);
}

#endif // CLIENT_DLL
