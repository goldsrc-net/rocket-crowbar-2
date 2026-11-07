/***
*
*	Rocket Crowbar 2: the antifriction timer (dlls/rc2/antifrictimer.cpp).
*
****/

#pragma once

// Ends every antifriction on pent at once (CBaseMonster::Killed and the player's spawn and
// death). Name ours.
void StopAntiFriction(edict_t* pent);
