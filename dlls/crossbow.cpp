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
#include "hltv.h"
#ifndef CLIENT_DLL
#include "rc2/shield.h"
#include "rc2/crossbowbolts.h"
#include "explode.h"
#endif

#ifdef CLIENT_DLL

// halflife-updated's crossbow, kept for the client's weapon prediction until RC2's client.dll
// is reconstructed. The server's crossbow is Rocket Crowbar 2's, below.

#ifndef CLIENT_DLL
#define BOLT_AIR_VELOCITY 2000
#define BOLT_WATER_VELOCITY 1000

// UNDONE: Save/restore this?  Don't forget to set classname and LINK_ENTITY_TO_CLASS()
//
// OVERLOADS SOME ENTVARS:
//
// speed - the ideal magnitude of my velocity
class CCrossbowBolt : public CBaseEntity
{
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	void EXPORT BubbleThink();
	void EXPORT BoltTouch(CBaseEntity* pOther);
	void EXPORT ExplodeThink();

	int m_iTrail;

public:
	static CCrossbowBolt* BoltCreate();
};
LINK_ENTITY_TO_CLASS(crossbow_bolt, CCrossbowBolt);

CCrossbowBolt* CCrossbowBolt::BoltCreate()
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt* pBolt = GetClassPtr((CCrossbowBolt*)NULL);
	pBolt->pev->classname = MAKE_STRING("bolt");
	pBolt->Spawn();

	return pBolt;
}

void CCrossbowBolt::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	pev->gravity = 0.5;

	SET_MODEL(ENT(pev), "models/crossbow_bolt.mdl");

	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	SetTouch(&CCrossbowBolt::BoltTouch);
	SetThink(&CCrossbowBolt::BubbleThink);
	pev->nextthink = gpGlobals->time + 0.2;
}


void CCrossbowBolt::Precache()
{
	PRECACHE_MODEL("models/crossbow_bolt.mdl");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("fvox/beep.wav");
	m_iTrail = PRECACHE_MODEL("sprites/streak.spr");
}


int CCrossbowBolt::Classify()
{
	return CLASS_NONE;
}

void CCrossbowBolt::BoltTouch(CBaseEntity* pOther)
{
	SetTouch(NULL);
	SetThink(NULL);

	if (0 != pOther->pev->takedamage)
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		entvars_t* pevOwner;

		pevOwner = VARS(pev->owner);

		// UNDONE: this needs to call TraceAttack instead
		ClearMultiDamage();

		if (pOther->IsPlayer())
		{
			pOther->TraceAttack(pevOwner, gSkillData.plrDmgCrossbowClient, pev->velocity.Normalize(), &tr, DMG_NEVERGIB);
		}
		else
		{
			pOther->TraceAttack(pevOwner, gSkillData.plrDmgCrossbowMonster, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB);
		}

		ApplyMultiDamage(pev, pevOwner);

		pev->velocity = Vector(0, 0, 0);
		// play body "thwack" sound
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM);
			break;
		}

		if (!g_pGameRules->IsMultiplayer())
		{
			Killed(pev, GIB_NEVER);
		}
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0, 7));

		SetThink(&CCrossbowBolt::SUB_Remove);
		pev->nextthink = gpGlobals->time; // this will get changed below if the bolt is allowed to stick in what it hit.

		if (FClassnameIs(pOther->pev, "worldspawn"))
		{
			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = pev->velocity.Normalize();
			UTIL_SetOrigin(pev, pev->origin - vecDir * 12);
			pev->angles = UTIL_VecToAngles(vecDir);
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_FLY;
			pev->velocity = Vector(0, 0, 0);
			pev->avelocity.z = 0;
			pev->angles.z = RANDOM_LONG(0, 360);
			pev->nextthink = gpGlobals->time + 10.0;
		}

		if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		{
			UTIL_Sparks(pev->origin);
		}
	}

	if (g_pGameRules->IsMultiplayer())
	{
		SetThink(&CCrossbowBolt::ExplodeThink);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CCrossbowBolt::BubbleThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->waterlevel == 0)
		return;

	UTIL_BubbleTrail(pev->origin - pev->velocity * 0.1, pev->origin, 1);
}

void CCrossbowBolt::ExplodeThink()
{
	int iContents = UTIL_PointContents(pev->origin);
	int iScale;

	pev->dmg = 40;
	iScale = 10;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	if (iContents != CONTENTS_WATER)
	{
		WRITE_SHORT(g_sModelIndexFireball);
	}
	else
	{
		WRITE_SHORT(g_sModelIndexWExplosion);
	}
	WRITE_BYTE(iScale); // scale * 10
	WRITE_BYTE(15);		// framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	entvars_t* pevOwner;

	if (pev->owner)
		pevOwner = VARS(pev->owner);
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	::RadiusDamage(pev->origin, pev, pevOwner, pev->dmg, 128, CLASS_NONE, DMG_BLAST | DMG_ALWAYSGIB);

	UTIL_Remove(this);
}
#endif

LINK_ENTITY_TO_CLASS(weapon_crossbow, CCrossbow); // orig 0x2ad30 (p1's range; the rest of crossbow.cpp is p2's)

void CCrossbow::Spawn()
{
	Precache();
	m_iId = WEAPON_CROSSBOW;
	SET_MODEL(ENT(pev), "models/w_crossbow.mdl");

	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;

	FallInit(); // get ready to fall down.
}

void CCrossbow::Precache()
{
	PRECACHE_MODEL("models/w_crossbow.mdl");
	PRECACHE_MODEL("models/v_crossbow.mdl");
	PRECACHE_MODEL("models/p_crossbow.mdl");

	PRECACHE_SOUND("weapons/xbow_fire1.wav");
	PRECACHE_SOUND("weapons/xbow_reload1.wav");

	UTIL_PrecacheOther("crossbow_bolt");

	m_usCrossbow = PRECACHE_EVENT(1, "events/crossbow1.sc");
	m_usCrossbow2 = PRECACHE_EVENT(1, "events/crossbow2.sc");
}


bool CCrossbow::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = BOLT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CROSSBOW_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = WEAPON_CROSSBOW;
	p->iFlags = 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return true;
}


bool CCrossbow::Deploy()
{
	if (0 != m_iClip)
		return DefaultDeploy("models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW1, "bow");
	return DefaultDeploy("models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW2, "bow");
}

void CCrossbow::Holster()
{
	m_fInReload = false; // cancel any reload in progress.

	if (m_pPlayer->m_iFOV != 0)
	{
		SecondaryAttack();
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if (0 != m_iClip)
		SendWeaponAnim(CROSSBOW_HOLSTER1);
	else
		SendWeaponAnim(CROSSBOW_HOLSTER2);
}

void CCrossbow::PrimaryAttack()
{

#ifdef CLIENT_DLL
	if (m_pPlayer->m_iFOV != 0 && bIsMultiplayer())
#else
	if (m_pPlayer->m_iFOV != 0 && g_pGameRules->IsMultiplayer())
#endif
	{
		FireSniperBolt();
		return;
	}

	FireBolt();
}

// this function only gets called in multiplayer
void CCrossbow::FireSniperBolt()
{
	m_flNextPrimaryAttack = GetNextAttackDelay(0.75);

	if (m_iClip == 0)
	{
		PlayEmptySound();
		return;
	}

	TraceResult tr;

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_iClip--;

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usCrossbow2, 0.0, g_vecZero, g_vecZero, 0, 0, m_iClip, 0, 0, 0);

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(anglesAim);
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

#ifndef CLIENT_DLL
	if (0 != tr.pHit->v.takedamage)
	{
		ClearMultiDamage();
		CBaseEntity::Instance(tr.pHit)->TraceAttack(m_pPlayer->pev, 120, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB);
		ApplyMultiDamage(pev, m_pPlayer->pev);
	}
#endif
}

void CCrossbow::FireBolt()
{
	TraceResult tr;

	if (m_iClip == 0)
	{
		PlayEmptySound();
		return;
	}

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usCrossbow, 0.0, g_vecZero, g_vecZero, 0, 0, m_iClip, 0, 0, 0);

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(anglesAim);

	anglesAim.x = -anglesAim.x;
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

#ifndef CLIENT_DLL
	CCrossbowBolt* pBolt = CCrossbowBolt::BoltCreate();
	pBolt->pev->origin = vecSrc;
	pBolt->pev->angles = anglesAim;
	pBolt->pev->owner = m_pPlayer->edict();

	if (m_pPlayer->pev->waterlevel == 3)
	{
		pBolt->pev->velocity = vecDir * BOLT_WATER_VELOCITY;
		pBolt->pev->speed = BOLT_WATER_VELOCITY;
	}
	else
	{
		pBolt->pev->velocity = vecDir * BOLT_AIR_VELOCITY;
		pBolt->pev->speed = BOLT_AIR_VELOCITY;
	}
	pBolt->pev->avelocity.z = 10;
#endif

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.75);

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.75;

	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
}


void CCrossbow::SecondaryAttack()
{
	if (m_pPlayer->m_iFOV != 0)
	{
		m_pPlayer->m_iFOV = 0; // 0 means reset to default fov
	}
	else if (m_pPlayer->m_iFOV != 20)
	{
		m_pPlayer->m_iFOV = 20;
	}

	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
}


void CCrossbow::Reload()
{
	if (m_pPlayer->ammo_bolts <= 0)
		return;

	if (m_pPlayer->m_iFOV != 0)
	{
		SecondaryAttack();
	}

	if (DefaultReload(5, CROSSBOW_RELOAD, 4.5))
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0, 0xF));
	}
}


void CCrossbow::WeaponIdle()
{
	m_pPlayer->GetAutoaimVector(AUTOAIM_2DEGREES); // get the autoaim vector but ignore it;  used for autoaim crosshair in DM

	ResetEmptySound();

	if (m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
	{
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
		if (flRand <= 0.75)
		{
			if (0 != m_iClip)
			{
				SendWeaponAnim(CROSSBOW_IDLE1);
			}
			else
			{
				SendWeaponAnim(CROSSBOW_IDLE2);
			}
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		}
		else
		{
			if (0 != m_iClip)
			{
				SendWeaponAnim(CROSSBOW_FIDGET1);
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 30.0;
			}
			else
			{
				SendWeaponAnim(CROSSBOW_FIDGET2);
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 80.0 / 30.0;
			}
		}
	}
}



class CCrossbowAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_crossbow_clip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PRECACHE_MODEL("models/w_crossbow_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_CROSSBOWCLIP_GIVE, "bolts", BOLT_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_crossbow, CCrossbowAmmo);

#else // !CLIENT_DLL

//=========================================================
// Rocket Crowbar 2's crossbow. Primary fires an impailer (zoomed: a hitscan hurt that also
// sprays the player's logo where it lands), secondary charges an exploding bolt (zoomed: a
// charged hitscan hurt), tertiary fires or recalls a tether (zoomed: a charged hitscan hurt
// that explodes), quaternary toggles the zoom. Every number is rc2.ini [Crossbow]/[Ammo].
// Reconstructed from rc2.dll (reference/dossiers/p2.md).
//=========================================================


// SDK 2.2's spectator director message, as rc2.dll sends it: svc 50, then the event, the two
// entities and priority 8 or 9 with DRC_FLAG_DRAMATIC
#define RC2_SVC_DIRECTOR 50
#define RC2_DRC_EVENT 3

static void DirectorEvent(edict_t* pVictim, edict_t* pAttacker, int iFlags) // name ours
{
	MESSAGE_BEGIN(MSG_SPEC, RC2_SVC_DIRECTOR);
	WRITE_BYTE(RC2_DRC_EVENT);
	WRITE_SHORT(ENTINDEX(pVictim));
	WRITE_SHORT(ENTINDEX(pAttacker));
	WRITE_LONG(iFlags);
	MESSAGE_END();
}

LINK_ENTITY_TO_CLASS(weapon_crossbow, CCrossbow); // rc2.dll 0x2ad30, in p1's run

// orig 0x2ae10
void CCrossbow::Spawn()
{
	Precache();
	m_iId = WEAPON_CROSSBOW;
	SET_MODEL(ENT(pev), "models/w_crossbow.mdl");
	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_CrossbowPickupGive).s;
	m_flNextSync = gpGlobals->time;
	FallInit();
	m_iChargeType = 0;
	m_fInAttack = 0;
}

// orig 0x2ae70. RC2 sends gmsgWeapPickup here once the base has added the weapon;
// halflife-updated sends that same message from CBasePlayer::AddPlayerItem, so it isn't
// repeated here (one message either way).
void CCrossbow::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

// orig 0x2aed0
void CCrossbow::Precache()
{
	PRECACHE_MODEL("models/w_crossbow.mdl");
	PRECACHE_MODEL("models/v_crossbow.mdl");
	PRECACHE_MODEL("models/p_crossbow.mdl");

	PRECACHE_SOUND("weapons/xbow_fire1.wav");
	PRECACHE_SOUND("weapons/xbow_reload1.wav");
	PRECACHE_SOUND("ambience/labdrone2.wav");
	PRECACHE_MODEL("sprites/xbeam5.spr");

	UTIL_PrecacheOther("impailer");
	UTIL_PrecacheOther("xbow_teather");
	UTIL_PrecacheOther("explode_bolt");

	m_usZoom1 = PRECACHE_EVENT(1, "events/xbowzoom1.sc");
	m_usImpailer = PRECACHE_EVENT(1, "events/xbowimpailer.sc");
	m_usTeather = PRECACHE_EVENT(1, "events/xbowteather.sc");
	m_usCharge = PRECACHE_EVENT(1, "events/xbowcharge.sc");
	m_usCharge2 = PRECACHE_EVENT(1, "events/xbowcharge2.sc");
	m_usSuperZoomed = PRECACHE_EVENT(1, "events/xbowsuperzoomed.sc");
	m_usExploZoomed = PRECACHE_EVENT(1, "events/xbowexplozoomed.sc");
	m_usExplodeBolt = PRECACHE_EVENT(1, "events/xbowimpailer.sc");
}

// orig 0x2aff0
bool CCrossbow::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarryCrossbowBolts).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RC2Pref(RC2P_Ammo_CrossbowClipSize).s;
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = WEAPON_CROSSBOW;
	p->iFlags = m_fTeatherOut ? ITEM_FLAG_SELECTONEMPTY : 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return true;
}

// orig 0x2b080
bool CCrossbow::Deploy()
{
	m_iChargeType = 0;
	m_fInAttack = 0;
	if (m_iClip)
		return DefaultDeploy("models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW1, "bow");
	return DefaultDeploy("models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW2, "bow");
}

// orig 0x2b0c0
void CCrossbow::Holster()
{
	m_fInReload = false; // cancel any reload in progress.
	m_iChargeType = 0;
	m_fInAttack = 0;

	if (m_fInZoom)
		QuaternaryAttack(); // unzoom

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if (m_iClip)
		SendWeaponAnim(CROSSBOW_HOLSTER1);
	else
		SendWeaponAnim(CROSSBOW_HOLSTER2);
}

// orig 0x2b140: no holstering mid-charge
bool CCrossbow::CanHolster()
{
	return m_fInAttack == 0;
}

// orig 0x2b150. The zoomed shots' damage, a frame after the shot (so the director message
// goes first): to what m_trHurt hit, if it is still the same entity.
void CCrossbow::DelayedHurt()
{
	SetThink(NULL);

	if (!UTIL_IsValidEntity(m_trHurt.pHit))
		return;
	if (m_trHurt.pHit->serialnumber != m_iHurtSerial)
		return;

	if (m_iHurtType == 0)
	{
		CBaseEntity* pEnt = CBaseEntity::Instance(m_trHurt.pHit);
		if (pEnt)
		{
			CBaseEntity* pEffected = pEnt->GetEffectedEnt();
			float flDamage = ShieldDeflect(pEnt->GetEffectedEnt(), pEffected->ZoomedPrimaryDamage(), 0.2, m_trHurt.vecEndPos, m_vecHurtDir, NULL, NULL, 10);
			if (flDamage > 0)
			{
				UTIL_BloodDrips(m_trHurt.vecEndPos, m_vecHurtDir, pEnt->GetEffectedEnt()->BloodColor(), 60);
				ClearMultiDamage();
				pEnt->GetEffectedEnt()->TraceAttack(m_pPlayer->pev, flDamage, m_vecHurtDir, &m_trHurt, DMG_BULLET | DMG_NEVERGIB);
				ApplyMultiDamage(pev, m_pPlayer->pev);
			}
		}
	}
	else if (m_iHurtType == 1)
	{
		CBaseEntity* pEnt = CBaseEntity::Instance(m_trHurt.pHit);
		if (pEnt)
		{
			ClearMultiDamage();
			CBaseEntity* pDamage = pEnt->GetEffectedEnt();
			CBaseEntity* pVictim = pEnt->GetEffectedEnt();
			pVictim->TraceAttack(m_pPlayer->pev, pDamage->ZoomedSecondaryDamage(), m_vecHurtDir, &m_trHurt, DMG_BULLET | DMG_NEVERGIB);
			ApplyMultiDamage(pev, m_pPlayer->pev);
		}
	}
	else if (m_iHurtType == 2)
	{
		CBaseEntity* pEnt = CBaseEntity::Instance(m_trHurt.pHit);

		Vector vecBlast = m_trHurt.vecEndPos - m_vecHurtDir * 16;
		float flDamage = RC2Pref(RC2P_Crossbow_ZoomedTertiaryDirectDamage).f;

		if (pEnt)
		{
			flDamage = ShieldDeflect(pEnt->GetEffectedEnt(), RC2Pref(RC2P_Crossbow_ZoomedTertiaryDirectDamage).f, RC2Pref(RC2P_Crossbow_ZoomedTertiaryShieldFraction).f,
				m_trHurt.vecEndPos, m_vecHurtDir, &vecBlast, NULL, 8);
			if (flDamage > 0)
			{
				ClearMultiDamage();
				pEnt->GetEffectedEnt()->TraceAttack(m_pPlayer->pev, flDamage, m_vecHurtDir, &m_trHurt, DMG_BULLET | DMG_NEVERGIB);
				ApplyMultiDamage(pev, m_pPlayer->pev);
			}
		}

		// the blast goes off unless a shield took the whole hit
		if (flDamage > 0)
			ExplodeAt(vecBlast, m_vecHurtDir, RC2Pref(RC2P_Crossbow_ZoomedTertiaryBlastDamage).f, "xbow_snipeexplo", m_pPlayer->pev, true);
	}

	m_iHurtType = -1;
}

// orig 0x2b500
void CCrossbow::PrimaryAttack()
{
	if (m_fInZoom)
	{
		// zoomed: a hitscan shot, hurt a frame later by DelayedHurt
		if (m_iClip < (RC2Pref(RC2P_Crossbow_ZoomedPrimaryAmmoCost).c & 0xff))
		{
			PlayEmptySound();
			return;
		}

		m_pPlayer->m_iWeaponVolume = 200;
		m_iClip -= RC2Pref(RC2P_Crossbow_ZoomedPrimaryAmmoCost).c & 0xff;

		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usZoom1, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, m_iClip != 0, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);

		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_ZoomedPrimaryRefirePrimary).f);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_ZoomedPrimaryRefireSecondary).f);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_ZoomedPrimaryRefireTertiary).f);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_ZoomedPrimaryRefireQuaternary).f);

		Vector anglesAim = m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle;
		UTIL_MakeVectors(anglesAim);
		Vector vecSrc = m_pPlayer->GetGunPosition();
		Vector vecDir = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

		if (tr.pHit && UTIL_IsValidEntity(tr.pHit))
		{
			CBaseEntity* pEnt = CBaseEntity::Instance(tr.pHit);
			if (pEnt && pEnt->GetEffectedEnt()->pev->takedamage != DAMAGE_NO)
			{
				DirectorEvent(pEnt->edict(), m_pPlayer->edict(), 8 | DRC_FLAG_DRAMATIC);

				SetThink(&CCrossbow::DelayedHurt);
				pev->nextthink = gpGlobals->time + 0.01;

				m_iHurtType = 0;
				m_trHurt = tr;
				m_iHurtSerial = tr.pHit->serialnumber;
				m_vecHurtDir = vecDir;

				// on through what it hurt, for the decal
				UTIL_TraceLine(tr.vecEndPos, tr.vecEndPos + vecDir * 256, ignore_monsters, tr.pHit, &tr);
			}
		}

		if (tr.flFraction < 1.0)
		{
			if (tr.pHit && UTIL_IsValidEntity(tr.pHit))
			{
				// Original bug, kept: a null instance here is called through anyway (dossier)
				CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
				if (!pHit->IsBSPModel())
					return;
			}

			if (m_pPlayer->GetCustomDecalFrames() != -1)
			{
				// the player's own spray where the shot lands
				MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
				WRITE_BYTE(TE_PLAYERDECAL);
				WRITE_BYTE(ENTINDEX(m_pPlayer->edict()));
				WRITE_COORD(tr.vecEndPos.x);
				WRITE_COORD(tr.vecEndPos.y);
				WRITE_COORD(tr.vecEndPos.z);
				WRITE_SHORT(ENTINDEX(tr.pHit));
				WRITE_BYTE(0);
				MESSAGE_END();
			}
		}
		return;
	}

	// an impailer
	if (m_iClip < (RC2Pref(RC2P_Crossbow_ImpailerAmmoCost).c & 0xff))
	{
		PlayEmptySound();
		return;
	}

	m_iClip -= RC2Pref(RC2P_Crossbow_ImpailerAmmoCost).c & 0xff;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = 600;
	m_pPlayer->m_iWeaponFlash = 256;

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImpailer, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, m_iClip != 0, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_PrimaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_PrimaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_PrimaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_PrimaryRefireQuaternary).f);

	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);

	CBaseEntity* pBolt = CBaseEntity::Create("impailer", vecSrc, UTIL_VecToAngles(gpGlobals->v_forward), m_pPlayer->edict());
	pBolt->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Crossbow_ImpailerLaunchVelocity).f;
	pBolt->pev->movedir = pBolt->pev->velocity;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pBolt->pev->owner = pControlled;
	}

	// recoil
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * RC2Pref(RC2P_Crossbow_ImpailerRecoilVelocity).f;
}

// orig 0x2bce0: start charging an exploding bolt (zoomed: a charged hitscan shot); WeaponFrame
// burns the clip while it charges and fires when it is done
void CCrossbow::SecondaryAttack()
{
	if (!m_fInZoom)
	{
		if ((RC2Pref(RC2P_Crossbow_SecondaryAmmoUse).c & 0xff) <= m_iClip)
		{
			if (g_fRC2PlayWeaponEvents)
				PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCharge2, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
			m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
			m_iChargeType = 3;
			m_fInAttack = 3;
			m_flChargeDone = RC2Pref(RC2P_Crossbow_SecondaryChargeTime).f + gpGlobals->time;
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
			return;
		}
	}
	else
	{
		if ((RC2Pref(RC2P_Crossbow_ZoomedSecondaryAmmoUse).c & 0xff) <= m_iClip)
		{
			if (g_fRC2PlayWeaponEvents)
				PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCharge, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
			m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
			m_iChargeType = 1;
			m_fInAttack = 1;
			m_flChargeDone = RC2Pref(RC2P_Crossbow_ZoomedSecondaryChargeTime).f + gpGlobals->time;
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
			return;
		}
	}

	if (!m_fFireOnEmpty)
	{
		Reload();
		return;
	}
	PlayEmptySound();
}

// orig 0x2bec0: a tether, or recall the one out (zoomed: start charging an exploding hitscan shot)
void CCrossbow::TertiaryAttack()
{
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_TertiaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_TertiaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_TertiaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_TertiaryRefireQuaternary).f);

	if (m_fInZoom)
	{
		if (m_iClip < (RC2Pref(RC2P_Crossbow_ZoomedTertiaryAmmoUse).c & 0xff))
		{
			if (!m_fFireOnEmpty)
			{
				Reload();
				return;
			}
			PlayEmptySound();
			return;
		}

		// Original oddity, kept: bparam1 is 1 here, unlike the other charge events
		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCharge, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		m_iChargeType = 2;
		m_fInAttack = 2;
		m_flChargeDone = RC2Pref(RC2P_Crossbow_ZoomedTertiaryChargeTime).f + gpGlobals->time;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
		return;
	}

	CBaseEntity* pTeather = FindMyTeather();
	if (pTeather)
	{
		static_cast<CXBowTeather*>(pTeather)->DieThink();
		m_fTeatherOut = false;
		return;
	}

	m_fTeatherOut = false;

	if (m_pPlayer->m_iRidingIndex != 0)
	{
		PlayEmptySound();
		return;
	}

	if (m_iClip < (RC2Pref(RC2P_Crossbow_TeatherAmmoCost).c & 0xff))
	{
		PlayEmptySound();
		return;
	}

	m_iClip -= RC2Pref(RC2P_Crossbow_TeatherAmmoCost).c & 0xff;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = 600;
	m_pPlayer->m_iWeaponFlash = 256;

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usTeather, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, m_iClip != 0, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);

	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);

	CBaseEntity* pBolt = CBaseEntity::Create("xbow_teather", vecSrc, UTIL_VecToAngles(gpGlobals->v_forward), m_pPlayer->edict());
	pBolt->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Crossbow_TeatherLaunchVelocity).f;

	// Original oddity, kept: the check above already returned when this is set
	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pBolt->pev->owner = pControlled;
	}

	m_fTeatherOut = true;
}

// orig 0x2c250: zoom in or out
void CCrossbow::QuaternaryAttack()
{
	if (m_pPlayer->pev->fov == 0)
	{
		if (m_pPlayer->pev->fov != RC2Pref(RC2P_Crossbow_ZoomedFOV).f)
		{
			m_pPlayer->m_iFOV = (int)RC2Pref(RC2P_Crossbow_ZoomedFOV).f;
			m_pPlayer->pev->fov = m_pPlayer->m_iFOV;
			m_fInZoom = 1;
		}
	}
	else
	{
		m_pPlayer->m_iFOV = 0;
		m_pPlayer->pev->fov = 0;
		m_fInZoom = 0;
	}

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_QuaternaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_QuaternaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_QuaternaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Crossbow_QuaternaryRefireQuaternary).f);
}

// The zoomed charged shots (orig 0x2c350 and 0x2c680 are the same code with their own keys,
// event, director priority and hurt type).
#define CROSSBOW_ZOOMED_CHARGE_FIRE(Keys, usEvent, iDirectorFlags, iHurt)                                                                    \
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_##Keys##RefirePrimary).f;                                          \
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_##Keys##RefireSecondary).f;                                      \
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_##Keys##RefireTertiary).f;                                        \
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_##Keys##RefireQuaternary).f;                                    \
	m_pPlayer->m_iWeaponVolume = 200;                                                                                                        \
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);                                                                                                 \
	if (g_fRC2PlayWeaponEvents)                                                                                                                          \
	{                                                                                                                                        \
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), usEvent, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, m_iClip != 0, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0); \
		m_iChargeType = 0;                                                                                                                   \
		m_fInAttack = 0;                                                                                                                     \
	}                                                                                                                                        \
	Vector anglesAim = m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle;                                                                 \
	UTIL_MakeVectors(anglesAim);                                                                                                             \
	Vector vecSrc = m_pPlayer->GetGunPosition();                                                                                             \
	Vector vecDir = gpGlobals->v_forward;                                                                                                    \
	TraceResult tr;                                                                                                                          \
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);                                          \
	if (!tr.pHit || !UTIL_IsValidEntity(tr.pHit))                                                                                            \
		return;                                                                                                                              \
	CBaseEntity* pEnt = CBaseEntity::Instance(tr.pHit);                                                                                      \
	if (!pEnt || pEnt->GetEffectedEnt()->pev->takedamage == DAMAGE_NO)                                                                       \
		return;                                                                                                                              \
	DirectorEvent(pEnt->edict(), m_pPlayer->edict(), iDirectorFlags);                                                                        \
	SetThink(&CCrossbow::DelayedHurt);                                                                                                       \
	pev->nextthink = gpGlobals->time + 0.01;                                                                                                 \
	m_trHurt = tr;                                                                                                                           \
	m_iHurtType = iHurt;                                                                                                                     \
	m_iHurtSerial = tr.pHit->serialnumber;                                                                                                   \
	m_vecHurtDir = vecDir;

// orig 0x2c350
void CCrossbow::ZoomedSecondaryFire()
{
	CROSSBOW_ZOOMED_CHARGE_FIRE(ZoomedSecondary, m_usSuperZoomed, 9 | DRC_FLAG_DRAMATIC, 1)
}

// orig 0x2c680
void CCrossbow::ZoomedTertiaryFire()
{
	CROSSBOW_ZOOMED_CHARGE_FIRE(ZoomedTertiary, m_usExploZoomed, 8 | DRC_FLAG_DRAMATIC, 2)
}

#undef CROSSBOW_ZOOMED_CHARGE_FIRE

// orig 0x2c9b0: the charged exploding bolt
void CCrossbow::SecondaryFire()
{
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_SecondaryRefirePrimary).f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_SecondaryRefireSecondary).f;
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_SecondaryRefireTertiary).f;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_SecondaryRefireQuaternary).f;
	m_pPlayer->m_iWeaponVolume = 200;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	if (g_fRC2PlayWeaponEvents)
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usExplodeBolt, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, m_iClip != 0, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
		m_iChargeType = 0;
		m_fInAttack = 0;
	}

	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);

	CBaseEntity* pBolt = CBaseEntity::Create("explode_bolt", vecSrc, UTIL_VecToAngles(gpGlobals->v_forward), m_pPlayer->edict());
	pBolt->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Crossbow_ExplodeBoltLaunchVelocity).f;
	pBolt->pev->movedir = pBolt->pev->velocity;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pBolt->pev->owner = pControlled;
	}
}

// orig 0x2cbb0: this player's tether, if one is out (the tether keeps its player in dmg_inflictor)
CBaseEntity* CCrossbow::FindMyTeather()
{
	if (!m_pPlayer)
		return NULL;

	for (CBaseEntity* pTeather = UTIL_FindEntityByClassname(NULL, "xbow_teather"); pTeather; pTeather = UTIL_FindEntityByClassname(pTeather, "xbow_teather"))
	{
		if (UTIL_IsValidEntity(pTeather->pev->dmg_inflictor) && pTeather->pev->dmg_inflictor == m_pPlayer->edict())
			return pTeather;
	}
	return NULL;
}

// orig 0x2cc30
void CCrossbow::Reload()
{
	if (m_pPlayer->ammo_bolts > 0 && m_fInAttack == 0)
	{
		m_pPlayer->m_iFOV = 0;
		m_pPlayer->pev->fov = 0;
		m_fInZoom = 0;

		if (DefaultReload(RC2Pref(RC2P_Ammo_CrossbowClipSize).s, CROSSBOW_RELOAD, RC2Pref(RC2P_Crossbow_ReloadTime).f))
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0, 0xF));
	}
}

// orig 0x2ccf0
void CCrossbow::WeaponIdle()
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
	{
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
		if (flRand <= 0.75)
		{
			if (m_iClip)
				SendWeaponAnim(CROSSBOW_IDLE1);
			else
				SendWeaponAnim(CROSSBOW_IDLE2);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		}
		else
		{
			if (m_iClip)
			{
				SendWeaponAnim(CROSSBOW_FIDGET1);
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 30.0;
			}
			else
			{
				SendWeaponAnim(CROSSBOW_FIDGET2);
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 80.0 / 30.0;
			}
		}
	}
}

// orig 0x2cdf0: tell the client the zoom and tether state every quarter second, and run a charge
void CCrossbow::WeaponFrame()
{
	if (m_flNextSync < gpGlobals->time)
	{
		m_fTeatherOut = FindMyTeather() != NULL;

		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->pev);
		WRITE_BYTE(WEAPON_CROSSBOW);
		WRITE_BYTE((m_fInZoom ? 2 : 0) | (m_fTeatherOut ? 1 : 0));
		MESSAGE_END();

		m_flNextSync = gpGlobals->time + 0.25;
	}

	if (m_iChargeType == 0)
		return;

	// a charge eats the clip a round at a time, the whole cost over the charge time
	if (m_iChargeType == 1)
	{
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_ZoomedSecondaryChargeTime).f / (float)(RC2Pref(RC2P_Crossbow_ZoomedSecondaryAmmoUse).c & 0xff);
			if (--m_iClip < 0)
				m_iClip = 0;
		}
	}
	else if (m_iChargeType == 2)
	{
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_ZoomedTertiaryChargeTime).f / (float)(RC2Pref(RC2P_Crossbow_ZoomedTertiaryAmmoUse).c & 0xff);
			if (--m_iClip < 0)
				m_iClip = 0;
		}
	}
	else if (m_iChargeType == 3)
	{
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Crossbow_SecondaryChargeTime).f / (float)(RC2Pref(RC2P_Crossbow_SecondaryAmmoUse).c & 0xff);
			if (--m_iClip < 0)
				m_iClip = 0;
		}
	}

	if (m_flChargeDone < gpGlobals->time)
	{
		if (m_iChargeType == 1)
			ZoomedSecondaryFire();
		else if (m_iChargeType == 2)
			ZoomedTertiaryFire();
		else if (m_iChargeType == 3)
			SecondaryFire();
		return;
	}

	// still charging: hold every attack
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

// orig 0x2d040: the client's half of WeaponSync (on the server the reader returns 0)
void CCrossbow::ReadWeaponState(void* pData, int iSize)
{
	int iFlags = RC2ReadStateByte(&pData, &iSize);
	m_fTeatherOut = (iFlags & 1) != 0;
	m_fInZoom = (iFlags >> 1) & 1;
}

class CCrossbowAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_crossbow_clip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PRECACHE_MODEL("models/w_crossbow_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		// VERIFY: RC2's GiveAmmo takes a fourth argument, 0 here (cbase.h has the SDK's three)
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_CrossbowSpareClipGive).s, "bolts", RC2Pref(RC2P_Ammo_MaxCarryCrossbowBolts).s) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
// orig 0x2d0f0 (Spawn), orig 0x2d120 (Precache), orig 0x2d140 (AddAmmo), above
LINK_ENTITY_TO_CLASS(ammo_crossbow, CCrossbowAmmo); // orig 0x2d080


#endif // CLIENT_DLL
