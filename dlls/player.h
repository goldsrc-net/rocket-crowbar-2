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
	int m_iMoveLock;		// orig +0x648: when it clears to 0, bit 2 of m_iPhysKeyFlags goes too
	bool m_fJetpackOn;		// orig +0x650 (a byte). VERIFY: what sets it; riding clears it with phys flag 1 and a CLTE 4
	int m_iAdminLevel;			// orig +0x674: >0 may setkey/reset, >1 save/savediff/load
	int m_iShrunk;				// orig +0x68c: nonzero while shrunk (physics key "shr")
	int m_iRidingIndex;			// orig +0x69c: entity index of the player this one rides, 0 if none
	int m_iRideShoulder;		// orig +0x6a0: 1 on the left shoulder, 0 on the right
	int m_iShoulderRider[2];	// orig +0x6a4, +0x6a8: entity indices of the players riding this one's right and left shoulder (p5's array; p6: right, left)
	int m_iRC2State4ac;			// orig +0x4ac: bit flags, 0 in Spawn. VERIFY: bit 1 goes with base byte +0x66, bit 0x10 with shrinking
	float m_flEnergy;			// orig +0x758: shield and weapon energy, 100 in Spawn; riding costs 40, getting off 20, the sphere mine and roman candle their EnergyCost
	float m_flEnergyRegenTime;	// orig +0x760: gpGlobals->time + the EnergyRegenDelay whenever energy is spent (p7)
	unsigned int m_iAbilities; // orig +0x708: the attribute system's ability bits in effect (p1); bit 0x200 lets the shotgun fire under water (p7)
	bool m_fShieldOn;			// orig +0x778 (a byte): the shield deflects only while set (p7), cleared below [Shield] MinRunEnergy; riding clears it
	int m_iTransformType;		// orig +0x10f8: what the player is transformed into, 0 if not (6: the race forklift); also pev->playerclass
	bool m_bTransforming;		// orig +0x10fc (a byte), read with m_iTransformType. VERIFY: name

	void StartRiding();				   // ours: the "ride" command (dlls/rc2/playerride.cpp)
	void StopRiding(bool bQuiet);	   // ours (orig 0xb1f50)
	void SetPhysFlag10For(float flDuration); // ours: 0x9a860, bit 0x10 of m_iPhysKeyFlags, and +0x64c = time + flDuration (p7; player.cpp's package)
	void ClearPhysFlags(int iBits);	// ours, player.cpp's package (TEMP STUB in dlls/rc2/playerride.cpp)
	void RemovePhysFlag2Source(int iBits); // ours, as above

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
	float m_flNextVoteTime = 0; // orig +0x678: when this player may call the next vote (p9). VERIFY: p5 reads it as when a new client may join from the spectator view; one member either way
	class CVirtualHull* m_rgpVirtualHulls[16] = {}; // orig +0x7a8: the player's virtual hulls (dlls/rc2/virtualhull.cpp)
	float m_flNextHullUpdate = 0;					 // orig +0x7e8
	void UpdateVirtualHulls();						 // orig 0xf6fb0, from PreThink
	// p8 needs: RC2's player members (orig offsets), named here from what p8's code does with them

	// p8 needs (transforms, dlls/rc2/t*.cpp and transform.cpp): RC2's player members the transform
	// code uses, orig offsets. Names ours. The block at +0x10f8..+0x1173 is shared by all five
	// transforms and by the race (transform 6, the race forklift; dlls/rc2/racecontroller.cpp); where a slot means something different in each, it has a neutral name and the
	// comment lists every use.
	float m_flFrameInterval;		 // orig +0x668, the time a player frame took (initialized to 0.0001); scales regen and drains. VERIFY: the player package sets it
	int m_iChosenTransform;			 // orig +0x710, 0-4: the transform the player picked (assassin, houndeye, controller, bat, headcrab); m_iTransformType becomes this + 1
	CBaseEntity* m_pCam2;	 // orig +0x79c, an entity of the player's own, shown (EF_NODRAW cleared, m_iRC2Flags 8) while a transform is in progress and hidden again (flags 0) when it completes: the client draws the change with it (CLTE 0x40). VERIFY: what it is
	float m_flTransformDoneTime;	 // orig +0x1100: when the change in progress completes
	float m_flPreTransformHealth;	 // orig +0x1104
	float m_flPreTransformMaxHealth; // orig +0x1108
	Vector m_vecTransformAngles;	 // orig +0x110c: the (yaw-only) angles the change is made at
	float m_flTransformNextAttack1;	 // orig +0x1118: refire times; each transform attack pushes all four out
	float m_flTransformNextAttack2;	 // orig +0x111c
	float m_flTransformNextAttack3;	 // orig +0x1120 (assassin, houndeye: cloak / locator toggle sound time)
	float m_flTransformNextAttack4;	 // orig +0x1124 (houndeye: locator toggle time)
	float m_flTransformLastAttack;	 // orig +0x1128, -1 none
	float m_flTransformIdleTime;	 // orig +0x112c: assassin, when its idle animation was picked (time - 120 when a transform starts)
	int m_iTransformIdleAnim;		 // orig +0x1130: assassin, the idle animation picked (0 or 1)
	float m_flTransformVar1;		 // orig +0x1134: assassin gun spread; bat and controller health regen accumulator; headcrab and houndeye last yaw
	float m_flTransformVar2;		 // orig +0x1138: assassin, headcrab and houndeye health regen accumulator
	float m_flTransformVar3;		 // orig +0x113c: only cleared
	float m_flTransformVar4;		 // orig +0x1140: only cleared
	float m_flTransformTimer1;		 // orig +0x1144: assassin kick hit time; bat, controller and headcrab next pain sound; houndeye beam fire time
	float m_flTransformTimer2;		 // orig +0x1148: bat idle animation time; controller throw / charge animation time; houndeye sonic blast time
	float m_flTransformTimer3;		 // orig +0x114c: bat bite hit time; controller charge start time; houndeye sleep animation time
	float m_flTransformTimer4;		 // orig +0x1150: houndeye next pain sound
	int m_iTransformState1;			 // orig +0x1154: assassin zoom; bat roost (0 flying, 1 landed on a floor, 2 hanging from a ceiling); controller throwing; headcrab leaping; houndeye beam charging
	int m_iTransformState2;			 // orig +0x1158: assassin cloak; bat roost entity index; controller charging; houndeye locator
	int m_iTransformState3;			 // orig +0x115c: assassin kick phase (1, 2); houndeye sonic (the sound picked, 1 or 2)
	int m_iTransformState4;			 // orig +0x1160: houndeye sleep (0 awake, 1 lying down, 2 asleep, 3 getting up)
	Vector m_vecTransformVec;		 // orig +0x1164: bat, where it hangs (from an entity: in that entity's frame)
	edict_t* m_pentRaceController;	 // orig +0x1170: the race's race_controller (transform 6, p6); the other transforms only clear it

#ifndef CLIENT_DLL
	// Overrides RC2 gave CBasePlayer in transform.cpp
	int BloodColor() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	float BeamCoreDamage() override;
	float BeamRadialMaxDamage() override;
	float BeamViewDamage() override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override;

	// transform.cpp
	void Transform_SetAnimation(PLAYER_ANIM playerAnim);
	void Transform_Think();
	void Transform_FixAngle();
	void Transform_Killed();
	void Transform_DeathSound();
	void Transform_PainSound();
	void StartTransform();
	void FinishTransform();
	void StartUntransform();
	void FinishUntransform();

	// tassassin.cpp
	void TAssassin_SetAnimation(PLAYER_ANIM playerAnim);
	void TAssassin_Jump();
	void TAssassin_GunAttack();
	void TAssassin_KickStart();
	void TAssassin_KickHit();
	void TAssassin_Think();

	// tbat.cpp
	void TBat_SetAnimation(PLAYER_ANIM playerAnim);
	bool TBat_TryRoost(TraceResult* ptr);
	void TBat_BiteStart();
	void TBat_BiteHit();
	void TBat_Think();
	void TBat_DeathSound();
	void TBat_PainSound();

	// tcontroller.cpp
	void TController_SetAnimation(PLAYER_ANIM playerAnim);
	void TController_ThrowStart();
	void TController_ChargeStart();
	void TController_ChargeLaunch();
	void TController_ChargeThink();
	void TController_ThrowRelease();
	void TController_Think();
	void TController_DeathSound();
	void TController_PainSound();

	// theadcrab.cpp
	void THeadcrab_SetAnimation(PLAYER_ANIM playerAnim);
	void THeadcrab_Think();
	void THeadcrab_FixAngle();
	void THeadcrab_DeathSound();
	void THeadcrab_PainSound();

	// thoundeye.cpp
	void THoundeye_SetAnimation(PLAYER_ANIM playerAnim);
	void THoundeye_SonicStart();
	void THoundeye_BeamStart();
	void THoundeye_Jump();
	void THoundeye_SonicBlast();
	void THoundeye_BeamFire();
	void THoundeye_SendStatus();
	void THoundeye_Think();
	void THoundeye_FixAngle();
	void THoundeye_DeathSound();
	void THoundeye_PainSound();
	void THoundeye_HandleAnimEvent(MonsterEvent_t* pEvent);

	// The player package's (TEMP STUBs in p8_stubs_transform.cpp, at 0x9a800, 0x9a820, 0x9a730,
	// 0x9a780): bits of the move-lock mask at +0x648 (the first set / last cleared sets / clears
	// physinfo bit 2), and the physinfo mask at +0x644 itself
	void AddPhysFlag2Source(int bits);
	void RemoveMoveLock(int bits);
	void SetPhysFlags(int bits);
#endif

	float m_flJoinTime; // orig +0x780 (p5's name). VERIFY: p2 reads it as when the egon's nuke may fire again; one member either way

	// p1: the admin commands (dlls/rc2/maintainent.cpp), client.cpp's commands (setattr,
	// AddToFullPack, ...) and the attribute system. Names ours. VERIFY (p1): RC2's player layout
	// differs from the RC2-layout reference around +0x63c, so offsets there can't be checked
	// against it.
	bool m_fGiveBan;				  // orig +0x690 (an int there): set by the admin command giveban (p1). VERIFY: p5 reads it as \"an HLTV proxy\" (FL_PROXY at connect)
	bool m_fSuicideBan;				  // orig +0x694 (an int there): set by suicideban
	bool m_fForceSpawn;				  // orig +0x698 (an int there): set by forcespawn
	bool m_bDebugCounts;			  // orig +0x784: send DBGCounts / DBGEntInfo (p5); cleared with the admin selection (p1)
	edict_t* m_pentDebug;	  // orig +0x78c: the entity DBGEntInfo watches, the admin's selection (selectent)
	CBaseEntity* m_pCam;	  // orig +0x798: RC2's dying camera, an invisible copy of the player model (p5); removed on disconnect
	float m_flNextImpulse;			  // orig +0x684: impulses at most every 0.1 s (204 always; p5); give, givefor and drop wait until this (p1): give, givefor and drop wait until this (0.1 s after each)
	float m_flDmgScore;				  // orig +0x67c: the damage-based score (mp_scoremode 1); CBaseMonster::TakeDamage adds to it
	bool m_bNotJoined;			  // orig +0x688: still in the spectator view before joining (p5); IsActivePlayer is its opposite; 0 while the player is in play (IsActivePlayer); nonzero, AddToFullPack sends it the entity iuser2 names when +0x7a4 is 1 and iuser1 is 2. VERIFY
	int m_iClientId;			  // orig +0x66c: WON id (or entity index for a bot) of the client that last spawned here (p5); ServerDeactivate clears it (p1)
	float m_flAttrPending[10];		  // orig +0x6ac: the attribute values setattr asked for (jump, speed, attack, defend, health, extra health, armor, extra armor, energy regen, damage reflect)
	unsigned int m_iAbilitiesPending; // orig +0x6d4: setattr's ability bits
	unsigned int m_iWeaponsPending;	  // orig +0x6d8: setattr's weapon bits
	int m_iTransformPending;		  // orig +0x6dc: setattr's transform, -1 for none
	float m_flAttributes[10];		  // orig +0x6e0: the attribute values in effect
	unsigned int m_iAttrWeapons;	  // orig +0x70c: the weapon bits in effect
	bool m_fAttributesSet;			  // orig +0x754 (byte): single player lets attributes change once
	float m_flEnergyDrawDone;		  // orig +0x764: the energy draw ends then
	bool m_fDrawingEnergy;			  // orig +0x768 (byte)
	float m_flCreateHealthDone;		  // orig +0x76c: making health ends then
	bool m_fCreatingHealth;			  // orig +0x770 (byte)
	float m_flHealthCreated;			  // orig +0x774: health given so far by this creation (p5)
	int m_iRC2Player788;			  // orig +0x788: nonzero, AddToFullPack sends this player every entity. VERIFY: what sets it
	bool m_bSpecChase;			  // orig +0x7a4: the pre-join view follows a player, else spawn points (p5)
	void RC2StartObserver();		  // orig 0x9b990 (player.cpp): "spectate"/"observe". Name ours

	// p5 (the player package): RC2's members from orig +0x638 that no other package named. From
	// +0x638 on RC2 has none of SDK 2.3's status bar members and moves the gauss charge floats to
	// +0x654..+0x660 (halflife-updated's members above stay). Not saved; names ours.
	int m_iGaussBeacons;		// orig +0x638: bit 0 while one of this player's gauss beacons is out (EIcons)
	int m_iClientGaussBeacons;	// orig +0x63c: what the client last got
	byte m_iClientPlayerStat;	// orig +0x641: what PlrStat last sent of m_ucRideInfo (0xff forces a resend)
	float m_flPhysFlag10Time;	// orig +0x64c: when phys flag 0x10 goes off (-1: never)
	float m_flLastPreThink;		// orig +0x664
	int m_iInitStage;			// orig +0x670: the first frames of a new client: 3 resets the HUD, 2 the MOTD, 1 AttrConf
	float m_flClientDmgScore;	// orig +0x680: what ScoreInfo last sent of m_flDmgScore
	int m_rgiAttrInt[8];		// orig +0x714: per-player ints the rest of rc2.dll keeps through Get/SetPlayerAttrInt
	float m_rgflAttrValue[8];	// orig +0x734: per-player floats, through Get/SetPlayerAttrFloat
	int m_iClientEnergy;		// orig +0x75c
	bool m_bClientShield;		// orig +0x779: the shield's effect was sent (CLTE 0x25)
	float m_flHealthRegen;		// orig +0x77c: fractional health regenerated (or decayed over max)
	int m_iDebugSerial;			// orig +0x790: m_pentDebug's serial number when picked
	float m_flNextDebugCounts;	// orig +0x794
	int m_iSpecIndex;			// orig +0x7a0: the spawn point or player the pre-join view is on (-1: none)
	Vector m_vecClientPull;		// orig +0xdec: what PullerCtrl last sent of m_vecRC2Base58

	bool IsActivePlayer() override { return !m_bNotJoined; } // orig 0x9bd00
	bool IsViewOfPlayer(CBaseEntity* pPlayer) override;

	// RC2's own player functions (names ours)
	void StartDyingCam(bool bGibbed);		   // RC 1.9's name
	void SendDebugCounts();
	CBaseEntity* FindNextNamed(bool bReverse, const char* pszClassname);
	bool InitNewClient();
	void GoToSpectatorView();
	void ResetPhysFlags();
	void ClearPhysFlag2Sources();
	bool RidersAddPlayerItem(CBasePlayerItem* pItem);
	bool RidersHavePlayerItem(CBasePlayerItem* pItem);
	int RidersGiveAmmo(int iAmount, const char* szName, int iMax);
	void UpdateGaussBeacons();
	int GiveAmmo(int iAmount, const char* szName, int iMax, int* piGiven) override;
};

// Rocket Crowbar 2's attributes (m_flAttributes), in its order; names ours, from the rc2.ini cost
// keys AttrPoints reads
enum
{
	RC2ATTR_JUMP = 0,
	RC2ATTR_SPEED,
	RC2ATTR_ATTACK, // scales the damage the player does
	RC2ATTR_DEFEND, // scales the damage the player takes
	RC2ATTR_HEALTH,
	RC2ATTR_EXTRAHEALTH,
	RC2ATTR_ARMOR,
	RC2ATTR_EXTRAARMOR,
	RC2ATTR_ENERGYREGEN,
	RC2ATTR_DMGREFLECT,
};

#define RC2ATTRF_NODROWN 0x002	// can't drown
#define RC2ATTRF_SHIELD 0x080	// starts with the shield up
#define RC2ATTRF_LONGJUMP 0x100 // has the long jump

// Rocket Crowbar 2's per-player attribute access by entity index, as the rest of rc2.dll uses
// it (orig 0x95690-0x95970: each is a wrapper around an inner function). Names ours.
float PlayerAttr(int iAttr, int iPlayer);
int PlayerAttrFlags(int iPlayer);
void SetPlayerAttrInt(int iPlayer, int i, int iValue);
int GetPlayerAttrInt(int iPlayer, int i);
void SetPlayerAttrFloat(int iPlayer, int i, float flValue);
float GetPlayerAttrFloat(int iPlayer, int i);
float PlayerEnergy(int iPlayer);
void UsePlayerEnergy(int iPlayer, float flAmount, float flRegenDelay);
void PlayerEnergyEffect(int iPlayer, int iMode, Vector vecA, Vector vecB);
bool PlayerHasAttrFlag200(CBaseEntity* pPlayer);
int AttrPoints(int iAttr, float flValue);
float AttrDefault(int iAttr);
void PlayerHideViewEntities(int iPlayer, void (*pfnSetSolid)(int iEnt, int iSolid, float* vecOrigin));
void PlayerShowViewEntities(int iPlayer, void (*pfnSetSolid)(int iEnt, int iSolid, float* vecOrigin));

// RC2's transformation events (CBasePlayer::Precache); names ours
inline unsigned short g_usTAssassinGun;
inline unsigned short g_usTAssassinKick;
inline unsigned short g_usTHoundeyeBeam;
inline unsigned short g_usTHoundeyeSonic;
inline unsigned short g_usTControllerThrow;
inline unsigned short g_usTBatBite;

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
