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

	// Rocket Crowbar 2. RC2's CBasePlayer is 0x1174 bytes: from orig +0x638 on, after
	// m_szAnimExtention, it has none of SDK 2.3's status bar members and moves the gauss
	// charge floats to +0x654..+0x660 (halflife-updated's members above stay). None of these is
	// saved; every name is ours (reference/dossiers/p5.md has what each is read from).
	int m_iGaussBeacons;		// orig +0x638: bit 0 while one of this player's gauss beacons is out (EIcons)
	int m_iClientGaussBeacons;	// orig +0x63c: what the client last got
	byte m_iPlayerStat;			// orig +0x640: what PlrStat sends about this player; 0 on spawn and death
	byte m_iClientPlayerStat;	// orig +0x641: what it last sent (0xff forces a resend)
	int m_iPhysFlags;			// orig +0x644: mirrored into the physinfo key "phf" (%02X)
	int m_iPhysFlag2Sources;	// orig +0x648: reasons holding phys flag 2 set
	float m_flPhysFlag10Time;	// orig +0x64c: when phys flag 0x10 goes off (-1: never)
	bool m_bJetpack;			// orig +0x650: the jetpack is on (CLTE 4)
	float m_flLastPreThink;		// orig +0x664
	float m_flFrameTime;		// orig +0x668: since the last PreThink, 0.0001 to 10
	int m_iClientId;			// orig +0x66c: WON id (or entity index for a bot) of the client that last spawned here
	int m_iInitStage;			// orig +0x670: the first frames of a new client: 3 resets the HUD, 2 the MOTD, 1 AttrConf
	int m_iRC2_674;				// orig +0x674: set from the function at orig 0x1050 when a new client spawns. VERIFY: meaning
	float m_flJoinAllowedTime;	// orig +0x678: before it a new client can't join from the spectator view
	float m_flDmgScore;			// orig +0x67c: the damage-based score (mp_scoremode 1)
	float m_flClientDmgScore;	// orig +0x680: what ScoreInfo last sent of it
	float m_flNextImpulse;		// orig +0x684: impulses are taken at most every 0.1 s (204 always)
	bool m_bNotJoined;			// orig +0x688: still in the spectator view before joining; IsActivePlayer is its opposite
	int m_iCarrying;			// orig +0x68c: VERIFY: holds the player's weapons and use (no weapon switch, holster or reload); gets a virtual hull
	bool m_bIsProxy;			// orig +0x690: an HLTV proxy (FL_PROXY when it connected)
	bool m_bForceSpawn;			// orig +0x698: the forcespawn command: respawn or join now
	int m_iRidingPlayer;		// orig +0x69c: entity index of the player whose shoulders this one rides (p1 needs it)
	int m_iRidingShoulder;		// orig +0x6a0: which of his shoulders, 0 or 1
	int m_iShoulderRider[2];	// orig +0x6a4, +0x6a8: entity indices of the players riding this one's shoulders (p1 needs them)
	float m_rgflNextAttr[10];	// orig +0x6ac: the attributes chosen for the next spawn (rc2.ini [Attributes])
	int m_iNextAttrFlags;		// orig +0x6d4
	int m_iNextSpawnWeapons;	// orig +0x6d8 (1, the glock, by default)
	int m_iNextTransformKind;	// orig +0x6dc
	float m_rgflAttr[10];		// orig +0x6e0: the attributes in effect (RC2ATTR_*), applied by Spawn
	int m_iAttrFlags;			// orig +0x708: RC2ATTRF_* bits
	int m_iSpawnWeapons;		// orig +0x70c: the weapons a multiplayer spawn gives (bits: glock, 357, mp5, shotgun, crossbow, rpg, egon, gauss, hornetgun)
	int m_iTransformKind;		// orig +0x710: what this player transforms into (6: none, and no +use)
	int m_rgiAttrInt[8];		// orig +0x714: per-player ints the rest of rc2.dll keeps through Get/SetPlayerAttrInt
	float m_rgflAttrValue[8];	// orig +0x734: per-player floats, through Get/SetPlayerAttrFloat
	bool m_bRC2_754;			// orig +0x754: cleared for a new client. VERIFY: meaning
	float m_flEnergy;			// orig +0x758: 0-100
	int m_iClientEnergy;		// orig +0x75c
	float m_flEnergyRegenTime;	// orig +0x760: energy regenerates after it (-1: never)
	float m_flEnergyDrawTime;	// orig +0x764: the energy draw ends (or can be used again) then
	bool m_bEnergyDraw;			// orig +0x768: drawing energy (CLTE 6)
	float m_flCreateHealthTime; // orig +0x76c
	bool m_bCreatingHealth;		// orig +0x770
	float m_flHealthCreated;	// orig +0x774: health given so far by this creation
	bool m_bShield;				// orig +0x778: the shield is up
	bool m_bClientShield;		// orig +0x779: its effect was sent (CLTE 0x25)
	float m_flHealthRegen;		// orig +0x77c: fractional health regenerated (or decayed over max)
	float m_flJoinTime;			// orig +0x780
	bool m_bDebugCounts;		// orig +0x784: send DBGCounts / DBGEntInfo
	edict_t* m_pentDebug;		// orig +0x78c: the entity DBGEntInfo watches
	int m_iDebugSerial;			// orig +0x790: its serial number when picked
	float m_flNextDebugCounts;	// orig +0x794
	CBaseEntity* m_pCam;		// orig +0x798: RC2's dying camera (an invisible copy of the player model)
	CBaseEntity* m_pCam2;		// orig +0x79c: a second, hidden helper entity. VERIFY: role
	int m_iSpecIndex;			// orig +0x7a0: the spawn point or player the pre-join view is on (-1: none)
	bool m_bSpecChase;			// orig +0x7a4: the pre-join view follows a player (else spawn points)
	CBaseEntity* m_rgpHulls[16]; // orig +0x7a8: the player's virtual hulls (UpdateVirtualHulls, util)
	float m_flNextHullUpdate;	// orig +0x7e8
	// orig +0x7ec..+0xdeb and +0xdf8..+0x10f7: per-player tables the pm_shared side of rc2.dll
	// keeps (ResetPMTables, orig 0xaaa60); names left to that package
	int m_rgiPM7ec[64];
	int m_rgiPM8ec[64];
	Vector m_rgvecPM9ec[64];
	int m_rgiPMcec[64];
	Vector m_vecClientPull;		// orig +0xdec: what PullerCtrl last sent of m_vecRC2Base58
	Vector m_rgvecPMdf8[64];
	int m_iTransform;			// orig +0x10f8: 0 human, 1-5 transformed (TformStat), 6 a special state with no damage, use or think
	bool m_bTransforming;		// orig +0x10fc
	float m_flTransformTime;	// orig +0x1100

	bool IsActivePlayer() override { return !m_bNotJoined; } // orig 0x9bd00
	bool IsViewOfPlayer(CBaseEntity* pPlayer) override;

	// RC2's own player functions (names ours)
	void StartDyingCam(bool bGibbed);		   // RC 1.9's name
	void SendDebugCounts();
	CBaseEntity* FindNextNamed(bool bReverse, const char* pszClassname);
	bool InitNewClient();
	void GoToSpectatorView();
	void ResetPhysFlags();
	void SetPhysFlags(int iBits);
	void ClearPhysFlags(int iBits);
	void AddPhysFlag2Source(int iBits);
	void RemovePhysFlag2Source(int iBits);
	void ClearPhysFlag2Sources();
	void SetPhysFlag10For(float flTime);
	bool RidersAddPlayerItem(CBasePlayerItem* pItem);
	bool RidersHavePlayerItem(CBasePlayerItem* pItem);
	int RidersGiveAmmo(int iAmount, const char* szName, int iMax);
	void UpdateGaussBeacons();
	int GiveAmmo(int iAmount, const char* szName, int iMax, int* piGiven) override;
};

// Rocket Crowbar 2's attributes (m_rgflAttr), in its order; names ours, from the rc2.ini cost
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
