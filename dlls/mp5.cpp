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

// Rocket Crowbar 2: the MP5 fires the bolts of rc2/mp5bolts.cpp, every number from rc2.ini's
// [MP5] and [Ammo]. The rules are server-side; the client build of this file (weapon
// prediction) keeps only what it needs to compile, since RC2's own client.dll does its
// prediction.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "rc2prefs.h"
#ifndef CLIENT_DLL
#else
// The client's prediction (client.dll, reference/dossiers/c1.md) compiles the same code without
// the bolts, the pulse, the fart's damage and push, and the WeaponSync messages. The original's
// one event flag is g_runfuncs there (weapons.h).
#endif

// orig client 0x3d510 and orig client 0x3d540 (and their GetClassPtr<CMP5>, orig client 0x3ee20); the
// inline SecondaryAmmoIndex, iItemSlot and UseDecrement are orig client 0x3d570, orig client 0x3bb90 and
// orig client 0x3bbb0
LINK_ENTITY_TO_CLASS(weapon_mp5, CMP5);	  // orig 0x7be40
LINK_ENTITY_TO_CLASS(weapon_9mmAR, CMP5); // orig 0x7beb0


//=========================================================
//=========================================================
// orig 0x7bf30, orig client 0x3d590
void CMP5::Spawn()
{
	pev->classname = MAKE_STRING("weapon_9mmAR"); // hack to allow for old names
	Precache();
	SET_MODEL(ENT(pev), "models/w_9mmAR.mdl");
	m_iId = WEAPON_MP5;

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_MP5PickupGive).s;
	m_iDefaultAmmo2 = RC2Pref(RC2P_Ammo_MP5PickupGrenadeGive).s;
	m_sBlasterCount = 0;
	m_sBouncyCount = 0;
	m_sPulseCount = 0;
	m_flNextSync = gpGlobals->time;

	FallInit(); // get ready to fall down.
}


// orig 0x7bfc0, orig client 0x3d650
void CMP5::Precache()
{
	PRECACHE_MODEL("models/v_9mmAR.mdl");
	PRECACHE_MODEL("models/w_9mmAR.mdl");
	PRECACHE_MODEL("models/p_9mmAR.mdl");

	m_iShell = PRECACHE_MODEL("models/shell.mdl"); // brass shellTE_MODEL

	PRECACHE_MODEL("models/grenade.mdl"); // grenade

	PRECACHE_MODEL("models/w_9mmARclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_SOUND("weapons/hks1.wav"); // H to the K
	PRECACHE_SOUND("weapons/hks2.wav"); // H to the K
	PRECACHE_SOUND("weapons/hks3.wav"); // H to the K
	PRECACHE_SOUND("weapons/mp5charge.wav");

	PRECACHE_SOUND("weapons/glauncher.wav");
	PRECACHE_SOUND("weapons/glauncher2.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");
	PRECACHE_SOUND("debris/beamstart4.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("ambience/biggun3.wav");

	UTIL_PrecacheOther("blaster_bolt");
	UTIL_PrecacheOther("phase_pulse");
	UTIL_PrecacheOther("bouncy_bolt");

	m_usMP5Primary = PRECACHE_EVENT(1, "events/mp5primary.sc");
	m_usMP5Fart = PRECACHE_EVENT(1, "events/mp5fart.sc");
	m_usMP5Bouncy = PRECACHE_EVENT(1, "events/mp5bouncy.sc");
	m_usMP5Pulse = PRECACHE_EVENT(1, "events/mp5pulse.sc");
	m_usMP5Charge = PRECACHE_EVENT(1, "events/mp5charge.sc");
}

// orig 0x7c130, orig client 0x3d820
bool CMP5::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->pszAmmo2 = "ARgrenades";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarry9mmBullets).s;
	p->iMaxAmmo2 = RC2Pref(RC2P_Ammo_MaxCarryMP5Grenades).s;
	p->iMaxClip = RC2Pref(RC2P_Ammo_MP5ClipSize).s;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_MP5;
	p->iWeight = 20; // Rocket Crowbar 2: MP5_WEIGHT is 15

	return true;
}

// orig 0x7c1b0, orig client 0x3d8c0
void CMP5::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
#ifndef CLIENT_DLL
	// SDK 2.3's (stock in RC2); halflife-updated sent WeapPickup from CBasePlayer::AddPlayerItem
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
#endif
}

// orig 0x7c210, orig client 0x3d940
bool CMP5::Deploy()
{
	m_iPulseCharging = 0;
	m_fInAttack = 0;

#ifndef CLIENT_DLL
	// the bolt counts, for the client's bolts to match the server's
	MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
	WRITE_BYTE(WEAPON_MP5);
	WRITE_SHORT(m_sBlasterCount);
	WRITE_SHORT(m_sBouncyCount);
	WRITE_SHORT(m_sPulseCount);
	MESSAGE_END();
#endif
	m_flNextSync = gpGlobals->time + 0.25;

	return DefaultDeploy("models/v_9mmAR.mdl", "models/p_9mmAR.mdl", MP5_DEPLOY, "mp5");
}

// orig 0x7c2c0, orig client 0x3d980
bool CMP5::CanHolster()
{
	return 0 == m_fInAttack && 0 == m_iPulseCharging;
}

// orig 0x7c2e0, orig client 0x3d9b0
void CMP5::Holster()
{
	m_fInReload = false;
	m_iPulseCharging = 0;
	m_fInAttack = 0;
	CBasePlayerWeapon::Holster();
}

// Every attack timer pushed at least flDelay out (the "can't fire" answer). Name ours.
void CMP5::WeaponTimeout(float flDelay)
{
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flDelay);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flDelay);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flDelay);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flDelay);
}

// The primary and tertiary fire, which differ only in the bolt, its count, event and rc2.ini
// keys: one bolt from the clip, from the gun along the view, with the player's ping as its
// head start. Name ours.
void CMP5::FireBolt(const char* pszBolt, short& sCount, unsigned short usEvent, int iAmmoCost, float flRefire1, float flRefire2, float flRefire3, float flRefire4)
{
	// under water only with the player's attribute for it
	if (m_pPlayer->pev->waterlevel == 3 && !PlayerHasAttrFlag200(m_pPlayer))
	{
		PlayEmptySound();
		WeaponTimeout(0.15);
		return;
	}

	if (m_iClip < iAmmoCost)
	{
		PlayEmptySound();
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] + m_iClip >= iAmmoCost)
		{
			Reload();
			return;
		}
		WeaponTimeout(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_iClip -= iAmmoCost;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector vecSrc = m_pPlayer->GetGunPosition();

	if (0 != g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), usEvent, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, sCount, 0, 0);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flRefire1);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flRefire2);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flRefire3);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flRefire4);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	CBaseEntity* pBolt = CBaseEntity::Create(pszBolt, vecSrc, g_vecZero, m_pPlayer->edict());
	pBolt->pev->movedir = gpGlobals->v_forward;
	pBolt->pev->impulse = sCount;
	pBolt->pev->button = ENTINDEX(m_pPlayer->edict());

	int iPing = 0, iLoss = 0;
	PLAYER_CNX_STATS(m_pPlayer->edict(), &iPing, &iLoss);
	pBolt->pev->armorvalue = iPing * 0.001;
#endif

	if (++sCount > 0x7ffe)
		sCount = 0;

	m_flNextSync = gpGlobals->time + 0.2;
}

// orig 0x7c300, orig client 0x3d9f0
void CMP5::PrimaryAttack()
{
	FireBolt("blaster_bolt", m_sBlasterCount, m_usMP5Primary, (unsigned short)RC2Pref(RC2P_MP5_PrimaryAmmoCost).s,
		RC2Pref(RC2P_MP5_PrimaryRefirePrimary).f, RC2Pref(RC2P_MP5_PrimaryRefireSecondary).f, RC2Pref(RC2P_MP5_PrimaryRefireTertiary).f, RC2Pref(RC2P_MP5_PrimaryRefireQuaternary).f);
}

// orig 0x7c6e0, orig client 0x3dd90
void CMP5::SecondaryAttack()
{
	// the fart: hurts what is behind the player, throws the player forward. Grenade ammo.
	if (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] < (unsigned short)RC2Pref(RC2P_MP5_FartAmmoCost).s || 0 != m_pPlayer->m_iRidingIndex)
	{
		PlayEmptySound();
		WeaponTimeout(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] -= (unsigned short)RC2Pref(RC2P_MP5_FartAmmoCost).s;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_SUPERJUMP);

	if (0 != g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usMP5Fart, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, m_pPlayer->pev->waterlevel >= 2 ? 1 : 0, 0);

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_MP5_SecondaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_MP5_SecondaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_MP5_SecondaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_MP5_SecondaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecDir = gpGlobals->v_forward;
	if (RC2Pref(RC2P_MP5_FartForceHorizontal).b)
	{
		const float flLength = sqrt(vecDir.x * vecDir.x + vecDir.y * vecDir.y);
		if (flLength == 0)
			vecDir = Vector(0, 0, 1);
		else
			vecDir = Vector(vecDir.x * (1 / flLength), vecDir.y * (1 / flLength), 0);
	}

	float flScale;
	if (m_pPlayer->pev->waterlevel >= 2)
	{
		// under water: no damage, a different push
		flScale = RC2Pref(RC2P_MP5_FartWaterSpeedScale).f;
	}
	else
	{
		flScale = 1.0;

		const Vector vecSrc = m_pPlayer->pev->origin;
		const float flMinDot = RC2Pref(RC2P_MP5_FartDamageMinDot).f;
		const float flMaxDist = RC2Pref(RC2P_MP5_FartDamageMaxDist).f;

		UTIL_PushEntityMarks();
		CBaseEntity* pEntity = NULL;
		Vector vecSpot;
		while ((pEntity = UTIL_FindEntityNearestInSphere(pEntity, vecSrc, flMaxDist, NULL, &vecSpot)) != NULL)
		{
			CBaseEntity* pTarget = pEntity->GetEffectedEnt();
			if (!pTarget)
				continue;
			if (UTIL_IsEntityMarked(ENTINDEX(pTarget->edict())))
				continue;
			if (pTarget->edict() == m_pPlayer->edict())
				continue;
			if (pTarget->pev->takedamage != DAMAGE_AIM)
				continue;
			if (!pTarget->IsAlive())
				continue;
			if (!(pTarget->FartDamageInflict() > 0))
				continue;

			// only what is behind the player (the direction from it to the player is the aim)
			Vector vecToPlayer = vecSrc - vecSpot;
			const float flLength = vecToPlayer.Length();
			const Vector vecToPlayerDir = flLength == 0 ? Vector(0, 0, 1) : vecToPlayer * (1 / flLength);
			if (DotProduct(vecToPlayerDir, vecDir) < flMinDot)
				continue;

			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecSpot, ignore_monsters, m_pPlayer->edict(), &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
				continue;

			pTarget->TakeDamage(UTIL_RC2DamageInflictor("mp5_fart"), m_pPlayer->pev, pTarget->FartDamageInflict(), DMG_NERVEGAS | DMG_NEVERGIB);
			UTIL_MarkEntity(ENTINDEX(pTarget->edict()));
		}
		UTIL_PopEntityMarks();

		// an event for HLTV (the old SVC_HLTV director message, type 50)
		MESSAGE_BEGIN(MSG_SPEC, 50);
		WRITE_BYTE(3); // DRC_EVENT
		WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
		WRITE_SHORT(0);
		WRITE_LONG(1);
		MESSAGE_END();
	}

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + (vecDir * RC2Pref(RC2P_MP5_FartSpeed).f + Vector(0, 0, RC2Pref(RC2P_MP5_FartZBoost).f)) * flScale;
#endif
}

// orig 0x7cdc0, orig client 0x3e0b0
void CMP5::TertiaryAttack()
{
	FireBolt("bouncy_bolt", m_sBouncyCount, m_usMP5Bouncy, (unsigned short)RC2Pref(RC2P_MP5_TertiaryAmmoCost).s,
		RC2Pref(RC2P_MP5_TertiaryRefirePrimary).f, RC2Pref(RC2P_MP5_TertiaryRefireSecondary).f, RC2Pref(RC2P_MP5_TertiaryRefireTertiary).f, RC2Pref(RC2P_MP5_TertiaryRefireQuaternary).f);
}

// orig 0x7d1a0, orig client 0x3e450
void CMP5::QuaternaryAttack()
{
	// start charging a phase pulse: grenade ammo now, clip ammo while it charges (WeaponFrame)
	if (m_pPlayer->pev->waterlevel == 3 && !PlayerHasAttrFlag200(m_pPlayer))
	{
		PlayEmptySound();
		WeaponTimeout(0.15);
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] < (unsigned short)RC2Pref(RC2P_MP5_PulseSecondaryAmmoCost).s)
	{
		PlayEmptySound();
		WeaponTimeout(0.15);
		return;
	}

	const int iCost = (unsigned short)RC2Pref(RC2P_MP5_PulsePrimaryAmmoCost).s;
	if (m_iClip < iCost)
	{
		PlayEmptySound();
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] + m_iClip >= iCost)
		{
			Reload();
			return;
		}
		WeaponTimeout(0.15);
		return;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
	m_iPulseCharging = 1;
	m_fInAttack = 1;
	m_flPulseReady = gpGlobals->time + RC2Pref(RC2P_MP5_PulseChargeTime).f;
	m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] -= (unsigned short)RC2Pref(RC2P_MP5_PulseSecondaryAmmoCost).s;

	if (0 != g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usMP5Charge, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0);
}

// orig 0x7d420, orig client 0x3e790
void CMP5::FirePulse()
{
	if (m_pPlayer->pev->waterlevel == 3 && !PlayerHasAttrFlag200(m_pPlayer))
	{
		// went under water while charging: the charge is lost
		PlayEmptySound();
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.3;
		m_flNextTertiaryAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		if (0 != g_fRC2PlayWeaponEvents)
		{
			m_iPulseCharging = 0;
			m_fInAttack = 0;
		}
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector vecSrc = m_pPlayer->GetGunPosition();

	if (0 != g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usMP5Pulse, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, m_sPulseCount, 0, 0);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_MP5_QuaternaryRefirePrimary).f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_MP5_QuaternaryRefireSecondary).f;
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_MP5_QuaternaryRefireTertiary).f;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_MP5_QuaternaryRefireQuaternary).f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	CBaseEntity* pPulse = CBaseEntity::Create("phase_pulse", vecSrc + gpGlobals->v_forward * 12, g_vecZero, m_pPlayer->edict());
	pPulse->pev->movedir = gpGlobals->v_forward;
	pPulse->pev->impulse = m_sPulseCount;
	pPulse->pev->button = ENTINDEX(m_pPlayer->edict());

	int iPing = 0, iLoss = 0;
	PLAYER_CNX_STATS(m_pPlayer->edict(), &iPing, &iLoss);
	pPulse->pev->armorvalue = iPing * 0.001;
#endif

	if (0 != g_fRC2PlayWeaponEvents)
	{
		m_sPulseCount++;
		m_fInAttack = 0;
		m_iPulseCharging = 0;
		if (m_sPulseCount > 0x7ffe)
			m_sPulseCount = 0;
	}
}

// orig 0x7d7b0, orig client 0x3ea50
void CMP5::Reload()
{
	if (0 != m_fInAttack) // Rocket Crowbar 2: not while charging
		return;

	if (m_pPlayer->ammo_9mm <= 0)
		return;

	DefaultReload(RC2Pref(RC2P_Ammo_MP5ClipSize).s, MP5_RELOAD, RC2Pref(RC2P_MP5_ReloadTime).f);
}


// orig 0x7d7e0, orig client 0x3eaa0
void CMP5::WeaponIdle()
{
	if (0 != m_fInAttack) // Rocket Crowbar 2: not while charging
		return;

	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		iAnim = MP5_LONGIDLE;
		break;

	default:
	case 1:
		iAnim = MP5_IDLE1;
		break;
	}

	SendWeaponAnim(iAnim);

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15); // how long till we do this again.
}

// orig 0x7d870, orig client 0x3eb60
void CMP5::WeaponFrame()
{
#ifndef CLIENT_DLL
	// the bolt counts to the client every half second
	if (m_flNextSync < gpGlobals->time)
	{
		m_flNextSync = gpGlobals->time + 0.5;
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_MP5);
		WRITE_SHORT(m_sBlasterCount);
		WRITE_SHORT(m_sBouncyCount);
		WRITE_SHORT(m_sPulseCount);
		MESSAGE_END();
	}
#endif

	if (0 == m_iPulseCharging)
		return;

	if (m_fInAttack == 1)
	{
		// the charge draws [MP5] PulsePrimaryAmmoCost from the clip over PulseChargeTime
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_MP5_PulseChargeTime).f / (unsigned short)RC2Pref(RC2P_MP5_PulsePrimaryAmmoCost).s;
			if (--m_iClip < 0)
				m_iClip = 0;
		}
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 2.0;
	}

	if (m_flPulseReady < gpGlobals->time)
	{
		FirePulse();
		return;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
	// no UTIL_WeaponTimeBase() here (kept)
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

// orig 0x7da00, orig client 0x3ecc0
void CMP5::ReadWeaponState(void* pData, int iSize)
{
	// the server's readers answer 0, so on the server this zeroes the counts
	m_sBlasterCount = RC2ReadStateShort(&pData, &iSize);
	m_sBouncyCount = RC2ReadStateShort(&pData, &iSize);
	m_sPulseCount = RC2ReadStateShort(&pData, &iSize);
}


class CMP5AmmoClip : public CBasePlayerAmmo
{
	// orig 0x7dac0, orig client 0x3ef20
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_9mmARclip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x7daf0, orig client 0x3ef60
	void Precache() override
	{
		PRECACHE_MODEL("models/w_9mmARclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x7db10, orig client 0x3ef90
	bool AddAmmo(CBaseEntity* pOther) override
	{
		// Rocket Crowbar 2: the amounts from rc2.ini (GiveAmmo takes a fourth argument there, 0 here)
		bool bResult = (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_MP5SpareClipGive).s, "9mm", RC2Pref(RC2P_Ammo_MaxCarry9mmBullets).s) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
// orig client 0x3ed30 and orig client 0x3ed60 (GetClassPtr<CMP5AmmoClip>, orig client 0x3eea0)
LINK_ENTITY_TO_CLASS(ammo_mp5clip, CMP5AmmoClip); // orig 0x7da50
LINK_ENTITY_TO_CLASS(ammo_9mmAR, CMP5AmmoClip);	  // orig 0x7db70



class CMP5Chainammo : public CBasePlayerAmmo
{
	// orig 0x7dc50, orig client 0x3f090
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x7dc80, orig client 0x3f0d0
	void Precache() override
	{
		PRECACHE_MODEL("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x7dca0, orig client 0x3f100
	bool AddAmmo(CBaseEntity* pOther) override
	{
		// Rocket Crowbar 2: the amounts from rc2.ini (GiveAmmo takes a fourth argument there, 0 here)
		bool bResult = (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_Chainbox9mmGive).s, "9mm", RC2Pref(RC2P_Ammo_MaxCarry9mmBullets).s) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
// orig client 0x3ed90 (GetClassPtr<CMP5Chainammo>, orig client 0x3f010)
LINK_ENTITY_TO_CLASS(ammo_9mmbox, CMP5Chainammo); // orig 0x7dbe0


class CMP5AmmoGrenade : public CBasePlayerAmmo
{
	// orig 0x7dd70, orig client 0x3f200
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_ARgrenade.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x7dda0, orig client 0x3f240
	void Precache() override
	{
		PRECACHE_MODEL("models/w_ARgrenade.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x7ddc0, orig client 0x3f270
	bool AddAmmo(CBaseEntity* pOther) override
	{
		// Rocket Crowbar 2: the amounts from rc2.ini (GiveAmmo takes a fourth argument there, 0 here)
		bool bResult = (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_MP5LooseGrenadeGive).s, "ARgrenades", RC2Pref(RC2P_Ammo_MaxCarryMP5Grenades).s) != -1);

		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
// orig client 0x3edc0 and orig client 0x3edf0 (GetClassPtr<CMP5AmmoGrenade>, orig client 0x3f180)
LINK_ENTITY_TO_CLASS(ammo_mp5grenades, CMP5AmmoGrenade); // orig 0x7dd00
LINK_ENTITY_TO_CLASS(ammo_ARgrenades, CMP5AmmoGrenade);	 // orig 0x7de20
