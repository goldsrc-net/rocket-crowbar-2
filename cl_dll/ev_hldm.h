//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#pragma once

void EV_HLDM_GunshotDecalTrace(pmtrace_t* pTrace, char* decalName);
void EV_HLDM_DecalGunshot(pmtrace_t* pTrace, int iBulletType);
void EV_HLDM_CheckTracer(int idx, float* vecSrc, float* end, float* forward, float* right, int iBulletType, int iTracerFreq, int* tracerCount);
void EV_HLDM_FireBullets(int idx, float* forward, float* right, float* up, int cShots, float* vecSrc, float* vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int* tracerCount, float flSpreadX, float flSpreadY);

// HLDM handlers RC2 kept (SDK 2.3's crowbar, rpg and train; the tripmine, snark and hornet gun
// changed)
void EV_Crowbar(event_args_t* args);
void EV_FireRpg(event_args_t* args);
void EV_HornetGunFire(event_args_t* args);
void EV_TripmineFire(event_args_t* args);
void EV_SnarkFire(event_args_t* args);

void EV_TrainPitchAdjust(event_args_t* args);
void EV_VehiclePitchAdjust(event_args_t* args);

// RC2's own (names ours), hooked in hl/hl_events.cpp; the last three are in rc2/ev_rc2.cpp
void EV_GlockFireGib(event_args_t* args);
void EV_GlockTPCharge(event_args_t* args);
void EV_GlockTPGibFire(event_args_t* args);
void EV_GlockFart(event_args_t* args);
void EV_GlockGibImpact(event_args_t* args);
void EV_StickyGibAttach(event_args_t* args);
void EV_FireShotgunSci(event_args_t* args);
void EV_FireShotgunSuit(event_args_t* args);
void EV_FireShotgunFL(event_args_t* args);
void EV_FireShotgunCrate(event_args_t* args);
void EV_ShotgunCrateCharge(event_args_t* args);
void EV_ShotgunCrateHit(event_args_t* args);
void EV_MP5Primary(event_args_t* args);
void EV_MP5Fart(event_args_t* args);
void EV_MP5Bouncy(event_args_t* args);
void EV_MP5Pulse(event_args_t* args);
void EV_MP5BlasterHit(event_args_t* args);
void EV_MP5PulseHit(event_args_t* args);
void EV_MP5BouncyHit(event_args_t* args);
void EV_MP5Charge(event_args_t* args);
void EV_PythonLightning(event_args_t* args);
void EV_PythonChain(event_args_t* args);
void EV_PythonPrefire(event_args_t* args);
void EV_PythonFireball(event_args_t* args);
void EV_PythBallImpact(event_args_t* args);
void EV_GaussPrimary(event_args_t* args);
void EV_GaussThird(event_args_t* args);
void EV_GaussBecon(event_args_t* args);
void EV_GaussPrimaryDeny(event_args_t* args);
void EV_GaussCharge(event_args_t* args);
void EV_TeleportFlash(event_args_t* args);
void EV_CrowbarSwing(event_args_t* args);
void EV_CbarCharge(event_args_t* args);
void EV_XbowZoom1(event_args_t* args);
void EV_XbowSuperZoomed(event_args_t* args);
void EV_XbowExploZoomed(event_args_t* args);
void EV_XbowImpailer(event_args_t* args);
void EV_XbowTeather(event_args_t* args);
void EV_XbowCharge(event_args_t* args);
void EV_XbowCharge2(event_args_t* args);
void EV_RpgSphereMineImpact(event_args_t* args);
void EV_EgonTractor(event_args_t* args);
void EV_EgonFireShrinker(event_args_t* args);
void EV_EgonImploderCharge(event_args_t* args);
void EV_EgonImploderFire(event_args_t* args);
void EV_EgonFireNuke(event_args_t* args);
void EV_EgonShrinkerImpact(event_args_t* args);
void EV_EgonNukeExplode(event_args_t* args);
void EV_HornetGunSetCharge(event_args_t* args);
void EV_HornetGrenadeDet(event_args_t* args);
void EV_SnarkCamPlace(event_args_t* args);
void EV_SnarkProjectHit(event_args_t* args);
void EV_MiniGruntBlast(event_args_t* args);
void EV_GravGrenadeGlow(event_args_t* args);
void EV_GravGrenadeDet(event_args_t* args);
void EV_AFGrenadeDet(event_args_t* args);
void EV_UltimaGrenadeDet(event_args_t* args);
void EV_ConcChargeExplode(event_args_t* args);
void EV_TAssassinGun(event_args_t* args);
void EV_TAssassinKick(event_args_t* args);
void EV_THoundeyeBeam(event_args_t* args);
void EV_THoundeyeSonic(event_args_t* args);
void EV_TControllerThrow(event_args_t* args);
void EV_TControllerBallHit(event_args_t* args);
void EV_TBatBite(event_args_t* args);
void EV_Gib(event_args_t* args);
void EV_Explode(event_args_t* args);
void EV_ExpSmoke(event_args_t* args);
