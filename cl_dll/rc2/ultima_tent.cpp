/***
*
*	Rocket Crowbar 2 client: the ultima grenade armed (CLTE 0x37).
*
*	The server's ultimagrenade.cpp sends it when the grenade arms and hangs in the air, and
*	kills it (CLTE 0xfe/0x37) when it blows or aborts. For the arming time ([Grenade]
*	UltimaGrenArmTime from the client's copy of rc2.ini) the client builds a machine of light
*	around it that winds up as the time runs out: four rings of laser beams (a tumbling one of
*	32 units on the grenade, a spinning one of 48 on the ground below with glows at its points,
*	spokes from the grenade to a ring of 104, and a ring that rises from the ground, shrinks,
*	shoots up and fades, over and over), two beams down to the ground ring, one up into the sky
*	and one to the ground, glows, a flare, two lights, particles streaming out along the ground
*	and bursting from the grenade, and smoke trails off a spinning bar and a tumbling
*	tetrahedron; plus a drill sound rising in pitch, and a white flash for a player looking at
*	it when it appears. Below cl_detail 0 the spokes, glows and trails go, at 0 and below the
*	sparks and the rising ring's trails.
*
*	The cvar cl_debuggren turns on a debug counter kept in grendebug.txt in the game directory:
*	each grenade bumps it (1-32) and its bits switch parts of the drawing off.
*
*	The original's functions 0x83c60-0x899ab (and the copy constructor 0x9a540) of client.dll's
*	run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>
#include <cstdlib>

#include "pmtrace.h"
#include "pm_defs.h"
#include "rc2prefs.h"

// c3 needs:
// The client's own copy of rc2.ini: rc2prefs.h's RC2Pref() over the client's g_rc2PrefValues
// (orig .bss 0x10210aa8, filled from the RCCFG message by orig 0xbd490). name ours, after the
// server's

// the engine's sound flags (dlls/util.h; the client has no header with them)
#ifndef SND_CHANGE_VOL
#define SND_CHANGE_VOL (1 << 6)
#define SND_CHANGE_PITCH (1 << 7)
#endif

// the engine's beam and tent flags the effect touches (r_efx.h names)
#define ULTIMA_FTENT_FLAGS (FTENT_SPRANIMATE | FTENT_PERSIST | FTENT_SPRANIMATELOOP) // 0x12100

// A wobbling angle: sin(sin(t * flFreq + flPhase) * flInner + flPhase2) * flAmplitude + flBase.
struct UltimaOscillator // name ours
{
	float flAmplitude; // +0: 180 for pitch, 360 for yaw and roll
	float flInner;	   // +4: 1-2 (0.75-1.5 for the tetrahedron's), either sign
	float flFreq;	   // +8: the same range
	float flBase;	   // +0xc: -90 for pitch, 0 for yaw and roll
	float flPhase2;	   // +0x10: -4-4
	float flPhase;	   // +0x14: -4-4
};

static float EvalOscillator(const UltimaOscillator& osc, float flTime) // name ours, inline in the original
{
	return sin(sin(osc.flFreq * flTime + osc.flPhase) * osc.flInner + osc.flPhase2) * osc.flAmplitude + osc.flBase;
}

// A beam handed back to the engine: no longer endless or entity-bound, it dies now. Inline at
// each use in the original. name ours
static void ReleaseBeam(BEAM*& pBeam)
{
	if (!pBeam)
		return;
	pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
	pBeam->t = 1.0;
	pBeam->die = 0.1;
	pBeam->endEntity = 0;
	pBeam->startEntity = 0;
	pBeam = nullptr;
}

// A beam parked at vecWhere, as good as invisible. Inline in the original. name ours
static void ParkBeam(BEAM* pBeam, const Vector& vecWhere)
{
	pBeam->brightness = 0.01;
	pBeam->target = vecWhere;
	pBeam->source = pBeam->target;
	pBeam->delta = Vector(0, 0, 0);
}

static void SetBeamEnds(BEAM* pBeam, const Vector& vecSource, const Vector& vecTarget) // name ours, inline in the original
{
	pBeam->source = vecSource;
	pBeam->target = vecTarget;
	pBeam->delta = pBeam->target - pBeam->source;
}

// A looping glow tent of animglow01's seven frames. Inline in the original. name ours
static void SetupGlowTent(TEMPENTITY* pTent, float flScale, int iAmount, float flFramerate)
{
	pTent->flags |= ULTIMA_FTENT_FLAGS;
	pTent->entity.curstate.scale = flScale;
	pTent->entity.curstate.renderamt = iAmount;
	pTent->entity.curstate.rendermode = kRenderGlow;
	pTent->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTent->entity.curstate.rendercolor.r = 240;
	pTent->entity.curstate.rendercolor.g = 240;
	pTent->entity.curstate.rendercolor.b = 255;
	pTent->entity.curstate.framerate = flFramerate;
	pTent->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 6);
	pTent->frameMax = 7;
	pTent->die += 10;
}

//
// CLTE 0x37: short the grenade.
//
class CUltimaTent : public CClientTent // name ours
{
public:
	CUltimaTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	Vector OscillatorAngles(const UltimaOscillator* pOsc);
	void UpdateParticles(float flFrametime);

	Vector GroundPoint() const { return Vector(m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z - m_flGroundDist); }

	int m_iEntIndex;			  // orig +0x8
	bool m_bVisible;			  // orig +0xc
	model_s* m_pSmokeSprite;	  // orig +0x10: sprites/gradsmoke2.spr, the trails
	model_s* m_pRingSparkSprite;  // orig +0x14: sprites/glow02.spr
	model_s* m_pBurstSparkSprite; // orig +0x18: sprites/glow03.spr
	Vector m_vecOrigin;			  // orig +0x1c
	float m_flTime;				  // orig +0x28: the oscillators' clock, faster as it winds up
	float m_flGroundDist;		  // orig +0x2c: from the grenade to the ground (144 at most, 8 less on a hit)
	float m_flLife;				  // orig +0x30
	float m_flMaxLife;			  // orig +0x34: [Grenade] UltimaGrenArmTime
	float m_flCharge;			  // orig +0x38: 0-1, 1 - m_flFrac
	float m_flFrac;				  // orig +0x3c: the life left, 1-0
	BEAM* m_pSkyBeam;			  // orig +0x40: up 256 units
	TEMPENTITY* m_pCoreGlow;	  // orig +0x44
	TEMPENTITY* m_pFlare;		  // orig +0x48
	dlight_t* m_pDLight;		  // orig +0x4c
	dlight_t* m_pELight;		  // orig +0x50
	float m_flPulse;			  // orig +0x54: the lights' and flare's pulse phase
	BEAM* m_pGroundBeam;		  // orig +0x58: down to the ground, sprites/xbeam1.spr
	TEMPENTITY* m_pGroundGlow;	  // orig +0x5c

	// the ground ring: 8 points of radius m_flGroundRingRadius, spinning about the vertical
	Vector m_vecGroundRing[9];	   // orig +0x60, the last a copy of the first
	Vector m_vecGroundRingPos[8];  // orig +0xcc: the points in the world
	BEAM* m_pGroundRingBeam[8];	   // orig +0x12c
	TEMPENTITY* m_pGroundRingGlow[8]; // orig +0x14c: only with cl_detail >= 0
	Vector m_vecGroundRingAngles;  // orig +0x16c
	Vector m_vecGroundRingSpin;	   // orig +0x178: 200-250 a second of yaw, either way
	float m_flGroundRingRadius;	   // orig +0x184: 48

	// the spoke ring: 7 points of radius 104 round the grenade, beams to them from it
	Vector m_vecSpokeRing[8];	  // orig +0x188, the last a copy of the first
	Vector m_vecSpokeRingPos[7];  // orig +0x1e8
	BEAM* m_pSpokeBeam[7];		  // orig +0x23c: only with cl_detail >= 0
	Vector m_vecSpokeRingAngles;  // orig +0x258
	Vector m_vecSpokeRingSpin;	  // orig +0x264: 100-150 of yaw, against the ground ring

	BEAM* m_pDownBeam[2];			// orig +0x270: from the grenade to ground ring points 0 and 4
	CTentParticles m_RingSparks;		// orig +0x278: streaming out along the ground
	float m_flRingSparksDue;		// orig +0x284
	CTentParticles m_BurstSparks;	// orig +0x288: bursting from the grenade
	float m_flBurstSparksDue;		// orig +0x294
	Vector m_vecBarAngles;			// orig +0x298
	Vector m_vecBarSpin;			// orig +0x2a4
	CTentTrail m_BarTrail[2];		// orig +0x2b0
	Vector m_vecBarEnd[2];			// orig +0x300: the grenade +- 48 along the bar
	float m_flBarTrailDue;			// orig +0x318

	// the tetrahedron: four vertices 16 out, tumbling by m_TetraOsc
	Vector m_vecTetra[4];			// orig +0x31c
	Vector m_vecTetraPos[4];		// orig +0x34c
	UltimaOscillator m_TetraOsc[3]; // orig +0x37c
	Vector m_vecTetraAngles;		// orig +0x3c4
	float m_flTetraTrailDue;		// orig +0x3d0
	CTentTrail m_TetraTrail[4];		// orig +0x3d4

	// the tumbling ring: 8 points of radius 32 on the grenade, turned by m_RingOsc
	Vector m_vecTumbleRing[9];		// orig +0x474, the last a copy of the first
	Vector m_vecTumbleRingPos[8];	// orig +0x4e0
	BEAM* m_pTumbleBeam[8];			// orig +0x540
	Vector m_vecTumbleRingAngles;	// orig +0x560
	UltimaOscillator m_RingOsc[3];	// orig +0x56c

	// the rising ring: 8 unit points scaled by m_flRiseRadius, at m_flRiseZ above the grenade
	Vector m_vecRiseRing[9];		// orig +0x5b4, the last a copy of the first
	Vector m_vecRiseRingPos[8];		// orig +0x620
	BEAM* m_pRiseBeam[8];			// orig +0x680
	Vector m_vecRiseRingAngles;		// orig +0x6a0
	Vector m_vecRiseRingSpin;		// orig +0x6ac: 400-450 of yaw, against the ground ring; flips each cycle
	float m_flRiseZ;				// orig +0x6b8
	float m_flRiseAlpha;			// orig +0x6bc
	float m_flRiseRadius;			// orig +0x6c0
	float m_flRiseWait;				// orig +0x6c4: -1 while it runs, else the wait for the next cycle
	CTentTrail m_RiseTrail[2][2];	// orig +0x6c8: two pairs, from points 0 and 4; they alternate by cycle
	int m_iRiseTrailSet;			// orig +0x768
	float m_flRiseTrailDue;			// orig +0x76c
	short m_sDebug;					// orig +0x770: the cl_debuggren counter; its complement's bits gate the drawing
};

// The factory (the dispatcher's case 0x37). pCopy: through the implicit copy constructor, out
// of line in the original (orig client 0x9a540); the copy shares every beam, tent, light,
// trail and particle array.
CClientTent* RC2_NewUltimaTent(const CClientTent* pCopy) // orig client 0x83c60
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CUltimaTent* pTent = pCopy ? new CUltimaTent(*static_cast<const CUltimaTent*>(pCopy)) : new CUltimaTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

CUltimaTent::CUltimaTent() // orig client 0x83dd0
{
	m_iEntIndex = READ_SHORT();
	m_bVisible = false;
	m_pSmokeSprite = nullptr;
	m_vecOrigin = Vector(0, 0, 0);
	m_flPulse = gEngfuncs.pfnRandomFloat(0, 6.2831855);
	m_flTime = 0;
	// Original bug (crash), guarded: the original leaves the down beams unset when
	// laserbeam.spr doesn't load (Think's guard below then holds)
	m_pDownBeam[0] = nullptr;
	m_pDownBeam[1] = nullptr;
	m_pGroundBeam = nullptr;
	m_pSkyBeam = nullptr;
	m_pFlare = nullptr;
	m_pCoreGlow = nullptr;
	m_pGroundGlow = nullptr;
	m_flBurstSparksDue = 0;
	m_flRingSparksDue = 10;
	m_flRiseWait = 0;
	m_iRiseTrailSet = 0;
	m_flMaxLife = RC2Pref(RC2P_Grenade_UltimaGrenArmTime).f;
	m_flLife = RC2Pref(RC2P_Grenade_UltimaGrenArmTime).f;
	m_flCharge = 0;
	m_flFrac = 1.0;
	m_sDebug = 0;

	// the debug counter: read, bumped (1-32), written back
	if ((int)cl_debuggren->value != 0)
	{
		char szPath[256];
		char szLine[256];
		int iSecond = 0;
		sprintf(szPath, "%s/grendebug.txt", gEngfuncs.pfnGetGameDirectory());
		FILE* pFile = fopen(szPath, "r");
		if (pFile)
		{
			fgets(szLine, 255, pFile);
			szLine[255] = 0;
			m_sDebug = atoi(szLine);
			fgets(szLine, 255, pFile);
			szLine[255] = 0;
			iSecond = atoi(szLine);
			fclose(pFile);
		}
		m_sDebug++;
		if (m_sDebug > 32)
			m_sDebug = 1;
		if (m_sDebug < 1)
			m_sDebug = 1;
		pFile = fopen(szPath, "w");
		if (pFile)
		{
			fprintf(pFile, "%d\n%d\n", (int)m_sDebug, (int)(short)iSecond);
			fflush(pFile);
			fclose(pFile);
		}
	}

	float flAngle = 0;
	for (int i = 0; i < 8; i++, flAngle += 0.7853982)
	{
		m_vecTumbleRing[i] = Vector(cos(flAngle) * 32.0, sin(flAngle) * 32.0, 0);
		m_pTumbleBeam[i] = nullptr;
		m_vecTumbleRingPos[i] = Vector(0, 0, 0);
	}
	m_vecTumbleRing[8] = m_vecTumbleRing[0];
	m_vecTumbleRingAngles = Vector(0, 0, 0);

	{
		const float flRoll = gEngfuncs.pfnRandomFloat(0, 360);
		const float flYaw = gEngfuncs.pfnRandomFloat(0, 360);
		const float flPitch = gEngfuncs.pfnRandomFloat(-90, 90);
		m_vecBarAngles = Vector(flPitch, flYaw, flRoll);
	}
	int iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecBarSpin.x = gEngfuncs.pfnRandomFloat(150, 200) * iDir;
	iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecBarSpin.y = gEngfuncs.pfnRandomFloat(100, 150) * iDir;
	iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecBarSpin.z = gEngfuncs.pfnRandomFloat(75, 125) * iDir;
	m_flBarTrailDue = 0;

	m_flGroundRingRadius = 48;
	flAngle = 0;
	for (int i = 0; i < 8; i++, flAngle += 0.7853982)
	{
		m_vecGroundRing[i] = Vector(cos(flAngle) * m_flGroundRingRadius, sin(flAngle) * m_flGroundRingRadius, 0);
		m_pGroundRingBeam[i] = nullptr;
		m_pGroundRingGlow[i] = nullptr;
		m_vecGroundRingPos[i] = Vector(0, 0, 0);
	}
	m_vecGroundRing[8] = m_vecGroundRing[0];
	m_vecGroundRingAngles = Vector(0, gEngfuncs.pfnRandomFloat(0, 360), 0);
	m_vecGroundRingSpin.z = 0;
	m_vecGroundRingSpin.x = 0;
	iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecGroundRingSpin.y = gEngfuncs.pfnRandomFloat(200, 250) * iDir;

	flAngle = 0;
	for (int i = 0; i < 7; i++, flAngle += 0.8975979)
	{
		m_vecSpokeRing[i] = Vector(cos(flAngle) * 104.0, sin(flAngle) * 104.0, 0);
		m_pSpokeBeam[i] = nullptr;
		m_vecSpokeRingPos[i] = Vector(0, 0, 0);
	}
	m_vecSpokeRing[7] = m_vecSpokeRing[0];
	m_vecSpokeRingAngles = Vector(0, gEngfuncs.pfnRandomFloat(0, 360), 0);
	m_vecSpokeRingSpin.z = 0;
	m_vecSpokeRingSpin.x = 0;
	m_vecSpokeRingSpin.y = gEngfuncs.pfnRandomFloat(100, 150) * (m_vecGroundRingSpin.y < 0 ? 1 : -1);

	flAngle = 0;
	for (int i = 0; i < 8; i++, flAngle += 0.7853982)
	{
		m_vecRiseRing[i] = Vector(cos(flAngle), sin(flAngle), 0);
		m_pRiseBeam[i] = nullptr;
		m_vecRiseRingPos[i] = Vector(0, 0, 0);
	}
	m_vecRiseRing[8] = m_vecRiseRing[0];
	m_vecRiseRingAngles = Vector(0, gEngfuncs.pfnRandomFloat(0, 360), 0);
	m_vecRiseRingSpin.z = 0;
	m_vecRiseRingSpin.x = 0;
	m_vecRiseRingSpin.y = gEngfuncs.pfnRandomFloat(400, 450) * (m_vecGroundRingSpin.y <= 0 ? 1 : -1);

	// a regular tetrahedron of radius 16: the apex along x, the base 19.47 degrees behind
	const float flBaseCos = cos(0.3403392);
	const float flBaseSin = -sin(0.3403392);
	flAngle = 0;
	for (int i = 0; i < 3; i++, flAngle += 2.0943952)
		m_vecTetra[i + 1] = Vector(flBaseSin * 16, cos(flAngle) * flBaseCos * 16, sin(flAngle) * flBaseCos * 16);
	m_vecTetra[0] = Vector(16, 0, 0);
	for (int i = 0; i < 4; i++)
		m_vecTetraPos[i] = Vector(0, 0, 0);
	m_vecTetraAngles = Vector(0, 0, 0);
	m_flTetraTrailDue = 0;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, m_vecOrigin, CHAN_STATIC, "ambience/alien_minddrill.wav", 0.4, 0.25, 0, 80);

	// how far down the ground is
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	pmtrace_t tr;
	Vector vecDown(m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z - 144);
	gEngfuncs.pEventAPI->EV_PlayerTrace(m_vecOrigin, vecDown, PM_WORLD_ONLY, -1, &tr);
	m_flGroundDist = m_vecOrigin.z - tr.endpos[2];
	if (tr.fraction < 1.0)
		m_flGroundDist -= 8;

	for (int i = 0; i < 3; i++)
	{
		iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
		m_RingOsc[i].flInner = gEngfuncs.pfnRandomFloat(1, 2) * iDir;
		iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
		m_RingOsc[i].flFreq = gEngfuncs.pfnRandomFloat(1, 2) * iDir;
		m_RingOsc[i].flPhase2 = gEngfuncs.pfnRandomFloat(-4, 4);
		m_RingOsc[i].flPhase = gEngfuncs.pfnRandomFloat(-4, 4);
		iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
		m_TetraOsc[i].flInner = gEngfuncs.pfnRandomFloat(0.75, 1.5) * iDir;
		iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
		m_TetraOsc[i].flFreq = gEngfuncs.pfnRandomFloat(0.75, 1.5) * iDir;
		m_TetraOsc[i].flPhase2 = gEngfuncs.pfnRandomFloat(-4, 4);
		m_TetraOsc[i].flPhase = gEngfuncs.pfnRandomFloat(-4, 4);
	}
	m_TetraOsc[0].flBase = -90;
	m_TetraOsc[0].flAmplitude = 180;
	m_RingOsc[0].flAmplitude = 180;
	m_RingOsc[0].flBase = -90;
	m_TetraOsc[1].flBase = 0;
	m_TetraOsc[1].flAmplitude = 360;
	m_RingOsc[1].flAmplitude = 360;
	m_TetraOsc[2].flAmplitude = 360;
	m_RingOsc[2].flAmplitude = 360;
	m_RingOsc[1].flBase = 0;
	m_TetraOsc[2].flBase = 0;
	m_RingOsc[2].flBase = 0;

	int iModelIndex;
	m_pSmokeSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke2.spr", &iModelIndex);
	m_pRingSparkSprite = gEngfuncs.CL_LoadModel("sprites/glow02.spr", &iModelIndex);
	m_pBurstSparkSprite = gEngfuncs.CL_LoadModel("sprites/glow03.spr", &iModelIndex);

	if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModelIndex))
	{
		// every beam starts collapsed on the grenade; life 0 makes them endless
		for (int i = 0; i < 8; i++)
		{
			m_vecTumbleRingPos[i] = m_vecOrigin;
			m_pTumbleBeam[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iModelIndex, 0, 1.25, 0.5, 0.95, 0, 0, 0, 0.9, 0.9, 1.0);
		}
		for (int i = 0; i < 8; i++)
		{
			m_vecGroundRingPos[i] = m_vecOrigin;
			m_pGroundRingBeam[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iModelIndex, 0, 1.0, 0.25, 0.95, 0, 0, 0, 0.85, 0.85, 1.0);
		}
		for (int i = 0; i < 8; i++)
		{
			m_vecRiseRingPos[i] = m_vecOrigin;
			m_pRiseBeam[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iModelIndex, 0, 0.5, 0.05, 0.95, 0, 0, 0, 0.95, 0.95, 1.0);
		}
		if (g_iClientDetail >= 0)
		{
			for (int i = 0; i < 7; i++)
			{
				m_vecSpokeRingPos[i] = m_vecOrigin;
				m_pSpokeBeam[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iModelIndex, 0, 0.75, 0.05, 0.95, 3, 0, 0, 0.85, 0.85, 1.0);
				if (m_pSpokeBeam[i])
					m_pSpokeBeam[i]->flags |= FBEAM_SHADEOUT;
			}
		}
		for (int i = 0; i < 2; i++)
			m_pDownBeam[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iModelIndex, 0, 0.75, 0.1, 0.8, -2, 0, 0, 0.9, 0.9, 1.0);

		Vector vecSky(m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z + 256);
		m_pSkyBeam = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, vecSky, iModelIndex, 0, 5, 0.15, 0.95, 1, 0, 0, 0.9, 0.9, 1.0);
		if (m_pSkyBeam)
			m_pSkyBeam->flags |= FBEAM_SHADEOUT;
	}

	if (gEngfuncs.CL_LoadModel("sprites/xbeam1.spr", &iModelIndex))
	{
		Vector vecGround = GroundPoint();
		m_pGroundBeam = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, vecGround, iModelIndex, 0, 4, 0.05, 0.95, -6, 0, 0, 0.9, 0.9, 1.0);
	}

	model_s* pGlowModel = gEngfuncs.CL_LoadModel("sprites/animglow01.spr", &iModelIndex);
	if (pGlowModel)
	{
		Vector vecGround = GroundPoint();
		m_pGroundGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecGround, pGlowModel);
		if (m_pGroundGlow)
			SetupGlowTent(m_pGroundGlow, 0.4, 254, 18);

		m_pCoreGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pGlowModel);
		if (m_pCoreGlow)
			SetupGlowTent(m_pCoreGlow, 1.0, 254, 20);

		if (g_iClientDetail >= 0)
		{
			for (int i = 0; i < 8; i++)
			{
				m_pGroundRingGlow[i] = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pGlowModel);
				if (!m_pGroundRingGlow[i])
					continue;
				// the two the down beams reach are bigger
				if (i == 0 || i == 4)
					SetupGlowTent(m_pGroundRingGlow[i], 0.3, 254, 18);
				else
					SetupGlowTent(m_pGroundRingGlow[i], 0.2, 200, 18);
			}
		}
	}

	model_s* pFlareModel = gEngfuncs.CL_LoadModel("sprites/bigflare1.spr", &iModelIndex);
	if (pFlareModel)
	{
		m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pFlareModel);
		if (m_pFlare)
		{
			m_pFlare->flags |= FTENT_PERSIST;
			m_pFlare->entity.curstate.scale = 2.0;
			m_pFlare->entity.curstate.renderamt = 180;
			m_pFlare->entity.curstate.rendermode = kRenderGlow;
			m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
			m_pFlare->entity.curstate.rendercolor.r = 225;
			m_pFlare->entity.curstate.rendercolor.g = 220;
			m_pFlare->entity.curstate.rendercolor.b = 255;
			m_pFlare->die += 10;
		}
	}

	m_pDLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
	if (m_pDLight)
	{
		m_pDLight->origin = m_vecOrigin;
		m_pDLight->radius = 400;
		m_pDLight->color.r = 220;
		m_pDLight->color.g = 220;
		m_pDLight->color.b = 255;
		m_pDLight->die = gEngfuncs.GetClientTime() + 10.0;
	}
	m_pELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
	if (m_pELight)
	{
		m_pELight->origin = m_vecOrigin;
		m_pELight->radius = 300;
		m_pELight->color.r = 220;
		m_pELight->color.g = 220;
		m_pELight->color.b = 255;
		m_pELight->die = gEngfuncs.GetClientTime() + 10.0;
	}

	// a teleport flash where it appears (not kept)
	model_s* pTeleModel = gEngfuncs.CL_LoadModel("sprites/b-tele1.spr", &iModelIndex);
	if (pTeleModel)
	{
		TEMPENTITY* pTele = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pTeleModel);
		if (pTele)
		{
			pTele->die += 5;
			pTele->entity.curstate.framerate = 28;
			pTele->flags |= FTENT_SPRANIMATE;
			pTele->entity.curstate.scale = 2.0;
			pTele->entity.curstate.renderamt = 250;
			pTele->entity.curstate.rendermode = kRenderGlow;
			pTele->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTele->entity.curstate.rendercolor.r = 255;
			pTele->entity.curstate.rendercolor.g = 255;
			pTele->entity.curstate.rendercolor.b = 255;
			pTele->entity.curstate.frame = 0;
			pTele->frameMax = 25;
		}
	}

	// a white flash for the local player if he faces it and can see it
	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	if (pPlayer)
	{
		Vector vecViewOfs, vecAngles, vecForward;
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecViewOfs);
		gEngfuncs.GetViewAngles(vecAngles);
		gEngfuncs.pfnAngleVectors(vecAngles, vecForward, nullptr, nullptr);
		Vector vecEye = pPlayer->origin + vecViewOfs;
		Vector vecToIt = m_vecOrigin - vecEye;
		// the original normalizes it componentwise, (0, 0, 1) for a zero vector
		const float flLength = vecToIt.Length();
		Vector vecDir = flLength == 0 ? Vector(0, 0, 1) : vecToIt * (1.0 / flLength);
		if (DotProduct(vecForward, vecDir) > 0)
		{
			// VERIFY: the trace skips physent m_iEntIndex, an entity index used as a physent one
			gEngfuncs.pEventAPI->EV_PlayerTrace(m_vecOrigin, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, m_iEntIndex, &tr);
			if (tr.fraction >= 1.0 || (tr.ent != 0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == pPlayer->index))
				V_StartScreenTint(255, 255, 255, 255, 255, 255, 200, 0, 0.4, 1);
		}
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// The three oscillators' angles at m_flTime. Inline for m_RingOsc in the think.
Vector CUltimaTent::OscillatorAngles(const UltimaOscillator* pOsc) // orig client 0x88ac0
{
	return Vector(EvalOscillator(pOsc[0], m_flTime), EvalOscillator(pOsc[1], m_flTime), EvalOscillator(pOsc[2], m_flTime));
}

bool CUltimaTent::Update(float flFrametime) // orig client 0x85a20
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		return true;
	}

	m_flTime += (m_flCharge * 0.15 + 1.0) * flFrametime;
	m_flLife -= flFrametime;
	m_flFrac = m_flLife / m_flMaxLife;
	if (m_flFrac < 0)
		m_flFrac = 0;
	else if (m_flFrac > 1)
		m_flFrac = 1;
	m_flCharge = 1.0 - m_flFrac;

	// keep the tents and lights alive
	if (m_pCoreGlow)
		m_pCoreGlow->die = gEngfuncs.GetClientTime() + 10;
	if (m_pFlare)
		m_pFlare->die = gEngfuncs.GetClientTime() + 10;
	if (m_pGroundGlow)
		m_pGroundGlow->die = gEngfuncs.GetClientTime() + 10;
	if (m_pDLight)
		m_pDLight->die = gEngfuncs.GetClientTime() + 10;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 10;
	for (int i = 0; i < 8; i++)
	{
		if (m_pGroundRingGlow[i])
			m_pGroundRingGlow[i]->die = gEngfuncs.GetClientTime() + 10;
	}

	m_bVisible = true;
	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	if (pPlayer && pEntity->curstate.messagenum < pPlayer->curstate.messagenum)
	{
		// out of the local player's PVS: park everything out of the way
		const Vector vecAway(4096, 4096, 4096);
		if (m_pCoreGlow)
			m_pCoreGlow->entity.origin = vecAway;
		if (m_pFlare)
			m_pFlare->entity.origin = vecAway;
		if (m_pGroundGlow)
			m_pGroundGlow->entity.origin = vecAway;
		if (m_pDLight)
			m_pDLight->origin = vecAway;
		if (m_pELight)
			m_pELight->origin = vecAway;
		for (int i = 0; i < 8; i++)
		{
			if (m_pGroundRingGlow[i])
				m_pGroundRingGlow[i]->entity.origin = vecAway;
			if (m_pGroundRingBeam[i])
				ParkBeam(m_pGroundRingBeam[i], vecAway);
		}
		for (int i = 0; i < 8; i++)
		{
			if (m_pRiseBeam[i])
				ParkBeam(m_pRiseBeam[i], vecAway);
		}
		for (int i = 0; i < 8; i++)
		{
			if (m_pTumbleBeam[i])
				ParkBeam(m_pTumbleBeam[i], vecAway);
		}
		m_bVisible = false;
		return false;
	}

	m_vecOrigin = pEntity->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, m_vecOrigin, CHAN_STATIC, "ambience/alien_minddrill.wav", m_flCharge * 0.6 + 0.4, 0.25, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(m_flCharge * 40 + 80));

	Vector vecForward, vecRight, vecUp;
	const float flSpinScale = m_flCharge * 0.25 + 1.0;
	const Vector vecGround = GroundPoint();

	// the tumbling ring on the grenade
	m_vecTumbleRingAngles = Vector(EvalOscillator(m_RingOsc[0], m_flTime), EvalOscillator(m_RingOsc[1], m_flTime), EvalOscillator(m_RingOsc[2], m_flTime));
	gEngfuncs.pfnAngleVectors(m_vecTumbleRingAngles, vecForward, vecRight, vecUp);
	for (int i = 0; i < 8; i++)
	{
		if (!m_pTumbleBeam[i])
			continue;
		const Vector& p = m_vecTumbleRing[i];
		const Vector& q = m_vecTumbleRing[i + 1];
		m_vecTumbleRingPos[i] = m_vecOrigin + vecForward * p.x + vecRight * p.y + vecUp * p.z;
		SetBeamEnds(m_pTumbleBeam[i], m_vecTumbleRingPos[i], m_vecOrigin + vecForward * q.x + vecRight * q.y + vecUp * q.z);
	}

	// the ground ring, its glows, and the two beams down to it
	m_vecGroundRingAngles = m_vecGroundRingAngles + m_vecGroundRingSpin * flFrametime * flSpinScale;
	gEngfuncs.pfnAngleVectors(m_vecGroundRingAngles, vecForward, vecRight, vecUp);
	for (int i = 0; i < 8; i++)
	{
		const Vector& p = m_vecGroundRing[i];
		const Vector& q = m_vecGroundRing[i + 1];
		m_vecGroundRingPos[i] = vecGround + vecForward * p.x + vecRight * p.y + vecUp * p.z;
		if (m_pGroundRingBeam[i])
			SetBeamEnds(m_pGroundRingBeam[i], m_vecGroundRingPos[i], vecGround + vecForward * q.x + vecRight * q.y + vecUp * q.z);
		if (m_pGroundRingGlow[i])
			m_pGroundRingGlow[i]->entity.origin = m_vecGroundRingPos[i];
	}
	// Original bug (crash), guarded: no null check on the down beams in the original; one is
	// NULL when the engine's beam list was full (R_BeamPoints) or laserbeam.spr didn't load.
	// Both targets are the grenade's origin, as the original's copy of [1]'s into [0]'s.
	if (m_pDownBeam[1])
	{
		m_pDownBeam[1]->target = m_vecOrigin;
		m_pDownBeam[1]->source = vecGround + vecForward * m_vecGroundRing[4].x + vecRight * m_vecGroundRing[4].y + vecUp * m_vecGroundRing[4].z;
		m_pDownBeam[1]->delta = m_pDownBeam[1]->target - m_pDownBeam[1]->source;
	}
	if (m_pDownBeam[0])
	{
		m_pDownBeam[0]->target = m_vecOrigin;
		m_pDownBeam[0]->source = vecGround + vecForward * m_vecGroundRing[0].x + vecRight * m_vecGroundRing[0].y + vecUp * m_vecGroundRing[0].z;
		m_pDownBeam[0]->delta = m_pDownBeam[0]->target - m_pDownBeam[0]->source;
	}

	// the spokes
	m_vecSpokeRingAngles = m_vecSpokeRingAngles + m_vecSpokeRingSpin * flFrametime * flSpinScale;
	gEngfuncs.pfnAngleVectors(m_vecSpokeRingAngles, vecForward, vecRight, vecUp);
	for (int i = 0; i < 7; i++)
	{
		if (!m_pSpokeBeam[i])
			continue;
		const Vector& p = m_vecSpokeRing[i];
		m_vecSpokeRingPos[i] = m_vecOrigin + vecForward * p.x + vecRight * p.y + vecUp * p.z;
		SetBeamEnds(m_pSpokeBeam[i], m_vecOrigin, m_vecSpokeRingPos[i]);
	}

	// the rising ring
	if (m_flRiseWait == -1.0)
	{
		if (m_flRiseZ < 0)
		{
			// rising from the ground to the grenade, shrinking to 16, brightening
			const float flTime = m_flFrac * 0.2 + 0.3;
			m_flRiseZ += m_flGroundDist / flTime * flFrametime;
			m_flRiseRadius -= (m_flGroundRingRadius - 16.0) / flTime * flFrametime;
			m_flRiseAlpha += flFrametime * 4.0;
			if (m_flRiseAlpha > 0.95)
				m_flRiseAlpha = 0.95;
			if (m_flRiseRadius < 16.0)
				m_flRiseRadius = 16;
		}
		else
		{
			// shooting up 256 units and fading, then a wait
			const float flTime = m_flFrac * 0.25 + 0.5;
			m_flRiseRadius = 16;
			m_flRiseZ += 256.0 / flTime * flFrametime;
			m_flRiseAlpha -= 1.0 / flTime * flFrametime;
			if (m_flRiseAlpha <= 0.01)
			{
				m_flRiseAlpha = 0.01;
				m_flRiseWait = m_flFrac * 0.125 + 0.025;
			}
		}

		m_vecRiseRingAngles = m_vecRiseRingAngles + m_vecRiseRingSpin * flFrametime * flSpinScale;
		gEngfuncs.pfnAngleVectors(m_vecRiseRingAngles, vecForward, vecRight, vecUp);
		const Vector vecCentre(m_vecOrigin.x, m_vecOrigin.y, m_flRiseZ + m_vecOrigin.z);
		for (int i = 0; i < 8; i++)
		{
			if (!m_pRiseBeam[i])
				continue;
			const Vector& p = m_vecRiseRing[i];
			const Vector& q = m_vecRiseRing[i + 1];
			m_vecRiseRingPos[i] = vecCentre + (vecForward * p.x + vecRight * p.y + vecUp * p.z) * m_flRiseRadius;
			SetBeamEnds(m_pRiseBeam[i], m_vecRiseRingPos[i], vecCentre + (vecForward * q.x + vecRight * q.y + vecUp * q.z) * m_flRiseRadius);
			m_pRiseBeam[i]->brightness = m_flRiseAlpha;
		}

		if (g_iClientDetail > 0)
		{
			m_flRiseTrailDue -= flFrametime;
			if (m_flRiseTrailDue <= 0)
			{
				m_flRiseTrailDue = 0.04;
				m_RiseTrail[m_iRiseTrailSet][0].AddPoint(m_vecRiseRingPos[0], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.6, 0.6, 1.0), m_flRiseAlpha * 0.7, 1.0, 0.53846157, 0, nullptr);
				m_RiseTrail[m_iRiseTrailSet][1].AddPoint(m_vecRiseRingPos[4], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.6, 0.6, 1.0), m_flRiseAlpha * 0.7, 1.0, 0.53846157, 0, nullptr);
			}
			m_RiseTrail[0][0].Update(flFrametime);
			m_RiseTrail[0][1].Update(flFrametime);
			m_RiseTrail[1][0].Update(flFrametime);
			m_RiseTrail[1][1].Update(flFrametime);
		}
	}
	else
	{
		for (int i = 0; i < 8; i++)
		{
			if (m_pRiseBeam[i])
				ParkBeam(m_pRiseBeam[i], m_vecOrigin);
		}
		m_flRiseWait -= flFrametime;
		if (m_flRiseWait < 0)
		{
			// a new cycle from the ground, turning the other way, into the other pair of trails
			m_flRiseWait = -1.0;
			m_vecRiseRingAngles.y = gEngfuncs.pfnRandomFloat(0, 360);
			m_vecRiseRingSpin.y = m_vecRiseRingSpin.y * -1.0;
			m_iRiseTrailSet = m_iRiseTrailSet == 0;
			m_RiseTrail[m_iRiseTrailSet][0].Reset();
			m_RiseTrail[m_iRiseTrailSet][1].Reset();
			m_flRiseAlpha = 0.25;
			m_flRiseZ = -m_flGroundDist;
			m_flRiseRadius = m_flGroundRingRadius;
		}
	}

	// the glows and lights pulse a third of a turn a second, faster as it winds up
	m_flPulse += (m_flCharge * 0.15 + 1.0) * flFrametime * 2.0943952;
	const float flPulse = sin(m_flPulse);
	if (m_pCoreGlow)
		m_pCoreGlow->entity.origin = m_vecOrigin;
	if (m_pFlare)
	{
		m_pFlare->entity.origin = m_vecOrigin;
		m_pFlare->entity.curstate.renderamt = (int)(flPulse * 10 + m_flCharge * 30 + 140);
	}
	if (m_pGroundGlow)
	{
		m_pGroundGlow->entity.origin = m_vecOrigin;
		m_pGroundGlow->entity.origin[2] -= m_flGroundDist;
	}
	if (m_pDLight)
	{
		m_pDLight->origin = m_vecOrigin;
		m_pDLight->radius = flPulse * 20 + m_flCharge * 40 + 360;
	}
	if (m_pELight)
	{
		m_pELight->origin = m_vecOrigin;
		m_pELight->radius = flPulse * 20 + m_flCharge * 40 + 230;
	}

	if (g_iClientDetail >= 0)
	{
		// the spinning bar's two trails
		m_vecBarAngles = m_vecBarAngles + m_vecBarSpin * flFrametime * flSpinScale;
		gEngfuncs.pfnAngleVectors(m_vecBarAngles, vecForward, vecRight, vecUp);
		m_vecBarEnd[0] = m_vecOrigin + vecForward * 48;
		m_vecBarEnd[1] = m_vecOrigin - vecForward * 48;
		m_flBarTrailDue -= flFrametime;
		if (m_flBarTrailDue <= 0)
		{
			if (g_iClientDetail == 0)
				m_flBarTrailDue = 0.075;
			else
				m_flBarTrailDue = 0.05;
			m_BarTrail[0].AddPoint(m_vecBarEnd[0], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.9, 0.9, 1.0), 0.8, 2, 1.0666667, 0, nullptr);
			m_BarTrail[1].AddPoint(m_vecBarEnd[1], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.9, 0.9, 1.0), 0.8, 2, 1.0666667, 0, nullptr);
		}
		m_BarTrail[0].Update(flFrametime);
		m_BarTrail[1].Update(flFrametime);

		if (g_iClientDetail > 0)
		{
			// the tumbling tetrahedron, shedding trails that fly out and turn blue
			m_vecTetraAngles = OscillatorAngles(m_TetraOsc);
			gEngfuncs.pfnAngleVectors(m_vecTetraAngles, vecForward, vecRight, vecUp);
			for (int i = 0; i < 4; i++)
			{
				const Vector& p = m_vecTetra[i];
				m_vecTetraPos[i] = m_vecOrigin + vecForward * p.x + vecRight * p.y + vecUp * p.z;
			}
			m_flTetraTrailDue -= flFrametime;
			if (m_flTetraTrailDue <= 0)
			{
				m_flTetraTrailDue = 0.05;
				for (int i = 0; i < 4; i++)
				{
					const Vector vecDir = (m_vecTetraPos[i] - m_vecOrigin).Normalize();
					m_TetraTrail[i].AddPoint(m_vecTetraPos[i], vecDir * 160, Vector(0, 0, 0), Vector(0.8, 0.8, 1.0), 0.95, 1.5, 1.3571428, 0, Point_Darken);
				}
			}
			for (int i = 0; i < 4; i++)
				m_TetraTrail[i].Update(flFrametime);
		}
	}

	UpdateParticles(flFrametime);
	return false;
}

// The sparks: streaming out along the ground to the ground ring's radius (20-30 a second,
// more as it winds up), and above cl_detail 0 bursting from the grenade (30-50 a second).
void CUltimaTent::UpdateParticles(float flFrametime) // orig client 0x88b50
{
	if (g_iClientDetail < 0)
		return;

	float flRate;
	if (g_iClientDetail == 0)
		flRate = m_flCharge * 10 + 20;
	else
		flRate = m_flCharge * 15 + 30;
	m_flRingSparksDue += flRate * flFrametime;
	if (m_flRingSparksDue >= 1.0)
	{
		const Vector vecGround = GroundPoint();
		while (m_flRingSparksDue >= 1.0)
		{
			m_flRingSparksDue -= 1.0;
			const float flA = gEngfuncs.pfnRandomFloat(-1, 1);
			const float flB = gEngfuncs.pfnRandomFloat(-1, 1);
			// VERIFY: which random number goes to x and which to y
			const float flLength = sqrt(flA * flA + flB * flB);
			const Vector vecDir = flLength == 0 ? Vector(0, 0, 1) : Vector(flB / flLength, flA / flLength, 0);
			const float flLife = gEngfuncs.pfnRandomFloat(0.3, 0.4) * (1.0 - m_flCharge * 0.1);
			const Vector vecVelocity = vecDir * (m_flGroundRingRadius / flLife);
			m_RingSparks.Add(vecGround, vecVelocity, Vector(0, 0, 0), Vector(0.9, 0.9, 1.0), 0.8, 3, 0.4 / flLife, flLife, 0, nullptr);
		}
	}
	m_RingSparks.Update(flFrametime);

	if (g_iClientDetail > 0)
	{
		m_flBurstSparksDue += (m_flCharge * 20 + 30) * flFrametime;
		while (m_flBurstSparksDue >= 1.0)
		{
			m_flBurstSparksDue -= 1.0;
			const float flA = gEngfuncs.pfnRandomFloat(-1, 1);
			const float flB = gEngfuncs.pfnRandomFloat(-1, 1);
			const float flC = gEngfuncs.pfnRandomFloat(-1, 1);
			const Vector vecDir = Vector(flC, flB, flA).Normalize();
			const float flLife = gEngfuncs.pfnRandomFloat(0.5, 0.7);
			const float flSpeed = gEngfuncs.pfnRandomFloat(350, 450) * (m_flCharge * 0.5 + 1.0);
			// each slows to a stop over its life
			const Vector vecAccel = -vecDir * (flSpeed / flLife);
			m_BurstSparks.Add(m_vecOrigin, vecDir * flSpeed, vecAccel, Vector(0.7, 0.7, 1.0), 0.95, 3, 0.95 / flLife, flLife, 0, nullptr);
		}
		m_BurstSparks.Update(flFrametime);
	}
}

// The trails and sparks (the beams, tents and lights draw themselves). m_sDebug's complement
// gates each part: bit 0 the bar's trails, 1 the tetrahedron's, 2 the rising ring's, 3 the
// ground sparks, 4 the burst sparks; with cl_debuggren off it is all ones.
void CUltimaTent::DrawTransparent() // orig client 0x892a0
{
	const short sMask = ~m_sDebug;
	if (!m_bVisible || g_iClientDetail < 0)
		return;

	// VERIFY: the original tests the first bar trail twice and the first rising trail four
	// times, never the others (kept)
	bool bBarTrails = true;
	bool bTetraTrails = true;
	bool bRiseTrails = true;
	bool bRingSparks = true;
	bool bBurstSparks = true;
	if (!m_BarTrail[0].CanDraw() && !m_BarTrail[0].CanDraw())
		bBarTrails = false;
	if (!m_RingSparks.HasAlive())
		bRingSparks = false;
	if (g_iClientDetail > 0)
	{
		if (!m_RiseTrail[0][0].CanDraw() && !m_RiseTrail[0][0].CanDraw() && !m_RiseTrail[0][0].CanDraw() && !m_RiseTrail[0][0].CanDraw())
			bRiseTrails = false;
		int i;
		for (i = 0; i < 4; i++)
		{
			if (m_TetraTrail[i].CanDraw())
				break;
		}
		if (i >= 4)
			bTetraTrails = false;
		if (!m_BurstSparks.HasAlive())
			bBurstSparks = false;
	}
	else
	{
		bRiseTrails = false;
		bTetraTrails = false;
		bBurstSparks = false;
	}

	if (!bBarTrails && !bTetraTrails && !bRiseTrails && !bRingSparks && !bBurstSparks)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSmokeSprite && (bBarTrails || bTetraTrails || bRiseTrails))
		gEngfuncs.pTriAPI->SpriteTexture(m_pSmokeSprite, 0);

	if ((sMask & 1) != 0)
	{
		m_BarTrail[0].DrawToHead(m_vecBarEnd[0], Vector(0.9, 0.9, 1.0), 0.8, 2, true);
		m_BarTrail[1].DrawToHead(m_vecBarEnd[1], Vector(0.9, 0.9, 1.0), 0.8, 2, true);
	}

	if (g_iClientDetail > 0)
	{
		if ((sMask & 2) != 0)
		{
			for (int i = 0; i < 4; i++)
				m_TetraTrail[i].DrawToHead(m_vecTetraPos[i], Vector(0.8, 0.8, 1.0), 0.95, 1.5, true);
		}
		if ((sMask & 4) != 0)
		{
			m_RiseTrail[m_iRiseTrailSet][0].DrawToHead(m_vecRiseRingPos[0], Vector(0.6, 0.6, 1.0), m_flRiseAlpha * 0.7, 1.0, true);
			m_RiseTrail[m_iRiseTrailSet][1].DrawToHead(m_vecRiseRingPos[4], Vector(0.6, 0.6, 1.0), m_flRiseAlpha * 0.7, 1.0, true);
			// the last cycle's pair, fading
			m_RiseTrail[m_iRiseTrailSet == 0][0].Draw(true);
			m_RiseTrail[m_iRiseTrailSet == 0][1].Draw(true);
		}
	}

	if ((sMask & 8) != 0)
	{
		if (m_pRingSparkSprite && bRingSparks)
			gEngfuncs.pTriAPI->SpriteTexture(m_pRingSparkSprite, 0);
		m_RingSparks.Draw();
	}

	if ((sMask & 0x10) != 0 && g_iClientDetail > 0)
	{
		if (m_pBurstSparkSprite && bBurstSparks)
			gEngfuncs.pTriAPI->SpriteTexture(m_pBurstSparkSprite, 0);
		m_BurstSparks.Draw();
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The grenade is gone: the sound stops, the tents, lights and beams are let go. The trails and
// particle arrays are never freed (the effect is deleted without a destructor).
bool CUltimaTent::KillEntity(int iEntIndex) // orig client 0x897a0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_STATIC, "ambience/alien_minddrill.wav");

	if (m_pCoreGlow)
	{
		m_pCoreGlow->die = 0;
		m_pCoreGlow = nullptr;
	}
	if (m_pFlare)
	{
		m_pFlare->die = 0;
		m_pFlare = nullptr;
	}
	if (m_pGroundGlow)
	{
		m_pGroundGlow->die = 0;
		m_pGroundGlow = nullptr;
	}
	if (m_pDLight)
	{
		m_pDLight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pDLight = nullptr;
	}
	if (m_pELight)
	{
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pELight = nullptr;
	}

	ReleaseBeam(m_pSkyBeam);
	ReleaseBeam(m_pGroundBeam);
	for (int i = 0; i < 8; i++)
		ReleaseBeam(m_pTumbleBeam[i]);
	for (int i = 0; i < 7; i++)
		ReleaseBeam(m_pSpokeBeam[i]);
	for (int i = 0; i < 8; i++)
	{
		ReleaseBeam(m_pGroundRingBeam[i]);
		if (m_pGroundRingGlow[i])
		{
			m_pGroundRingGlow[i]->die = 0;
			m_pGroundRingGlow[i] = nullptr;
		}
	}
	for (int i = 0; i < 8; i++)
		ReleaseBeam(m_pRiseBeam[i]);
	for (int i = 0; i < 2; i++)
		ReleaseBeam(m_pDownBeam[i]);

	m_iEntIndex = 0;
	return true;
}
