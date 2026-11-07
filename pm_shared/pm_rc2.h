/***
*
*	Rocket Crowbar 2: what RC2's player movement (pm_shared.cpp) shares with the rest of the
*	game library. All names here are ours (rc2.dll keeps none).
*
*	PM_Move reads three physinfo keys every move ("eid", "shr", "phf") and asks the game for
*	the rest by entity index through the hooks below. The server's hooks live with the
*	player and its RC2 classes; the client library has its own (pm_shared.cpp, CLIENT_DLL).
*
****/

#pragma once

// The player being moved: physinfo "eid" (.data 0x1014ddc4, initially 1).
extern int g_iPMPlayerEnt;

// The player's movement mode (.bss 0x10191598), from PM_GetPlayerMode: CBasePlayer +0x10f8.
// 1-5 follow the rc2.ini [Transform*] headings each mode reads.
enum
{
	PM_MODE_NORMAL = 0,
	PM_MODE_ASSASSIN = 1,
	PM_MODE_HOUNDEYE = 2,
	PM_MODE_CONTROLLER = 3,
	PM_MODE_BAT = 4,
	PM_MODE_HEADCRAB = 5,
	PM_MODE_RACE = 6, // VERIFY: PM_PlayerMove hands the whole move to PM_RaceMove (fork B's race path code)
};

// physinfo "phf", read with "%02X" (.bss 0x10191594). Meanings read off the movement code;
// VERIFY each against the server code that sets "phf".
enum
{
	PMF_JETPACK = 0x01,	   // jetpack physics (PM_JetpackMove), no step sounds, no ladders
	PMF_NOINPUT = 0x02,	   // movement keys and jump/duck cleared
	PMF_HELD = 0x04,	   // as NOINPUT; no gravity on foot, gravity for the bat; no charged jumps
	PMF_BOUNCE = 0x08,	   // the AF grenade's bounce physics: no friction, no ladders
	PMF_NOLADDER = 0x10,   // no ladders
	PMF_FROZEN = 0x20,	   // no movement at all
};

// The callback PM_Move hands the hide/restore hooks: set solid and origin of every physent,
// visent and moveent whose info is iInfo (rc2.dll 0xa8d10).
typedef void (*PM_SETENTFUNC)(int iInfo, int iSolid, float* vecOrigin);

// player (CBasePlayer by entity index)
float PM_GetAttributeScale(int iWhich, int iEnt);					 // rc2.dll 0x95890: 0 jump, 1 speed; 1.0 if no player
int PM_GetAbilityFlags(int iEnt);									 // rc2.dll 0x958b0: 1 wallhop, 4 jetboost
void PM_SetPlayerTimer(int iEnt, int iSlot, int iMsec);				 // rc2.dll 0x958c0: 0 wallhop/jump, 1 jetboost, 2 charge
int PM_GetPlayerTimer(int iEnt, int iSlot);							 // rc2.dll 0x958e0
float PM_GetEnergy(int iEnt);										 // rc2.dll 0x95940: 100 if no player
void PM_UseEnergy(int iEnt, float flAmount, float flRechargeDelay);	 // rc2.dll 0x95950
void PM_JetboostEffect(int iEnt, int iFlag, float* vecDir, float* vecVelocity); // rc2.dll 0x95970. VERIFY: arguments 3-4
void PM_HideViewers(int iEnt, PM_SETENTFUNC pfnSet);				 // rc2.dll 0x959b0
void PM_RestoreViewers(int iEnt, PM_SETENTFUNC pfnSet);				 // rc2.dll 0x95ab0
unsigned char* PM_GetRideInfo(int iEnt);							 // rc2.dll 0x9f320: low 7 bits ridden player, bit 7 side
void PM_GetEntityBox(int iEntIndex, float* vecOrigin, float* vecVAngle, float* vecMins, float* vecMaxs); // rc2.dll 0xb2810
int PM_GetPlayerMode(int iEnt);										 // rc2.dll 0xe3400
void PM_HideOwnedEnts(int iEnt, PM_SETENTFUNC pfnSet);				 // rc2.dll 0xf7150
void PM_RestoreOwnedEnts(int iEnt, PM_SETENTFUNC pfnSet);			 // rc2.dll 0xf7200

// transforms
int PM_GetBatPerch(int iEnt, int* piAttachInfo, float* vecOffset); // rc2.dll 0xd9a60: 0 flying, 1 walking, 2 perched
void PM_BatUnperch(int iEnt);										// rc2.dll 0xd9b00
void PM_SetHeadcrabLeap(int iEnt, int bLeaping);					// rc2.dll 0xdf260
int PM_GetHeadcrabLeap(int iEnt);									// rc2.dll 0xdf2c0
int PM_HeadcrabLeapHit(int iEnt, int iHitInfo, float flSpeed);		// rc2.dll 0xdf320: true = fly through it

// pm_math.cpp: VectorAngles with a roll from right.z and up.z (rc2.dll 0xa0260)
void VectorAnglesWithRoll(const float* forward, const float* right, const float* up, float* angles);

// pullers and the race path (dlls/rc2/pullers.cpp)
void PM_RaceMove();																  // rc2.dll 0xa9170
bool PM_GetPlayerTether(int playerEnt, int slot, float* org, float* force, float* dist); // rc2.dll 0xa9710
bool PM_GetPlayerPullVec(int playerEnt, float* vec);								  // rc2.dll 0xa9840
void PM_ClearPlayerPull(int playerEnt);											  // rc2.dll 0xa98b0
void PM_GetGlobalPuller(int idx, float* org, float* force, float* dist);			  // rc2.dll 0xa98f0
