/***
*
*	Rocket Crowbar 2: pullers.h (file name ours). What pulls an entity or a player: the
*	players' tethers (CBasePlayer::AddTether, the suck mine), the 64 global pull points (the
*	gravity and suck grenades), an entity's tractor point (the egon's tractor), and every
*	entity's GetPullOrg (CBaseEntity slot 58). dlls/rc2/pullers.cpp; the entity side is
*	CBaseEntity::ApplyExternalForce (cbase.cpp).
*
*	The PM_ functions are what the server's pm_shared calls: they read server state, so the
*	client build of pm_shared needs its own versions (client.dll has them, from the
*	PullerCtrl message).
*
****/

#pragma once

class CBaseEntity;

// pm_shared's own globals (pm_shared.cpp)
extern int g_iPMPlayerEnt; // orig .data 0x1014ddc4 (initially 1): entity index of the player pm moves

void PM_RaceMove();																	 // rc2.dll 0xa9170
bool PM_GetPlayerTether(int iPlayer, int iSlot, float* pvecOrigin, float* pflForce, float* pflLength); // rc2.dll 0xa9710
bool PM_GetPlayerPullVec(int iEnt, float* pvecPoint);								 // rc2.dll 0xa9840
void PM_ClearPlayerPull(int iEnt);													 // rc2.dll 0xa98b0
void PM_GetGlobalPuller(int iPoint, float* pvecOrigin, float* pflRadius, float* pflPlayerForce); // rc2.dll 0xa98f0

// the 64 global pull points; a point is free while its origin is g_vecZero
extern Vector g_vecPullPointOrg[64];		 // orig .bss 0x1915c8
extern float g_flPullPointMonsterForce[64]; // orig .bss 0x1918c8: for entities (ApplyExternalForce)
extern float g_flPullPointPlayerForce[64];	 // orig .bss 0x1919c8: for players (pm_shared, sent to the client)
extern float g_flPullPointRadius[64];		 // orig .bss 0x191ac8

int AddPullPoint(Vector vecOrigin, float flRadius, float flPlayerForce, float flMonsterForce); // -1 when full
void RemovePullPoint(int iPoint);

Vector RC2_GetPullForce(CBaseEntity* pEnt); // the sum of every GetPullOrg on pEnt
