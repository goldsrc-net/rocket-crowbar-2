/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
#include "../hud.h"
#include "../cl_util.h"
#include "event_api.h"
#include "pmtrace.h"
#include "ev_hldm.h"

/*
======================
Game_HookEvents

Associate script file name with callback functions. Note that the format is
 always the same.  Of course, a clever mod team could actually embed parameters, behavior
 into the actual .sc files and create a .sc file parser and hook their functionality through
 that.. i.e., a scripting system.

That was what we were going to do, but we ran out of time...oh well.
======================
*/
// orig client 0x38960. RC2's list: none of SDK 2.3's events but crowbar, rpg and train, which
// keep their SDK handlers; the rest are RC2's own (ev_hldm.cpp, rc2/ev_rc2.cpp)
void Game_HookEvents()
{
	gEngfuncs.pfnHookEvent("events/glockfiregib.sc", EV_GlockFireGib);
	gEngfuncs.pfnHookEvent("events/glocktpcharge.sc", EV_GlockTPCharge);
	gEngfuncs.pfnHookEvent("events/glocktpgibfire.sc", EV_GlockTPGibFire);
	gEngfuncs.pfnHookEvent("events/glockfart.sc", EV_GlockFart);
	gEngfuncs.pfnHookEvent("events/glockgibimpact.sc", EV_GlockGibImpact);
	gEngfuncs.pfnHookEvent("events/stickygibattach.sc", EV_StickyGibAttach);
	gEngfuncs.pfnHookEvent("events/shotgunsci.sc", EV_FireShotgunSci);
	gEngfuncs.pfnHookEvent("events/shotgunsuit.sc", EV_FireShotgunSuit);
	gEngfuncs.pfnHookEvent("events/shotgunfl.sc", EV_FireShotgunFL);
	gEngfuncs.pfnHookEvent("events/firesgcrate.sc", EV_FireShotgunCrate);
	gEngfuncs.pfnHookEvent("events/sgcratecharge.sc", EV_ShotgunCrateCharge);
	gEngfuncs.pfnHookEvent("events/sgcratehit.sc", EV_ShotgunCrateHit);
	gEngfuncs.pfnHookEvent("events/mp5primary.sc", EV_MP5Primary);
	gEngfuncs.pfnHookEvent("events/mp5fart.sc", EV_MP5Fart);
	gEngfuncs.pfnHookEvent("events/mp5bouncy.sc", EV_MP5Bouncy);
	gEngfuncs.pfnHookEvent("events/mp5pulse.sc", EV_MP5Pulse);
	gEngfuncs.pfnHookEvent("events/mp5blasterhit.sc", EV_MP5BlasterHit);
	gEngfuncs.pfnHookEvent("events/mp5pulsehit.sc", EV_MP5PulseHit);
	gEngfuncs.pfnHookEvent("events/mp5bouncyhit.sc", EV_MP5BouncyHit);
	gEngfuncs.pfnHookEvent("events/mp5charge.sc", EV_MP5Charge);
	gEngfuncs.pfnHookEvent("events/pythonlightning.sc", EV_PythonLightning);
	gEngfuncs.pfnHookEvent("events/pythonchain.sc", EV_PythonChain);
	gEngfuncs.pfnHookEvent("events/pythonprefire.sc", EV_PythonPrefire);
	gEngfuncs.pfnHookEvent("events/pythonfireball.sc", EV_PythonFireball);
	gEngfuncs.pfnHookEvent("events/pythballimpact.sc", EV_PythBallImpact);
	gEngfuncs.pfnHookEvent("events/gaussprimary.sc", EV_GaussPrimary);
	gEngfuncs.pfnHookEvent("events/gaussthird.sc", EV_GaussThird);
	gEngfuncs.pfnHookEvent("events/gaussbecon.sc", EV_GaussBecon);
	gEngfuncs.pfnHookEvent("events/gaussprimarydeny.sc", EV_GaussPrimaryDeny);
	gEngfuncs.pfnHookEvent("events/gausscharge.sc", EV_GaussCharge);
	gEngfuncs.pfnHookEvent("events/teleportflash.sc", EV_TeleportFlash);
	gEngfuncs.pfnHookEvent("events/crowbar.sc", EV_Crowbar);
	gEngfuncs.pfnHookEvent("events/crowbarswing.sc", EV_CrowbarSwing);
	gEngfuncs.pfnHookEvent("events/cbarcharge.sc", EV_CbarCharge);
	gEngfuncs.pfnHookEvent("events/xbowzoom1.sc", EV_XbowZoom1);
	gEngfuncs.pfnHookEvent("events/xbowsuperzoomed.sc", EV_XbowSuperZoomed);
	gEngfuncs.pfnHookEvent("events/xbowexplozoomed.sc", EV_XbowExploZoomed);
	gEngfuncs.pfnHookEvent("events/xbowimpailer.sc", EV_XbowImpailer);
	gEngfuncs.pfnHookEvent("events/xbowteather.sc", EV_XbowTeather);
	gEngfuncs.pfnHookEvent("events/xbowcharge.sc", EV_XbowCharge);
	gEngfuncs.pfnHookEvent("events/xbowcharge2.sc", EV_XbowCharge2);
	gEngfuncs.pfnHookEvent("events/rpg.sc", EV_FireRpg);
	gEngfuncs.pfnHookEvent("events/rpgsmimpact.sc", EV_RpgSphereMineImpact);
	gEngfuncs.pfnHookEvent("events/egontractor.sc", EV_EgonTractor);
	gEngfuncs.pfnHookEvent("events/egonfireshrinker.sc", EV_EgonFireShrinker);
	gEngfuncs.pfnHookEvent("events/egonimplodercharge.sc", EV_EgonImploderCharge);
	gEngfuncs.pfnHookEvent("events/egonimploderfire.sc", EV_EgonImploderFire);
	gEngfuncs.pfnHookEvent("events/egonfirenuke.sc", EV_EgonFireNuke);
	gEngfuncs.pfnHookEvent("events/egonshrinkerimpact.sc", EV_EgonShrinkerImpact);
	gEngfuncs.pfnHookEvent("events/egonnukeexplode.sc", EV_EgonNukeExplode);
	gEngfuncs.pfnHookEvent("events/hgunfire.sc", EV_HornetGunFire);
	gEngfuncs.pfnHookEvent("events/hgunsetcharge.sc", EV_HornetGunSetCharge);
	gEngfuncs.pfnHookEvent("events/hgungrenadedet.sc", EV_HornetGrenadeDet);
	gEngfuncs.pfnHookEvent("events/tripfire.sc", EV_TripmineFire);
	gEngfuncs.pfnHookEvent("events/snarkfire.sc", EV_SnarkFire);
	gEngfuncs.pfnHookEvent("events/snarkcamplace.sc", EV_SnarkCamPlace);
	gEngfuncs.pfnHookEvent("events/snarkprojecthit.sc", EV_SnarkProjectHit);
	gEngfuncs.pfnHookEvent("events/minigruntblast.sc", EV_MiniGruntBlast);
	gEngfuncs.pfnHookEvent("events/gravgrenadeglow.sc", EV_GravGrenadeGlow);
	gEngfuncs.pfnHookEvent("events/gravgrenadedet.sc", EV_GravGrenadeDet);
	gEngfuncs.pfnHookEvent("events/afgrendet.sc", EV_AFGrenadeDet);
	gEngfuncs.pfnHookEvent("events/ultgrendet.sc", EV_UltimaGrenadeDet);
	gEngfuncs.pfnHookEvent("events/concchgexp.sc", EV_ConcChargeExplode);
	gEngfuncs.pfnHookEvent("events/tassassingun.sc", EV_TAssassinGun);
	gEngfuncs.pfnHookEvent("events/tassassinkick.sc", EV_TAssassinKick);
	gEngfuncs.pfnHookEvent("events/thoundeyebeam.sc", EV_THoundeyeBeam);
	gEngfuncs.pfnHookEvent("events/thoundeyesonic.sc", EV_THoundeyeSonic);
	gEngfuncs.pfnHookEvent("events/tcontrollerthrow.sc", EV_TControllerThrow);
	gEngfuncs.pfnHookEvent("events/tcontrollertballhit.sc", EV_TControllerBallHit);
	gEngfuncs.pfnHookEvent("events/tbatbite.sc", EV_TBatBite);
	gEngfuncs.pfnHookEvent("events/train.sc", EV_TrainPitchAdjust);
	gEngfuncs.pfnHookEvent("events/gib.sc", EV_Gib);
	gEngfuncs.pfnHookEvent("events/explode.sc", EV_Explode);
	gEngfuncs.pfnHookEvent("events/expsmoke.sc", EV_ExpSmoke);

	// halflife-updated's func_vehicle; RC2 has no such hook
	gEngfuncs.pfnHookEvent("events/vehicle.sc", EV_VehiclePitchAdjust);
}
