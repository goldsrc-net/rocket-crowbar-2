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

#include "pm_materials.h"


#define PLAYER_FATAL_FALL_SPEED 1024															  // approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED 580															  // approx 20 feet
#define DAMAGE_FOR_FALL_SPEED (float)100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED) // damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED 200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.

//
// Player PHYSICS FLAGS bits
//
#define PFLAG_ONLADDER (1 << 0)
#define PFLAG_ONSWING (1 << 0)
#define PFLAG_ONTRAIN (1 << 1)
#define PFLAG_ONBARNACLE (1 << 2)
#define PFLAG_DUCKING (1 << 3)	// In the process of ducking, but totally squatted yet
#define PFLAG_USING (1 << 4)	// Using a continuous entity
#define PFLAG_OBSERVER (1 << 5) // player is locked in stationary cam mode. Spectators can move, observers can't.

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST 4 // max of 4 suit sentences queued up at any time

#define SUIT_GROUP true
#define SUIT_SENTENCE false

#define SUIT_REPEAT_OK 0
#define SUIT_NEXT_IN_30SEC 30
#define SUIT_NEXT_IN_1MIN 60
#define SUIT_NEXT_IN_5MIN 300
#define SUIT_NEXT_IN_10MIN 600
#define SUIT_NEXT_IN_30MIN 1800
#define SUIT_NEXT_IN_1HOUR 3600

#define CSUITNOREPEAT 32

#define SOUND_FLASHLIGHT_ON "items/flashlight1.wav"
#define SOUND_FLASHLIGHT_OFF "items/flashlight1.wav"

#define TEAM_NAME_LENGTH 16

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
} PLAYER_ANIM;

#define MAX_ID_RANGE 2048
#define SBAR_STRING_SIZE 128

enum sbar_data
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END,
};

#define CHAT_INTERVAL 1.0f

class CBasePlayer : public CBaseMonster
{
public:
	// Spectator camera
	void Observer_FindNextPlayer(bool bReverse);
	void Observer_HandleButtons();
	void Observer_SetMode(int iMode);
	void Observer_CheckTarget();
	void Observer_CheckProperties();
	EHANDLE m_hObserverTarget;
	float m_flNextObserverInput;
	int m_iObserverWeapon;	 // weapon of current tracked target
	int m_iObserverLastMode; // last used observer mode
	bool IsObserver() { return 0 != pev->iuser1; }

	int random_seed; // See that is shared between client & server for shared weapons code

	int m_iPlayerSound;		// the index of the sound list slot reserved for this player
	int m_iTargetVolume;	// ideal sound volume.
	int m_iWeaponVolume;	// how loud the player's weapon is right now.
	int m_iExtraSoundTypes; // additional classification for this weapon's sound
	int m_iWeaponFlash;		// brightness of the weapon flash
	float m_flStopExtraSoundTime;

	float m_flFlashLightTime; // Time until next battery draw/Recharge
	int m_iFlashBattery;	  // Flashlight Battery Draw

	int m_afButtonLast;
	int m_afButtonPressed;
	int m_afButtonReleased;

	EHANDLE m_SndLast;	   // last sound entity to modify player room type
	int m_SndRoomtype = 0; // last roomtype set by sound entity. Defaults to 0 on new maps to disable it by default.
	int m_ClientSndRoomtype;
	float m_flSndRange; // dist from player to sound entity

	float m_flFallVelocity;

	int m_rgItems[MAX_ITEMS];
	bool m_fKnownItem; // True when a new item needs to be added
	int m_fNewAmmo;	   // True when a new item has been added

	unsigned int m_afPhysicsFlags; // physics flags - set when 'normal' physics should be revisited or overriden
	float m_fNextSuicideTime;	   // the time after which the player can next use the suicide command


	// these are time-sensitive things that we keep track of
	float m_flTimeStepSound;  // when the last stepping sound was made
	float m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float m_flSwimTime;		  // how long player has been underwater
	float m_flDuckTime;		  // how long we've been ducking
	float m_flWallJumpTime;	  // how long until next walljump

	float m_flSuitUpdate;						 // when to play next suit update
	int m_rgSuitPlayList[CSUITPLAYLIST];		 // next sentencenum to play for suit update
	int m_iSuitPlayNext;						 // next sentence slot for queue storage;
	int m_rgiSuitNoRepeat[CSUITNOREPEAT];		 // suit sentence no repeat list
	float m_rgflSuitNoRepeatTime[CSUITNOREPEAT]; // how long to wait before allowing repeat
	int m_lastDamageAmount;						 // Last damage taken
	float m_tbdPrev;							 // Time-based damage timer

	float m_flgeigerRange; // range to nearest radiation source
	float m_flgeigerDelay; // delay per update of range msg to client
	int m_igeigerRangePrev;
	int m_iStepLeft;						// alternate left/right foot stepping sound
	char m_szTextureName[CBTEXTURENAMEMAX]; // current texture name we're standing on
	char m_chTextureType;					// current texture type

	int m_idrowndmg;	  // track drowning damage taken
	int m_idrownrestored; // track drowning damage restored

	int m_bitsHUDDamage; // Damage bits for the current fame. These get sent to
						 // the hude via the DAMAGE message
	bool m_fInitHUD;	 // True when deferred HUD restart msg needs to be sent
	bool m_fGameHUDInitialized;
	int m_iTrain;	// Train control position
	bool m_fWeapon; // Set this to false to force a reset of the current weapon HUD info

	EHANDLE m_pTank;		 // the tank which the player is currently controlling,  NULL if no tank
	EHANDLE m_hViewEntity;	 // The view entity being used, or null if the player is using itself as the view entity
	bool m_bResetViewEntity; //True if the player's view needs to be set back to the view entity
	float m_fDeadTime;		 // the time at which the player died  (used in PlayerDeathThink())

	bool m_fNoPlayerSound; // a debugging feature. Player makes no sound if this is true.
	bool m_fLongJump;	   // does this player have the longjump module?

	float m_tSneaking;
	int m_iUpdateTime;	  // stores the number of frame ticks before sending HUD update messages
	int m_iClientHealth;  // the health currently known by the client.  If this changes, send a new
	int m_iClientBattery; // the Battery currently known by the client.  If this changes, send a new
	int m_iHideHUD;		  // the players hud weapon info is to be hidden
	int m_iClientHideHUD;
	int m_iFOV;		  // field of view
	int m_iClientFOV; // client's known FOV
	// usable player items
	CBasePlayerItem* m_rgpPlayerItems[MAX_ITEM_TYPES];
	CBasePlayerItem* m_pActiveItem;
	CBasePlayerItem* m_pClientActiveItem; // client version of the active item
	CBasePlayerItem* m_pLastItem;

	std::uint64_t m_WeaponBits;

	//Not saved, used to update client.
	std::uint64_t m_ClientWeaponBits;

	// shared ammo slots
	int m_rgAmmo[MAX_AMMO_SLOTS];
	int m_rgAmmoLast[MAX_AMMO_SLOTS];

	Vector m_vecAutoAim;
	bool m_fOnTarget;
	int m_iDeaths;
	float m_flRespawnTimer; // used in PlayerDeathThink() to make sure players can always respawn

	int m_lastx, m_lasty; // These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	int m_nCustomSprayFrames = -1; // Custom clan logo frames for this player
	float m_flNextDecalTime;  // next time this player can spray a decal

	char m_szTeamName[TEAM_NAME_LENGTH];

	void Spawn() override;
	void Pain();

	//	void Think() override;
	virtual void Jump();
	virtual void Duck();
	virtual void PreThink();
	virtual void PostThink();
	Vector GetGunPosition() override;
	bool TakeHealth(float flHealth, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	Vector BodyTarget(const Vector& posSrc) override { return Center() + pev->view_ofs * RANDOM_FLOAT(0.5, 1.1); } // position to shoot at
	void StartSneaking() override { m_tSneaking = gpGlobals->time - 1; }
	void StopSneaking() override { m_tSneaking = gpGlobals->time + 30; }
	bool IsSneaking() override { return m_tSneaking <= gpGlobals->time; }
	bool IsAlive() override { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	bool ShouldFadeOnDeath() override { return false; }
	bool IsPlayer() override { return true; } // Spectators should return false for this, they aren't "players" as far as game logic is concerned

	bool IsNetClient() override { return true; } // Bots should return false for this, they can't receive NET messages
												 // Spectators should return true for this
	const char* TeamID() override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	void RenewItems();
	void PackDeadPlayerItems();
	void RemoveAllItems(bool removeSuit);
	bool SwitchWeapon(CBasePlayerItem* pWeapon);

	/**
	*	@brief Equips an appropriate weapon for the player if they don't have one equipped already.
	*/
	void EquipWeapon();

	void SetWeaponBit(int id);
	void ClearWeaponBit(int id);

	bool HasSuit() const;
	void SetHasSuit(bool hasSuit);

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void UpdateClientData();

	static TYPEDESCRIPTION m_playerSaveData[];

	// Player is moved across the transition by other means
	int ObjectCaps() override { return CBaseMonster::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Precache() override;
	bool IsOnLadder();
	bool FlashlightIsOn();
	void FlashlightTurnOn();
	void FlashlightTurnOff();

	void UpdatePlayerSound();
	void DeathSound() override;

	int Classify() override;
	void SetAnimation(PLAYER_ANIM playerAnim);
	void SetWeaponAnimType(const char* szExtention);
	char m_szAnimExtention[32];

	// custom player functions
	virtual void ImpulseCommands();
	void CheatImpulseCommands(int iImpulse);

	void StartDeathCam();
	void StartObserver(Vector vecPosition, Vector vecViewAngle);

	void AddPoints(int score, bool bAllowNegativeScore) override;
	void AddPointsToTeam(int score, bool bAllowNegativeScore) override;
	bool AddPlayerItem(CBasePlayerItem* pItem) override;
	bool RemovePlayerItem(CBasePlayerItem* pItem) override;
	void DropPlayerItem(char* pszItemName);
	bool HasPlayerItem(CBasePlayerItem* pCheckItem);
	bool HasNamedPlayerItem(const char* pszItemName);
	bool HasPlayerItemFromID(int nID);
	bool HasWeapons(); // do I have ANY weapons?
	void SelectPrevItem(int iItem);
	void SelectNextItem(int iItem);
	void SelectLastItem();
	void SelectItem(const char* pstr);
	void ItemPreFrame();
	void ItemPostFrame();
	void GiveNamedItem(const char* szName);
	void GiveNamedItem(const char* szName, int defaultAmmo);
	void EnableControl(bool fControl);

	int GiveAmmo(int iAmount, const char* szName, int iMax) override;
	void SendAmmoUpdate();
	void SendSingleAmmoUpdate(int ammoIndex);

private:
	void InternalSendSingleAmmoUpdate(int ammoIndex);

public:
	void WaterMove();
	void EXPORT PlayerDeathThink();
	void PlayerUse();

	void CheckSuitUpdate();
	void SetSuitUpdate(const char* name, bool fgroup, int iNoRepeat);
	void UpdateGeigerCounter();
	void CheckTimeBasedDamage();

	bool FBecomeProne() override;
	void BarnacleVictimBitten(entvars_t* pevBarnacle) override;
	void BarnacleVictimReleased() override;
	static int GetAmmoIndex(const char* psz);
	int AmmoInventory(int iAmmoIndex);
	int Illumination() override;

	void ResetAutoaim();
	Vector GetAutoaimVector(float flDelta);
	Vector AutoaimDeflection(Vector& vecSrc, float flDist, float flDelta);

	void ForceClientDllUpdate(); // Forces all client .dll specific data to be resent to client.

	void DeathMessage(entvars_t* pevKiller);

	void SetCustomDecalFrames(int nFrames);
	int GetCustomDecalFrames();

	void TabulateAmmo();

	float m_flStartCharge;
	float m_flAmmoStartCharge;
	float m_flPlayAftershock;
	float m_flNextAmmoBurn; // while charging, when to absorb another unit of player's ammo?

	//Player ID
	void InitStatusBar();
	void UpdateStatusBar();
	int m_izSBarState[SBAR_END];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char m_SbarString0[SBAR_STRING_SIZE];
	char m_SbarString1[SBAR_STRING_SIZE];

	float m_flNextChatTime;

	void SetPrefsFromUserinfo(char* infobuffer);

	int m_iAutoWepSwitch;

	bool m_bRestored;

	//True if the player is currently spawning.
	bool m_bIsSpawning = false;

	// p6 needs: RC2's player members the rc2.ini console command and the shoulder riding
	// (dlls/rc2prefs.cpp, dlls/rc2/playerride.cpp) use. Not saved, so every name is ours.
	unsigned char m_ucRideInfo; // orig +0x640: 0, or the ridden player's index (|0x80 on the left shoulder); 0 in Spawn
	int m_iPhysKeyFlags;		// orig +0x644: sent to the engine as physics key "phf" ("%02X")
	int m_iPhysKeyFlags2;		// orig +0x648: when it clears to 0, bit 2 of m_iPhysKeyFlags goes too
	bool m_bRC2State650;		// orig +0x650 (a byte). VERIFY: what sets it; riding clears it with phys flag 1 and a CLTE 4
	int m_iAdminLevel;			// orig +0x674: >0 may setkey/reset, >1 save/savediff/load
	int m_iShrunk;				// orig +0x68c: nonzero while shrunk (physics key "shr")
	int m_iRidingIndex;			// orig +0x69c: entity index of the player this one rides, 0 if none
	int m_iRideShoulder;		// orig +0x6a0: 1 on the left shoulder, 0 on the right
	int m_iRightRider;			// orig +0x6a4: entity index of the player on the right shoulder
	int m_iLeftRider;			// orig +0x6a8: entity index of the player on the left shoulder
	int m_iRC2State4ac;			// orig +0x4ac: bit flags, 0 in Spawn. VERIFY: bit 1 goes with base byte +0x66, bit 0x10 with shrinking
	float m_flEnergy;			// orig +0x758: shield and weapon energy, 100 in Spawn; riding costs 40, getting off 20, the sphere mine and roman candle their EnergyCost
	float m_flEnergyRegenTime;	// orig +0x760: gpGlobals->time + the EnergyRegenDelay whenever energy is spent (p7)
	int m_iRC2PlayerFlags708;	// orig +0x708: flags; bit 0x200 lets the shotgun fire under water (p7)
	bool m_fShieldOn;			// orig +0x778 (a byte): the shield deflects only while set (p7), cleared below [Shield] MinRunEnergy; riding clears it
	int m_iTransformType;		// orig +0x10f8: what the player is transformed into, 0 if not (6: the race forklift); also pev->playerclass
	bool m_bTransforming;		// orig +0x10fc (a byte), read with m_iTransformType. VERIFY: name
	// the race (race_controller's Use, dlls/rc2/racecontroller.cpp): what the race sets up
	float m_flRaceSavedHealth;		  // orig +0x1104: pev->health when the race began
	float m_flRaceSavedMaxHealth;	  // orig +0x1108: pev->max_health then; the untransform restores it
	float m_flRaceTimes[4];			  // orig +0x1118: time + 0.25 at the start. VERIFY: what they time
	float m_flRace1128;				  // orig +0x1128: -1 at the start. VERIFY
	float m_flRace112c;				  // orig +0x112c: time - 120 at the start. VERIFY
	int m_iRace1130;				  // orig +0x1130: 0 at the start. VERIFY
	float m_flRaceTrackRadius;		  // orig +0x1134: the controller's track_radius (pm_shared reads it)
	int m_iRace1138[3];				  // orig +0x1138: 0 at the start. VERIFY
	float m_flRace1144[4];			  // orig +0x1144: -1 at the start. VERIFY
	int m_iRacePoint;				  // orig +0x1154: index of the race_path point the player heads for
	int m_iRace1158[3];				  // orig +0x1158: 0 at the start. VERIFY
	Vector m_vecRace1164;			  // orig +0x1164: zero at the start. VERIFY
	edict_t* m_pentRaceController;	  // orig +0x1170: the race_controller

	void StartRiding();				   // ours: the "ride" command (dlls/rc2/playerride.cpp)
	void StopRiding(bool bQuiet);	   // ours (orig 0xb1f50)
	void SetPhysicsFlag10(float flDuration); // ours: 0x9a860, bit 0x10 of m_iPhysKeyFlags, and +0x64c = time + flDuration (p7; player.cpp's package)
	void ClearPhysKeyFlags(int iBits);	// ours, player.cpp's package (TEMP STUB in dlls/rc2/playerride.cpp)
	void ClearPhysKeyFlags2(int iBits); // ours, as above

	// p6 needs (fork B): the player's 64 tethers, what pulls this player towards an entity
	// (the suck mine; dlls/rc2/pullers.cpp). pm_shared reads them through PM_GetPlayerTether;
	// the client gets each change in a PullerCtrl message. Not saved, names ours.
	float m_flTetherForce[64];	   // orig +0x7ec: 0 = slot free
	float m_flTetherLength[64];	   // orig +0x8ec: slack before it pulls
	Vector m_vecTetherOffset[64];  // orig +0x9ec: from the entity's origin (absolute when no entity)
	int m_iTetherEnt[64];		   // orig +0xcec: entity index, -1 (or < 1) for none
	Vector m_vecSentPullPoint[64]; // orig +0xdf8: g_vecPullPointOrg as last sent to this client
	int AddTether(int iEnt, Vector vecOffset, float flForce, float flLength); // ours (dlls/rc2/pullers.cpp)
	void RemoveTether(int iSlot);											  // ours
	void ResetTethers();													  // ours
	void SendPullPoints();													  // ours

	// RC2's CBasePlayer has 28 more bytes between m_szAnimExtention (+0x618, as stock) and
	// m_flStartCharge: from there on the stock members sit 0x1c further than in SDK 2.3 (the
	// gauss reads m_flPlayAftershock at +0x65c and m_flNextAmmoBurn at +0x660, stock +0x640 and
	// +0x644). Found by p3; the player package (p5) places them.

	// p9: the votes and the virtual hulls. Names ours.
	float m_flNextVoteTime = 0; // orig +0x678: when this player may call the next vote
	class CVirtualHull* m_rgpVirtualHulls[16] = {}; // orig +0x7a8: the player's virtual hulls (dlls/rc2/virtualhull.cpp)
	float m_flNextHullUpdate = 0;					 // orig +0x7e8
	void UpdateVirtualHulls();						 // orig 0xf6fb0, from PreThink
};

inline void CBasePlayer::SetWeaponBit(int id)
{
	m_WeaponBits |= 1ULL << id;
	// Mirror to pev->weapons so server-side consumers (Metamod plugins,
	// the engine's cdata->iWeaponBits delivery to vanilla clients, any
	// HL mod code that pre-dates m_WeaponBits) see the player's actual
	// weapon mask. Bits 32..63 don't fit in pev->weapons (int32), but
	// today only WEAPON_SUIT (31) and weapons 0..30 are used.
	if (id < 32)
		pev->weapons |= 1 << id;
}

inline void CBasePlayer::ClearWeaponBit(int id)
{
	m_WeaponBits &= ~(1ULL << id);
	if (id < 32)
		pev->weapons &= ~(1 << id);
}

inline bool CBasePlayer::HasSuit() const
{
	return (m_WeaponBits & (1ULL << WEAPON_SUIT)) != 0;
}

inline void CBasePlayer::SetHasSuit(bool hasSuit)
{
	if (hasSuit)
	{
		SetWeaponBit(WEAPON_SUIT);
	}
	else
	{
		ClearWeaponBit(WEAPON_SUIT);
	}
}

#define AUTOAIM_2DEGREES 0.0348994967025
#define AUTOAIM_5DEGREES 0.08715574274766
#define AUTOAIM_8DEGREES 0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669

inline bool gInitHUD = true;
inline bool gEvilImpulse101 = false;
inline bool giPrecacheGrunt = false;

/**
*	@brief Display the game title if this key is set
*/
inline DLL_GLOBAL bool gDisplayTitle = false;
inline DLL_GLOBAL unsigned int g_ulModelIndexPlayer = 0;
inline DLL_GLOBAL CBaseEntity* g_pLastSpawn = nullptr;
