/***
*
*	Rocket Crowbar 2 client: c3's client temp effects, for c2's CLTE dispatcher (orig 0x510b0).
*
*	One factory per CLTE type: it takes a free slot of g_pClientTents, constructs the effect
*	(which reads the rest of the message) and returns it, or null when every slot is taken; the
*	dispatcher then sets m_iType. pCopy, which the dispatcher always passes as null, makes a copy
*	of that effect instead. Names ours.
*
****/

#pragma once

class CClientTent;

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
