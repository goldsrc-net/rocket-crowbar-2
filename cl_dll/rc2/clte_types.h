/***
*
*	Rocket Crowbar 2: the client effect classes the CLTE manager reaches into by type (clte.cpp
*	cases 4 and 6; 0x29, 0x3a and 0x3b are in c3_tents.h), the shield (shield_tents.cpp draws
*	it), and the factories of the types past 0x25 (c3_tents.h).
*
****/

#pragma once

#include "clte.h"
#include "rc2prefs.h"

// CLTE 3: the visible jetpack (vtable client 0x1011d89c, 0x18 bytes). clte_effects1.cpp
class CTentJetpack : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;
	void UpdatePosition();

	int m_iEntity;			// +8
	TEMPENTITY* m_pSpark;	// +0xc
	BEAM* m_pSmoke;			// +0x10
	BEAM* m_pFlame;			// +0x14
};

// CLTE 5: the energy draw's beams (vtable client 0x1011d8c8, 0x1d0 bytes). clte_effects1.cpp
class CTentEnergyDraw : public CClientTent
{
public:
	enum
	{
		NUM_BEAMS = 16
	};

	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;						 // +8
	BEAM* m_pBeams[NUM_BEAMS];			 // +0xc
	Vector m_vecBeamPoints[NUM_BEAMS][2]; // +0x4c: each beam's start and end off the player
	float m_flSparkTime;				 // +0x1cc
};

// CLTE 37 (0x25): a player's shield (vtable client 0x1011dd80, 0x6c bytes). Its Create, Update and
// KillEntity are in clte_effects4.cpp; DrawTransparent (client 0x71ad0) and Flash (client 0x727a0),
// which the shield hit (CLTE 0x26) calls, in shield_tents.cpp
class CTentShield : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;
	void Flash(int iEnt, int iStrength);

	int m_iEntity;		   // +8
	float m_flFlash;	   // +0xc: 0-0.8, raised by Flash, falls 20/3 a second
	bool m_bSweeping;	   // +0x10
	float m_flSweepWait;   // +0x14
	float m_flSweepZ;	   // +0x18
	model_s* m_pNoise;	   // +0x1c: sprites/noise1.spr
	model_s* m_pLightning; // +0x20: sprites/lgtning.spr
	float m_flYaw;		   // +0x24: turns 180 a second
	float m_flHeight;	   // +0x28: 40, 30 ducked (the ellipsoid's half height)
	float m_flWidth;	   // +0x2c: 20, 26 ducked (its radius)
	float m_flSweepMargin; // +0x30: 2 (the sweep band's half height)
	float m_flFrame;	   // +0x34: 0-3, 12 a second
	Vector m_vecOrigin;	   // +0x38
	Vector m_vecForward;   // +0x44
	Vector m_vecRight;	   // +0x50
	Vector m_vecUp;		   // +0x5c
	bool m_bVisible;	   // +0x68
};

// CLTE 0x26-0x49 (client 0x71ad0 on): their factories, and the classes of them the manager
// reaches into (0x29, 0x39, 0x3b, 0x41)
#include "c3_tents.h"
