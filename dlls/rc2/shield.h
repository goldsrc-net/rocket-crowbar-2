/***
*
*	Rocket Crowbar 2: the player shield's deflection (dlls/rc2/shield.cpp).
*
****/

#pragma once

class CBaseEntity;

// The damage left of flDamage after the shield of pEntity's player absorbs what its energy
// allows (flDamage * flEnergyScale * [Shield] BaseEnergyScale is drawn; flDamage itself when
// there is no shield up). vecHitPos is where the hit lands and vecDir where it is going
// (g_vecZero: from vecHitPos to the player); *pvecDeflectPos gets the point on the shield and
// *pvecDeflectDir the shield's normal there (either may be NULL). iEffect (0-15) goes into the
// shield-hit message. Name ours.
float ShieldDeflect(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecHitPos, Vector vecDir, Vector* pvecDeflectPos, Vector* pvecDeflectDir, int iEffect);
