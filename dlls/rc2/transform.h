/***
*
*	Rocket Crowbar 2: what the transformations (dlls/rc2/transform.cpp) share with the rest of
*	the game library besides CBasePlayer's transform methods (player.h). Names ours.
*
****/

#pragma once

class CBasePlayer;

// The direction a transformed player dies in: 1 shot in the head, 2 in the stomach, 3 forward
// (room ahead), 4 backward (room behind), 0 otherwise
int Transform_DeathDirection(CBasePlayer* pPlayer);
// What the five transforms need, from the world's precache (ClientPrecache)
void Transform_Precache();
// CBasePlayer::Classify's answer for a transformed player
int Transform_Classify(int iTransform);
