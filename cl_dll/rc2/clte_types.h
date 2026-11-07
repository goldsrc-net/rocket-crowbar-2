/***
*
*	Rocket Crowbar 2: the client effect classes the CLTE manager reaches into by type (clte.cpp
*	cases 4, 6, 0x29, 0x3a and 0x3b), and the Create functions of the types past 0x25.
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

// c2 needs: the types of the rest of the effect file (client 0x71ad0 on, another package's) as far
// as the manager uses them; their full classes and Create functions belong there.

// CLTE 0x29 (Create client 0x991f0): two beams from the entity to a point the server moves
class CTentType29 : public CClientTent
{
public:
	int m_iEntity;	  // +8
	int m_iValue;	  // +0xc: the message's short. VERIFY: what it is
	BEAM* m_pBeam;	  // +0x10
	BEAM* m_pBeam2;	  // +0x14
	int m_iUnknown18; // +0x18. VERIFY
	Vector m_vecEnd;  // +0x1c
	int m_iLaserModel; // +0x28
};

// CLTE 0x39 (Create client 0x8a790): the tripmine's beam
class CTentType39 : public CClientTent
{
public:
	int m_iEntity;	  // +8
	int m_iUnknownC;  // +0xc. VERIFY
	int m_iUnknown10; // +0x10. VERIFY
	int m_iActive;	  // +0x14: cleared by CLTE 0x3a. VERIFY: what it is
};

// CLTE 0x3b (Create client 0x97b40): the roach charge (satchel.cpp CLTE_ROACHWALK)
class CTentRoachWalk : public CClientTent
{
public:
	int m_iEntity;			  // +8
	int m_iUnknownC;		  // +0xc. VERIFY
	int m_iUnknown10;		  // +0x10. VERIFY
	Vector m_vecOrigin;		  // +0x14
	model_s* m_pSmokeModel;	  // +0x20
	CTentTrail m_Trail;		  // +0x24
	BEAM* m_pBeam;			  // +0x4c
	bool m_bArmed;			  // +0x50
	float m_flArmTime;		  // +0x54
	float m_flArmTimeLeft;	  // +0x58
};

// CLTE 0x41 (Create client 0x8f460): its particles seek the point at +0x18 (Particle_Seek's user)
class CTentType41 : public CClientTent
{
public:
	int m_iEntity;		 // +8
	int m_iUnknown[3];	 // +0xc. VERIFY
	Vector m_vecTarget; // +0x18
};

// The Create functions of the types past 0x25 (client 0x721b0-0x991f0, another package's)
CClientTent* CLTE_CreateType26(CClientTent* pCopy);	 // client 0x721b0
CClientTent* CLTE_CreateType27b(CClientTent* pCopy); // client 0x72bf0
CClientTent* CLTE_CreateType28b(CClientTent* pCopy); // client 0x74600
CClientTent* CLTE_CreateType29(CClientTent* pCopy);	 // client 0x991f0
CClientTent* CLTE_CreateType2a(CClientTent* pCopy);	 // client 0x75a80
CClientTent* CLTE_CreateType2b(CClientTent* pCopy);	 // client 0x772d0
CClientTent* CLTE_CreateType2c(CClientTent* pCopy);	 // client 0x77cc0
CClientTent* CLTE_CreateType2d(CClientTent* pCopy);	 // client 0x788a0
CClientTent* CLTE_CreateType2e(CClientTent* pCopy);	 // client 0x7aa70
CClientTent* CLTE_CreateType2f(CClientTent* pCopy);	 // client 0x7bff0
CClientTent* CLTE_CreateType30(CClientTent* pCopy);	 // client 0x7c240
CClientTent* CLTE_CreateType31(CClientTent* pCopy);	 // client 0x7d2b0
CClientTent* CLTE_CreateType32(CClientTent* pCopy);	 // client 0x7dee0
CClientTent* CLTE_CreateType33b(CClientTent* pCopy); // client 0x814b0
CClientTent* CLTE_CreateType34(CClientTent* pCopy);	 // client 0x819a0
CClientTent* CLTE_CreateType35b(CClientTent* pCopy); // client 0x83520
CClientTent* CLTE_CreateType36b(CClientTent* pCopy); // client 0x83770
CClientTent* CLTE_CreateType37(CClientTent* pCopy);	 // client 0x83c60
CClientTent* CLTE_CreateType38(CClientTent* pCopy);	 // client 0x89a40
CClientTent* CLTE_CreateType39(CClientTent* pCopy);	 // client 0x8a790
CClientTent* CLTE_CreateType3a(CClientTent* pCopy);	 // client 0x98230
CClientTent* CLTE_CreateRoachWalk(CClientTent* pCopy); // client 0x97b40
CClientTent* CLTE_CreateType3c(CClientTent* pCopy);	 // client 0x8ae60
CClientTent* CLTE_CreateType3d(CClientTent* pCopy);	 // client 0x8b2f0
CClientTent* CLTE_CreateType3e(CClientTent* pCopy);	 // client 0x8baf0
CClientTent* CLTE_CreateType3f(CClientTent* pCopy);	 // client 0x8c5c0
CClientTent* CLTE_CreateType40(CClientTent* pCopy);	 // client 0x8d960
CClientTent* CLTE_CreateType41(CClientTent* pCopy);	 // client 0x8f460
CClientTent* CLTE_CreateType42(CClientTent* pCopy);	 // client 0x914e0
CClientTent* CLTE_CreateType43(CClientTent* pCopy);	 // client 0x91c40
CClientTent* CLTE_CreateType44(CClientTent* pCopy);	 // client 0x92ac0
CClientTent* CLTE_CreateType45(CClientTent* pCopy);	 // client 0x93220
CClientTent* CLTE_CreateType46(CClientTent* pCopy);	 // client 0x93a50
CClientTent* CLTE_CreateType47(CClientTent* pCopy);	 // client 0x940f0
CClientTent* CLTE_CreateType48(CClientTent* pCopy);	 // client 0x94790
CClientTent* CLTE_CreateType49(CClientTent* pCopy);	 // client 0x95c20
