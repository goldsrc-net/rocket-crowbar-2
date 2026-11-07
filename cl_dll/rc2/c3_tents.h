/***
*
*	Rocket Crowbar 2 client: c3's client temp effects, for c2's CLTE dispatcher (orig 0x510b0).
*
*	One factory per CLTE type: it takes a free slot of g_pClientTents, constructs the effect
*	(which reads the rest of the message) and returns it, or null when every slot is taken; the
*	dispatcher then sets m_iType. pCopy, which the dispatcher always passes as null, makes a copy
*	of that effect instead. Names ours.
*
*	The classes here are the four the framework reaches into itself: the dispatcher writes
*	members of 0x29, 0x39 and 0x3b (clte.cpp), and Particle_Seek reads 0x41's origin through
*	its user pointer (clte_particles.cpp). The other effect classes stay in their own files.
*
****/

#pragma once

#include "clte.h"

CClientTent* RC2_NewShieldHitTent(const CClientTent* pCopy); // CLTE 0x26, shield_tents.cpp: a hit on a player's shield
CClientTent* RC2_NewShrinkerGlowTent(const CClientTent* pCopy); // CLTE 0x27, shrinker_tents.cpp: the shrinker shot in flight
CClientTent* RC2_NewShrinkTent(const CClientTent* pCopy); // CLTE 0x28, shrinker_tents.cpp: an entity shrinking or growing back
CClientTent* RC2_NewEgonTractorTent(const CClientTent* pCopy); // CLTE 0x29, egontractor_tent.cpp: the egon's tractor beams
CClientTent* RC2_NewNukeTent(const CClientTent* pCopy); // CLTE 0x2a, nuke_tents.cpp: the nuclear missile
CClientTent* RC2_NewBlastSphereTent(const CClientTent* pCopy); // CLTE 0x2b, blastsphere_tent.cpp: a blast sphere with a view wobble
CClientTent* RC2_NewImploderChargeTent(const CClientTent* pCopy); // CLTE 0x2c, implodercharge_tent.cpp: the imploder charging
CClientTent* RC2_NewImploderShotTent(const CClientTent* pCopy); // CLTE 0x2d, imploder_tents.cpp: the imploder's homing sparks
CClientTent* RC2_NewImplodeTent(const CClientTent* pCopy); // CLTE 0x2e, imploder_tents.cpp: an implosion
CClientTent* RC2_NewFrozenTent(const CClientTent* pCopy); // CLTE 0x2f, freeze_tents.cpp: an entity frozen
CClientTent* RC2_NewFreezeRayTent(const CClientTent* pCopy); // CLTE 0x30, freeze_tents.cpp: a freeze ray flare
CClientTent* RC2_NewFreezeTrailTent(const CClientTent* pCopy); // CLTE 0x31, freeze_tents.cpp: the freeze grenade's trail
CClientTent* RC2_NewGravGrenadeTent(const CClientTent* pCopy); // CLTE 0x32, gravgrenade_tents.cpp: the gravity grenade
CClientTent* RC2_NewBlueShockRingTent(const CClientTent* pCopy); // CLTE 0x33, shockring_tents.cpp: a blue shock ring
CClientTent* RC2_NewSuckGrenadeTent(const CClientTent* pCopy); // CLTE 0x34, suckgrenade_tents.cpp: the suck grenade
CClientTent* RC2_NewAntiFrictionTent(const CClientTent* pCopy); // CLTE 0x35, antifriction_tents.cpp: an entity made frictionless
CClientTent* RC2_NewPurpleShockRingTent(const CClientTent* pCopy); // CLTE 0x36, shockring_tents.cpp: a purple shock ring
CClientTent* RC2_NewUltimaTent(const CClientTent* pCopy); // CLTE 0x37, ultima_tent.cpp: the ultima grenade
CClientTent* RC2_NewEnergyBlastTent(const CClientTent* pCopy); // CLTE 0x38, energyblast_tents.cpp: an energy blast
CClientTent* RC2_NewTripmineChargeTent(const CClientTent* pCopy); // CLTE 0x39, charge_tents.cpp: a tripmine charging
CClientTent* RC2_NewTripmineZapTent(const CClientTent* pCopy); // CLTE 0x3a, tripminezap_tent.cpp: a tripmine's zap
CClientTent* RC2_NewRoachWalkTent(const CClientTent* pCopy); // CLTE 0x3b, roachwalk_tent.cpp: the roach charge walking
CClientTent* RC2_NewConcChargeTent(const CClientTent* pCopy); // CLTE 0x3c, charge_tents.cpp: the conc charge arming
CClientTent* RC2_NewExpandingSphereTent(const CClientTent* pCopy); // CLTE 0x3d, energyblast_tents.cpp: an expanding sphere
CClientTent* RC2_NewSnarkTrailTent(const CClientTent* pCopy); // CLTE 0x3e, snarktrail_tent.cpp: the snark cam's trail
CClientTent* RC2_NewForkliftLightsTent(const CClientTent* pCopy); // CLTE 0x3f, forklift_tents.cpp: a forklift's headlights
CClientTent* RC2_NewTransformTent(const CClientTent* pCopy); // CLTE 0x40, transform_tents.cpp: a player transforming
CClientTent* RC2_NewHoundeyeChargeTent(const CClientTent* pCopy); // CLTE 0x41, houndeye_tents.cpp: the houndeye's beam charge
CClientTent* RC2_NewCurvedBeamTent(const CClientTent* pCopy); // CLTE 0x42, curvedbeam_tents.cpp: a curved beam
CClientTent* RC2_NewRingBlastTent(const CClientTent* pCopy); // CLTE 0x43, ringblast_tents.cpp: a ring blast
CClientTent* RC2_NewAfterimageTent(const CClientTent* pCopy); // CLTE 0x44, afterimage_tents.cpp: an entity's afterimages
CClientTent* RC2_NewControllerThrowGlowTent(const CClientTent* pCopy); // CLTE 0x45, tcontroller_tents.cpp: the controller's throw, its hand glow
CClientTent* RC2_NewControllerThrowBallTent(const CClientTent* pCopy); // CLTE 0x46, tcontroller_tents.cpp: the controller's thrown ball
CClientTent* RC2_NewControllerSparksTent(const CClientTent* pCopy); // CLTE 0x47, tcontroller_tents.cpp: controller sparks
CClientTent* RC2_NewControllerChargeGlowTent(const CClientTent* pCopy); // CLTE 0x48, tcontroller_tents.cpp: the controller charging, its hand glows
CClientTent* RC2_NewControllerChargeBallTent(const CClientTent* pCopy); // CLTE 0x49, tcontroller_tents.cpp: the controller's charge ball

// CLTE 0x29 (egontractor_tent.cpp)
class CEgonTractorTent : public CClientTent // name ours
{
public:
	CEgonTractorTent();

	bool Update(float flFrametime) override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;	 // orig +0x8: the player (the dispatcher sets it); -1 once ended
	int m_iHeld;		 // orig +0xc: what the beam holds, 0 nothing
	BEAM* m_pLightning;	 // orig +0x10 (the dispatcher makes it)
	BEAM* m_pLaser;		 // orig +0x14 (the dispatcher makes it)
	BEAM* m_pHoldBeam;	 // orig +0x18: from the end to what is held
	Vector m_vecEnd;	 // orig +0x1c
	int m_iLaserModel;	 // orig +0x28: sprites/laserbeam.spr's index (the dispatcher sets it)
};

//
// CLTE 0x39 (charge_tents.cpp): short tripmine, short its beam entity, short its glow entity. VERIFY: the server
// (tripmine.cpp) writes tripmine, beam, 0, glow, 0, so the third short read here is the 0 and
// the glow is never touched; whether the server reconstruction or the original is off is open.
// c2's dispatcher also touches this effect: CLTE 0x3a (the laser tripped) plays
// debris/beamstart9.wav on the tripmine and zeroes m_flCharge of every 0x39 effect on it.
//
class CTripmineChargeTent : public CClientTent // name ours
{
public:
	CTripmineChargeTent();

	bool Update(float flFrametime) override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;	// orig +0x8: the tripmine
	int m_iBeam;		// orig +0xc: its beam entity
	int m_iGlow;		// orig +0x10: its glow entity
	float m_flCharge;	// orig +0x14: 0-1 (CLTE 0x3a zeroes it)
	float m_flRoll;		// orig +0x18: the glow's roll
	float m_flRollSpeed; // orig +0x1c: 200-300 a second, either way
	float m_flHue;		// orig +0x20: 0-3 around red, green, blue
};

// CLTE 0x3b (roachwalk_tent.cpp)
class CRoachWalkTent : public CClientTent // name ours
{
public:
	CRoachWalkTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;		 // orig +0x8: the charge (the dispatcher sets it)
	float m_flTrailTime;	 // orig +0xc: until the next trail point
	bool m_bVisible;		 // orig +0x10: in the PVS this frame
	Vector m_vecOrigin;		 // orig +0x14
	model_s* m_pTrailSprite; // orig +0x20: sprites/gradsmoke.spr (the dispatcher loads it)
	CTentTrail m_Trail;		 // orig +0x24
	BEAM* m_pBeam;			 // orig +0x4c: the follow beam below cl_detail 0 (the dispatcher makes it)
	bool m_bArmed;			 // orig +0x50 (the dispatcher sets it)
	float m_flArmTimeLeft;	 // orig +0x54
	float m_flArmTime;		 // orig +0x58
};

// CLTE 0x41 (houndeye_tents.cpp)
#define HOUNDEYE_ARCS 4		 // name ours
#define HOUNDEYE_ARC_POINTS 6 // name ours
#define HOUNDEYE_SPARKS 16	 // name ours

//
// CLTE 0x41: byte the houndeye (a player).
//
class CHoundeyeChargeTent : public CClientTent // name ours
{
public:
	CHoundeyeChargeTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;			 // orig +0x8
	bool m_bVisible;			 // orig +0xc
	bool m_bDrawInner;			 // orig +0xd: the particles and sparks; off for the houndeye himself in first person
	float m_flTimeLeft;			 // orig +0x10
	float m_flChargeTime;		 // orig +0x14
	Vector m_vecOrigin;			 // orig +0x18: in front of his head
	TEMPENTITY* m_pGlow;		 // orig +0x24: sprites/heflare1.spr
	CTentParticles m_Particles;	 // orig +0x28
	model_s* m_pParticleSprite;	 // orig +0x34: sprites/heflare2.spr
	float m_flNextParticle;		 // orig +0x38
	Vector m_vecArc[HOUNDEYE_ARCS][HOUNDEYE_ARC_POINTS]; // orig +0x3c
	Vector m_colArc[HOUNDEYE_ARC_POINTS];				 // orig +0x15c
	float m_flArcSegLen[HOUNDEYE_ARC_POINTS];			 // orig +0x1a4: scratch for CLTE_DrawStrip
	float m_flArcAlpha[HOUNDEYE_ARC_POINTS];			 // orig +0x1bc
	float m_flArcWidth[HOUNDEYE_ARC_POINTS];			 // orig +0x1d4
	model_s* m_pArcSprite;		 // orig +0x1ec: sprites/gradsmoke3.spr
	float m_flArcPhase;			 // orig +0x1f0
	float m_flArcSpin;			 // orig +0x1f4: radians a second, +-4 pi at most
	Vector m_vecSpark[HOUNDEYE_SPARKS][2]; // orig +0x1f8: each spark's head, and its place in front of him (forward, right, up)
	Vector m_colSpark[HOUNDEYE_SPARKS];	   // orig +0x378
	float m_flSparkLife[HOUNDEYE_SPARKS];  // orig +0x438
	int m_iSparks;				 // orig +0x478
	model_s* m_pSparkSprite;	 // orig +0x47c: sprites/gradsmoke.spr
};
