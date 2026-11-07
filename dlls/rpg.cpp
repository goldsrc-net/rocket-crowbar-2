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
#include "UserMessages.h"
#include "rc2prefs.h"
#include <algorithm>
// The client reads its copy of rc2.ini as the server does (client.dll, reference/dossiers/c1.md).
#define RC2_RPG_PICKUP_GIVE RC2Pref(RC2P_Ammo_RPGPickupGive).s
#define RC2_RPG_MAX_CARRY RC2Pref(RC2P_Ammo_MaxCarryRPG).s
#define RC2_RPG_CLIP_SIZE RC2Pref(RC2P_Ammo_RPGClipSize).s

// Rocket Crowbar 2: RC2's rpg file starts here (0xb9420-0xb952c are the p6 package's; CRpg's
// 0xb9530-0xb9770 are in no package yet). The inline CRpg methods in weapons.h are compiled here
// in the original: iItemSlot returns 4 (orig 0xb9490), ShouldWeaponIdle and UseDecrement return
// true (orig 0xb94a0, orig 0xb94b0), all as SDK 2.3's.
// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("weapon_rpg") (0xf5a20); the shared macro doesn't yet
// orig client 0x40cb0 (and its GetClassPtr<CRpg>, orig client 0x41f60); the inline iItemSlot,
// ShouldWeaponIdle and UseDecrement are orig client 0x3bc10, orig client 0x3bc30 and orig client 0x3bc50
LINK_ENTITY_TO_CLASS(weapon_rpg, CRpg); // orig 0xb9420

#ifndef CLIENT_DLL

LINK_ENTITY_TO_CLASS(laser_spot, CLaserSpot);

//=========================================================
//=========================================================
CLaserSpot* CLaserSpot::CreateSpot()
{
	CLaserSpot* pSpot = GetClassPtr((CLaserSpot*)NULL);
	pSpot->Spawn();

	pSpot->pev->classname = MAKE_STRING("laser_spot");

	return pSpot;
}

//=========================================================
//=========================================================
void CLaserSpot::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;

	SET_MODEL(ENT(pev), "sprites/laserdot.spr");
	UTIL_SetOrigin(pev, pev->origin);
};

//=========================================================
// Suspend- make the laser sight invisible.
//=========================================================
void CLaserSpot::Suspend(float flSuspendTime)
{
	pev->effects |= EF_NODRAW;

	SetThink(&CLaserSpot::Revive);
	pev->nextthink = gpGlobals->time + flSuspendTime;
}

//=========================================================
// Revive - bring a suspended laser sight back.
//=========================================================
void CLaserSpot::Revive()
{
	pev->effects &= ~EF_NODRAW;

	SetThink(NULL);
}

void CLaserSpot::Precache()
{
	PRECACHE_MODEL("sprites/laserdot.spr");
};

// Rocket Crowbar 2: the rocket is SDK 2.3's (func_tank's rocket turret and RC2's own code fire it);
// RC2 has no launcher link, so the launcher bookkeeping below is HLU's, kept for HLU's CRpg code.
// The link order puts the rocket's code inside func_tank.cpp's in the original.
LINK_ENTITY_TO_CLASS(rpg_rocket, CRpgRocket); // orig 0x46070

CRpgRocket::~CRpgRocket()
{
	if (m_hLauncher)
	{
		// my launcher is still around, tell it I'm dead.
		static_cast<CRpg*>(static_cast<CBaseEntity*>(m_hLauncher))->m_cActiveRockets--;
	}
}

//=========================================================
//=========================================================
CRpgRocket* CRpgRocket::CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity* pOwner, CRpg* pLauncher)
{
	CRpgRocket* pRocket = GetClassPtr((CRpgRocket*)NULL);

	UTIL_SetOrigin(pRocket->pev, vecOrigin);
	pRocket->pev->angles = vecAngles;
	pRocket->Spawn();
	pRocket->SetTouch(&CRpgRocket::RocketTouch);
	pRocket->m_hLauncher = pLauncher; // remember what RPG fired me.
	pLauncher->m_cActiveRockets++;	  // register this missile as active for the launcher
	pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

//=========================================================
//=========================================================
// orig 0x46150. RC2: a fixed 120 damage instead of the skill setting.
void CRpgRocket::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink(&CRpgRocket::IgniteThink);
	SetTouch(&CRpgRocket::ExplodeTouch);

	pev->angles.x -= 30;
	UTIL_MakeVectors(pev->angles);
	pev->angles.x = -(pev->angles.x + 30);

	pev->velocity = gpGlobals->v_forward * 250;
	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + 0.4;

	pev->dmg = 120;
}

//=========================================================
//=========================================================
// orig 0x462c0. RC2: a rocket that carries a dmg_inflictor explodes as that entity's.
void CRpgRocket::RocketTouch(CBaseEntity* pOther)
{
	if (!FNullEnt(pev->dmg_inflictor))
		pev->owner = pev->dmg_inflictor;

	STOP_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav");
	ExplodeTouch(pOther);
}

//=========================================================
//=========================================================
// orig 0x46310
void CRpgRocket::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}


// orig 0x46340
void CRpgRocket::IgniteThink()
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5);

	// rocket trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);

	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex()); // entity
	WRITE_SHORT(m_iTrail);	 // model
	WRITE_BYTE(40);			 // life
	WRITE_BYTE(5);			 // width
	WRITE_BYTE(224);		 // r, g, b
	WRITE_BYTE(224);		 // r, g, b
	WRITE_BYTE(255);		 // r, g, b
	WRITE_BYTE(255);		 // brightness

	MESSAGE_END(); // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink(&CRpgRocket::FollowThink);
	pev->nextthink = gpGlobals->time + 0.1;
}


CRpg* CRpgRocket::GetLauncher()
{
	if (!m_hLauncher)
		return NULL;

	return (CRpg*)((CBaseEntity*)m_hLauncher);
}

// orig 0x46440. RC2: SDK 2.3's, without HLU's launcher release and sky check.
void CRpgRocket::FollowThink()
{
	CBaseEntity* pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors(pev->angles);

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;

	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname(pOther, "laser_spot")) != NULL)
	{
		Vector vSpotLocation = pOther->pev->origin;

		/*if (UTIL_PointContents(vSpotLocation) == CONTENTS_SKY)
		{
			ALERT(at_console, "laser spot is in the sky...\n");
		}*/

		UTIL_TraceLine(pev->origin, vSpotLocation, dont_ignore_monsters, ENT(pev), &tr);
		//ALERT(at_console, "fraction: %f\n", tr.flFraction);

		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length();
			vecDir = vecDir.Normalize();
			flDot = DotProduct(gpGlobals->v_forward, vecDir);
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	pev->angles = UTIL_VecToAngles(vecTarget);

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		pev->velocity = pev->velocity * 0.2 + vecTarget * (flSpeed * 0.8 + 400);
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if (pev->velocity.Length() > 300)
			{
				pev->velocity = pev->velocity.Normalize() * 300;
			}
			UTIL_BubbleTrail(pev->origin - pev->velocity * 0.1, pev->origin, 4);
		}
		else
		{
			if (pev->velocity.Length() > 2000)
			{
				pev->velocity = pev->velocity.Normalize() * 2000;
			}
		}
	}
	else
	{
		if ((pev->effects & EF_LIGHT) != 0)
		{
			pev->effects = 0;
			STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
		}
		pev->velocity = pev->velocity * 0.2 + vecTarget * flSpeed * 0.798;
		if (pev->waterlevel == 0 && pev->velocity.Length() < 1500)
		{
			Detonate();
		}
	}


	pev->nextthink = gpGlobals->time + 0.1;
}
#endif



void CRpg::Reload() // orig 0xb94c0, orig client 0x40ce0
{
	// Rocket Crowbar 2: the clip size and reload time come from rc2.ini, and none of SDK 2.3's
	// laser-spot handling is left (no next-attack delay, no suspending the spot, no waiting
	// for rockets in flight)
	const int iClipSize = RC2Pref(RC2P_Ammo_RPGClipSize).s;
	if (m_iClip == iClipSize)
		return;

	if (m_pPlayer->ammo_rockets <= 0)
		return;

	if (DefaultReload(iClipSize, RPG_RELOAD, RC2Pref(RC2P_RPG_ReloadTime).f))
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

// orig 0xb9530, orig client 0x40d70. RC2: rc2.ini's pickup amount; no laser spot to start with.
void CRpg::Spawn()
{
	Precache();
	m_iId = WEAPON_RPG;

	SET_MODEL(ENT(pev), "models/w_rpg.mdl");
	m_iDefaultAmmo = RC2_RPG_PICKUP_GIVE;
	m_flNextMineCount = gpGlobals->time;

	FallInit(); // get ready to fall down.
}


// orig 0xb9580, orig client 0x40df0. RC2: the launcher throws RC2's four mines instead of laser-guided rockets.
void CRpg::Precache()
{
	PRECACHE_MODEL("models/w_rpg.mdl");
	PRECACHE_MODEL("models/v_rpg.mdl");
	PRECACHE_MODEL("models/p_rpg.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther("rpg_rocket");
	UTIL_PrecacheOther("scientist_mine");
	UTIL_PrecacheOther("suck_mine");
	UTIL_PrecacheOther("sphere_mine");
	UTIL_PrecacheOther("rocket_mine");

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound

	m_usRpg = PRECACHE_EVENT(1, "events/rpg.sc");
}


// orig 0xb9610, orig client 0x40eb0. RC2: carry and clip from rc2.ini, no flags, weight 12.
bool CRpg::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = RC2_RPG_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RC2_RPG_CLIP_SIZE;
	p->iSlot = 3;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_RPG;
	p->iFlags = 0;
	p->iWeight = 12;

	return true;
}

// orig 0xb9680 (orig client 0x40f40): CRpg::AddToPlayer is SDK 2.3's (CBasePlayerWeapon::AddToPlayer, then the
// WeapPickup message); HLU sends that message from CBasePlayer::AddPlayerItem for every weapon,
// so there is nothing to write here.

// orig client 0x40fc0
bool CRpg::Deploy()
{
	if (m_iClip == 0)
	{
		return DefaultDeploy("models/v_rpg.mdl", "models/p_rpg.mdl", RPG_DRAW_UL, "rpg");
	}

	return DefaultDeploy("models/v_rpg.mdl", "models/p_rpg.mdl", RPG_DRAW1, "rpg");
}


// orig 0xb9720, orig client 0x41010. RC2: always.
bool CRpg::CanHolster()
{
	return true;
}

// orig 0xb9730, orig client 0x41030. RC2: no laser spot to kill.
void CRpg::Holster()
{
	m_fInReload = false; // cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	SendWeaponAnim(RPG_HOLSTER1);
}



// Rocket Crowbar 2: CRpg launches mines, one kind per attack. Each attack costs ammo (clip
// first, then the reserve), is refused while the player has the kind's MaxDeployed of it out,
// and the sphere mine and roman candle also cost shield energy. The four functions below are
// the same code with the [RPG] keys changed; LaunchMine holds it once.

// How many of szClassname this player has out (the mines keep their launcher in
// pev->dmg_inflictor).
// The client's (orig client 0x41080) answers with the counts WeaponSync last sent.
int CRpg::CountMines(const char* szClassname) // orig 0xb9770
{
#ifdef CLIENT_DLL
	if (0 == strcmp(szClassname, "scientist_mine"))
		return m_iSciMines;
	if (0 == strcmp(szClassname, "suck_mine"))
		return m_iSuckMines;
	if (0 == strcmp(szClassname, "sphere_mine"))
		return m_iSphereMines;
	if (0 == strcmp(szClassname, "rocket_mine"))
		return m_iRomanCandles;
	return 0;
#else
	int iCount = 0;
	for (CBaseEntity* pMine = UTIL_FindEntityByClassname(NULL, szClassname); pMine != NULL; pMine = UTIL_FindEntityByClassname(pMine, szClassname))
	{
		if (pMine->pev != NULL && UTIL_IsValidEntity(pMine->pev->pContainingEntity) && pMine->pev->dmg_inflictor == m_pPlayer->pev->pContainingEntity)
			iCount++;
	}
	return iCount;
#endif
}

// name ours: the body the original repeats in each of the four attacks
void CRpg::LaunchMine(const char* szClassname, int iAmmoCost, int iMaxDeployed, const float* pflEnergyCost, const float* pflEnergyRegenDelay,
	float flLaunchVelocity, const float* pflLaunchZBoost, float flRefirePrimary, float flRefireSecondary, float flRefireTertiary, float flRefireQuaternary)
{
	CRpg* pRpg = this;
	CBasePlayer* pPlayer = m_pPlayer;

	if (pPlayer->m_rgAmmo[pRpg->m_iPrimaryAmmoType] + pRpg->m_iClip >= iAmmoCost && pRpg->CountMines(szClassname) < iMaxDeployed && (pflEnergyCost == NULL || pPlayer->m_flEnergy >= *pflEnergyCost))
	{
		pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
		pRpg->m_flNextMineCount = gpGlobals->time; // recount on the next frame

#ifndef CLIENT_DLL
		pPlayer->SetAnimation(PLAYER_ATTACK1);

		UTIL_MakeVectors(pPlayer->pev->v_angle);
		Vector vecSrc = pPlayer->GetGunPosition() + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

		CBaseEntity* pMine = CBaseEntity::Create(szClassname, vecSrc, Vector(0, pPlayer->pev->v_angle.y, 0), pPlayer->edict());
		pMine->pev->dmg_inflictor = pPlayer->edict();
		pMine->pev->velocity = gpGlobals->v_forward * flLaunchVelocity;
		if (pflLaunchZBoost != NULL)
			pMine->pev->velocity.z += *pflLaunchZBoost;

		if (0 != pPlayer->m_iRidingIndex)
		{
			edict_t* pentOwner = INDEXENT(pPlayer->m_iRidingIndex);
			if (UTIL_IsValidEntity(pentOwner))
				pMine->pev->owner = pentOwner;
		}
#endif

		PLAYBACK_EVENT_FULL(FEV_NOTHOST, pPlayer->edict(), pRpg->m_usRpg, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, false, false);

		if (pRpg->m_iClip < iAmmoCost)
		{
			pPlayer->m_rgAmmo[pRpg->m_iPrimaryAmmoType] += pRpg->m_iClip - iAmmoCost;
			pRpg->m_iClip = 0;
		}
		else
			pRpg->m_iClip -= iAmmoCost;

		if (pflEnergyCost != NULL)
		{
			pPlayer->m_flEnergy -= *pflEnergyCost;
			UTIL_ExtendTime(pPlayer->m_flEnergyRegenTime, gpGlobals->time, *pflEnergyRegenDelay);
		}

		UTIL_ExtendTime(pRpg->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flRefirePrimary);
		UTIL_ExtendTime(pRpg->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flRefireSecondary);
		UTIL_ExtendTime(pRpg->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flRefireTertiary);
		UTIL_ExtendTime(pRpg->m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flRefireQuaternary);
		pRpg->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	}
	else
	{
		pRpg->PlayEmptySound();
		UTIL_ExtendTime(pRpg->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.25);
		UTIL_ExtendTime(pRpg->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.25);
		UTIL_ExtendTime(pRpg->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.25);
		UTIL_ExtendTime(pRpg->m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.25);
	}
}

// The scientist mine.
void CRpg::PrimaryAttack() // orig 0xb97e0, orig client 0x41200
{
	LaunchMine("scientist_mine", RC2Pref(RC2P_RPG_SciMineAmmoCost).s, RC2Pref(RC2P_RPG_SciMineMaxDeployed).s, NULL, NULL,
		RC2Pref(RC2P_RPG_SciMineLaunchVelocity).f, &RC2Pref(RC2P_RPG_SciMineLaunchZBoost).f,
		RC2Pref(RC2P_RPG_PrimaryRefirePrimary).f, RC2Pref(RC2P_RPG_PrimaryRefireSecondary).f, RC2Pref(RC2P_RPG_PrimaryRefireTertiary).f, RC2Pref(RC2P_RPG_PrimaryRefireQuaternary).f);
}

// The suck mine.
void CRpg::SecondaryAttack() // orig 0xb9cb0, orig client 0x414b0
{
	LaunchMine("suck_mine", RC2Pref(RC2P_RPG_SuckMineAmmoCost).s, RC2Pref(RC2P_RPG_SuckMineMaxDeployed).s, NULL, NULL,
		RC2Pref(RC2P_RPG_SuckMineLaunchVelocity).f, &RC2Pref(RC2P_RPG_SuckMineLaunchZBoost).f,
		RC2Pref(RC2P_RPG_SecondaryRefirePrimary).f, RC2Pref(RC2P_RPG_SecondaryRefireSecondary).f, RC2Pref(RC2P_RPG_SecondaryRefireTertiary).f, RC2Pref(RC2P_RPG_SecondaryRefireQuaternary).f);
}

// The sphere mine: costs energy, and has no z boost (rc2.ini has no key for one).
void CRpg::TertiaryAttack() // orig 0xba180, orig client 0x41760
{
	LaunchMine("sphere_mine", RC2Pref(RC2P_RPG_SphereMineAmmoCost).s, RC2Pref(RC2P_RPG_SphereMineMaxDeployed).s, &RC2Pref(RC2P_RPG_SphereMineEnergyCost).f, &RC2Pref(RC2P_RPG_SphereMineEnergyRegenDelay).f,
		RC2Pref(RC2P_RPG_SphereMineLaunchVelocity).f, NULL,
		RC2Pref(RC2P_RPG_TertiaryRefirePrimary).f, RC2Pref(RC2P_RPG_TertiaryRefireSecondary).f, RC2Pref(RC2P_RPG_TertiaryRefireTertiary).f, RC2Pref(RC2P_RPG_TertiaryRefireQuaternary).f);
}

// The roman candle (rocket_mine): costs energy.
void CRpg::QuaternaryAttack() // orig 0xba670, orig client 0x41a70
{
	LaunchMine("rocket_mine", RC2Pref(RC2P_RPG_RomanCandleAmmoCost).s, RC2Pref(RC2P_RPG_RomanCandleMaxDeployed).s, &RC2Pref(RC2P_RPG_RomanCandleEnergyCost).f, &RC2Pref(RC2P_RPG_RomanCandleEnergyRegenDelay).f,
		RC2Pref(RC2P_RPG_RomanCandleLaunchVelocity).f, &RC2Pref(RC2P_RPG_RomanCandleLaunchZBoost).f,
		RC2Pref(RC2P_RPG_QuaternaryRefirePrimary).f, RC2Pref(RC2P_RPG_QuaternaryRefireSecondary).f, RC2Pref(RC2P_RPG_QuaternaryRefireTertiary).f, RC2Pref(RC2P_RPG_QuaternaryRefireQuaternary).f);
}

// Every frame: four times a second, recount this player's mines and send the counts to the HUD.
void CRpg::WeaponFrame() // orig 0xbab40, orig client 0x41d80 (empty on the client)
{
#ifndef CLIENT_DLL
	if (m_flNextMineCount < gpGlobals->time)
	{
		m_flNextMineCount = gpGlobals->time + 0.25;
		m_iSciMines = CountMines("scientist_mine");
		m_iSuckMines = CountMines("suck_mine");
		m_iSphereMines = CountMines("sphere_mine");
		m_iRomanCandles = CountMines("rocket_mine");

		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_RPG);
		WRITE_BYTE(std::clamp(m_iSciMines, 0, 255));
		WRITE_BYTE(std::clamp(m_iSuckMines, 0, 255));
		WRITE_BYTE(std::clamp(m_iSphereMines, 0, 255));
		WRITE_BYTE(std::clamp(m_iRomanCandles, 0, 255));
		MESSAGE_END();
	}
#endif
}

// The four counts, a byte each (the server's reader, 0xfb780, answers 0; the client's,
// orig client 0x3b940, reads them)
void CRpg::ReadWeaponState(void* pData, int iSize) // orig 0xbac90, orig client 0x41da0
{
	m_iSciMines = RC2ReadStateByte(&pData, &iSize);
	m_iSuckMines = RC2ReadStateByte(&pData, &iSize);
	m_iSphereMines = RC2ReadStateByte(&pData, &iSize);
	m_iRomanCandles = RC2ReadStateByte(&pData, &iSize);
}

void CRpg::WeaponIdle() // orig 0xbad10, orig client 0x41e40
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
		if (flRand <= 0.75)
		{
			iAnim = (m_iClip == 0) ? RPG_IDLE_UL : RPG_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.0;
		}
		else
		{
			iAnim = (m_iClip == 0) ? RPG_FIDGET_UL : RPG_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0;
		}

		SendWeaponAnim(iAnim);
	}
	else
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
	}
}



void CRpg::UpdateSpot()
{

#ifndef CLIENT_DLL
	// Don't turn on the laser if we're in the middle of a reload.
	if (m_fInReload)
	{
		return;
	}

	if (m_fSpotActive)
	{
		if (!m_pSpot)
		{
			m_pSpot = CLaserSpot::CreateSpot();
		}

		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		Vector vecSrc = m_pPlayer->GetGunPosition();
		Vector vecAiming = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

		UTIL_SetOrigin(m_pSpot->pev, tr.vecEndPos);
	}
#endif
}

class CRpgAmmo : public CBasePlayerAmmo
{
	void Spawn() override // orig 0xbae40, orig client 0x42060
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_rpgammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override // orig 0xbae70, orig client 0x420a0
	{
		PRECACHE_MODEL("models/w_rpgammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override // orig 0xbae90, orig client 0x420d0
	{
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_RPGRocketBoxGive).s, "rockets", RC2Pref(RC2P_Ammo_MaxCarryRPG).s) != -1)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			return true;
		}
		return false;
	}
};
// orig client 0x41f30 (and its GetClassPtr<CRpgAmmo>, orig client 0x41fe0)
LINK_ENTITY_TO_CLASS(ammo_rpgclip, CRpgAmmo); // orig 0xbadd0
