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
*	Rocket Crowbar 2: the hand grenade throws RC2's grenades, one kind per attack: primary the
*	antifriction_grenade, secondary the suck_grenade, tertiary the gravity_grenade (at most
*	[Grenade] SuckGrenMaxDeployed / GravGrenMaxDeployed of the last two out at once), each
*	costing its [Grenade] ...AmmoCost. The attack only pulls the pin; WeaponFrame throws when the
*	button is let go or after SharedMaxTimer, and tells the client how many are out
*	(WeaponSync). The quaternary attack is empty, so the ultima_grenade it precaches is never
*	thrown.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "rc2prefs.h"
#ifndef CLIENT_DLL
#include "UserMessages.h"
#else
// The client's prediction (client.dll, reference/dossiers/c1.md) compiles the same code: it pulls
// pins, throws (animations, timers, ammo) and counts nothing itself: WeaponSync tells it.
#endif


#define HANDGRENADE_PRIMARY_VOLUME 450

// orig client 0x35190 (and its GetClassPtr<CHandGrenade>, orig client 0x36200); the inline
// iItemSlot and UseDecrement are orig client 0x36280 and orig client 0x362a0, the empty
// QuaternaryAttack orig client 0x35770
LINK_ENTITY_TO_CLASS(weapon_handgrenade, CHandGrenade); // orig 0x5bdd0


// orig 0x5be60, orig client 0x351c0
void CHandGrenade::Spawn()
{
	Precache();
	m_iId = WEAPON_HANDGRENADE;
	SET_MODEL(ENT(pev), "models/w_grenade.mdl");

#ifndef CLIENT_DLL
	pev->dmg = gSkillData.plrDmgHandGrenade;
#endif
	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_GrenadePickupGive).s;

	m_flNextCountTime = gpGlobals->time;
	m_iGravDeployed = 0;
	m_iSuckDeployed = 0;

	FallInit(); // get ready to fall down.
}


// orig 0x5bed0, orig client 0x35240
void CHandGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("models/v_grenade.mdl");
	PRECACHE_MODEL("models/p_grenade.mdl");
	UTIL_PrecacheOther("gravity_grenade");
	UTIL_PrecacheOther("antifriction_grenade");
	UTIL_PrecacheOther("suck_grenade");
	UTIL_PrecacheOther("ultima_grenade");
}

// orig 0x5bf20, orig client 0x352b0
bool CHandGrenade::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hand Grenade";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarryGrenades).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_HANDGRENADE;
	p->iWeight = HANDGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return true;
}


// orig 0x5bf90, orig client 0x35340
bool CHandGrenade::Deploy()
{
	m_flNextCountTime = gpGlobals->time;
	m_iGravDeployed = 0;
	m_iSuckDeployed = 0;
	m_flReleaseThrow = -1;
	return DefaultDeploy("models/v_grenade.mdl", "models/p_grenade.mdl", HANDGRENADE_DRAW, "crowbar");
}

// orig client 0x35390
bool CHandGrenade::CanHolster()
{
	// can only holster hand grenades when not primed!
	return (m_flStartThrow == 0);
}

// orig 0x5bff0, orig client 0x353c0. RC2's doesn't clear m_flStartThrow (halflife-updated's does).
void CHandGrenade::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		SendWeaponAnim(HANDGRENADE_HOLSTER);
	}
	else
	{
		// no more grenades!
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CHandGrenade::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

// The three attacks pull the pin of grenade kind m_fInAttack (1 antifriction, 2 suck, 3 gravity).
// orig 0x5c0a0, orig client 0x35530
void CHandGrenade::PrimaryAttack()
{
	if (0 == m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= (unsigned short)RC2Pref(RC2P_Grenade_AFGrenAmmoCost).s)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;
		m_fInAttack = 1;
		SendWeaponAnim(HANDGRENADE_PINPULL);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}

// orig 0x5c120, orig client 0x355d0
void CHandGrenade::SecondaryAttack()
{
	if (0 == m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= (unsigned short)RC2Pref(RC2P_Grenade_SuckGrenAmmoCost).s && m_iSuckDeployed < (unsigned char)RC2Pref(RC2P_Grenade_SuckGrenMaxDeployed).c)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;
		m_fInAttack = 2;
		SendWeaponAnim(HANDGRENADE_PINPULL);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}

// orig 0x5c1b0, orig client 0x356a0
void CHandGrenade::TertiaryAttack()
{
	if (0 == m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= (unsigned short)RC2Pref(RC2P_Grenade_GravGrenAmmoCost).s && m_iGravDeployed < (unsigned char)RC2Pref(RC2P_Grenade_GravGrenMaxDeployed).c)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;
		m_fInAttack = 3;
		SendWeaponAnim(HANDGRENADE_PINPULL);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}


// orig 0x5c250, orig client 0x35790: only the idle and fidget animations; the throw is WeaponFrame's
void CHandGrenade::WeaponIdle()
{
	if (0 != m_flStartThrow)
		return;
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;
	if (m_flReleaseThrow > 0)
		return;

	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
		if (flRand <= 0.75)
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15); // how long till we do this again.
		}
		else
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;
		}

		SendWeaponAnim(iAnim);
	}
}

// orig 0x5c330, orig client 0x358b0 (0 on the client): how many of the player's pszClassname grenades are out (they keep the thrower
// in pev->dmg_inflictor)
int CHandGrenade::CountThrown(const char* pszClassname)
{
	int iCount = 0;
#ifndef CLIENT_DLL
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityByClassname(pEntity, pszClassname)) != NULL)
	{
		if (pEntity->pev && UTIL_IsValidEntity(pEntity->edict()) && pEntity->pev->dmg_inflictor == m_pPlayer->edict())
			iCount++;
	}
#endif
	return iCount;
}

// orig 0x5c3a0, orig client 0x358d0
void CHandGrenade::ReadWeaponState(void* pData, int iSize)
{
	m_iGravDeployed = RC2ReadStateByte(&pData, &iSize);
	m_iSuckDeployed = RC2ReadStateByte(&pData, &iSize);
}

// The attack timers each throw (or abort) pushes forward.
static void RaiseAttackTimers(CHandGrenade* pWeapon, float flPrimary, float flSecondary, float flTertiary)
{
	UTIL_ExtendTime(&pWeapon->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flPrimary);
	UTIL_ExtendTime(&pWeapon->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flSecondary);
	UTIL_ExtendTime(&pWeapon->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flTertiary);
}

// orig 0x5c3e0, orig client 0x35920 (the client's counts nothing and creates nothing): count what is out twice a second, and throw the primed grenade once its button
// is up or SharedMaxTimer has passed
void CHandGrenade::WeaponFrame()
{
#ifndef CLIENT_DLL
	if (m_flNextCountTime < gpGlobals->time)
	{
		m_flNextCountTime = gpGlobals->time + 0.5;
		m_iGravDeployed = CountThrown("gravity_grenade");
		m_iSuckDeployed = CountThrown("suck_grenade");

		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_HANDGRENADE);
		WRITE_BYTE(V_max(0, V_min(m_iGravDeployed, 255)));
		WRITE_BYTE(V_max(0, V_min(m_iSuckDeployed, 255)));
		MESSAGE_END();
	}
#endif

	if (m_flReleaseThrow == 0 && m_flStartThrow != 0)
		m_flReleaseThrow = gpGlobals->time;

	if (m_flStartThrow != 0)
	{
		// still holding the button that pulled the pin
		if (gpGlobals->time - m_flStartThrow < RC2Pref(RC2P_Grenade_SharedMaxTimer).f)
		{
			const int iButtons = m_pPlayer->pev->button;
			if ((m_fInAttack == 1 && (iButtons & IN_ATTACK) != 0) ||
				(m_fInAttack == 2 && (iButtons & IN_ATTACK2) != 0) ||
				(m_fInAttack == 3 && (iButtons & IN_CANCEL) != 0) ||
				(m_fInAttack == 4 && (iButtons & IN_ALT1) != 0))
				return;
		}

		// not enough ammo left for it (another weapon may have used it): put the pin back
		const int iAmmo = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
		if ((m_fInAttack == 1 && iAmmo < (unsigned short)RC2Pref(RC2P_Grenade_AFGrenAmmoCost).s) ||
			(m_fInAttack == 2 && iAmmo < (unsigned short)RC2Pref(RC2P_Grenade_SuckGrenAmmoCost).s))
		{
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
			m_flReleaseThrow = -1;
			m_flStartThrow = 0;
			RaiseAttackTimers(this, 0.5, 0.5, 0.5);
			SendWeaponAnim(HANDGRENADE_DRAW);
			return;
		}
		if (m_fInAttack == 3 && iAmmo < (unsigned short)RC2Pref(RC2P_Grenade_GravGrenAmmoCost).s)
		{
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
			m_flReleaseThrow = -1;
			m_flStartThrow = 0;
			RaiseAttackTimers(this, 0.5, 0.5, 0.5);
			UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.5);
			SendWeaponAnim(HANDGRENADE_DRAW);
			return;
		}

		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if (angThrow.x < 0)
			angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
		else
			angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

		float flVel = (90 - angThrow.x) * RC2Pref(RC2P_Grenade_SharedVelAngScaler).f;
		if (flVel > RC2Pref(RC2P_Grenade_SharedMaxVel).f)
			flVel = RC2Pref(RC2P_Grenade_SharedMaxVel).f;

		UTIL_MakeVectors(angThrow);

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		float time = m_flStartThrow - gpGlobals->time + RC2Pref(RC2P_Grenade_SharedMaxTimer).f;
		if (time < 0)
			time = 0;

		// VERIFY: any other m_fInAttack names the original's empty string (a zeroed .bss buffer at
		// orig 0x18c080), so Create makes nothing
		const char* pszClassname = "";
		switch (m_fInAttack)
		{
		case 1:
			pszClassname = "antifriction_grenade";
			break;
		case 2:
			pszClassname = "suck_grenade";
			break;
		case 3:
			pszClassname = "gravity_grenade";
			break;
		}

#ifndef CLIENT_DLL
		CBaseEntity* pGrenade = CBaseEntity::Create(pszClassname, vecSrc, angThrow, m_pPlayer->edict());
		if (pGrenade)
		{
			if (time > 0)
			{
				pGrenade->pev->velocity = vecThrow;
				pGrenade->pev->dmgtime = time + gpGlobals->time;
			}
			else
			{
				pGrenade->pev->velocity = g_vecZero;
				pGrenade->pev->dmgtime = gpGlobals->time;
			}

			// with a linked player (m_iRidingIndex, orig +0x69c) the grenade is owned by that
			// one (dmg_inflictor still names the thrower: Spawn copied it from pev->owner)
			if (m_pPlayer->m_iRidingIndex != 0)
			{
				edict_t* pentLinked = INDEXENT(m_pPlayer->m_iRidingIndex);
				if (UTIL_IsValidEntity(pentLinked))
					pGrenade->pev->owner = pentLinked;
			}
		}
#endif

		if (flVel < 500)
			SendWeaponAnim(HANDGRENADE_THROW1);
		else if (flVel < 1000)
			SendWeaponAnim(HANDGRENADE_THROW2);
		else
			SendWeaponAnim(HANDGRENADE_THROW3);

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		m_flReleaseThrow = 0;
		m_flStartThrow = 0;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		m_flNextCountTime = gpGlobals->time;

		// The refire keys don't follow the attack: the antifriction (primary) grenade uses the
		// [Grenade] Secondary* keys, the suck (secondary) the Tertiary* keys, the gravity
		// (tertiary) the Primary* keys and is the only one to push the quaternary timer.
		switch (m_fInAttack)
		{
		case 1:
			RaiseAttackTimers(this, RC2Pref(RC2P_Grenade_SecondaryRefirePrimary).f, RC2Pref(RC2P_Grenade_SecondaryRefireSecondary).f, RC2Pref(RC2P_Grenade_SecondaryRefireTertiary).f);
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= (unsigned short)RC2Pref(RC2P_Grenade_AFGrenAmmoCost).s;
			break;
		case 2:
			RaiseAttackTimers(this, RC2Pref(RC2P_Grenade_TertiaryRefirePrimary).f, RC2Pref(RC2P_Grenade_TertiaryRefireSecondary).f, RC2Pref(RC2P_Grenade_TertiaryRefireTertiary).f);
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= (unsigned short)RC2Pref(RC2P_Grenade_SuckGrenAmmoCost).s;
			break;
		case 3:
			RaiseAttackTimers(this, RC2Pref(RC2P_Grenade_PrimaryRefirePrimary).f, RC2Pref(RC2P_Grenade_PrimaryRefireSecondary).f, RC2Pref(RC2P_Grenade_PrimaryRefireTertiary).f);
			UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Grenade_PrimaryRefireQuaternary).f);
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= (unsigned short)RC2Pref(RC2P_Grenade_GravGrenAmmoCost).s;
			break;
		}

		if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			// just threw the last one: let the throw animation finish
			RaiseAttackTimers(this, 0.5, 0.5, 0.5);
			UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.5);
		}
		return;
	}

	// the throw is done: draw the next one, or put the weapon away
	// VERIFY: RC2Frozen() is read off the weapon itself here, not the player
	if (m_flReleaseThrow > 0 && !RC2Frozen() && m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
	{
		m_flStartThrow = 0;
		if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			SendWeaponAnim(HANDGRENADE_DRAW);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
			m_flReleaseThrow = -1;
		}
		else
		{
			RetireWeapon();
		}
	}
}
