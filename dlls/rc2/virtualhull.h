/***
*
*	Rocket Crowbar 2: CVirtualHull, the per-player virtual hulls (virtualhull.cpp). Name ours.
*
****/

#pragma once

#include "cbase.h"

class CBasePlayer;

class CVirtualHull : public CBaseEntity
{
public:
	void Spawn() override;
	CBaseEntity* GetEffectedEnt() override;
	bool IsViewOfPlayer(CBaseEntity* pPlayer) override;

	bool OwnerGone();
	void EXPORT HullTouch(CBaseEntity* pOther);
	void EXPORT HullUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT HullBlocked(CBaseEntity* pOther);

	EHANDLE m_hOwner; // name ours. orig +0x70: the player
};

// At a level change: the hulls of players who aren't connected any more go. Name ours
void RemoveOrphanVirtualHulls();
// Whether pent stands (groundentity) on one of pPlayer's hulls. Name ours
bool IsOnVirtualHull(CBasePlayer* pPlayer, edict_t* pent);
