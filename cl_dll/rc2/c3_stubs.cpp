/***
*
*	Rocket Crowbar 2 client, work package c3: temporary definitions of what c3's files use
*	from other packages (declared in c3_needs.h and in the "c3 needs" blocks of c3's files),
*	so c3 links on its own. Each block goes when the real one lands; none of them carries the
*	original's address.
*
****/

#include "c3_needs.h"

// TEMP STUB (c3): real one belongs in c2's HUD_Frame part (orig .data 0x101a9598)
int g_iClDetail = 0;

// TEMP STUB (c3): real one belongs in "new: entity .. ev_common" (orig 0x9e9a0)
cl_entity_t* RC2_GetEntity(int iEntIndex)
{
	return gEngfuncs.GetEntityByIndex(iEntIndex);
}

// TEMP STUB (c3): real one belongs in c2's CLTE file (orig 0x59be0); the prediction table is left out
Vector RC2_GetEntityOrigin(int iEntIndex)
{
	cl_entity_t* pEntity = RC2_GetEntity(iEntIndex);
	if (!pEntity)
		return Vector(0, 0, 0);
	return pEntity->origin;
}

// TEMP STUB (c3): real one belongs in c2's CLTE file (orig 0x539b0)
void RC2_DrawEllipsoidCap(Vector vecOrigin, Vector vecForward, Vector vecRight, Vector vecUp, float flPoleZ, float flRingZ, float flRingRadius, int iCircle, Vector colPole, float flPoleAlpha, Vector colRing, float flRingAlpha)
{
}

// TEMP STUB (c3): real one belongs in c2's CLTE file (orig 0x53ea0)
void RC2_DrawEllipsoidBand(Vector vecOrigin, Vector vecForward, Vector vecRight, Vector vecUp, float flRadius1, float flRadius2, float flZ1, float flZ2, int iCircle, Vector col1, float flAlpha1, Vector col2, float flAlpha2)
{
}

// TEMP STUB (c3): real one belongs in "new: hl_hl_weapons .. common_interface" (orig 0x3b630); no spread
Vector RC2_SpreadVector(Vector vecDir, Vector vecRight, Vector vecUp, float flSpread)
{
	return vecDir.Normalize();
}

// TEMP STUB (c3): real ones belong in c2's CLTE file (orig 0x5a6a0-0x5ad60)
CRC2Trail::CRC2Trail() : m_Data() {}
void CRC2Trail::Free() {}
int CRC2Trail::Update(float flFrametime) { return 0; }
bool CRC2Trail::CanDraw() { return false; }
void CRC2Trail::Draw(bool bFlag) {}
void CRC2Trail::DrawToHead(Vector vecHead, Vector colColor, float flAlpha, float flWidth, bool bFlag) {}
void CRC2Trail::AddPoint(Vector vecOrigin, Vector vecVelocity, Vector vecAccel, Vector colColor, float flAlpha, float flWidth, float flFade, int iUser, RC2TrailPointFunc pfnThink) {}
void CRC2Trail::Clear() {}

// TEMP STUB (c3): real ones belong in c2's CLTE file (orig 0x5af10-0x5b9e0)
CRC2Particles::CRC2Particles() : m_Data() {}
void CRC2Particles::Add(Vector vecOrigin, Vector vecVelocity, Vector vecAccel, Vector colColor, float flAlpha, float flSize, float flFade, float flLife, int iUser, RC2ParticleFunc pfnThink) {}
int CRC2Particles::Update(float flFrametime) { return 0; }
void CRC2Particles::Draw() {}
bool CRC2Particles::HasLive() { return false; }

// TEMP STUB (c3): real ones belong in c2's CLTE file (CLTE 0x25: orig 0x717c0, 0x71a80)
bool CShieldTent::Think(float flFrametime) { return true; }
bool CShieldTent::EntityGone(int iEntIndex) { return false; }

// TEMP STUB (c3): real one belongs in c2's CLTE file and the RC2 part of HUD_Init (registered
// with cl_detail: orig .data 0x1020bb28; cl_forkliftlights, 0x1020b724, is hud.cpp's). Here it
// points at an unregistered cvar at 0, since the effects read it without a null check.
static cvar_t s_StubDebugGren = {(char*)"cl_debuggren", (char*)"0", 0, 0, nullptr};
cvar_t* g_pClDebugGren = &s_StubDebugGren;

// TEMP STUB (c3): real ones belong in c2's CLTE file (orig .bss 0x101edc7c, 0x101f24c4,
// 0x101f69f8, 0x101f69fc, 0x101f6a00, 0x10206918)
unsigned char g_abRC2AntiFriction[1024];
byte g_bEntityFrozen[0x400];
int g_iLocalPlayerFrozen = 0;
int g_iRC2LocalAntiFriction = 0;
int g_bLocalTransforming = 0;

// TEMP STUB (c3): real one belongs in in_camera.cpp (orig 0xa9fd0)
bool CAM_IsThirdPerson()
{
	return false;
}

// TEMP STUB (c3): real one belongs in "new: start .. ev_hldm" (orig 0xdd80); no water
float RC2_WaterLevel(const float* vecPos, float flMinZ, float flMaxZ)
{
	return flMinZ;
}

// TEMP STUB (c3): real ones belong in view.cpp (orig 0xe9ca0, 0xec590)
void RC2_ViewShake(float flAmplitude, float flFrequency, float flDuration, int iPriority) {}
void V_StartColorFade(Vector colFrom, Vector colTo, float flFrom, float flTo, float flDuration, int iPriority) {}

// TEMP STUB (c3): real ones belong in c2's CLTE file (orig 0x532f0, 0x54ad0, 0x55190, 0x58f90, 0x59ee0)
void RC2_AddAnimatingDisk(Vector vecOrigin, Vector vecRight, Vector vecUp, Vector vecNormal, int iCircle,
	float flA0, float flA1, float flB0, float flB1, float flC0, float flC1, float flD0, float flD1,
	Vector col1Start, Vector col1End, float flE0, float flE1, Vector col2Start, Vector col2End, float flF0, float flF1,
	float flLife, const char* pszModel, int iUser1, int iUser2, int iUser3) {}
void RC2_DrawBeamQuad(Vector vecStart, Vector vecEnd, float flWidth, int iUnused, Vector col1, float flAlpha1, Vector col2, float flAlpha2) {}
void RC2_DrawStrip(Vector* pvecPoints, float* pflWidths, Vector* pcolColors, float* pflAlphas, int iCount, bool bStretch, bool bByLength, float* pflSegLengths) {}
void RC2_DrawTexturedStrip(Vector* pvecPoints, float* pflWidths, Vector* pcolColors, float* pflAlphas, int iCount, float flTexScale, float flTexOffset) {}
Vector RC2_ShieldIntercept(int iEntIndex, Vector vecSrc, Vector vecDir, float* pflUnused, int* pbHit)
{
	if (pbHit)
		*pbHit = 0;
	return vecSrc;
}

// TEMP STUB (c3): real ones belong in c2's CLTE file (point thinks, orig 0x508e0, 0x509c0, 0x50b90);
// 0 lets the default motion run
int RC2_TrailPointDrainToBlue(float flFrametime, Vector* pvecOrigin, Vector* pvecVelocity, Vector* pvecAccel, Vector* pcolColor, float* pflAlpha, float* pflWidth, float* pflFade, int iUser) { return 0; }
int RC2_ParticleTintBlue(float flFrametime, Vector* pvecOrigin, Vector* pvecVelocity, Vector* pvecAccel, Vector* pcolColor, float* pflAlpha, float* pflSize, float* pflFade, float* pflLife, int iUser) { return 0; }
int RC2_HoundeyeChargeParticle(float flFrametime, Vector* pvecOrigin, Vector* pvecVelocity, Vector* pvecAccel, Vector* pcolColor, float* pflAlpha, float* pflSize, float* pflFade, float* pflLife, int iUser) { return 0; }
