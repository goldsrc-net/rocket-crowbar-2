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

// Rocket Crowbar 2: the hornet gun no longer fires hornets. It is RC2's freeze gun: a hitscan
// freeze shot, a charged freeze shot, the freeze grenade (rc2/freezegrenade.cpp) and a zoom,
// every number from rc2.ini's [HornetGun]. The rules are server-side; the client build of this
// file (weapon prediction) keeps only what it needs to compile, since RC2's own client.dll does
// its prediction.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "hornet.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "rc2prefs.h"
#ifndef CLIENT_DLL
#include "p4_needs.h"
#include "freezegrenade.h"
#else
// The client's prediction (client.dll, reference/dossiers/c1.md) compiles the same code without
// the freeze shots' damage and the grenade. The original's one event flag is g_runfuncs there.
#define g_iRC2WeaponFrame g_runfuncs
extern bool g_runfuncs; // cl_dll/com_weapons.cpp
#endif


// orig client 0x3c050 (and its GetClassPtr<CHgun>, orig client 0x3d330); the inline iItemSlot and
// UseDecrement are orig client 0x3bc70 and orig client 0x3bc90
LINK_ENTITY_TO_CLASS(weapon_hornetgun, CHgun); // orig 0x64fb0

// orig 0x65020, orig client 0x3c080
bool CHgun::IsUseable()
{
	return true;
}

// orig 0x65030, orig client 0x3c0a0
void CHgun::Spawn()
{
	Precache();
	m_iId = WEAPON_HORNETGUN;
	SET_MODEL(ENT(pev), "models/w_hgun.mdl");

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_HornetGunPickupGive).s;

	FallInit(); // get ready to fall down.
}


// orig 0x65070, orig client 0x3c110
void CHgun::Precache()
{
	PRECACHE_MODEL("models/v_hgun.mdl");
	PRECACHE_MODEL("models/w_hgun.mdl");
	PRECACHE_MODEL("models/p_hgun.mdl");

	PRECACHE_SOUND("agrunt/ag_fire1.wav");
	PRECACHE_SOUND("agrunt/ag_fire2.wav");
	PRECACHE_SOUND("agrunt/ag_fire3.wav");
	PRECACHE_SOUND("ambience/alien_purrmachine.wav");
	PRECACHE_MODEL("sprites/freezef.spr");

	m_usHornetFire = PRECACHE_EVENT(1, "events/hgunfire.sc");
	m_usHgunSetCharge = PRECACHE_EVENT(1, "events/hgunsetcharge.sc");

	UTIL_PrecacheOther("freeze_grenade");
	UTIL_PrecacheOther("freeze_timer");
}

// orig 0x65110, orig client 0x3c1e0
void CHgun::AddToPlayer(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL
	if (g_pGameRules->IsMultiplayer())
	{
		// in multiplayer, all hivehands come full.
		m_iDefaultAmmo = RC2Pref(RC2P_Ammo_MaxCarryHornets).s;
	}
#endif

	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

// orig 0x65190, orig client 0x3c260
bool CHgun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hornets";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarryHornets).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_HORNETGUN;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = HORNETGUN_WEIGHT;

	return true;
}


// orig 0x65200, orig client 0x3c2f0
bool CHgun::Deploy()
{
	m_fInAttack = 0;
	m_fCharging = false;
	m_flChargeStart = gpGlobals->time;
	m_pPlayer->m_iFOV = 0;
	m_pPlayer->pev->fov = 0;

	// clear the client's charge display
	PLAYBACK_EVENT_FULL(FEV_GLOBAL, m_pPlayer->edict(), m_usHgunSetCharge, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, ENTINDEX(m_pPlayer->edict()), 0, 1, 0);

	return DefaultDeploy("models/v_hgun.mdl", "models/p_hgun.mdl", HGUN_UP, "hive");
}

// orig 0x652a0, orig client 0x3c3d0
void CHgun::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(HGUN_DOWN);

	//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
	if (0 == m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()])
	{
		m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] = 1;
	}

	// Rocket Crowbar 2: unzoom, drop any charge
	m_pPlayer->m_iFOV = 0;
	m_pPlayer->pev->fov = 0;
	m_fInAttack = 0;
	m_fCharging = false;
	m_flChargeStart = gpGlobals->time;

	PLAYBACK_EVENT_FULL(FEV_GLOBAL | FEV_RELIABLE, m_pPlayer->edict(), m_usHgunSetCharge, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, ENTINDEX(m_pPlayer->edict()), 0, 1, 0);
}


// orig 0x65380, orig client 0x3c510
void CHgun::PrimaryAttack()
{
	// the hitscan freeze shot
	if (m_fCharging || 0 != m_fInAttack)
		return;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < (unsigned short)RC2Pref(RC2P_HornetGun_PrimaryAmmoCost).s)
		return;

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	if (0 != g_iRC2WeaponFrame)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usHornetFire, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0);

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_PrimaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_PrimaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_PrimaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_PrimaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	UTIL_ExtendTime(&m_flRechargeTime, gpGlobals->time, RC2Pref(RC2P_HornetGun_PrimaryRechargeDelay).f);

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= (unsigned short)RC2Pref(RC2P_HornetGun_PrimaryAmmoCost).s;

#ifndef CLIENT_DLL
	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.flFraction < 1.0 && 0 != UTIL_IsValidEdict(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && g_pGameRules->PlayerCanAffect(m_pPlayer, pHit))
		{
			FreezeDamage(pHit, m_pPlayer->pev, UTIL_NamedInflictor("freeze"),
				RC2Pref(RC2P_HornetGun_PrimaryDamage).f, RC2Pref(RC2P_HornetGun_PrimaryFreezeDamage).f, RC2Pref(RC2P_HornetGun_PrimaryFreezeTime).f,
				&tr, gpGlobals->v_forward, true);
		}
	}
#endif
}

// orig 0x656b0, orig client 0x3c710
void CHgun::SecondaryAttack()
{
	// start charging; WeaponFrame draws more ammo into it and lets it go
	if (m_fCharging || 0 != m_fInAttack)
		return;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return;

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_iChargeAmmo = 1;
	m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_HornetGun_SecondaryChargeTime).f / (unsigned short)RC2Pref(RC2P_HornetGun_SecondaryMaxAmmoCost).s;
	m_flChargeStart = gpGlobals->time;
	m_flChargeReady = gpGlobals->time + RC2Pref(RC2P_HornetGun_SecondaryMinChargeTime).f;
	m_fInAttack = 1;
	m_fCharging = true;

	if (0 != g_iRC2WeaponFrame)
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usHgunSetCharge, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0.01, 0, ENTINDEX(m_pPlayer->edict()), 0, 0, 1);
}

// orig 0x657d0, orig client 0x3c8a0
void CHgun::TertiaryAttack()
{
	// the freeze grenade
	if (m_fCharging || 0 != m_fInAttack)
		return;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < (unsigned short)RC2Pref(RC2P_HornetGun_GrenadeAmmoCost).s)
		return;

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= (unsigned short)RC2Pref(RC2P_HornetGun_GrenadeAmmoCost).s;

	if (0 != g_iRC2WeaponFrame)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usHornetFire, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 2, 0);

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_TertiaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_TertiaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_TertiaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_TertiaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	UTIL_ExtendTime(&m_flRechargeTime, gpGlobals->time, RC2Pref(RC2P_HornetGun_GrenadeRechargeDelay).f);

#ifndef CLIENT_DLL
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAngles = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(vecAngles);
	vecAngles.x = -vecAngles.x;

	CBaseEntity* pGrenade = CBaseEntity::Create("freeze_grenade", vecSrc, vecAngles, m_pPlayer->edict());
	pGrenade->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_HornetGun_GrenadeLaunchVel).f;
	pGrenade->pev->velocity.z += RC2Pref(RC2P_HornetGun_GrenadeLaunchZBoost).f;

	// thrown from what the player is linked to, it belongs to that entity
	if (0 != m_pPlayer->m_iRidingIndex)
	{
		edict_t* pent = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (0 != UTIL_IsValidEdict(pent))
			pGrenade->pev->owner = pent;
	}
#endif
}

// orig 0x65aa0, orig client 0x3caa0
void CHgun::QuaternaryAttack()
{
	// zoom toggle
	if (m_fCharging || 0 != m_fInAttack)
		return;

	if (m_pPlayer->pev->fov != 0)
	{
		m_pPlayer->m_iFOV = 0;
		m_pPlayer->pev->fov = 0;
	}
	else
	{
		m_pPlayer->m_iFOV = (int)RC2Pref(RC2P_HornetGun_ZoomedFOV).f;
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV;
	}

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_QuaternaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_QuaternaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_QuaternaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_QuaternaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

// orig 0x65bd0, orig client 0x3cc20
void CHgun::ReleaseCharge(bool bBackfire)
{
	// the charged shot, or with bBackfire the overcharge going off on the player
	if (0 != g_iRC2WeaponFrame)
	{
		m_fInAttack = 0;
		m_fCharging = false;
	}

	float flCharge = (gpGlobals->time - m_flChargeStart) / RC2Pref(RC2P_HornetGun_SecondaryChargeTime).f;
	if (flCharge < 0.0)
		flCharge = 0.0;
	else if (flCharge > 1.0)
		flCharge = 1.0;

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	if (0 != g_iRC2WeaponFrame)
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usHornetFire, 0, (float*)&g_vecZero, (float*)&g_vecZero, flCharge + 1.0, 0, 0, 0, bBackfire ? 1 : 0, 0);
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usHgunSetCharge, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, ENTINDEX(m_pPlayer->edict()), 0, 1, 0);
		PLAYBACK_EVENT_FULL(FEV_GLOBAL | FEV_RELIABLE, m_pPlayer->edict(), m_usHgunSetCharge, 0.01, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, ENTINDEX(m_pPlayer->edict()), 0, 1, 0);
	}

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_SecondaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_SecondaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_SecondaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_HornetGun_SecondaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	UTIL_ExtendTime(&m_flRechargeTime, gpGlobals->time, RC2Pref(RC2P_HornetGun_SecondaryRechargeDelay).f);

#ifndef CLIENT_DLL
	Vector vecSrc = m_pPlayer->GetGunPosition();

	float flDamage = RC2Pref(RC2P_HornetGun_SecondaryAddDamage).f * flCharge + RC2Pref(RC2P_HornetGun_SecondaryBaseDamage).f;
	float flFreezeDamage = RC2Pref(RC2P_HornetGun_SecondaryAddFreezeDamage).f * flCharge + RC2Pref(RC2P_HornetGun_SecondaryBaseFreezeDamage).f;
	float flFreezeTime = RC2Pref(RC2P_HornetGun_SecondaryAddFreezeTime).f * flCharge + RC2Pref(RC2P_HornetGun_SecondaryBaseFreezeTime).f;

	if (!bBackfire)
	{
		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (tr.flFraction < 1.0 && 0 != UTIL_IsValidEdict(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit && g_pGameRules->PlayerCanAffect(m_pPlayer, pHit))
				FreezeDamage(pHit, m_pPlayer->pev, UTIL_NamedInflictor("freeze"), flDamage, flFreezeDamage, flFreezeTime, &tr, gpGlobals->v_forward, true);
		}
	}
	else
	{
		// the player takes [HornetGun] SecondaryBackfireScale of it, past any shield
		const float flScale = RC2Pref(RC2P_HornetGun_SecondaryBackfireScale).f;
		FreezeDamage(m_pPlayer, m_pPlayer->pev, UTIL_NamedInflictor("freeze"), flScale * flDamage, flScale * flFreezeDamage, flScale * flFreezeTime, NULL, g_vecZero, false);
	}
#endif
}

// orig 0x66080, orig client 0x3cf40
void CHgun::Recharge()
{
	// what Reload did in the SDK: an ammo point per [HornetGun] SharedRechargeGiveDelay
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= RC2Pref(RC2P_Ammo_MaxCarryHornets).s)
		return;

	while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2Pref(RC2P_Ammo_MaxCarryHornets).s && m_flRechargeTime < gpGlobals->time)
	{
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
		m_flRechargeTime = RC2Pref(RC2P_HornetGun_SharedRechargeGiveDelay).f + m_flRechargeTime;
	}

#ifndef CLIENT_DLL
	m_pPlayer->TabulateAmmo();
#endif
}


// orig 0x66100, orig client 0x3cfe0
void CHgun::WeaponIdle()
{
	// Rocket Crowbar 2: no Reload() here; WeaponFrame recharges
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
	if (flRand <= 0.75)
	{
		iAnim = HGUN_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = HGUN_FIDGETSWAY;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
	}
	else
	{
		iAnim = HGUN_FIDGETSHAKE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 35.0 / 16.0;
	}
	SendWeaponAnim(iAnim);
}

// orig 0x661a0, orig client 0x3d0b0
void CHgun::WeaponFrame()
{
	if (!m_pPlayer->m_bFrozen && !m_fCharging && 0 == m_fInAttack)
		Recharge();

	if (!m_fCharging)
		return;

	// let go when the button is up and the minimum charge is in
	if ((m_pPlayer->pev->button & IN_ATTACK2) == 0 && m_flChargeReady < gpGlobals->time)
	{
		ReleaseCharge(false);
		return;
	}

	// held too long: it goes off on the player
	if (gpGlobals->time - m_flChargeStart > RC2Pref(RC2P_HornetGun_SecondaryOverChargeTime).f)
	{
		ReleaseCharge(true);
		return;
	}

	// draw another ammo point into the charge, up to [HornetGun] SecondaryMaxAmmoCost
	if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn && m_iChargeAmmo < (unsigned short)RC2Pref(RC2P_HornetGun_SecondaryMaxAmmoCost).s)
	{
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		{
			ReleaseCharge(false);
			return;
		}

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
		m_iChargeAmmo++;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_HornetGun_SecondaryChargeTime).f / (unsigned short)RC2Pref(RC2P_HornetGun_SecondaryMaxAmmoCost).s;
	}

	double flCharge = (gpGlobals->time - m_flChargeStart) / RC2Pref(RC2P_HornetGun_SecondaryChargeTime).f;
	if (flCharge < 0.0)
		flCharge = 0.0;
	else if (flCharge > 1.0)
		flCharge = 1.0;

	if (0 != g_iRC2WeaponFrame)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usHgunSetCharge, 0, (float*)&g_vecZero, (float*)&g_vecZero, (float)flCharge, 0, ENTINDEX(m_pPlayer->edict()), 0, 0, 0);
}
