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
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "UserMessages.h"

#ifdef CLIENT_DLL
#include "hud.h"
#include "com_weapons.h"
#else
extern bool IsBustingGame();
#endif

#ifdef CLIENT_DLL

// halflife-updated's egon, kept for the client's weapon prediction until RC2's client.dll is
// reconstructed. The server's egon is Rocket Crowbar 2's, below.

#define EGON_SWITCH_NARROW_TIME 0.75 // Time it takes to switch fire modes
#define EGON_SWITCH_WIDE_TIME 1.5

LINK_ENTITY_TO_CLASS(weapon_egon, CEgon);

void CEgon::Spawn()
{
	Precache();
	m_iId = WEAPON_EGON;
	SET_MODEL(ENT(pev), "models/w_egon.mdl");

	m_iDefaultAmmo = EGON_DEFAULT_GIVE;

	FallInit(); // get ready to fall down.
}


void CEgon::Precache()
{
	PRECACHE_MODEL("models/w_egon.mdl");
	PRECACHE_MODEL("models/v_egon.mdl");
	PRECACHE_MODEL("models/p_egon.mdl");

	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND(EGON_SOUND_OFF);
	PRECACHE_SOUND(EGON_SOUND_RUN);
	PRECACHE_SOUND(EGON_SOUND_STARTUP);

	PRECACHE_MODEL(EGON_BEAM_SPRITE);
	PRECACHE_MODEL(EGON_FLARE_SPRITE);

	PRECACHE_SOUND("weapons/357_cock1.wav");

	m_usEgonFire = PRECACHE_EVENT(1, "events/egon_fire.sc");
	m_usEgonStop = PRECACHE_EVENT(1, "events/egon_stop.sc");
}


bool CEgon::Deploy()
{
	m_deployed = false;
	m_fireState = FIRE_OFF;
	return DefaultDeploy("models/v_egon.mdl", "models/p_egon.mdl", EGON_DRAW, "egon");
}

void CEgon::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(EGON_HOLSTER);

	EndAttack();
}

bool CEgon::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_EGON;
	p->iFlags = 0;
	p->iWeight = EGON_WEIGHT;

	return true;
}

#define EGON_PULSE_INTERVAL 0.1
#define EGON_DISCHARGE_INTERVAL 0.1

float CEgon::GetPulseInterval()
{
	return EGON_PULSE_INTERVAL;
}

float CEgon::GetDischargeInterval()
{
	return EGON_DISCHARGE_INTERVAL;
}

bool CEgon::HasAmmo()
{
	if (m_pPlayer->ammo_uranium <= 0)
		return false;

	return true;
}

void CEgon::UseAmmo(int count)
{
#ifndef CLIENT_DLL
	if (IsBustingGame())
		return;
#endif

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= count)
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= count;
	else
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
}

void CEgon::Attack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{

		if (m_fireState != FIRE_OFF || m_pBeam)
		{
			EndAttack();
		}
		else
		{
			PlayEmptySound();
		}
		return;
	}

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition();

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	switch (m_fireState)
	{
	case FIRE_OFF:
	{
		if (!HasAmmo())
		{
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
			PlayEmptySound();
			return;
		}

		m_flAmmoUseTime = gpGlobals->time; // start using ammo ASAP.

		PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usEgonFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, m_fireMode, 1, 0);

		m_shakeTime = 0;

		m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		pev->fuser1 = UTIL_WeaponTimeBase() + 2;

		pev->dmgtime = gpGlobals->time + GetPulseInterval();
		m_fireState = FIRE_CHARGE;
	}
	break;

	case FIRE_CHARGE:
	{
		Fire(vecSrc, vecAiming);
		m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;

		if (pev->fuser1 <= UTIL_WeaponTimeBase())
		{
			PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usEgonFire, 0, g_vecZero, g_vecZero, 0.0, 0.0, 0, m_fireMode, 0, 0);
			pev->fuser1 = 1000;
		}

		if (!HasAmmo())
		{
			EndAttack();
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
		}
	}
	break;
	}
}

void CEgon::PrimaryAttack()
{
	m_fireMode = FIRE_WIDE;
	Attack();
}

void CEgon::Fire(const Vector& vecOrigSrc, const Vector& vecDir)
{
	Vector vecDest = vecOrigSrc + vecDir * 2048;
	edict_t* pentIgnore;
	TraceResult tr;

	pentIgnore = m_pPlayer->edict();
	Vector tmpSrc = vecOrigSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;

	// ALERT( at_console, "." );

	UTIL_TraceLine(vecOrigSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

	if (0 != tr.fAllSolid)
		return;

#ifndef CLIENT_DLL
	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity == NULL)
		return;

	if (g_pGameRules->IsMultiplayer())
	{
		if (m_pSprite && 0 != pEntity->pev->takedamage)
		{
			m_pSprite->pev->effects &= ~EF_NODRAW;
		}
		else if (m_pSprite)
		{
			m_pSprite->pev->effects |= EF_NODRAW;
		}
	}


#endif

	float timedist;

	switch (m_fireMode)
	{
	default:
	case FIRE_NARROW:
#ifndef CLIENT_DLL
		if (pev->dmgtime < gpGlobals->time)
		{
			// Narrow mode only does damage to the entity it hits
			ClearMultiDamage();
			if (0 != pEntity->pev->takedamage)
			{
				pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgEgonNarrow, vecDir, &tr, DMG_ENERGYBEAM);
			}
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

			if (g_pGameRules->IsMultiplayer())
			{
				// multiplayer uses 1 ammo every 1/10th second
				if (gpGlobals->time >= m_flAmmoUseTime)
				{
					UseAmmo(1);
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				}
			}
			else
			{
				// single player, use 3 ammo/second
				if (gpGlobals->time >= m_flAmmoUseTime)
				{
					UseAmmo(1);
					m_flAmmoUseTime = gpGlobals->time + 0.166;
				}
			}

			pev->dmgtime = gpGlobals->time + GetPulseInterval();
		}
#endif
		timedist = (pev->dmgtime - gpGlobals->time) / GetPulseInterval();
		break;

	case FIRE_WIDE:
#ifndef CLIENT_DLL
		if (pev->dmgtime < gpGlobals->time)
		{
			// wide mode does damage to the ent, and radius damage
			ClearMultiDamage();
			if (0 != pEntity->pev->takedamage)
			{
				pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgEgonWide, vecDir, &tr, DMG_ENERGYBEAM | DMG_ALWAYSGIB);
			}
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

			if (g_pGameRules->IsMultiplayer())
			{
				// radius damage a little more potent in multiplayer.
				::RadiusDamage(tr.vecEndPos, pev, m_pPlayer->pev, gSkillData.plrDmgEgonWide / 4, 128, CLASS_NONE, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB);
			}

			if (!m_pPlayer->IsAlive())
				return;

			if (g_pGameRules->IsMultiplayer())
			{
				//multiplayer uses 5 ammo/second
				if (gpGlobals->time >= m_flAmmoUseTime)
				{
					UseAmmo(1);
					m_flAmmoUseTime = gpGlobals->time + 0.2;
				}
			}
			else
			{
				// Wide mode uses 10 charges per second in single player
				if (gpGlobals->time >= m_flAmmoUseTime)
				{
					UseAmmo(1);
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				}
			}

			pev->dmgtime = gpGlobals->time + GetDischargeInterval();
			if (m_shakeTime < gpGlobals->time)
			{
				UTIL_ScreenShake(tr.vecEndPos, 5.0, 150.0, 0.75, 250.0);
				m_shakeTime = gpGlobals->time + 1.5;
			}
		}
#endif
		timedist = (pev->dmgtime - gpGlobals->time) / GetDischargeInterval();
		break;
	}

	if (timedist < 0)
		timedist = 0;
	else if (timedist > 1)
		timedist = 1;
	timedist = 1 - timedist;

	UpdateEffect(tmpSrc, tr.vecEndPos, timedist);
}


void CEgon::UpdateEffect(const Vector& startPoint, const Vector& endPoint, float timeBlend)
{
#ifndef CLIENT_DLL
	if (!m_pBeam)
	{
		CreateEffect();
	}

	m_pBeam->SetStartPos(endPoint);
	m_pBeam->SetBrightness(255 - (timeBlend * 180));
	m_pBeam->SetWidth(40 - (timeBlend * 20));

	if (m_fireMode == FIRE_WIDE)
		m_pBeam->SetColor(30 + (25 * timeBlend), 30 + (30 * timeBlend), 64 + 80 * fabs(sin(gpGlobals->time * 10)));
	else
		m_pBeam->SetColor(60 + (25 * timeBlend), 120 + (30 * timeBlend), 64 + 80 * fabs(sin(gpGlobals->time * 10)));


	UTIL_SetOrigin(m_pSprite->pev, endPoint);
	m_pSprite->pev->frame += 8 * gpGlobals->frametime;
	if (m_pSprite->pev->frame > m_pSprite->Frames())
		m_pSprite->pev->frame = 0;

	m_pNoise->SetStartPos(endPoint);

#endif
}

void CEgon::CreateEffect()
{

#ifndef CLIENT_DLL
	DestroyEffect();

	m_pBeam = CBeam::BeamCreate(EGON_BEAM_SPRITE, 40);
	m_pBeam->PointEntInit(pev->origin, m_pPlayer->entindex());
	m_pBeam->SetFlags(BEAM_FSINE);
	m_pBeam->SetEndAttachment(1);
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY; // Flag these to be destroyed on save/restore or level transition
	m_pBeam->pev->flags |= FL_SKIPLOCALHOST;
	m_pBeam->pev->owner = m_pPlayer->edict();

	m_pNoise = CBeam::BeamCreate(EGON_BEAM_SPRITE, 55);
	m_pNoise->PointEntInit(pev->origin, m_pPlayer->entindex());
	m_pNoise->SetScrollRate(25);
	m_pNoise->SetBrightness(100);
	m_pNoise->SetEndAttachment(1);
	m_pNoise->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_pNoise->pev->flags |= FL_SKIPLOCALHOST;
	m_pNoise->pev->owner = m_pPlayer->edict();

	m_pSprite = CSprite::SpriteCreate(EGON_FLARE_SPRITE, pev->origin, false);
	m_pSprite->pev->scale = 1.0;
	m_pSprite->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation);
	m_pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	// Josh: This sprite is not predicted on the client, so was missing
	// for many years after it got broken in an update.
	// m_pSprite->pev->flags |= FL_SKIPLOCALHOST;
	m_pSprite->pev->owner = m_pPlayer->edict();

	if (m_fireMode == FIRE_WIDE)
	{
		m_pBeam->SetScrollRate(50);
		m_pBeam->SetNoise(20);
		m_pNoise->SetColor(50, 50, 255);
		m_pNoise->SetNoise(8);
	}
	else
	{
		m_pBeam->SetScrollRate(110);
		m_pBeam->SetNoise(5);
		m_pNoise->SetColor(80, 120, 255);
		m_pNoise->SetNoise(2);
	}
#endif
}


void CEgon::DestroyEffect()
{

#ifndef CLIENT_DLL
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = NULL;
	}
	if (m_pNoise)
	{
		UTIL_Remove(m_pNoise);
		m_pNoise = NULL;
	}
	if (m_pSprite)
	{
		if (m_fireMode == FIRE_WIDE)
			m_pSprite->Expand(10, 500);
		else
			UTIL_Remove(m_pSprite);
		m_pSprite = NULL;
	}
#endif
}



void CEgon::WeaponIdle()
{
	if ((m_pPlayer->m_afButtonPressed & IN_ATTACK2) == 0 && (m_pPlayer->pev->button & IN_ATTACK) != 0)
	{
		return;
	}

	ResetEmptySound();

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_fireState != FIRE_OFF)
		EndAttack();

	int iAnim;

	float flRand = RANDOM_FLOAT(0, 1);

	if (flRand <= 0.5)
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
	else
	{
		iAnim = EGON_FIDGET1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	}

	SendWeaponAnim(iAnim);
	m_deployed = true;
}

bool CEgon::CanHolster()
{
#ifndef CLIENT_DLL
	if (IsBustingGame())
	{
		return false;
	}
#endif

	return true;
}

void CEgon::EndAttack()
{
	bool bMakeNoise = false;

	if (m_fireState != FIRE_OFF) //Checking the button just in case!.
		bMakeNoise = true;

	PLAYBACK_EVENT_FULL(FEV_GLOBAL | FEV_RELIABLE, m_pPlayer->edict(), m_usEgonStop, 0, m_pPlayer->pev->origin, m_pPlayer->pev->angles, 0.0, 0.0,
		static_cast<int>(bMakeNoise), 0, 0, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;

	m_fireState = FIRE_OFF;

	DestroyEffect();
}



class CEgonAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PRECACHE_MODEL("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_egonclip, CEgonAmmo);

#else // !CLIENT_DLL

//=========================================================
// Rocket Crowbar 2's egon. Primary: a tractor beam that grabs what it hits and holds it at a
// distance the secondary and tertiary attacks lengthen and shorten, and the quaternary throws
// it. With the beam off: secondary fires the shrinker, tertiary charges the imploder (the
// secondary during the charge shrinks yourself instead), quaternary fires the nuke. Every number
// is rc2.ini [Egon]/[Ammo]. Reconstructed from rc2.dll (reference/dossiers/p2.md).
//=========================================================

#include "rc2prefs.h"
#include "p2shared.h"

// from other files (TEMP STUBs in dlls/rc2/p2stubs_forkC.cpp)
extern int RC2PlayerWaterWeapons(CBasePlayer* pPlayer);		  // orig 0x95860 (player): bit 9 of RC2PlayerButtons, the player may fire under water
extern int UTIL_PackNormal(Vector vecNormal);					  // orig 0xf5920 (util): a unit vector in an int, 10 bits and a sign per axis

// can't fire: fully under water without the RC2 under-water bit
static bool EgonUnderwater(CBasePlayer* pPlayer) // name ours
{
	return pPlayer->pev->waterlevel == 3 && RC2PlayerWaterWeapons(pPlayer) == 0;
}

// the bytes RC2 keeps in CBaseEntity +0x64 (cbase.h's m_ucTractorHeld, m_ucTractorKnocked, m_bFrozen, m_bAntiFriction): +0x64 an egon holds it,
// +0x65 another egon knocked it out of this one's beam
static byte& TractorHeld(CBaseEntity* pEnt) { return pEnt->m_ucTractorHeld; }	   // name ours
static byte& TractorKnocked(CBaseEntity* pEnt) { return pEnt->m_ucTractorKnocked; } // name ours

#define EGON_EXTEND4(flDelay)                                                     \
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flDelay);      \
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flDelay);    \
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flDelay);     \
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flDelay)

#define EGON_EXTEND4_KEYS(Keys)                                                                                             \
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefirePrimary).f);       \
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefireSecondary).f);   \
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefireTertiary).f);     \
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefireQuaternary).f)

#define EGON_AMMO (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])

LINK_ENTITY_TO_CLASS(weapon_egon, CEgon); // orig 0x3d290

// orig 0x3d320
void CEgon::Spawn()
{
	Precache();
	m_iId = WEAPON_EGON;
	SET_MODEL(ENT(pev), "models/w_egon.mdl");
	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_EgonPickupGive).s;
	FallInit(); // get ready to fall down.

	m_bHolding = false;
	m_flNextSync = gpGlobals->time;
	m_bAdjusting = false;
	m_flLastFrame = gpGlobals->time;
}

// orig 0x3d390
void CEgon::Precache()
{
	PRECACHE_MODEL("models/w_egon.mdl");
	PRECACHE_MODEL("models/v_egon.mdl");
	PRECACHE_MODEL("models/p_egon.mdl");
	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/egon_windup2.wav");
	PRECACHE_SOUND("weapons/egon_run3.wav");
	PRECACHE_SOUND("weapons/egon_off1.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");
	PRECACHE_SOUND("debris/beamstart8.wav");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("ambience/biggun3.wav");

	UTIL_PrecacheOther("shrinker_project");
	UTIL_PrecacheOther("nuclear_missile");
	UTIL_PrecacheOther("implode_timer");

	m_usFireShrinker = PRECACHE_EVENT(1, "events/egonfireshrinker.sc");
	m_usImploderCharge = PRECACHE_EVENT(1, "events/egonimplodercharge.sc");
	m_usImploderFire = PRECACHE_EVENT(1, "events/egonimploderfire.sc");
	m_usFireNuke = PRECACHE_EVENT(1, "events/egonfirenuke.sc");
	m_usTractor = PRECACHE_EVENT(1, "events/egontractor.sc");
	m_usShrinkerImpact = PRECACHE_EVENT(1, "events/egonshrinkerimpact.sc");
}

// orig 0x3d4c0
bool CEgon::Deploy()
{
	m_bTractorOn = false;
	m_bHolding = false;
	m_bAdjusting = false;
	m_iChargeType = 0;
	m_fInAttack = 0;
	m_hTractor = NULL;
	m_flNextSync = gpGlobals->time;
	m_flLastFrame = gpGlobals->time;
	return DefaultDeploy("models/v_egon.mdl", "models/p_egon.mdl", EGON_DRAW, "egon");
}

// orig 0x3d530. RC2 sends gmsgWeapPickup here once the base has added the weapon;
// halflife-updated sends that same message from CBasePlayer::AddPlayerItem (one message either way).
void CEgon::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

// orig 0x3d590
void CEgon::Holster()
{
	TractorStop();
	m_flLastFrame = gpGlobals->time;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(EGON_HOLSTER);

	m_iChargeType = 0;
	m_fInAttack = 0;

	// end the client's beam effects
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_KILL);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_BYTE(0x29);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_KILL);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_BYTE(0x2c);
	MESSAGE_END();

	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_STATIC, "weapons/egon_run3.wav");
}

// orig 0x3d6a0: not while the beam is on
bool CEgon::CanHolster()
{
	return !m_bTractorOn;
}

// orig 0x3d6b0. Original oddity, kept: it also sets m_iId.
bool CEgon::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarryCells).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	m_iId = WEAPON_EGON;
	p->iId = WEAPON_EGON;
	p->iFlags = 0;
	p->iWeight = 25;
	return true;
}

// orig 0x3d720: the tractor beam, every frame the button is held
void CEgon::PrimaryAttack()
{
	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		TractorStop();
		EGON_EXTEND4(0.2);
		return;
	}

	if (EGON_AMMO < 1)
	{
		PlayEmptySound();
		TractorStop();
		EGON_EXTEND4(0.5);
		return;
	}

	if (!m_bTractorOn)
	{
		// switch it on
		m_bTractorOn = true;
		m_bHolding = false;
		m_hTractor = NULL;
		m_flNextSync = gpGlobals->time;
		EGON_AMMO--;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		SendWeaponAnim(EGON_FIRE1 + RANDOM_LONG(0, 3));
		if (g_runfuncs)
			PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 1);
		return;
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	TraceResult tr;

	if (m_bHolding && m_hTractor)
	{
		// lose it when something else comes between
		UTIL_TraceLine(vecSrc, m_hTractor->Center(), dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (tr.flFraction < 1.0)
		{
			CBaseEntity* pHit = tr.pHit ? (CBaseEntity*)GET_PRIVATE(tr.pHit) : NULL;
			if (!pHit || pHit->GetEffectedEnt()->edict() != m_hTractor->edict())
			{
				TractorHeld(m_hTractor) = 0;
				TractorKnocked(m_hTractor) = 0;
				m_bHolding = false;
				m_hTractor = NULL;
				m_flNextSync = gpGlobals->time;
			}
		}
	}
	else
	{
		// look for something to grab
		m_bHolding = false;
		m_hTractor = NULL;

		UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

		CBaseEntity* pEnt = NULL;
		if (tr.flFraction < 1.0 && UTIL_IsValidEntity(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit)
				pEnt = pHit->GetEffectedEnt();
		}

		// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
		if (pEnt && !pEnt->IsBSPModel() && (pEnt->GetRCCaps(m_pPlayer, this) & 0x80) != 0 && g_pGameRules->PlayerCanAffect(m_pPlayer, pEnt))
		{
			if (TractorHeld(pEnt))
			{
				// someone else's beam has it: knock it out, and stop
				TractorStop();
				TractorHeld(pEnt) = 0;
				TractorKnocked(pEnt) = 1;
				EGON_EXTEND4_KEYS(TractorDrop);
				return;
			}

			m_bHolding = true;
			m_hTractor = pEnt;
			m_flHoldDist = (m_hTractor->Center() - vecSrc).Length();
			m_bTargetAlive = m_hTractor->IsAlive();
			m_hTractor->m_vecRC2Base58 = vecSrc + gpGlobals->v_forward * m_flHoldDist; // where ApplyExternalForce pulls it
			TractorHeld(m_hTractor) = 1;
			TractorKnocked(m_hTractor) = 0;
			m_flNextSync = gpGlobals->time;
		}
	}

	if (m_bHolding && m_hTractor)
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, m_flHoldDist, 0, 0, ENTINDEX(m_hTractor->edict()), 0, 0);
	else
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
}

// orig 0x3ddd0: push what the beam holds away; with the beam off, the shrinker
void CEgon::SecondaryAttack()
{
	if (m_bTractorOn)
	{
		if (m_bHolding)
		{
			m_bAdjusting = true;
			m_flHoldDist += RC2Pref(RC2P_Egon_TractorExtendRate).f * m_flFrameInterval;
		}
		return;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		EGON_EXTEND4(0.2);
		return;
	}

	if (EGON_AMMO < (RC2Pref(RC2P_Egon_ShrinkerAmmoCost).s & 0xffff))
	{
		PlayEmptySound();
		EGON_EXTEND4(0.5);
		return;
	}

	EGON_EXTEND4_KEYS(Secondary);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	EGON_AMMO -= RC2Pref(RC2P_Egon_ShrinkerAmmoCost).s & 0xffff;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = 600;
	m_pPlayer->m_iWeaponFlash = 256;
	(void)m_pPlayer->GetGunPosition(); // the original calls it here and drops the result

	if (g_runfuncs)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireShrinker, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	if (EGON_AMMO <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
	CBaseEntity* pShrinker = CBaseEntity::Create("shrinker_project", m_pPlayer->GetGunPosition(), g_vecZero, m_pPlayer->edict());
	pShrinker->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Egon_ShrinkerLaunchVelocity).f;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pShrinker->pev->owner = pControlled;
	}
}

// orig 0x3e1e0: pull what the beam holds in; with the beam off, start charging the imploder
void CEgon::TertiaryAttack()
{
	if (m_bTractorOn)
	{
		if (!m_bHolding)
			return;
		m_bAdjusting = true;
		m_flHoldDist -= RC2Pref(RC2P_Egon_TractorContractRate).f * m_flFrameInterval;
		return;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		EGON_EXTEND4(0.15);
		return;
	}

	// enough for the whole charge and the shot
	int iNeed = (int)(RC2Pref(RC2P_Egon_ImploderChargeTime).f / RC2Pref(RC2P_Egon_ImploderChargeAmmoBurnDelay).f) * (RC2Pref(RC2P_Egon_ImploderChargeAmmoPerBurn).s & 0xffff) + (RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff);
	if (iNeed <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f <= m_pPlayer->m_flEnergy)
	{
		m_iChargeType = 1;
		m_fInAttack = 1;
		m_flChargeDone = RC2Pref(RC2P_Egon_ImploderChargeTime).f + gpGlobals->time;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
		if (g_runfuncs)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderCharge, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
		return;
	}

	PlayEmptySound();
	EGON_EXTEND4(0.5);
}

// orig 0x3e400: throw what the beam holds; with the beam off, the nuke
void CEgon::QuaternaryAttack()
{
	if (m_bTractorOn)
	{
		if (!m_bHolding)
			return;

		if (EGON_AMMO < (RC2Pref(RC2P_Egon_TractorLaunchCost).s & 0xffff))
		{
			PlayEmptySound();
			// Original oddity, kept: only the fourth attack's timer
			UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.2);
			return;
		}

		if (m_hTractor)
		{
			UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
			m_hTractor->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Egon_TractorLaunchVel).f + m_hTractor->pev->velocity;

			// the original reads sv_maxvelocity through the pointer world.cpp keeps (orig .data 0x18c500)
			float flMax = CVAR_GET_FLOAT("sv_maxvelocity");
			if (m_hTractor->pev->velocity.Length() > flMax)
				m_hTractor->pev->velocity = m_hTractor->pev->velocity.Normalize() * flMax;
		}

		EGON_AMMO -= RC2Pref(RC2P_Egon_TractorLaunchCost).s & 0xffff;
		TractorStop();
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.5);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.6);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.9);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.9);
		return;
	}

	if (EgonUnderwater(m_pPlayer) || m_pPlayer->m_flJoinTime > gpGlobals->time)
	{
		PlayEmptySound();
		EGON_EXTEND4(0.5);
		return;
	}

	if (EGON_AMMO < (RC2Pref(RC2P_Egon_NukeAmmoCost).s & 0xffff) || m_pPlayer->m_flEnergy < RC2Pref(RC2P_Egon_NukeEnergyCost).f)
	{
		PlayEmptySound();
		EGON_EXTEND4(0.75);
		return;
	}

	EGON_AMMO -= RC2Pref(RC2P_Egon_NukeAmmoCost).s & 0xffff;
	m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Egon_NukeEnergyCost).f;
	UTIL_ExtendTime(&m_pPlayer->m_flJoinTime, gpGlobals->time, RC2Pref(RC2P_Egon_QuaternaryRefireSuperWeapon).f);
	EGON_EXTEND4_KEYS(Quaternary);
	UTIL_ExtendTime(&m_pPlayer->m_flNextAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_QuaternaryRefireAnything).f);
	UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_Egon_QuaternaryEnergyRegenDelay).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = 1000;
	m_pPlayer->m_iWeaponFlash = 512;
	(void)m_pPlayer->GetGunPosition(); // the original calls it here and drops the result

	if (g_runfuncs)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireNuke, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
	CBaseEntity* pNuke = CBaseEntity::Create("nuclear_missile", m_pPlayer->GetGunPosition(), g_vecZero, m_pPlayer->edict());
	pNuke->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Egon_NukeLaunchVel).f;
	pNuke->pev->angles.x = -m_pPlayer->pev->v_angle.x;
	pNuke->pev->angles.y = m_pPlayer->pev->v_angle.y;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pNuke->pev->owner = pControlled;
	}
}

// orig 0x3eb40: switch the beam off and drop what it holds
void CEgon::TractorStop()
{
	if (m_bTractorOn)
	{
		if (m_hTractor)
		{
			m_hTractor->m_vecRC2Base58 = g_vecZero;
			TractorHeld(m_hTractor) = 0;
			TractorKnocked(m_hTractor) = 0;
		}
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 2);
		PLAYBACK_EVENT_FULL(FEV_RELIABLE | FEV_GLOBAL, m_pPlayer->edict(), m_usTractor, 0.01, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 2);
		SendWeaponAnim(EGON_IDLE1);
	}

	m_bTractorOn = false;
	m_bHolding = false;
	m_bAdjusting = false;
	m_hTractor = NULL;
	m_flNextSync = gpGlobals->time;
}

// orig 0x3ec50: the charged imploder: an implode timer homes on the best target ahead
void CEgon::ImploderFire()
{
	CBaseEntity* pBest = NULL;

	if (g_runfuncs)
	{
		m_iChargeType = 0;
		m_fInAttack = 0;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
		PlayEmptySound();
	}
	else if ((RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff) <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f <= m_pPlayer->m_flEnergy)
	{
		m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f;
		UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, 1.5);
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefirePrimary).f;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireSecondary).f;
		m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireTertiary).f;
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireQuaternary).f;
		m_pPlayer->m_iWeaponVolume = 600;

		if (g_runfuncs)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		EGON_AMMO -= RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff;

		float flBest = -9999;
		float flMaxDist = RC2Pref(RC2P_Egon_ImploderTargetMaxDist).f;
		float flMinDot = RC2Pref(RC2P_Egon_ImploderTargetMinDot).f;

		UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
		Vector vecForward = gpGlobals->v_forward;
		Vector vecSrc = m_pPlayer->GetGunPosition();

		// every entity, not just players
		for (int i = 1; i < gpGlobals->maxEntities; i++)
		{
			edict_t* pent = INDEXENT(i);
			if (pent == m_pPlayer->edict() || !UTIL_IsValidEntity(pent))
				continue;
			CBaseEntity* pEnt = CBaseEntity::Instance(pent);
			if (!pEnt || !pEnt->IsAlive() || pEnt->pev->takedamage != DAMAGE_AIM || pEnt->pev->deadflag != DEAD_NO || pEnt->IsBSPModel())
				continue;
			// not one already shrunk below half
			if (pEnt->pev->scale > 0 && pEnt->pev->scale < 0.5)
				continue;
			if (g_pGameRules->PlayerRelationship(m_pPlayer, pEnt) == GR_TEAMMATE)
				continue;
			// VERIFY: slot 76's first argument is the player here (cbase.h has an int)
			if ((pEnt->GetRCCaps2(m_pPlayer, this) & 0x20) != 0)
				continue;

			Vector vecTarget = pEnt->Center();
			Vector vecTo = vecTarget - vecSrc;
			float flDist = vecTo.Length();
			if (flDist > flMaxDist)
				continue;

			Vector vecDir = flDist > 0 ? vecTo / flDist : vecForward;
			float flDot = DotProduct(vecDir, vecForward);
			if (flDot < flMinDot)
				continue;

			float flNear = 1.0 - flDist / flMaxDist;
			if (flNear < 0)
				flNear = 0;
			else if (flNear > 1)
				flNear = 1;
			float flAim = flDot / flMinDot;
			if (flAim < 0)
				flAim = 0;
			else if (flAim > 1)
				flAim = 1;
			float flScore = flAim * 3.0 + flNear + flNear;
			if (flScore < flBest)
				continue;

			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecTarget, ignore_monsters, m_pPlayer->edict(), &tr);
			if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
			{
				flBest = flScore;
				pBest = pEnt->GetEffectedEnt();
			}
		}

		// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
		if (pBest && (pBest->GetRCCaps(m_pPlayer, this) & 0x100) != 0)
		{
			CBaseEntity* pTimer = CBaseEntity::Create("implode_timer", vecSrc, g_vecZero, NULL);
			pTimer->pev->origin = vecSrc;
			pTimer->pev->velocity = vecForward * RC2Pref(RC2P_Egon_ImploderTrackSpeed).f;
			pTimer->pev->owner = pBest->edict();
			pTimer->pev->dmg_inflictor = m_pPlayer->edict();
		}

		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(RC2_CLTE_KILL);
		WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
		WRITE_BYTE(0x2c);
		MESSAGE_END();
		return;
	}
	else
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
		PlayEmptySound();
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

// orig 0x3f370: the charge turned on yourself: a shrink timer that never ends
void CEgon::ShrinkSelf()
{
	if (g_runfuncs)
	{
		m_iChargeType = 0;
		m_fInAttack = 0;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
	}
	// Original oddity, kept: the check is against the imploder shot's costs, the charge against
	// ShrinkSelf's
	else if (m_pPlayer->m_iRidingIndex == 0 && (RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff) <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f <= m_pPlayer->m_flEnergy)
	{
		m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Egon_ImploderShrinkSelfEnergyCost).f;
		UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, 1.5);
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefirePrimary).f;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireSecondary).f;
		m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireTertiary).f;
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireQuaternary).f;
		m_pPlayer->m_iWeaponVolume = 600;
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		EGON_AMMO -= RC2Pref(RC2P_Egon_ImploderShrinkSelfAmmoCost).s & 0xffff;

		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usShrinkerImpact, 0, g_vecZero, g_vecZero, 0, 0, UTIL_PackNormal(Vector(0, 0, -1)), ENTINDEX(m_pPlayer->edict()), 0, 0);

		CBaseEntity* pTimer = NULL;
		for (pTimer = UTIL_FindEntityByClassname(NULL, "shrink_timer"); pTimer; pTimer = UTIL_FindEntityByClassname(pTimer, "shrink_timer"))
		{
			if (pTimer->pev->owner == m_pPlayer->edict())
				break;
		}
		if (!pTimer)
			pTimer = CBaseEntity::Create("shrink_timer", g_vecZero, g_vecZero, m_pPlayer->edict());
		if (!pTimer)
			return;

		pTimer->pev->owner = m_pPlayer->edict();
		pTimer->pev->dmg = -1;
		pTimer->pev->dmgtime = gpGlobals->time + 1e6;
		return;
	}
	else
	{
		PlayEmptySound();
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

// orig 0x3f620
void CEgon::WeaponIdle()
{
	ResetEmptySound();

	if (UTIL_WeaponTimeBase() < m_flTimeWeaponIdle)
		return;
	if (m_bTractorOn)
		return;

	int iAnim;
	if (RANDOM_FLOAT(0, 1) <= 0.5)
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
	else
	{
		iAnim = EGON_FIDGET1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	}
	SendWeaponAnim(iAnim);
}

// orig 0x3f6c0: the client's half of WeaponSync (on the server the readers return 0)
void CEgon::ReadWeaponState(void* pData, int iSize)
{
	bool bWasHolding = m_bHolding;

	int iFlags = RC2ReadStateByte(&pData, &iSize);
	m_bHolding = ((iFlags >> 1) & 1) != 0;
	m_bTractorOn = (iFlags & 1) != 0;
	m_flHoldDist = RC2ReadStateShort(&pData, &iSize) * 0.125;

	if (bWasHolding && !m_bTractorOn)
	{
		if ((m_pPlayer->pev->button & IN_ATTACK) != 0)
			EGON_EXTEND4_KEYS(TractorDrop);
		TractorStop();
	}
}

// orig 0x3f7d0: sync, the imploder's charge, and the beam, every frame
void CEgon::WeaponFrame()
{
	if (m_flNextSync < gpGlobals->time)
	{
		m_flNextSync = gpGlobals->time + 0.5;
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->pev);
		WRITE_BYTE(WEAPON_EGON);
		WRITE_BYTE((m_bTractorOn ? 1 : 0) | (m_bHolding ? 2 : 0));
		WRITE_SHORT((int)(m_flHoldDist * 8));
		MESSAGE_END();
	}

	if (m_flHoldDist < 32)
		m_flHoldDist = 32;
	else if (m_flHoldDist > 8192)
		m_flHoldDist = 8192;

	m_flFrameInterval = gpGlobals->time - m_flLastFrame;
	m_flLastFrame = gpGlobals->time;

	if (m_iChargeType != 0)
	{
		if (m_iChargeType == 1)
		{
			// the secondary attack during the charge shrinks yourself
			if ((m_pPlayer->m_afButtonPressed & IN_ATTACK2) != 0)
			{
				if ((RC2Pref(RC2P_Egon_ImploderShrinkSelfAmmoCost).s & 0xffff) <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderShrinkSelfEnergyCost).f <= m_pPlayer->m_flEnergy)
				{
					PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 1);
					ShrinkSelf();
					return;
				}
				PlayEmptySound();
			}

			if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
			{
				m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_ImploderChargeAmmoBurnDelay).f;
				EGON_AMMO -= RC2Pref(RC2P_Egon_ImploderChargeAmmoPerBurn).s & 0xffff;
				if (EGON_AMMO < 0)
					EGON_AMMO = 0;
			}
		}

		if (m_flChargeDone < gpGlobals->time)
		{
			if (m_iChargeType == 1)
				ImploderFire();
			return;
		}

		// still charging: hold every attack
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		return;
	}

	if (!m_bTractorOn)
		return;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;

	// the beam burns ammo, faster when it holds something
	if (UTIL_WeaponTimeBase() > m_pPlayer->m_flNextAmmoBurn)
	{
		if (m_bHolding)
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TractorBurnTimeHasTarget).f;
		else
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TractorBurnTimeNoTarget).f;

		if (EGON_AMMO < (RC2Pref(RC2P_Egon_TractorAmmoUsePerBurn).s & 0xffff))
		{
			EGON_EXTEND4_KEYS(TractorNoAmmo);
			TractorStop();
			return;
		}
		EGON_AMMO -= RC2Pref(RC2P_Egon_TractorAmmoUsePerBurn).s & 0xffff;
	}

	// let go of the button
	if ((m_pPlayer->pev->button & IN_ATTACK) == 0)
	{
		EGON_EXTEND4_KEYS(TractorOff);
		TractorStop();
		return;
	}

	if (m_bAdjusting)
	{
		PrimaryAttack();
		m_bAdjusting = false;
	}

	if (!m_bHolding || !m_hTractor)
	{
		m_bHolding = false;
		m_hTractor = NULL;
		return;
	}

	if (!TractorHeld(m_hTractor))
	{
		// knocked out by another beam
		if (TractorKnocked(m_hTractor))
		{
			EGON_EXTEND4_KEYS(TractorDrop);
			TractorStop();
			return;
		}
		TractorHeld(m_hTractor) = 0;
		TractorKnocked(m_hTractor) = 0;
		m_bHolding = false;
		m_hTractor = NULL;
		m_flNextSync = gpGlobals->time;
		return;
	}

	// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
	if ((m_hTractor->GetRCCaps(m_pPlayer, this) & 0x80) != 0 && (!m_bTargetAlive || m_hTractor->IsAlive()))
	{
		// hold it where the beam points
		Vector vecSrc = m_pPlayer->GetGunPosition();
		UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
		m_hTractor->m_vecRC2Base58 = vecSrc + gpGlobals->v_forward * m_flHoldDist;
		return;
	}

	TractorHeld(m_hTractor) = 0;
	m_bHolding = false;
	m_hTractor = NULL;
	m_flNextSync = gpGlobals->time;
}

#undef EGON_EXTEND4
#undef EGON_EXTEND4_KEYS
#undef EGON_AMMO

class CEgonAmmo : public CBasePlayerAmmo
{
	void Spawn() override // orig 0x3fef0
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override // orig 0x3ff20
	{
		PRECACHE_MODEL("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override // orig 0x3ff40
	{
		// VERIFY: RC2's GiveAmmo takes a fourth argument, 0 here (cbase.h has the SDK's three)
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_CellBoxGive).s, "uranium", RC2Pref(RC2P_Ammo_MaxCarryCells).s) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_egonclip, CEgonAmmo); // orig 0x3fe80

#endif // CLIENT_DLL
