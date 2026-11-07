/***
*
*	Rocket Crowbar 2: CVirtualHull, the per-player virtual hulls (virtualhull.cpp). Name ours.
*
****/

#pragma once

#include "cbase.h"

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
