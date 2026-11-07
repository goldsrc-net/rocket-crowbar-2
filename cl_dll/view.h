//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#pragma once

void V_StartPitchDrift();
void V_StopPitchDrift();

// Rocket Crowbar 2 (view.cpp; names ours): the view shake (client 0xe9ca0) and the screen
// tint from (r1, g1, b1, a1) to (r2, g2, b2, a2) over flTime (client 0xec590) the client
// effects start
void V_StartViewShake(float flAmplitude, float flFrequency, float flDuration, int iPriority);
void V_StartScreenTint(float r1, float g1, float b1, float r2, float g2, float b2, float a1, float a2, float flTime, int iPriority);
