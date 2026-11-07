/***
*
*	Rocket Crowbar 2 client, work package c3: what c3's files use from other packages' files.
*
*	c3 is the second half (0x71ad0-0x9b445) of client.dll's run "new: cdll_int .. com_weapons":
*	the client temp effects (the "CLTE" user message) from type 0x26 on. The run's first half is
*	package c2's, and with it the framework every effect is built on: the effect base class, the
*	table of live effects, the CLTE dispatcher (orig 0x510b0) and the per-frame loops, plus the
*	drawing helpers and the two point-list classes below. client.dll keeps no names for any of
*	it, so every name here is ours, chosen from what the code does as read from c3's call sites
*	and the functions' own bodies; c2 may have named them differently, and the names are
*	reconciled at merge. Each has a temporary definition in c3_stubs.cpp until its real one
*	lands. reference/dossiers/c3.md lists them.
*
****/

#pragma once

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_efx.h"
#include "event_api.h"
#include "triangleapi.h"
#include "parsemsg.h"

#include <cstdio>

//
// The effect base class (c2's). Its vtable (orig .rdata 0x1011d84c) has no destructor: the
// table entries are freed with plain operator delete (orig 0xf09e0), so no effect's destructor
// ever runs. Every effect class of c3 derives from it directly.
//
class CClientTent // name ours
{
public:
	// slot 0: once a frame from HUD_TempEntUpdate (orig 0x523f0) with the frame time; true when
	// the effect is done, and it is deleted. Default: false.
	virtual bool Think(float flFrametime) { return false; } // name ours
	// slot 1: nothing in the original calls it. Default: nothing.
	virtual void Unused1() {} // name ours
	// slot 2: from HUD_DrawNormalTriangles (orig 0x52580). Default: nothing.
	virtual void DrawNormal() {} // name ours
	// slot 3: from HUD_DrawTransparentTriangles (orig 0x52650). Default: nothing.
	virtual void DrawTransparent() {} // name ours
	// slot 4: the CLTE kill messages (0xfe, 0xff) and level change call it with an entity index;
	// true when the effect belonged to that entity and is done (the caller deletes it).
	// Default: false.
	virtual bool EntityGone(int iEntIndex) { return false; } // name ours

	int m_iType = 0; // orig +0x4: the CLTE type that made it (set by the dispatcher after the factory returns). name ours
};

// orig .bss 0x101ee07c: the live effects, 600 slots (the factories' overflow message says so).
// Owned by c2's dispatcher. name ours
#define MAX_CLIENT_TENTS 600 // name ours
extern CClientTent* g_pClientTents[MAX_CLIENT_TENTS];

// The search for a free slot every factory starts with (inlined in each, so a macro or inline
// in the original's header): the first empty slot, or -1 after printing the overflow message.
inline int RC2_FindFreeClientTent() // name ours
{
	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (!g_pClientTents[i])
			return i;
	}
	char szMessage[256];
	sprintf(szMessage, "Error: overflow: %i client tents\n", MAX_CLIENT_TENTS);
	gEngfuncs.pfnConsolePrint(szMessage);
	return -1;
}

// orig .data 0x101a9598: (int)cl_detail->value, refreshed each frame by HUD_Frame's RC2 part
// (orig 0x4ff90; 0 while the cvar pointer is null). The effects draw less below 0 and more
// above 0. name ours
extern int g_iClDetail;

// orig .bss 0x101f3948, 0x101f69e8, 0x101ee9e0: forward, right and up of the view angles (or
// of the third-person camera's), set by AngleVectors before each frame's Think and draw loops
// (orig 0x523f0, 0x52580, 0x52650). Sprite quads face the view with them. names ours
extern Vector g_vecTentForward;
extern Vector g_vecTentRight;
extern Vector g_vecTentUp;

//
// Functions from other packages.
//

// pm_math.cpp (stock)
void VectorAngles(const float* forward, float* angles);

// orig 0x9e9a0 ("new: entity .. ev_common"): gEngfuncs.GetEntityByIndex(iEntIndex). name ours
cl_entity_t* RC2_GetEntity(int iEntIndex);

// orig 0x59be0 (c2): where an entity is drawn: its predicted position (c2's table at 0x101ee9f0,
// indexed by entity) while that is fresh (stamped on the current frame count, 0x101f6a08) and
// the entity moves as a player, step, toss, bounce or fly one (movetype 3, 4, 5, 6, 10) and
// isn't SOLID_BSP... i.e. curstate.solid != 4; otherwise ent->origin. (0,0,0) when there is no
// such entity and the index is outside 0-1023. name ours
Vector RC2_GetEntityOrigin(int iEntIndex);

// orig 0x539b0 (c2): a triangle fan closing an ellipsoid: the pole at origin + up * flPoleZ in
// colPole/flPoleAlpha, then the ring at height flRingZ of radius flRingRadius (fwd * cos +
// right * sin over c2's unit circle iCircle: its last point index is 0x10136df4[iCircle], its
// points at 0x101f19f0 + iCircle * 0x18c) in
// colRing/flRingAlpha, texture coordinates 0..1. name ours
void RC2_DrawEllipsoidCap(Vector vecOrigin, Vector vecForward, Vector vecRight, Vector vecUp, float flPoleZ, float flRingZ, float flRingRadius, int iCircle, Vector colPole, float flPoleAlpha, Vector colRing, float flRingAlpha);

// orig 0x53ea0 (c2): a quad strip between two rings around origin's up axis: radius
// flRadius1 at height flZ1 in col1/flAlpha1 and flRadius2 at flZ2 in col2/flAlpha2 (the unit
// circle iCircle as above). Both heights equal gives a flat ring between the two radii.
// name ours
void RC2_DrawEllipsoidBand(Vector vecOrigin, Vector vecForward, Vector vecRight, Vector vecUp, float flRadius1, float flRadius2, float flZ1, float flZ2, int iCircle, Vector col1, float flAlpha1, Vector col2, float flAlpha2);

// orig 0x3b630 ("new: hl_hl_weapons .. common_interface"): vecDir spread by up to flSpread
// along vecRight and vecUp (a random point in the unit disc, each coordinate the sum of two
// RandomFloat(-0.5, 0.5)), normalized ((0,0,1) when it comes out zero). name ours
Vector RC2_SpreadVector(Vector vecDir, Vector vecRight, Vector vecUp, float flSpread);

//
// c2's two point lists, embedded in many effects.
//

// A point's own motion, called in Update for each point that has one instead of the default
// integration (velocity += accel * dt, position += velocity * dt, alpha -= fade * dt); a
// nonzero return skips the default. names ours
typedef int (*RC2TrailPointFunc)(float flFrametime, Vector* pvecOrigin, Vector* pvecVelocity, Vector* pvecAccel, Vector* pcolColor, float* pflAlpha, float* pflWidth, float* pflFade, int iUser);

// A trail (a strip through its points, the newest first): 0x44-byte points in a malloc'd array
// that grows by 16, and the flattened copies Update builds for drawing. Constructor orig 0x5a6a0.
class CRC2Trail // name ours
{
public:
	CRC2Trail();					// orig 0x5a6a0 (c2)
	void Free();					// orig 0x5a720 (c2): frees the arrays, empties the list
	int Update(float flFrametime);	// orig 0x5a7c0 (c2): moves and fades the points, drops the faded tail, rebuilds the draw arrays; the point count
	bool CanDraw();					// orig 0x5ab80 (c2): has draw arrays and two points or more
	void Draw(bool bFlag);			// orig 0x5abb0 (c2): the strip (c2's 0x55190)
	void DrawToHead(Vector vecHead, Vector colColor, float flAlpha, float flWidth, bool bFlag); // orig 0x5ac00 (c2): the strip led by an extra point (c2's 0x56240)
	void AddPoint(Vector vecOrigin, Vector vecVelocity, Vector vecAccel, Vector colColor, float flAlpha, float flWidth, float flFade, int iUser, RC2TrailPointFunc pfnThink); // orig 0x5ac80 (c2): a new newest point
	void Clear();					// orig 0x5ad60 (c2): drops every point, keeps the arrays

	byte m_Data[0x28];				// layout c2's (+0 points, +4 capacity, +8 count, +0xc..+0x1c draw arrays, +0x20 their size, +0x24 their count)
};

// A point's own motion for CRC2Particles, as RC2TrailPointFunc with the life too. names ours
typedef int (*RC2ParticleFunc)(float flFrametime, Vector* pvecOrigin, Vector* pvecVelocity, Vector* pvecAccel, Vector* pcolColor, float* pflAlpha, float* pflSize, float* pflFade, float* pflLife, int iUser);

// Sprite particles drawn as view-facing quads: 0x48-byte particles, 128 to start, in a malloc'd
// array. Constructor orig 0x5af10.
class CRC2Particles // name ours
{
public:
	CRC2Particles();				// orig 0x5af10 (c2)
	void Add(Vector vecOrigin, Vector vecVelocity, Vector vecAccel, Vector colColor, float flAlpha, float flSize, float flFade, float flLife, int iUser, RC2ParticleFunc pfnThink); // orig 0x5afc0 (c2): into the first dead slot (grows the array when none)
	int Update(float flFrametime);	// orig 0x5b0f0 (c2): moves, fades and ages them; the live count
	void Draw();					// orig 0x5b370 (c2): the quads (TRI_QUADS), with the bound texture
	bool HasLive();					// orig 0x5b9e0 (c2): any particle with life left

	byte m_Data[0xc];				// layout c2's (+0 particles, +4 capacity, +8 live count)
};

//
// c2's effect classes that have functions in c3's range.
//

// CLTE 0x25 (c2's factory orig 0x714f0, think 0x717c0, vtable 0x1011dd80): a player's shield,
// sent when it goes up and killed with CLTE 0xfe/0x25 when it drops (server player.cpp). Its
// transparent draw (orig 0x71ad0) and the hit flash the shield-hit effect gives it (orig
// 0x727a0) are c3's, in shield_tents.cpp. Members as c2's factory sets them.
class CShieldTent : public CClientTent // name ours
{
public:
	bool Think(float flFrametime) override; // orig 0x717c0 (c2)
	void DrawTransparent() override;		// orig 0x71ad0 (c3)
	bool EntityGone(int iEntIndex) override; // orig 0x71a80 (c2)
	void Flash(int iEntIndex, int iStrength); // orig 0x727a0 (c3)

	int m_iEntIndex;	 // orig +0x8: the player
	float m_flFlash;	 // orig +0xc: how bright the hit flash still is, 0-0.8 (Flash raises it, Think fades it)
	bool m_bBand;		 // orig +0x10: the bright band is sweeping
	float m_flBandWait;	 // orig +0x14: time until the next band
	float m_flBandZ;	 // orig +0x18: the band's height
	model_s* m_pSprite;	 // orig +0x1c: sprites/noise1.spr
	model_s* m_pBandSprite; // orig +0x20: sprites/lgtning.spr
	float m_flYaw;		 // orig +0x24
	float m_flRadius;	 // orig +0x28: 40, 30 ducked
	float m_flHeight;	 // orig +0x2c: 20, 26 ducked
	float m_flBandHalf;	 // orig +0x30: the band's half height (2)
	float m_flBandFrame; // orig +0x34: 0-3
	Vector m_vecOrigin;	 // orig +0x38
	Vector m_vecForward; // orig +0x44
	Vector m_vecRight;	 // orig +0x50
	Vector m_vecUp;		 // orig +0x5c
	bool m_bVisible;	 // orig +0x68
};

//
// Compiler-generated copy constructors the original emits out of line at the end of this run
// (orig 0x998f0-0x9b430), used by the factories' copy path (a factory given a non-null pCopy;
// the dispatcher never does that). In C++ they are the implicit copy constructors, so there is
// nothing to write; the effects of c3 note theirs at their factories. These six are of classes
// that aren't c3's:
//
// orig client 0x998f0: c2's CLTE 0x0d effect (factory 0x62d10, vtable 0x1011da14)
// orig client 0x999b0: c2's CLTE 0x10 effect (factory 0x650a0, vtable 0x1011dacc)
// orig client 0x99aa0: c2's CLTE 0x12 effect (factory 0x670b0, vtable 0x1011db30)
// orig client 0x99bb0: c2's CLTE 0x14 effect (factory 0x689b0, vtable 0x1011db74)
// orig client 0x99c30: c2's CLTE 0x1d effect (factory 0x6c600, vtable 0x1011dc78)
// orig client 0x9b430: CClientTent's own (vtable 0x1011d84c, m_iType), called by the inline
//                      copy constructors of c3's CLTE 0x31 and 0x46 effects
