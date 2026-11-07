/***
*
*	Rocket Crowbar 2: client temp entities ("CLTE"), RC2's own client effects.
*
*	The server sends "CLTE" with a type byte first (dlls/UserMessages.cpp); each type is a class
*	derived from CClientTent whose Create reads the rest of the message. The manager keeps up to
*	600 of them (g_pClientTents), runs Update every HUD_TempEntUpdate (true: the effect is done and
*	deleted) and draws them from HUD_DrawNormalTriangles and HUD_DrawTransparentTriangles. Type 0xff
*	(and 0xfe for one type, 0xfc for all) asks the effects to let go of an entity (KillEntity: true
*	deletes), 0xfd sends a command to the types that take one (CClientTentCmd).
*
*	Alongside: "animating disks" (cones and rings drawn with the triangle API that fade between two
*	colors), fade quads, the strip drawers the trails use, two point-list classes (CTentTrail,
*	CTentParticles), an origin cache that lets an effect move an entity's apparent origin, and a
*	loopback buffer through which the client sends itself a CLTE message.
*
*	client.dll keeps no names here: every name is ours. The original is one source file running
*	from client 0x508e0 well past 0x71ad0 (its vtables and data are one block); this rebuild splits
*	it into cl_dll/rc2/clte*.cpp.
*
****/

#pragma once

#include "r_efx.h"

#define MAX_CLIENT_TENTS 600 // name ours

// The base of every client effect (vtable client 0x1011d84c, 8 bytes)
class CClientTent // name ours
{
public:
	virtual bool Update(float flFrametime) { return false; } // orig client 0x5bf50. true: done, the manager deletes it
	virtual void Think() {}									 // orig client 0x5bf70. Only CLTE_Think calls it, and nothing calls that
	virtual void DrawNormal() {}							 // orig client 0x5bf90
	virtual void DrawTransparent() {}						 // orig client 0x5bfb0
	virtual bool KillEntity(int iEnt) { return false; }		 // orig client 0x5bfd0. true: the manager deletes it

	int m_iType = 0; // +4: the CLTE type it was made for (set by the manager after Create)
};

// The types that answer CLTE 0xfd: byte type, byte command, short value. The original calls its
// sixth vtable slot on whatever object has the type
class CClientTentCmd : public CClientTent // name ours
{
public:
	virtual bool Command(int iCmd, int iValue) = 0; // true: the manager deletes it
};

extern CClientTent* g_pClientTents[MAX_CLIENT_TENTS]; // orig .bss 0x101ee07c
extern unsigned int g_iClientFrame;					  // orig .bss 0x101f6a08: HUD_Frame's count, cleared with the origin cache
extern int g_iClientDetail;							  // cdll_int.cpp: cl_detail
extern unsigned int g_iShieldPlayers;				  // orig .bss 0x101f69f4: bit (player - 1) set while that player's shield shows

// The view the effects face, refreshed by CLTE_Update and the draw passes
extern Vector g_vecTentForward; // orig .bss 0x101f3948
extern Vector g_vecTentRight;	// orig .bss 0x101f69e8
extern Vector g_vecTentUp;		// orig .bss 0x101ee9e0

extern Vector v_origin, v_angles; // view.cpp

// The free slot an effect's Create takes, or -1 after printing the overflow (inlined into every
// Create in the original)
inline int CLTE_FreeSlot()
{
	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (!g_pClientTents[i])
			return i;
	}
	char sz[256];
	sprintf(sz, "Error: overflow: %i client tents\n", MAX_CLIENT_TENTS);
	gEngfuncs.pfnConsolePrint(sz);
	return -1;
}

// What every Create does with pCopy: a copy of it in the slot
template <class T>
T* CLTE_CreateCopy(int iSlot, T* pCopy)
{
	T* p = new T(*pCopy);
	g_pClientTents[iSlot] = p;
	return p;
}

// Lets a beam go: no longer forever or tied to its entities (inlined in the original)
inline void Beam_Release(BEAM* pBeam)
{
	pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
	pBeam->endEntity = 0;
	pBeam->startEntity = 0;
}

// As Beam_Release, and it dies now (inlined in the original)
inline void Beam_Kill(BEAM* pBeam)
{
	pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
	pBeam->t = 1.0;
	pBeam->die = 0.1;
	pBeam->endEntity = 0;
	pBeam->startEntity = 0;
}

// Three random floats in VC6's order (z, then y, then x: right-to-left arguments), as the original's
// inline random vector draws them
inline Vector CLTE_RandomVector(float flLow, float flHigh)
{
	const float z = gEngfuncs.pfnRandomFloat(flLow, flHigh);
	const float y = gEngfuncs.pfnRandomFloat(flLow, flHigh);
	const float x = gEngfuncs.pfnRandomFloat(flLow, flHigh);
	return Vector(x, y, z);
}

// clte.cpp
int __MsgFunc_CLTE(const char* pszName, int iSize, void* pbuf);
void CLTE_KillByEntity(int iEnt, int iType);
void CLTE_Reset();
void CLTE_Shutdown();
void CLTE_Update(float flFrametime);
void CLTE_Think();
void CLTE_DrawNormal();
void CLTE_DrawTransparent();
void CLTE_ClearPlayerOverrides();
void CLTE_SetPlayerOverride(int iPlayer, float x, float y, float z, float flValue);
void CLTE_ClearPlayerOverride(int iPlayer);
bool CLTE_GetPlayerOverride(int iPlayer, float* vecOut, float* pflValue);
void CLTE_LoopbackBegin();
void CLTE_LoopbackSend();
void CLTE_LoopbackByte(int iValue);
void CLTE_LoopbackShort(int iValue);
void CLTE_LoopbackLong(int iValue);
void CLTE_LoopbackCoord(float flValue);
float CLTE_GetEntityScale(int iEnt);
void CLTE_ClearOrigins();
void CLTE_SetOrigin(int iEnt, float x, float y, float z);
Vector CLTE_GetOrigin(int iEnt);
void CLTE_SetOriginV(int iEnt, const float* vecOrigin);
void CLTE_GetOriginV(int iEnt, float* vecOut);
Vector CLTE_EllipsePoint(float flAngle, float z, float flRadius, float flScale);
Vector CLTE_ShieldPoint(int iPlayer, Vector vecPoint, Vector vecDir, float* vecNormal, bool* pbHit);

// clte_draw.cpp: animating disks, fade quads, strips
void CLTE_InitShapes();
void CLTE_UpdateShapes(float flFrametime);
void CLTE_DrawShapes();
void CLTE_AddDiskCone(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, int iRing,
	float flApex, float flApexEnd, float flBase, float flBaseEnd, float flRadius, float flRadiusEnd,
	Vector vecColor1, Vector vecColor1End, float flAlpha1, float flAlpha1End,
	Vector vecColor2, Vector vecColor2End, float flAlpha2, float flAlpha2End,
	float flLife, const char* pszModel, float flFrame, float flNumFrames, float flFramerate);
void CLTE_AddDiskRing(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, int iRing,
	float flHeight1, float flHeight1End, float flHeight2, float flHeight2End,
	float flRadius1, float flRadius1End, float flRadius2, float flRadius2End,
	Vector vecColor1, Vector vecColor1End, float flAlpha1, float flAlpha1End,
	Vector vecColor2, Vector vecColor2End, float flAlpha2, float flAlpha2End,
	float flLife, const char* pszModel, float flFrame, float flNumFrames, float flFramerate);
void CLTE_AddFadeQuads(int iCount, Vector* pV0, Vector* pV1, Vector* pV2, Vector* pV3, Vector* pColors,
	float* pAlphas, float* pFades, model_s* pModel);
void CLTE_DrawCone(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, float flApex, float flBase,
	float flRadius, int iRing, Vector vecColor1, float flAlpha1, Vector vecColor2, float flAlpha2);
void CLTE_DrawRing(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, float flRadius1, float flRadius2,
	float flHeight1, float flHeight2, int iRing, Vector vecColor1, float flAlpha1, Vector vecColor2, float flAlpha2);
void CLTE_DrawBeamQuad(Vector vecStart, Vector vecEnd, float flStartWidth, float flEndWidth,
	Vector vecStartColor, float flStartAlpha, Vector vecEndColor, float flEndAlpha);
void CLTE_DrawStrip(Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	bool bTexAlong, bool bTexByLength, float* pLengths);
void CLTE_DrawStripHead(Vector vecHead, float flHeadWidth, Vector vecHeadColor, float flHeadAlpha,
	Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	bool bTexAlong, bool bTexByLength, float* pLengths);
void CLTE_DrawStripLoop(Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	bool bTexAlong, bool bTexByLength, float* pLengths);
void CLTE_DrawStripScrolled(Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	float flTexScale, float flTexStart);

// The ring tables the disks are drawn with: ring i has g_iRingSegments[i] segments
#define CLTE_NUM_RINGS 7
extern const int g_iRingSegments[CLTE_NUM_RINGS + 1]; // orig .data 0x10136df4

// clte_particles.cpp: a trail of points, newest first (0x44 bytes a point)
typedef int (*pfnTrailPoint)(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pWidth, float* pFade, void* pUser); // 0: the default motion runs

struct tentpoint_t // name ours
{
	Vector pos;		// +0
	Vector vel;		// +0xc
	Vector accel;	// +0x18
	Vector color;	// +0x24
	float alpha;	// +0x30, 0..1; a point is alive while it is above 0
	float width;	// +0x34
	float fade;		// +0x38, alpha lost a second
	void* user;		// +0x3c
	pfnTrailPoint callback; // +0x40
};

class CTentTrail // name ours; 0x28 bytes
{
public:
	CTentTrail();
	~CTentTrail();
	int Update(float flFrametime);
	bool CanDraw();
	void Draw(bool bTexAlong);
	void DrawToHead(Vector vecHead, Vector vecColor, float flAlpha, float flWidth, bool bTexAlong);
	void AddPoint(Vector vecPos, Vector vecVel, Vector vecAccel, Vector vecColor, float flAlpha,
		float flWidth, float flFade, void* pUser, pfnTrailPoint pfnCallback);
	void Reset();
	void Grow(int iCount);
	void CountAlive();
	void InsertFront();

	tentpoint_t* m_pPoints; // +0
	int m_iAllocated;		// +4
	int m_iCount;			// +8: the live ones, first in the list
	Vector* m_pRenderPos;	// +0xc: what Draw hands the strip drawer, rebuilt by Update
	Vector* m_pRenderColor; // +0x10
	float* m_pRenderWidth;	// +0x14
	float* m_pRenderAlpha;	// +0x18
	float* m_pRenderLength; // +0x1c: always zeros
	int m_iRenderAllocated; // +0x20
	int m_iRenderCount;		// +0x24
};

// clte_particles.cpp: loose sprite particles, drawn as view-facing quads (0x48 bytes a particle)
typedef int (*pfnTentParticle)(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pSize, float* pFade, float* pLife, void* pUser); // 0: the default motion runs

struct tentparticle_t // name ours
{
	Vector pos;		 // +0
	Vector vel;		 // +0xc
	Vector accel;	 // +0x18
	Vector color;	 // +0x24
	float alpha;	 // +0x30
	float size;		 // +0x34
	float fade;		 // +0x38
	float life;		 // +0x3c: alive while above 0
	void* user;		 // +0x40
	pfnTentParticle callback; // +0x44
};

class CTentParticles // name ours; 0xc bytes
{
public:
	CTentParticles();
	~CTentParticles();
	void Add(Vector vecPos, Vector vecVel, Vector vecAccel, Vector vecColor, float flAlpha, float flSize,
		float flFade, float flLife, void* pUser, pfnTentParticle pfnCallback);
	int Update(float flFrametime);
	void Draw();
	bool HasAlive();
	void Grow(int iIndex);

	tentparticle_t* m_pParticles; // +0
	int m_iAllocated;			  // +4
	int m_iCount;				  // +8
};

int Point_Darken(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pWidth, float* pFade, void* pUser);
int Particle_FadeToBlue(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pSize, float* pFade, float* pLife, void* pUser);
int Particle_Seek(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pSize, float* pFade, float* pLife, void* pUser);

// The effects the manager reaches into by type. Their Create functions read the rest of the message
// (pCopy NULL) or copy pCopy. Defined in clte_effects*.cpp, except where noted.
class CTentJetpack;	   // CLTE 3
class CTentEnergyDraw; // CLTE 5

// clte_effects*.cpp, CLTE 1 to 0x25 (orig client 0x5bac0-0x714f0)
CClientTent* CLTE_CreateRocketTrail(CClientTent* pCopy);   // 1
CClientTent* CLTE_CreateJetboost(CClientTent* pCopy);	   // 2
CClientTent* CLTE_CreateJetpack(CClientTent* pCopy);	   // 3
CClientTent* CLTE_CreateEnergyDraw(CClientTent* pCopy);	   // 5
CClientTent* CLTE_CreateSciMineIdle(CClientTent* pCopy);   // 7
CClientTent* CLTE_CreateSciMineSpin(CClientTent* pCopy);   // 8
CClientTent* CLTE_CreateCreateHealth(CClientTent* pCopy);  // 9
CClientTent* CLTE_CreateGibBleed(CClientTent* pCopy);	   // 10
CClientTent* CLTE_CreateStickyGib(CClientTent* pCopy);	   // 11
CClientTent* CLTE_CreateTransphasic(CClientTent* pCopy);	   // 12
CClientTent* CLTE_CreateChargeGlow(CClientTent* pCopy);	   // 13
CClientTent* CLTE_CreatePythonCharge(CClientTent* pCopy);		   // 14
CClientTent* CLTE_CreateBallLightning(CClientTent* pCopy); // 15
CClientTent* CLTE_CreateMP5Bolt(CClientTent* pCopy);		   // 16
CClientTent* CLTE_CreateMP5Pulse(CClientTent* pCopy);		   // 17
CClientTent* CLTE_CreateMP5Bouncy(CClientTent* pCopy);		   // 18
CClientTent* CLTE_CreateCrateCharge(CClientTent* pCopy);		   // 19
CClientTent* CLTE_CreateHEVFlame(CClientTent* pCopy);	   // 20
CClientTent* CLTE_CreateCrowbarangGlint(CClientTent* pCopy); // 21
CClientTent* CLTE_CreateTracer(CClientTent* pCopy);		   // 22
CClientTent* CLTE_CreateBoltTrail(CClientTent* pCopy);	   // 23
CClientTent* CLTE_CreateImpailer(CClientTent* pCopy);	   // 24
CClientTent* CLTE_CreateAttach(CClientTent* pCopy);		   // 25
CClientTent* CLTE_CreateBoltRope(CClientTent* pCopy);	   // 26
CClientTent* CLTE_CreateColorTracer(CClientTent* pCopy);		   // 27
CClientTent* CLTE_CreateCrossbowCharge(CClientTent* pCopy);		   // 28
CClientTent* CLTE_CreateBoltGlow(CClientTent* pCopy);	   // 29
CClientTent* CLTE_CreateBoltArmed(CClientTent* pCopy);	   // 30
CClientTent* CLTE_CreateSuckMineRope(CClientTent* pCopy);  // 31
CClientTent* CLTE_CreateSphereMine(CClientTent* pCopy);	   // 32
CClientTent* CLTE_CreateLightningFlash(CClientTent* pCopy);		   // 33
CClientTent* CLTE_CreateBeaconStuck(CClientTent* pCopy);   // 34
CClientTent* CLTE_CreateChargeHum(CClientTent* pCopy);		   // 35
CClientTent* CLTE_CreateBlueLightning(CClientTent* pCopy);		   // 36
CClientTent* CLTE_CreateShield(CClientTent* pCopy);		   // 37
