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

#pragma once

#include "effects.h"
#include "weaponinfo.h"

class CBasePlayer;
class CBasePlayerWeapon;

void DeactivateSatchels(CBasePlayer* pOwner);

// Rocket Crowbar 2 (p3, ggrenade): from other files of the original, used by the grenades
extern unsigned short g_usExplodeEvent; // events/explode.sc, precached in W_Precache (weapons.cpp, .data 0x101afd18); name ours
void UTIL_KillEntityEffects(edict_t* pEdict); // util 0xf55b0: TE_KILLBEAM on it, and RC2's user message; name ours
int UTIL_PackNormal(Vector vecDir);			  // util 0xf5920: a direction as three signed 9-bit fractions; name ours
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, edict_t* pentIgnore, Vector vecSrc, float flRadius,
	float flDamage, float flExponent, int bitsDamageType, int iFlags); // util 0xf3900; name ours

// Contact Grenade / Timed grenade / Satchel Charge
class CGrenade : public CBaseMonster
{
public:
	void Spawn() override;

	typedef enum
	{
		SATCHEL_DETONATE = 0,
		SATCHEL_RELEASE
	} SATCHELCODE;

	static CGrenade* ShootTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time);
	static CGrenade* ShootContact(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity);
	static CGrenade* ShootSatchelCharge(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity);
	static void UseSatchelCharges(entvars_t* pevOwner, SATCHELCODE code);

	void Explode(Vector vecSrc, Vector vecAim);
	void Explode(TraceResult* pTrace, int bitsDamageType);
	void RadialExplode(TraceResult* pTrace);		  // Rocket Crowbar 2, ours
	void PlayExplosionEvent(TraceResult* pTrace);	  // Rocket Crowbar 2, ours
	void EXPORT Detonate2();						  // Rocket Crowbar 2
	void EXPORT ExplodeTouch2(CBaseEntity* pOther);	  // Rocket Crowbar 2
	void EXPORT Smoke();

	void EXPORT BounceTouch(CBaseEntity* pOther);
	void EXPORT SlideTouch(CBaseEntity* pOther);
	void EXPORT ExplodeTouch(CBaseEntity* pOther);
	void EXPORT DangerSoundThink();
	void EXPORT PreDetonate();
	void EXPORT Detonate();
	void EXPORT DetonateUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT TumbleThink();

	virtual void BounceSound();
	int BloodColor() override { return DONT_BLEED; }
	void Killed(entvars_t* pevAttacker, int iGib) override;

	bool m_fRegisteredSound; // whether or not this grenade has issued its DANGER sound to the world sound list yet.

	// p3 needs (A weapons):
	// end p3 (A weapons)

	// p3 needs (B gauss):
	// end p3 (B gauss)

	// p3 needs (C glock):
	// end p3 (C glock)

	// p3 needs (D grenades):
	// end p3 (D grenades)
};


// constant items
#define ITEM_HEALTHKIT 1
#define ITEM_ANTIDOTE 2
#define ITEM_SECURITY 3
#define ITEM_BATTERY 4

#define MAX_NORMAL_BATTERY 100


// weapon weight factors (for auto-switching)   (-1 = noswitch)
#define CROWBAR_WEIGHT 0
#define GLOCK_WEIGHT 10
#define PYTHON_WEIGHT 15
#define MP5_WEIGHT 15
#define SHOTGUN_WEIGHT 15
#define CROSSBOW_WEIGHT 10
#define RPG_WEIGHT 20
#define GAUSS_WEIGHT 20
#define EGON_WEIGHT 20
#define HORNETGUN_WEIGHT 15
#define HANDGRENADE_WEIGHT 5
#define SNARK_WEIGHT 5
#define SATCHEL_WEIGHT -10
#define TRIPMINE_WEIGHT -10


// weapon clip/carry ammo capacities
#define URANIUM_MAX_CARRY 100
#define _9MM_MAX_CARRY 250
#define _357_MAX_CARRY 36
#define BUCKSHOT_MAX_CARRY 125
#define BOLT_MAX_CARRY 50
#define ROCKET_MAX_CARRY 5
#define HANDGRENADE_MAX_CARRY 10
#define SATCHEL_MAX_CARRY 5
#define TRIPMINE_MAX_CARRY 5
#define SNARK_MAX_CARRY 15
#define HORNET_MAX_CARRY 8
#define M203_GRENADE_MAX_CARRY 10

// the maximum amount of ammo each weapon's clip can hold
#define WEAPON_NOCLIP -1

//#define CROWBAR_MAX_CLIP		WEAPON_NOCLIP
#define GLOCK_MAX_CLIP 17
#define PYTHON_MAX_CLIP 6
#define MP5_MAX_CLIP 50
#define MP5_DEFAULT_AMMO 25
#define SHOTGUN_MAX_CLIP 8
#define CROSSBOW_MAX_CLIP 5
#define RPG_MAX_CLIP 1
#define GAUSS_MAX_CLIP WEAPON_NOCLIP
#define EGON_MAX_CLIP WEAPON_NOCLIP
#define HORNETGUN_MAX_CLIP WEAPON_NOCLIP
#define HANDGRENADE_MAX_CLIP WEAPON_NOCLIP
#define SATCHEL_MAX_CLIP WEAPON_NOCLIP
#define TRIPMINE_MAX_CLIP WEAPON_NOCLIP
#define SNARK_MAX_CLIP WEAPON_NOCLIP


// the default amount of ammo that comes with each gun when it spawns
#define GLOCK_DEFAULT_GIVE 17
#define PYTHON_DEFAULT_GIVE 6
#define MP5_DEFAULT_GIVE 25
#define MP5_DEFAULT_AMMO 25
#define MP5_M203_DEFAULT_GIVE 0
#define SHOTGUN_DEFAULT_GIVE 12
#define CROSSBOW_DEFAULT_GIVE 5
#define RPG_DEFAULT_GIVE 1
#define GAUSS_DEFAULT_GIVE 20
#define EGON_DEFAULT_GIVE 20
#define HANDGRENADE_DEFAULT_GIVE 5
#define SATCHEL_DEFAULT_GIVE 1
#define TRIPMINE_DEFAULT_GIVE 1
#define SNARK_DEFAULT_GIVE 5
#define HIVEHAND_DEFAULT_GIVE 8

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE 20
#define AMMO_GLOCKCLIP_GIVE GLOCK_MAX_CLIP
#define AMMO_357BOX_GIVE PYTHON_MAX_CLIP
#define AMMO_MP5CLIP_GIVE MP5_MAX_CLIP
#define AMMO_CHAINBOX_GIVE 200
#define AMMO_M203BOX_GIVE 2
#define AMMO_BUCKSHOTBOX_GIVE 12
#define AMMO_CROSSBOWCLIP_GIVE CROSSBOW_MAX_CLIP
#define AMMO_RPGCLIP_GIVE RPG_MAX_CLIP
#define AMMO_URANIUMBOX_GIVE 20
#define AMMO_SNARKBOX_GIVE 5

// bullet types
typedef enum
{
	BULLET_NONE = 0,
	BULLET_PLAYER_9MM,		// glock
	BULLET_PLAYER_MP5,		// mp5
	BULLET_PLAYER_357,		// python
	BULLET_PLAYER_BUCKSHOT, // shotgun
	BULLET_PLAYER_CROWBAR,	// crowbar swipe

	BULLET_MONSTER_9MM,
	BULLET_MONSTER_MP5,
	BULLET_MONSTER_12MM,
} Bullet;


#define ITEM_FLAG_SELECTONEMPTY 1
#define ITEM_FLAG_NOAUTORELOAD 2
#define ITEM_FLAG_NOAUTOSWITCHEMPTY 4
#define ITEM_FLAG_LIMITINWORLD 8
#define ITEM_FLAG_EXHAUSTIBLE 16 // A player can totally exhaust their ammo supply and lose this weapon
#define ITEM_FLAG_NOAUTOSWITCHTO 32

#define WEAPON_IS_ONTARGET 0x40

typedef struct
{
	int iSlot;
	int iPosition;
	const char* pszAmmo1; // ammo 1 type
	int iMaxAmmo1;		  // max ammo 1
	const char* pszAmmo2; // ammo 2 type
	int iMaxAmmo2;		  // max ammo 2
	const char* pszName;
	int iMaxClip;
	int iId;
	int iFlags;
	int iWeight; // this value used to determine this weapon's importance in autoselection.
} ItemInfo;

struct AmmoInfo
{
	const char* pszName;
	int iId;

	/**
	*	@brief For exhaustible weapons. If provided, and the player does not have this weapon in their inventory yet it will be given to them.
	*/
	const char* WeaponName = nullptr;
};

inline int giAmmoIndex = 0;

void AddAmmoNameToAmmoRegistry(const char* szAmmoname, const char* weaponName);

// Items that the player has in their inventory that they can use
class CBasePlayerItem : public CBaseAnimating
{
public:
	void SetObjectCollisionBox() override;
	bool KeyValue(KeyValueData* pkvd) override; // Rocket Crowbar 2: "norandomize"

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	virtual bool CanAddToPlayer(CBasePlayer* player) { return true; } // return true if the item you want the item added to the player inventory

	virtual void AddToPlayer(CBasePlayer* pPlayer);
	virtual bool AddDuplicate(CBasePlayerItem* pItem) { return false; } // return true if you want your duplicate removed from world
	void EXPORT DestroyItem();
	void EXPORT DefaultTouch(CBaseEntity* pOther); // default weapon touch
	void EXPORT FallThink();					   // when an item is first spawned, this think is run to determine when the object has hit the ground.
	void EXPORT Materialize();					   // make a weapon visible and tangible
	void EXPORT AttemptToMaterialize();			   // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity* Respawn() override;			   // copy a weapon
	void FallInit();
	void CheckRespawn();
	virtual bool GetItemInfo(ItemInfo* p) { return false; } // returns false if struct not filled out
	virtual bool CanDeploy() { return true; }
	virtual bool Deploy() // returns is deploy was successful
	{
		return true;
	}

	virtual bool CanHolster() { return true; } // can this weapon be put away right now?
	virtual void Holster();
	virtual void UpdateItemInfo() {}

	virtual void ItemPreFrame() {}	// called each frame by the player PreThink
	virtual void ItemPostFrame() {} // called each frame by the player PostThink

	virtual void Drop();
	virtual void Kill();
	virtual void AttachToPlayer(CBasePlayer* pPlayer);

	virtual int PrimaryAmmoIndex() { return -1; }
	virtual int SecondaryAmmoIndex() { return -1; }

	virtual bool UpdateClientData(CBasePlayer* pPlayer) { return false; }

	virtual CBasePlayerWeapon* GetWeaponPtr() { return NULL; }

	virtual void GetWeaponData(weapon_data_t& data) {}

	virtual void SetWeaponData(const weapon_data_t& data) {}

	virtual void DecrementTimers() {}

	static inline ItemInfo ItemInfoArray[MAX_WEAPONS];
	static inline AmmoInfo AmmoInfoArray[MAX_AMMO_SLOTS];

	CBasePlayer* m_pPlayer;
	CBasePlayerItem* m_pNext;
	int m_iId; // WEAPON_???

	virtual int iItemSlot() { return 0; } // return 0 to MAX_ITEMS_SLOTS, used in hud

	int iItemPosition() { return ItemInfoArray[m_iId].iPosition; }
	const char* pszAmmo1() { return ItemInfoArray[m_iId].pszAmmo1; }
	int iMaxAmmo1() { return ItemInfoArray[m_iId].iMaxAmmo1; }
	const char* pszAmmo2() { return ItemInfoArray[m_iId].pszAmmo2; }
	int iMaxAmmo2() { return ItemInfoArray[m_iId].iMaxAmmo2; }
	const char* pszName() { return ItemInfoArray[m_iId].pszName; }
	int iMaxClip() { return ItemInfoArray[m_iId].iMaxClip; }
	int iWeight() { return ItemInfoArray[m_iId].iWeight; }
	int iFlags() { return ItemInfoArray[m_iId].iFlags; }

	// int		m_iIdPrimary;										// Unique Id for primary ammo
	// int		m_iIdSecondary;										// Unique Id for secondary ammo

	//Hack so deploy animations work when weapon prediction is enabled.
	bool m_ForceSendAnimations = false;
};


// inventory items that
class CBasePlayerWeapon : public CBasePlayerItem
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	// generic weapon versions of CBasePlayerItem calls
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool AddDuplicate(CBasePlayerItem* pItem) override;

	virtual bool ExtractAmmo(CBasePlayerWeapon* pWeapon);	  //{ return true; }			// Return true if you can add ammo to yourself when picked up
	virtual bool ExtractClipAmmo(CBasePlayerWeapon* pWeapon); // { return true; }			// Return true if you can add ammo to yourself when picked up

	// generic "shared" ammo handlers
	bool AddPrimaryAmmo(CBasePlayerWeapon* origin, int iCount, char* szName, int iMaxClip, int iMaxCarry);
	bool AddSecondaryAmmo(int iCount, char* szName, int iMaxCarry);

	void UpdateItemInfo() override {} // updates HUD state

	bool m_iPlayEmptySound;
	bool m_fFireOnEmpty; // True when the gun is empty and the player is still holding down the
						 // attack key(s)
	virtual bool PlayEmptySound();
	virtual void ResetEmptySound();

	virtual void SendWeaponAnim(int iAnim, int body = 0);

	bool CanDeploy() override;
	virtual bool IsUseable();
	bool DefaultDeploy(const char* szViewModel, const char* szWeaponModel, int iAnim, const char* szAnimExt, int body = 0);
	bool DefaultReload(int iClipSize, int iAnim, float fDelay, int body = 0);

	void ItemPostFrame() override; // called each frame by the player PostThink
	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack() {}						  // do "+ATTACK"
	virtual void SecondaryAttack() {}					  // do "+ATTACK2"
	virtual void Reload() {}							  // do "+RELOAD"
	virtual void WeaponIdle() {}						  // called when no buttons pressed
	bool UpdateClientData(CBasePlayer* pPlayer) override; // sends hud info to client dll, if things have changed
	void RetireWeapon();

	// Can't use virtual functions as think functions so this wrapper is needed.
	void EXPORT CallDoRetireWeapon()
	{
		DoRetireWeapon();
	}

	virtual void DoRetireWeapon();
	virtual bool ShouldWeaponIdle() { return false; }
	void Holster() override;
	virtual bool UseDecrement() { return false; }

	int PrimaryAmmoIndex() override;
	int SecondaryAmmoIndex() override;

	void PrintState();

	CBasePlayerWeapon* GetWeaponPtr() override { return this; }
	float GetNextAttackDelay(float delay);

	float m_flPumpTime;
	int m_fInSpecialReload;		   // Are we in the middle of a reload for the shotguns
	float m_flNextPrimaryAttack;   // soonest time ItemPostFrame will call PrimaryAttack
	float m_flNextSecondaryAttack; // soonest time ItemPostFrame will call SecondaryAttack
	float m_flTimeWeaponIdle;	   // soonest time ItemPostFrame will call WeaponIdle
	int m_iPrimaryAmmoType;		   // "primary" ammo index into players m_rgAmmo[]
	int m_iSecondaryAmmoType;	   // "secondary" ammo index into players m_rgAmmo[]
	int m_iClip;				   // number of shots left in the primary weapon clip, -1 it not used
	int m_iClientClip;			   // the last version of m_iClip sent to hud dll
	int m_iClientWeaponState;	   // the last version of the weapon state sent to hud dll (is current weapon, is on target)
	bool m_fInReload;			   // Are we in the middle of a reload;

	int m_iDefaultAmmo; // how much ammo you get when you pick up this weapon as placed by a level designer.

	// hle time creep vars
	float m_flPrevPrimaryAttack;
	float m_flLastFireTime;

	// Rocket Crowbar 2 (weapons, package p3). RC2 gave every weapon four attacks and two more
	// virtuals. In the original's vtable the attacks follow SecondaryAttack (slots 107, 108, so
	// Reload is 109 and WeaponIdle 110) and the other two follow UseDecrement (114, 115).
	// rc2.ini's [<Weapon>] keys name the four attacks Primary..Quaternary; the method names are ours.
	virtual void TertiaryAttack() {}   // slot 107, ours: "+cancel" (IN_CANCEL), timed by m_flNextTertiaryAttack
	virtual void QuaternaryAttack() {} // slot 108, ours: "+alt1" (IN_ALT1), timed by m_flNextQuaternaryAttack
	// slot 114, ours: called first thing every ItemPostFrame, also while the player is frozen
	virtual void WeaponFrame() {} // orig 0x58b20 (the default; h_cycler's place in .text)
	// slot 115, ours: each override reads its flags through the stub readers below (which return
	// 0 on the server), one per member. VERIFY: what the two arguments are and who calls it.
	virtual void ReadWeaponState(void* pData, int iSize) {} // orig 0x54c70 (the default)

	float m_flNextTertiaryAttack;	// orig +0xb0, ours: soonest time ItemPostFrame calls TertiaryAttack
	float m_flNextQuaternaryAttack; // orig +0xb4, ours: soonest time ItemPostFrame calls QuaternaryAttack
	int m_iDefaultAmmo2;			// orig +0xd8, saved: secondary ammo given on pickup
};

// Rocket Crowbar 2: mp_random's swap of an item for a random one (weapons.cpp; items.cpp's CItem
// uses it too), and the flags around it. Names ours.
void RC2RandomizeItem(CBaseEntity* pItem);
extern bool g_fRC2Randomize;
extern bool g_fRC2PlayWeaponEvents;
extern unsigned short g_usExplodeEvent;

// Rocket Crowbar 2: the readers ReadWeaponState's overrides call, as the server has them: each
// takes the two arguments by address and returns 0 (names ours). weapons.cpp
unsigned char RC2ReadStateByte(void** ppData, int* piSize);
short RC2ReadStateShort(void** ppData, int* piSize);
int RC2ReadStateInt(void** ppData, int* piSize);
float RC2ReadStateFloat(void** ppData, int* piSize);


class CBasePlayerAmmo : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT DefaultTouch(CBaseEntity* pOther); // default weapon touch
	virtual bool AddAmmo(CBaseEntity* pOther) { return true; }

	CBaseEntity* Respawn() override;
	void EXPORT Materialize();
};


inline DLL_GLOBAL short g_sModelIndexLaser; // holds the index for the laser beam
constexpr DLL_GLOBAL const char* g_pModelNameLaser = "sprites/laserbeam.spr";

inline DLL_GLOBAL short g_sModelIndexLaserDot;	 // holds the index for the laser beam dot
inline DLL_GLOBAL short g_sModelIndexFireball;	 // holds the index for the fireball
inline DLL_GLOBAL short g_sModelIndexSmoke;		 // holds the index for the smoke cloud
inline DLL_GLOBAL short g_sModelIndexWExplosion; // holds the index for the underwater explosion
inline DLL_GLOBAL short g_sModelIndexBubbles;	 // holds the index for the bubbles model
inline DLL_GLOBAL short g_sModelIndexBloodDrop;	 // holds the sprite index for blood drops
inline DLL_GLOBAL short g_sModelIndexBloodSpray; // holds the sprite index for blood spray (bigger)

extern void ClearMultiDamage();
extern void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker);
extern void AddMultiDamage(entvars_t* pevInflictor, CBaseEntity* pEntity, float flDamage, int bitsDamageType);

extern void DecalGunshot(TraceResult* pTrace, int iBulletType);
extern void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage);
extern int DamageDecal(CBaseEntity* pEntity, int bitsDamageType);
extern void RadiusDamage(Vector vecSrc, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType);

typedef struct
{
	CBaseEntity* pEntity;
	float amount;
	int type;
} MULTIDAMAGE;

inline MULTIDAMAGE gMultiDamage;


#define LOUD_GUN_VOLUME 1000
#define NORMAL_GUN_VOLUME 600
#define QUIET_GUN_VOLUME 200

#define BRIGHT_GUN_FLASH 512
#define NORMAL_GUN_FLASH 256
#define DIM_GUN_FLASH 128

#define BIG_EXPLOSION_VOLUME 2048
#define NORMAL_EXPLOSION_VOLUME 1024
#define SMALL_EXPLOSION_VOLUME 512

#define WEAPON_ACTIVITY_VOLUME 64

#define VECTOR_CONE_1DEGREES Vector(0.00873, 0.00873, 0.00873)
#define VECTOR_CONE_2DEGREES Vector(0.01745, 0.01745, 0.01745)
#define VECTOR_CONE_3DEGREES Vector(0.02618, 0.02618, 0.02618)
#define VECTOR_CONE_4DEGREES Vector(0.03490, 0.03490, 0.03490)
#define VECTOR_CONE_5DEGREES Vector(0.04362, 0.04362, 0.04362)
#define VECTOR_CONE_6DEGREES Vector(0.05234, 0.05234, 0.05234)
#define VECTOR_CONE_7DEGREES Vector(0.06105, 0.06105, 0.06105)
#define VECTOR_CONE_8DEGREES Vector(0.06976, 0.06976, 0.06976)
#define VECTOR_CONE_9DEGREES Vector(0.07846, 0.07846, 0.07846)
#define VECTOR_CONE_10DEGREES Vector(0.08716, 0.08716, 0.08716)
#define VECTOR_CONE_15DEGREES Vector(0.13053, 0.13053, 0.13053)
#define VECTOR_CONE_20DEGREES Vector(0.17365, 0.17365, 0.17365)

//=========================================================
// CWeaponBox - a single entity that can store weapons
// and ammo.
//=========================================================
class CWeaponBox : public CBaseEntity
{
	void Precache() override;
	void Spawn() override;
	void Touch(CBaseEntity* pOther) override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool IsEmpty();
	int GiveAmmo(int iCount, const char* szName, int iMax, int* pIndex = NULL);
	void SetObjectCollisionBox() override;

public:
	void EXPORT Kill();
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool HasWeapon(CBasePlayerItem* pCheckItem);
	bool PackWeapon(CBasePlayerItem* pWeapon);
	bool PackAmmo(int iszName, int iCount);

	CBasePlayerItem* m_rgpPlayerItems[MAX_ITEM_TYPES]; // one slot for each

	int m_rgiszAmmo[MAX_AMMO_SLOTS]; // ammo names
	int m_rgAmmo[MAX_AMMO_SLOTS];	 // ammo quantities

	int m_cAmmoTypes; // how many ammo types packed into this box (if packed by a level designer)
};

#ifdef CLIENT_DLL
bool bIsMultiplayer();
void LoadVModel(const char* szViewModel, CBasePlayer* m_pPlayer);
#endif

enum glock_e
{
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER
};

// Rocket Crowbar 2's glock: four attacks, all from rc2.ini's [Glock] heading. Primary launches a
// bouncing gib (glock_gib), secondary is the fart jump, tertiary a sticky gas gib
// (glock_stickygib), quaternary charges and then fires the transphasic gib (transphasic_gib).
// glock.cpp; the gibs are in rc2/glockgibs.cpp.
class CGlock : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 2; } // orig 0x54c80
	bool GetItemInfo(ItemInfo* p) override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void WeaponFrame() override;
	bool Deploy() override;
	bool CanHolster() override { return m_fInAttack == 0; } // orig 0x54ca0: not while charging
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;
	void FireTransphasic(); // name ours: the charge's shot, from WeaponFrame

	bool UseDecrement() override { return true; } // orig 0x54c90 (RC2 returns TRUE outright)

	bool m_fCharging;		// orig +0xdc, ours: the transphasic charge is running
	float m_flChargeDone;	// orig +0xe0, ours: when the charge fires

private:
	int m_iShell;			// orig +0xe8

	unsigned short m_usFireGib;		// orig +0xec: events/glockfiregib.sc (primary and tertiary)
	unsigned short m_usTPCharge;	// orig +0xee: events/glocktpcharge.sc
	unsigned short m_usTPGibFire;	// orig +0xf0: events/glocktpgibfire.sc
	unsigned short m_usFart;		// orig +0xf2: events/glockfart.sc
};

enum crowbar_e
{
	CROWBAR_IDLE = 0,
	CROWBAR_DRAW,
	CROWBAR_HOLSTER,
	CROWBAR_ATTACK1HIT,
	CROWBAR_ATTACK1MISS,
	CROWBAR_ATTACK2MISS,
	CROWBAR_ATTACK2HIT,
	CROWBAR_ATTACK3MISS,
	CROWBAR_ATTACK3HIT
};

class CCrowbar : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	// Rocket Crowbar 2's crowbar (p2, dlls/crowbar.cpp): a charged swing that shoves, a rocket,
	// the crowbarang throw, and the charge itself (fourth attack, held).
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 1; } // orig 0x30480
	bool GetItemInfo(ItemInfo* p) override;
	bool CanAddToPlayer(CBasePlayer* pPlayer) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool Deploy() override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	bool UseDecrement() override { return true; } // orig 0x30490
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;

	void EXPORT SwingAgain();
	void EXPORT Smack();
	bool Swing(bool fFirst);
	void ChargeStop(bool bSilent); // name ours

	int m_iSwing;				   // orig +0xdc
	TraceResult m_trHit;		   // orig +0xe0
	bool m_bCharging;			   // orig +0x118, name ours: the charge button is held
	float m_flCharge;			   // orig +0x11c, name ours: 0..[Crowbar] MaxCharge
	float m_flLastFrame;		   // orig +0x120, name ours
	float m_flDischargeStart;	   // orig +0x124, name ours: when a released charge starts draining
	float m_flNextChargeEvent;	   // orig +0x128, name ours
	float m_flNextSync;			   // orig +0x12c, name ours
	unsigned short m_usCrowbar;	   // orig +0x130, events/crowbar.sc
	unsigned short m_usCrowbarSwing; // orig +0x132, events/crowbarswing.sc
	unsigned short m_usCbarCharge; // orig +0x134, events/cbarcharge.sc
#else
	// halflife-updated's client prediction, until RC2's client.dll is reconstructed
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 1; }
	void EXPORT SwingAgain();
	void EXPORT Smack();
	bool GetItemInfo(ItemInfo* p) override;

	void PrimaryAttack() override;
	bool Swing(bool fFirst);
	bool Deploy() override;
	void Holster() override;
	int m_iSwing;
	TraceResult m_trHit;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usCrowbar;
#endif
};

enum python_e
{
	PYTHON_IDLE1 = 0,
	PYTHON_FIDGET,
	PYTHON_FIRE1,
	PYTHON_RELOAD,
	PYTHON_HOLSTER,
	PYTHON_DRAW,
	PYTHON_IDLE2,
	PYTHON_IDLE3
};

class CPython : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 2; }
	bool GetItemInfo(ItemInfo* p) override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usFirePython;

	// p6 needs: Rocket Crowbar 2's python (dlls/python.cpp, server only; the client keeps
	// halflife-updated's above). Size 0x140 in the original. No save table, so every member
	// name is ours.
#ifndef CLIENT_DLL
public:
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool CanHolster() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void WeaponFrame() override;

	void EXPORT DelayedHurt();

	void FireChain();																			 // ours
	void FireBall();																			 // ours
	void ChainTo(int iDepth, Vector vecFrom, edict_t* pentFrom, unsigned char* pHitBits, bool bDischarged); // ours

	float m_flReloadSoundTime;		   // orig +0xdc: 1.5 after a reload starts; WeaponIdle plays 357_reload1 once it runs out
	int m_fInZoom;					   // orig +0xe0
	EHANDLE m_hTarget;				   // orig +0xe4: what DelayedHurt hurts
	TraceResult m_trHurt;			   // orig +0xec: the primary's trace, for TraceAttack
	Vector m_vecHurtDir;			   // orig +0x124: gpGlobals->v_forward when it fired
	int m_iChargeState;				   // orig +0x130: 0 none, 2 charging the ball, 3 charging the chain
	float m_flChargeDone;			   // orig +0x134: when the charge fires (gpGlobals->time)
	unsigned short m_usPythonLightning; // orig +0x138: events/pythonlightning.sc
	unsigned short m_usPythonFireball; // orig +0x13a: events/pythonfireball.sc
	unsigned short m_usPythonChain;	   // orig +0x13c: events/pythonchain.sc
	unsigned short m_usPythonPrefire;  // orig +0x13e: events/pythonprefire.sc
#endif
};

enum mp5_e
{
	MP5_LONGIDLE = 0,
	MP5_IDLE1,
	MP5_LAUNCH,
	MP5_RELOAD,
	MP5_DEPLOY,
	MP5_FIRE1,
	MP5_FIRE2,
	MP5_FIRE3,
};

class CMP5 : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 3; }
	bool GetItemInfo(ItemInfo* p) override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;
	float m_flNextAnimTime;
	int m_iShell;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usMP5;
	unsigned short m_usMP52;
};

enum crossbow_e
{
	CROSSBOW_IDLE1 = 0, // full
	CROSSBOW_IDLE2,		// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,		// full
	CROSSBOW_FIRE2,		// reload
	CROSSBOW_FIRE3,		// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW1,		// full
	CROSSBOW_DRAW2,		// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2,	// empty
};

class CCrossbow : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	// Rocket Crowbar 2's crossbow (p2, dlls/crossbow.cpp): four attacks, zoomed or not, firing
	// impailers, tethers and exploding bolts, or delayed hitscan hurts when zoomed.
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 3; } // rc2.dll 0x2adf0, in p1's run
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void Reload() override;
	void WeaponIdle() override;
	bool UseDecrement() override { return true; } // rc2.dll 0x2ae00, in p1's run
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;

	void EXPORT DelayedHurt();

	void ZoomedSecondaryFire(); // name ours
	void ZoomedTertiaryFire();	// name ours
	void SecondaryFire();		// name ours
	CBaseEntity* FindMyTeather(); // name ours

	int m_fInZoom;			// orig +0xdc
	bool m_fTeatherOut;		// orig +0xe0, name ours: one of this player's tethers exists
	int m_iChargeType;		// orig +0xe4, name ours: 0 none, 1 zoomed secondary, 2 zoomed tertiary, 3 secondary
	float m_flChargeDone;	// orig +0xe8, name ours
	float m_flNextSync;		// orig +0xec, name ours: next gmsgWeaponSync
	TraceResult m_trHurt;	// orig +0xf0, name ours: what DelayedHurt hurts
	int m_iHurtSerial;		// orig +0x128, name ours: m_trHurt.pHit's serial number when traced
	int m_iHurtType;		// orig +0x12c, name ours: 0 zoomed primary, 1 zoomed secondary, 2 zoomed tertiary, -1 done
	Vector m_vecHurtDir;	// orig +0x130, name ours

	unsigned short m_usZoom1;		 // orig +0x13c, events/xbowzoom1.sc
	unsigned short m_usImpailer;	 // orig +0x13e, events/xbowimpailer.sc
	unsigned short m_usTeather;		 // orig +0x140, events/xbowteather.sc
	unsigned short m_usCharge;		 // orig +0x142, events/xbowcharge.sc
	unsigned short m_usCharge2;		 // orig +0x144, events/xbowcharge2.sc
	unsigned short m_usSuperZoomed;	 // orig +0x146, events/xbowsuperzoomed.sc
	unsigned short m_usExploZoomed;	 // orig +0x148, events/xbowexplozoomed.sc
	unsigned short m_usExplodeBolt;	 // orig +0x14a, events/xbowimpailer.sc again
#else
	// halflife-updated's client prediction, until RC2's client.dll is reconstructed
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 3; }
	bool GetItemInfo(ItemInfo* p) override;

	void FireBolt();
	void FireSniperBolt();
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usCrossbow;
	unsigned short m_usCrossbow2;
#endif
};

enum shotgun_e
{
	SHOTGUN_IDLE = 0,
	SHOTGUN_FIRE,
	SHOTGUN_FIRE2,
	SHOTGUN_RELOAD,
	SHOTGUN_PUMP,
	SHOTGUN_START_RELOAD,
	SHOTGUN_DRAW,
	SHOTGUN_HOLSTER,
	SHOTGUN_IDLE4,
	SHOTGUN_IDLE_DEEP
};

class CShotgun : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
#endif

	// Rocket Crowbar 2 (p7): launches what dlls/rc2/shotgunprojectiles.cpp holds. Primary a
	// scientist, secondary an HEV suit, tertiary a mini forklift, quaternary a crate it charges.
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override;
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;
	void WeaponFrame() override;
	void FireCrate(); // name ours
	int m_fInReload; //TODO: not used, remove
	float m_flNextReload;
	int m_iShell;

	bool UseDecrement() override;

	bool m_fCrateCharging;		   // orig +0xe8, name ours
	float m_flCrateChargeEnd;	   // orig +0xec, name ours
	unsigned short m_usFireSci;	   // orig +0xf0: events/shotgunsci.sc
	unsigned short m_usFireSuit;   // orig +0xf2: events/shotgunsuit.sc
	unsigned short m_usFireFL;	   // orig +0xf4: events/shotgunfl.sc
	unsigned short m_usFireCrate;  // orig +0xf6: events/firesgcrate.sc
	unsigned short m_usCrateCharge; // orig +0xf8: events/sgcratecharge.sc
};

class CLaserSpot : public CBaseEntity
{
	void Spawn() override;
	void Precache() override;

	int ObjectCaps() override { return FCAP_DONT_SAVE; }

public:
	void Suspend(float flSuspendTime);
	void EXPORT Revive();

	static CLaserSpot* CreateSpot();
};

enum rpg_e
{
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,	   // to reload
	RPG_FIRE2,	   // to empty
	RPG_HOLSTER1,  // loaded
	RPG_DRAW1,	   // loaded
	RPG_HOLSTER2,  // unloaded
	RPG_DRAW_UL,   // unloaded
	RPG_IDLE_UL,   // unloaded idle
	RPG_FIDGET_UL, // unloaded fidget
};

class CRpg : public CBasePlayerWeapon
{
public:
	// Rocket Crowbar 2: no save table of its own (CBasePlayerWeapon's)

	void Spawn() override;
	void Precache() override;
	void Reload() override;
	int iItemSlot() override { return 4; }
	bool GetItemInfo(ItemInfo* p) override;

	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void WeaponIdle() override;

	void UpdateSpot();
	bool ShouldWeaponIdle() override { return true; }

	CLaserSpot* m_pSpot;
	bool m_fSpotActive;
	int m_cActiveRockets; // how many missiles in flight from this launcher right now?

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usRpg; // orig +0xf0

public:
	// p7: RC2's CRpg is a mine launcher, one mine per attack (rpg.cpp's RC2 functions,
	// 0xb9770-0xbad10). Members from the original's offsets, names ours.
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;
	int CountMines(const char* szClassname); // name ours
	void LaunchMine(const char* szClassname, int iAmmoCost, int iMaxDeployed, const float* pflEnergyCost, const float* pflEnergyRegenDelay,
		float flLaunchVelocity, const float* pflLaunchZBoost, float flRefirePrimary, float flRefireSecondary, float flRefireTertiary, float flRefireQuaternary); // name ours
	int m_iSciMines;		 // orig +0xdc: deployed scientist_mine count, sent in WeaponSync
	int m_iSuckMines;		 // orig +0xe0: suck_mine
	int m_iSphereMines;		 // orig +0xe4: sphere_mine
	int m_iRomanCandles;	 // orig +0xe8: rocket_mine (rc2.ini's [RPG] RomanCandle keys)
	float m_flNextMineCount; // orig +0xec: gpGlobals->time at Spawn (p3); when to count the mines again (p7)
};

class CRpgRocket : public CGrenade
{
public:
	~CRpgRocket() override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void Spawn() override;
	void Precache() override;
	void EXPORT FollowThink();
	void EXPORT IgniteThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);
	static CRpgRocket* CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity* pOwner, CRpg* pLauncher);

	CRpg* GetLauncher();

	int m_iTrail;
	float m_flIgniteTime;
	EHANDLE m_hLauncher; // handle back to the launcher that fired me.
};

#define GAUSS_PRIMARY_CHARGE_VOLUME 256 // how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME 450	// how loud gauss is when discharged

enum gauss_e
{
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW
};

class CGaussBecon;

// Rocket Crowbar 2's gauss (gauss.cpp): a teleporter driven by a thrown beacon (CGaussBecon,
// dlls/rc2/gaussbecon.h). RC2 has no save table for it. Method and member names are ours unless
// the vtable slot gives a stock one.
class CGauss : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 4; } // orig 0x4fc80
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;

	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;

	void PrimaryAttack() override;	  // the beam: teleports what it hits to the beacon, or swaps places with it
	void SecondaryAttack() override;  // teleports the player to the beacon, or to a random spawn point
	void TertiaryAttack() override;	  // starts the charge whose release TertiaryFire*() does
	void QuaternaryAttack() override; // throws the beacon, or brings it back
	void WeaponIdle() override;
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;

	bool UseDecrement() override { return true; } // orig 0x4fc90 (RC2 returns 1 without the CLIENT_WEAPONS test)

#ifndef CLIENT_DLL
	CGaussBecon* FindBecon();
	bool HasBecon() { return FindBecon() != NULL; } // orig 0x4d4b0
	bool CanPlaceAt(CBaseEntity* pEnt, Vector vecSpot);
	bool TeleFragEnt(CBaseEntity* pMover, CBaseEntity* pInBox, CBaseEntity* pSkip);
	bool TeleFragBox(CBaseEntity* pMover, Vector vecSpot, CBaseEntity* pSkip);
	void TeleportEnt(CBaseEntity* pEnt, Vector vecDest, Vector vecAngles, Vector vecVelocity);
	bool BeconTeleport(CGaussBecon* pBecon, CBaseEntity* pEnt, bool bFlashArrival);
	void PrimaryDenyEvent();
	void TeleportFlash(CBaseEntity* pEnt, Vector vecOrigin, bool bArrival);
	void TertiaryFireNoBecon();
	void TertiaryFireBecon();
	void RaiseAttackTimes(float flDelay);
#endif

	bool m_blHasBecon;			  // orig +0xdc: the player's beacon is out (sent to the client with WeaponSync)
	float m_flNextBeconSync;	  // orig +0xe0: when WeaponFrame next sends m_blHasBecon
	int m_iTertiaryState;		  // orig +0xe4: 0 idle, 1 charging without a beacon, 2 with one
	float m_flTertiaryChargeEnd;  // orig +0xe8: when the tertiary charge fires
	unsigned short m_usGaussPrimary;	 // orig +0xec, events/gaussprimary.sc
	unsigned short m_usGaussBecon;		 // orig +0xee, events/gaussbecon.sc
	unsigned short m_usGaussPrimaryDeny; // orig +0xf0, events/gaussprimarydeny.sc
	unsigned short m_usTeleportFlash;	 // orig +0xf2, events/teleportflash.sc
	unsigned short m_usGaussCharge;		 // orig +0xf4, events/gausscharge.sc
	unsigned short m_usGaussThird;		 // orig +0xf6, events/gaussthird.sc
};

enum egon_e
{
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIRECYCLE,
	EGON_ALTFIREOFF,
	EGON_FIRE1,
	EGON_FIRE2,
	EGON_FIRE3,
	EGON_FIRE4,
	EGON_DRAW,
	EGON_HOLSTER
};

enum EGON_FIRESTATE
{
	FIRE_OFF,
	FIRE_CHARGE
};

enum EGON_FIREMODE
{
	FIRE_NARROW,
	FIRE_WIDE
};

#define EGON_PRIMARY_VOLUME 450
#define EGON_BEAM_SPRITE "sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE "sprites/XSpark1.spr"
#define EGON_SOUND_OFF "weapons/egon_off1.wav"
#define EGON_SOUND_RUN "weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP "weapons/egon_windup2.wav"

class CEgon : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	// Rocket Crowbar 2's egon (p2, dlls/egon.cpp): a tractor beam (primary; secondary and
	// tertiary push and pull what it holds, quaternary throws it), the shrinker (secondary),
	// the imploder (tertiary, charged; secondary while charging shrinks yourself) and the nuke
	// (quaternary).
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 4; } // orig 0x3d300
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void WeaponIdle() override;
	bool UseDecrement() override { return true; } // orig 0x3d310
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;

	void TractorStop();	 // name ours
	void ImploderFire(); // name ours
	void ShrinkSelf();	 // name ours

	EHANDLE m_hTractor;			  // orig +0xdc, name ours: what the beam holds
	bool m_bHolding;			  // orig +0xe4, name ours
	bool m_bTargetAlive;		  // orig +0xe5, name ours: it was alive when grabbed (let go if it dies)
	bool m_bTractorOn;			  // orig +0xe6, name ours
	bool m_bAdjusting;			  // orig +0xe7, name ours: the hold distance changed this frame
	float m_flHoldDist;			  // orig +0xe8, name ours: 32..8192
	int m_iChargeType;			  // orig +0xec, name ours: 1 while the imploder charges
	float m_flChargeDone;		  // orig +0xf0, name ours
	float m_flLastFrame;		  // orig +0xf4, name ours
	float m_flFrameTime;		  // orig +0xf8, name ours
	float m_flNextSync;			  // orig +0xfc, name ours: next gmsgWeaponSync
	unsigned short m_usFireShrinker;  // orig +0x100, events/egonfireshrinker.sc
	unsigned short m_usImploderCharge; // orig +0x102, events/egonimplodercharge.sc
	unsigned short m_usImploderFire;  // orig +0x104, events/egonimploderfire.sc
	unsigned short m_usFireNuke;	  // orig +0x106, events/egonfirenuke.sc
	unsigned short m_usTractor;		  // orig +0x108, events/egontractor.sc
	unsigned short m_usShrinkerImpact; // orig +0x10a, events/egonshrinkerimpact.sc
#else
	// halflife-updated's client prediction, until RC2's client.dll is reconstructed
#ifndef CLIENT_DLL
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 4; }
	bool GetItemInfo(ItemInfo* p) override;

	bool Deploy() override;
	void Holster() override;

	void UpdateEffect(const Vector& startPoint, const Vector& endPoint, float timeBlend);

	void CreateEffect();
	void DestroyEffect();

	void EndAttack();
	void Attack();
	void PrimaryAttack() override;
	bool ShouldWeaponIdle() override { return true; }
	void WeaponIdle() override;

	float m_flAmmoUseTime; // since we use < 1 point of ammo per update, we subtract ammo on a timer.

	float GetPulseInterval();
	float GetDischargeInterval();

	void Fire(const Vector& vecOrigSrc, const Vector& vecDir);

	bool HasAmmo();
	bool CanHolster();

	void UseAmmo(int count);

	CBeam* m_pBeam;
	CBeam* m_pNoise;
	CSprite* m_pSprite;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

	unsigned short m_usEgonStop;

private:
	float m_shootTime;
	EGON_FIREMODE m_fireMode;
	float m_shakeTime;
	bool m_deployed;

	unsigned short m_usEgonFire;
#endif
};

enum hgun_e
{
	HGUN_IDLE1 = 0,
	HGUN_FIDGETSWAY,
	HGUN_FIDGETSHAKE,
	HGUN_DOWN,
	HGUN_UP,
	HGUN_SHOOT
};

class CHgun : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 4; }
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	bool IsUseable() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;
	float m_flNextAnimTime;

	float m_flRechargeTime;

	int m_iFirePhase; // don't save me.

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usHornetFire;
};

enum handgrenade_e
{
	HANDGRENADE_IDLE = 0,
	HANDGRENADE_FIDGET,
	HANDGRENADE_PINPULL,
	HANDGRENADE_THROW1, // toss
	HANDGRENADE_THROW2, // medium
	HANDGRENADE_THROW3, // hard
	HANDGRENADE_HOLSTER,
	HANDGRENADE_DRAW
};

class CHandGrenade : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 5; } // orig 0x5be40
	bool GetItemInfo(ItemInfo* p) override;

	// Rocket Crowbar 2: three grenades, one per attack (the fourth is empty), and the throw
	// itself happens in WeaponFrame (handgrenade.cpp)
	void PrimaryAttack() override;	 // antifriction_grenade
	void SecondaryAttack() override; // suck_grenade
	void TertiaryAttack() override;	 // gravity_grenade
	void QuaternaryAttack() override {} // orig 0x5c240
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;
	void WeaponIdle() override;
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;
	int CountThrown(const char* pszClassname); // name ours

	bool UseDecrement() override { return true; } // orig 0x5be50: RC2 returns true on the server too

	int m_iGravDeployed;	   // orig +0xdc, ours: gravity_grenades of this player's still out
	int m_iSuckDeployed;	   // orig +0xe0, ours: suck_grenades of this player's still out
	float m_flNextCountTime; // orig +0xe4, ours: when WeaponFrame next counts them
};

enum satchel_e
{
	SATCHEL_IDLE1 = 0,
	SATCHEL_FIDGET1,
	SATCHEL_DRAW,
	SATCHEL_DROP
};

enum satchel_radio_e
{
	SATCHEL_RADIO_IDLE1 = 0,
	SATCHEL_RADIO_FIDGET1,
	SATCHEL_RADIO_DRAW,
	SATCHEL_RADIO_FIRE,
	SATCHEL_RADIO_HOLSTER
};

class CSatchel : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
#endif

	// Rocket Crowbar 2 (p7): the roach charge launcher. Primary throws a roach or arms the
	// thrown ones, secondary throws, tertiary throws a conc charge, quaternary a control forklift.
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override;
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	bool CanDeploy() override;
	bool Deploy() override;
	bool IsUseable() override;

	void Holster() override;
	void WeaponIdle() override;
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;
	void Throw();

	bool UseDecrement() override;

	float m_flNextRoachCount; // orig +0xdc, name ours
	int m_iRoachCount;		  // orig +0xe0, name ours: this player's roach_charge entities
};

enum tripmine_e
{
	TRIPMINE_IDLE1 = 0,
	TRIPMINE_IDLE2,
	TRIPMINE_ARM1,
	TRIPMINE_ARM2,
	TRIPMINE_FIDGET,
	TRIPMINE_HOLSTER,
	TRIPMINE_DRAW,
	TRIPMINE_WORLD,
	TRIPMINE_GROUND,
};

class CTripmine : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 5; } // orig 0xecae0
	bool GetItemInfo(ItemInfo* p) override;
	// orig 0xecaf0 (unchanged from SDK 2.3)
	void SetObjectCollisionBox() override
	{
		//!!!BUGBUG - fix the model!
		pev->absmin = pev->origin + Vector(-16, -16, -5);
		pev->absmax = pev->origin + Vector(16, 16, 28);
	}

	void PrimaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void WeaponIdle() override;

	// orig 0xecbb0 (RC2's build had CLIENT_WEAPONS)
	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

	// Rocket Crowbar 2 (dlls/rc2/tripmine_weapon.cpp): four mines, one per attack: monster_tripmine
	// (PrimaryAttack), bouncy_tripmine, punch_tripmine and target_tripmine, each with its [Tripmine]
	// ammo cost, limit and refire keys. Server only: the client's tripmine keeps the stock attack.
#ifndef CLIENT_DLL
	void SecondaryAttack() override;
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;

	int CountMines(const char* szClassname); // name ours
	void MineAttack(int iMine);				 // name ours: the body of all four attacks
#endif

	int m_iMinesOut[4];			// name ours. orig +0xdc..+0xe8: this player's mines of each kind in the world
	float m_flNextMinesOutSend; // name ours. orig +0xec: when WeaponFrame next sends m_iMinesOut

private:
	unsigned short m_usTripFire; // orig +0xf0
};

enum squeak_e
{
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

class CSqueak : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 5; } // orig 0xd3150
	bool GetItemInfo(ItemInfo* p) override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void WeaponIdle() override;
	bool m_fJustThrown;

	bool UseDecrement() override // orig 0xd3160 (RC2's returns true: CLIENT_WEAPONS)
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	unsigned short m_usSnarkFire; // orig +0xec, "events/snarkfire.sc"

public:
	// CSqueak's RC2 members and overrides: p8's attacks (dlls/rc2/squeak.cpp), p7's functions.
	// Names ours.
#ifndef CLIENT_DLL
	void TertiaryAttack() override;
	void QuaternaryAttack() override;
	void WeaponFrame() override;
	void ReadWeaponState(void* pData, int iSize) override;
#endif
	int m_iSnarksDeployed;			  // orig +0xe0: live monster_snarks this player owns
	int m_iCamsDeployed;			  // orig +0xe4: snark_cams this player owns
	float m_flNextWeaponSync;		  // orig +0xe8: gpGlobals->time at Spawn and Deploy (p7), then when WeaponFrame next sends WeaponSync (p8)
	unsigned short m_usSnarkCamPlace; // orig +0xee, "events/snarkcamplace.sc"
};
