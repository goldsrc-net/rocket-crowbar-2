/***
*
*	Rocket Crowbar 2: the race (dlls/rc2/racecontroller.cpp): a player in a race is transform 6,
*	the race forklift, driven along the controller's race_path points. Names ours.
*
****/

#pragma once

class CBasePlayer;

// The racer's frame (CBasePlayer::PreThink for transform 6): +use gives up the race at a spawn spot
void RaceUseFrame(CBasePlayer* pPlayer);

// The race path point player iPlayer is heading for, by entity index; nothing is written when
// the player isn't racing. The height is the controller's track_radius (m_flTransformVar1).
void RacePath_GetCurrent(int iPlayer, Vector* pvecPoint, float* pflRadius);
void RacePath_GetRelative(int iPlayer, int iOffset, Vector* pvecPoint, float* pflRadius); // wraps around
void RacePath_Advance(int iPlayer);
void RacePath_GetHeight(int iPlayer, float* pflHeight);
