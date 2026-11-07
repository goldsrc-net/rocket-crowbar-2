/***
*
*	Rocket Crowbar 2 client: a player's transformation into a monster and back (CLTE 0x40).
*
*	Sent by the server's transform code (transform.cpp) when a transformation starts, and
*	killed with CLTE 0xfe/0x40 when it completes. While it runs, the player's model fades out
*	and a stand-in entity of his (the server's m_pCam2, drawn with models/hassassin.mdl) fades
*	in at his place; around him, rings of sprites/gradpulse.spr spin on two ellipsoids, the old
*	shape's fading and the new one's growing, and ambience/labgear.wav rises in pitch. At the
*	end everyone who sees it gets a white flash, the player himself a longer one.
*
*	The original's functions 0x8d960-0x8ebf0 (with the constructor 0x8ebf0) of client.dll's
*	run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"
#include "pm_defs.h"
#include "rc2prefs.h"

#include <cmath>
#include <cstdlib>

// c3 needs:
// orig .bss 0x10210aa8: the client's copy of the rc2.ini values (8 bytes a key, in the server's
// key order, the NETWORK_SYNC ones filled from the RCCFG message), read through rc2prefs.h's
// RC2Pref() over the client's g_rc2PrefValues.
// orig .bss 0x101f6a00 (c2's): set while the local player is transforming. name ours
extern int g_bLocalTransforming;
// orig 0x55190 (c2): a strip of camera-facing quads through iCount points, each with its
// width, colour and alpha, using the bound texture; bStretch/bByLength choose how the texture
// runs along it, pflSegLengths is scratch for the second. CRC2Trail::Draw calls it. name ours
void RC2_DrawStrip(Vector* pvecPoints, float* pflWidths, Vector* pcolColors, float* pflAlphas, int iCount, bool bStretch, bool bByLength, float* pflSegLengths);
// orig 0xec590 (view, a changed SDK file): starts a screen colour fade from colFrom/flFrom to
// colTo/flTo over flDuration, unless one of higher iPriority is running. name ours
void V_StartColorFade(Vector colFrom, Vector colTo, float flFrom, float flTo, float flDuration, int iPriority);
// orig 0xa9fd0 (in_camera, a changed SDK file): the camera is in third person (cam_thirdperson
// set and RC2's 0x1020be40 clear). name ours
bool CAM_IsThirdPerson();

//
// CLTE 0x40: byte the player, short his stand-in, byte the type he changes from, byte the
// type he changes to (0 a human; 1-5 the assassin, houndeye, controller, bat and headcrab).
//
class CTransformTent : public CClientTent // name ours
{
public:
	CTransformTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	void BuildRing(Vector vecOrigin, Vector vecAxis1, Vector vecAxis2, float flRadius1, float flRadius2, float flPhase, Vector* pvecPoints);

	int m_iEntIndex;		 // orig +0x8: the player
	int m_iStandIn;			 // orig +0xc: his stand-in entity
	int m_iToType;			 // orig +0x10
	int m_iFromType;		 // orig +0x14
	bool m_bVisible;		 // orig +0x18
	float m_flTimeLeft;		 // orig +0x1c
	float m_flDuration;		 // orig +0x20
	float m_flToRadius[3];	 // orig +0x24, +0x2c, +0x34 (interleaved with the next): the new shape's ellipsoid along forward, right, up
	float m_flFromRadius[3]; // orig +0x28, +0x30, +0x38: the old shape's
	float m_flToPhase[3];	 // orig +0x3c, +0x44, +0x4c (interleaved with the next): each ring's turn
	float m_flFromPhase[3];	 // orig +0x40, +0x48, +0x50
	float m_flSpin[3];		 // orig +0x54: each ring pair's spin, radians a second, either way
	Vector* m_pvecToRing[3];   // orig +0x60, +0x68, +0x70 (interleaved with the next): malloc'd, m_iPoints each
	Vector* m_pvecFromRing[3]; // orig +0x64, +0x6c, +0x74: only with cl_detail >= 0
	float* m_pflToAlpha;	 // orig +0x78
	float* m_pflFromAlpha;	 // orig +0x7c: only with cl_detail >= 0
	Vector* m_pcolColors;	 // orig +0x80
	float* m_pflWidths;		 // orig +0x84
	float* m_pflSegLengths;	 // orig +0x88: m_iPoints + 1, scratch for RC2_DrawStrip
	int m_iPoints;			 // orig +0x8c: 11, 13 or 15 by cl_detail
	model_s* m_pSprite;		 // orig +0x90: sprites/gradpulse.spr
};

// The factory (the dispatcher's case 0x40; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor (inline here in the original). The dispatcher always
// passes null.
CClientTent* RC2_NewTransformTent(const CClientTent* pCopy) // orig client 0x8d960
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CTransformTent* pTent = pCopy ? new CTransformTent(*static_cast<const CTransformTent*>(pCopy)) : new CTransformTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// How long the change takes: rc2.ini's TransformParalyzeTime of a monster type (1-5).
static float TransformTime(int iType) // name ours; inline twice in the constructor
{
	switch (iType)
	{
	case 1:
		return RC2Pref(RC2P_TransformAssassin_TransformParalyzeTime).f;
	case 2:
		return RC2Pref(RC2P_TransformHoundeye_TransformParalyzeTime).f;
	case 3:
		return RC2Pref(RC2P_TransformController_TransformParalyzeTime).f;
	case 4:
		return RC2Pref(RC2P_TransformBat_TransformParalyzeTime).f;
	case 5:
		return RC2Pref(RC2P_TransformHeadcrab_TransformParalyzeTime).f;
	}
	return 1.0;
}

// An ellipsoid's height for a type: 42, 32 for the houndeye, 24 for the bat and headcrab.
static float TransformHeight(int iType) // name ours; inline twice in the constructor
{
	switch (iType)
	{
	case 2:
		return 32;
	case 4:
	case 5:
		return 24;
	}
	return 42;
}

CTransformTent::CTransformTent() // orig client 0x8ebf0
{
	m_iEntIndex = READ_BYTE();
	m_iStandIn = READ_SHORT();
	m_iFromType = READ_BYTE();
	m_iToType = READ_BYTE();

	// VERIFY: these keys are read off the client's pref table by position (0x10210f70 and on is
	// TransformParalyzeTime of each type if the table starts at 0x10210aa8); MaxHealth and
	// HealthRegenRate fit the same stride, but only these two are synced and read as times.
	// Original bug, kept: both lookups switch on the from type (the second was meant for the
	// to type), so the duration is the from type's time, or 1 when changing from a human.
	float flFrom = 1.0, flTo = 1.0;
	if (m_iFromType != 0)
		flFrom = TransformTime(m_iFromType);
	if (m_iToType != 0)
		flTo = TransformTime(m_iFromType);

	if (m_iFromType != 0 && m_iToType != 0)
		m_flDuration = m_flTimeLeft = flFrom > flTo ? flFrom : flTo;
	else if (m_iFromType != 0)
		m_flDuration = m_flTimeLeft = flFrom;
	else if (m_iToType != 0)
		m_flDuration = m_flTimeLeft = flTo;
	else
		m_flDuration = m_flTimeLeft = 1.0;

	m_bVisible = false;
	for (int i = 0; i < 3; i++)
		m_pvecToRing[i] = m_pvecFromRing[i] = nullptr;
	m_pflToAlpha = m_pflFromAlpha = nullptr;
	m_pcolColors = nullptr;
	m_pflWidths = nullptr;
	m_pflSegLengths = nullptr;
	m_iPoints = 0;
	m_pSprite = nullptr;

	m_flToRadius[0] = m_flToRadius[1] = 24;
	m_flToRadius[2] = TransformHeight(m_iToType);
	m_flFromRadius[0] = m_flFromRadius[1] = 24;
	m_flFromRadius[2] = TransformHeight(m_iFromType);

	for (int i = 0; i < 3; i++)
	{
		m_flToPhase[i] = gEngfuncs.pfnRandomFloat(0, 6.2831855);
		m_flFromPhase[i] = gEngfuncs.pfnRandomFloat(0, 6.2831855);
		const double flSign = gEngfuncs.pfnRandomLong(0, 1) ? 1.0 : -1.0;
		m_flSpin[i] = gEngfuncs.pfnRandomFloat(8.975979, 7.8539815) * flSign;
	}

	cl_entity_t* pPlayer = RC2_GetEntity(m_iEntIndex);
	if (!pPlayer)
	{
		m_iEntIndex = 0;
		return;
	}

	if (gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
		g_bLocalTransforming = 1;

	if (g_iClDetail < 0)
		m_iPoints = 11;
	else
		m_iPoints = g_iClDetail != 0 ? 15 : 13;

	Vector vecAngles = pPlayer->angles;
	vecAngles.x = 0;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	m_pflSegLengths = (float*)malloc(m_iPoints * 4 + 4);
	m_pflWidths = (float*)malloc(m_iPoints * 4);
	m_pflToAlpha = (float*)malloc(m_iPoints * 4);
	m_pcolColors = (Vector*)malloc(m_iPoints * 12);
	m_pvecToRing[0] = (Vector*)malloc(m_iPoints * 12);
	m_pvecToRing[1] = (Vector*)malloc(m_iPoints * 12);
	m_pvecToRing[2] = (Vector*)malloc(m_iPoints * 12);

	const Vector vecOrigin = pPlayer->origin;
	if (g_iClDetail >= 0)
	{
		m_pvecFromRing[0] = (Vector*)malloc(m_iPoints * 12);
		m_pvecFromRing[1] = (Vector*)malloc(m_iPoints * 12);
		m_pvecFromRing[2] = (Vector*)malloc(m_iPoints * 12);
		m_pflFromAlpha = (float*)malloc(m_iPoints * 4);

		BuildRing(vecOrigin, vecForward, vecRight, m_flFromRadius[0], m_flFromRadius[1], m_flFromPhase[0], m_pvecFromRing[0]);
		BuildRing(vecOrigin, vecForward, vecUp, m_flFromRadius[0], m_flFromRadius[2], m_flFromPhase[1], m_pvecFromRing[1]);
		BuildRing(vecOrigin, vecRight, vecUp, m_flFromRadius[1], m_flFromRadius[2], m_flFromPhase[2], m_pvecFromRing[2]);
	}

	BuildRing(vecOrigin, vecForward, vecRight, m_flToRadius[0], m_flToRadius[1], m_flToPhase[0], m_pvecToRing[0]);
	BuildRing(vecOrigin, vecForward, vecUp, m_flToRadius[0], m_flToRadius[2], m_flToPhase[1], m_pvecToRing[1]);
	BuildRing(vecOrigin, vecRight, vecUp, m_flToRadius[1], m_flToRadius[2], m_flToPhase[2], m_pvecToRing[2]);

	for (int i = 0; i < m_iPoints; i++)
	{
		m_pflWidths[i] = 2.0;
		// Original bug, kept: the alphas are set at index 1 each time, not i; the rest stay as
		// malloc left them until the first Think.
		m_pflToAlpha[1] = 0;
		if (m_pflFromAlpha)
			m_pflFromAlpha[1] = 0.95;
		m_pcolColors[i] = Vector(0.8, 0.8, 1.0);
	}

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/gradpulse.spr", &iModelIndex);
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pPlayer->origin, CHAN_ITEM, "ambience/labgear.wav", 0.4, 0.5, 0, 60);
}

// m_iPoints points around an ellipse on two axes through vecOrigin, from flPhase on, the last
// back where the first is.
void CTransformTent::BuildRing(Vector vecOrigin, Vector vecAxis1, Vector vecAxis2, float flRadius1, float flRadius2, float flPhase, Vector* pvecPoints) // orig client 0x8e1a0
{
	const float flStep = 6.283185307179586 / (m_iPoints - 1);
	for (int i = 0; i < m_iPoints; i++)
	{
		pvecPoints[i] = vecAxis1 * cos(flPhase) * flRadius1 + vecOrigin + vecAxis2 * sin(flPhase) * flRadius2;
		flPhase += flStep;
	}
}

bool CTransformTent::Think(float flFrametime) // orig client 0x8dc00
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pPlayer = RC2_GetEntity(m_iEntIndex);
	if (!pPlayer)
	{
		EntityGone(m_iEntIndex);
		return true;
	}

	m_flTimeLeft -= flFrametime;
	m_bVisible = true;
	float flFrac = m_flTimeLeft / m_flDuration;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;
	const double flDone = 1.0 - flFrac;

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pPlayer->origin, CHAN_ITEM, "ambience/labgear.wav", flDone * 0.6 + 0.4, 0.5, 0xc0 /* SND_CHANGE_VOL | SND_CHANGE_PITCH */, (int)((float)flDone * 190 + 60));

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pLocal->curstate.messagenum > pPlayer->curstate.messagenum && pLocal->index != pPlayer->index)
	{
		m_bVisible = false;
		return false;
	}

	cl_entity_t* pStandIn = RC2_GetEntity(m_iStandIn);
	if (pStandIn)
	{
		// the player himself in first person doesn't see the stand-in. Original bug, kept: a
		// null local player is dereferenced here.
		if (!CAM_IsThirdPerson() && pLocal->index == m_iEntIndex)
		{
			pStandIn->curstate.rendermode = kRenderTransTexture;
			pStandIn->curstate.renderamt = 0;
			pStandIn->curstate.renderfx = 0;
		}
		else
		{
			pStandIn->curstate.rendermode = kRenderTransTexture;
			pStandIn->curstate.renderamt = (int)(flDone * 255);
			pStandIn->curstate.renderfx = 0;
			pStandIn->curstate.playerclass = m_iToType | 0x40000000; // VERIFY: what the high bit tells the studio renderer
			pStandIn->curstate.owner = m_iEntIndex;
			pStandIn->curstate.messagenum = pPlayer->curstate.messagenum;
			pStandIn->model = gEngfuncs.CL_LoadModel("models/hassassin.mdl", &pStandIn->curstate.modelindex);
			pStandIn->origin = pPlayer->curstate.origin;
			pStandIn->curstate.origin = pPlayer->curstate.origin;
			pStandIn->angles = pPlayer->curstate.angles;
			pStandIn->curstate.angles = pPlayer->curstate.angles;
		}
	}

	pPlayer->curstate.rendermode = kRenderTransTexture;
	pPlayer->curstate.renderamt = (int)(flFrac * 255);
	pPlayer->curstate.renderfx = 0;
	pPlayer->curstate.playerclass = m_iFromType;

	for (int i = 0; i < 3; i++)
	{
		if (g_iClDetail >= 0)
			m_flFromPhase[i] = m_flFromPhase[i] - flFrametime * m_flSpin[i];
		m_flToPhase[i] = flFrametime * m_flSpin[i] + m_flToPhase[i];
	}

	const float flToAlpha = flDone * 0.95;
	for (int i = 0; i < m_iPoints; i++)
	{
		if (g_iClDetail >= 0 && m_pflFromAlpha)
			m_pflFromAlpha[i] = flFrac * 0.95;
		m_pflToAlpha[i] = flToAlpha;
	}

	Vector vecAngles = pPlayer->angles;
	vecAngles.x = 0;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	// Only the forward/right and right/up rings follow; the forward/up ones keep where the
	// constructor put them (as in the original).
	const Vector vecOrigin = pPlayer->origin;
	if (g_iClDetail >= 0)
	{
		if (m_pvecFromRing[0])
			BuildRing(vecOrigin, vecForward, vecRight, m_flFromRadius[0], m_flFromRadius[1], m_flFromPhase[0], m_pvecFromRing[0]);
		if (m_pvecFromRing[2])
			BuildRing(vecOrigin, vecRight, vecUp, m_flFromRadius[1], m_flFromRadius[2], m_flFromPhase[2], m_pvecFromRing[2]);
	}
	BuildRing(vecOrigin, vecForward, vecRight, m_flToRadius[0], m_flToRadius[1], m_flToPhase[0], m_pvecToRing[0]);
	BuildRing(vecOrigin, vecRight, vecUp, m_flToRadius[1], m_flToRadius[2], m_flToPhase[2], m_pvecToRing[2]);
	return false;
}

void CTransformTent::DrawTransparent() // orig client 0x8e360
{
	if (!m_bVisible)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	if (g_iClDetail >= 0 && m_pflFromAlpha)
	{
		if (m_pvecFromRing[0])
			RC2_DrawStrip(m_pvecFromRing[0], m_pflWidths, m_pcolColors, m_pflFromAlpha, m_iPoints, false, false, m_pflSegLengths);
		if (m_pvecFromRing[2])
			RC2_DrawStrip(m_pvecFromRing[2], m_pflWidths, m_pcolColors, m_pflFromAlpha, m_iPoints, false, false, m_pflSegLengths);
	}
	RC2_DrawStrip(m_pvecToRing[0], m_pflWidths, m_pcolColors, m_pflToAlpha, m_iPoints, false, false, m_pflSegLengths);
	RC2_DrawStrip(m_pvecToRing[2], m_pflWidths, m_pcolColors, m_pflToAlpha, m_iPoints, false, false, m_pflSegLengths);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The change is done (CLTE 0xfe/0x40) or the player is gone: hide the stand-in, show the
// player again with a zap, free the rings, and flash whoever sees it.
bool CTransformTent::EntityGone(int iEntIndex) // orig client 0x8e4e0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	if (gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
		g_bLocalTransforming = 0;
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_ITEM, "ambience/labgear.wav");

	cl_entity_t* pStandIn = RC2_GetEntity(m_iStandIn);
	if (pStandIn)
	{
		pStandIn->curstate.playerclass = 0;
		pStandIn->curstate.effects |= EF_NODRAW;
	}

	cl_entity_t* pPlayer = RC2_GetEntity(m_iEntIndex);
	if (pPlayer)
	{
		pPlayer->curstate.rendermode = kRenderNormal;
		pPlayer->curstate.renderamt = 0;
		pPlayer->curstate.renderfx = 0;
		gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pPlayer->origin, CHAN_ITEM, "debris/beamstart9.wav", 0.9, 0.5, 0, gEngfuncs.pfnRandomLong(130, 140));
	}
	m_iEntIndex = 0;

	for (int i = 0; i < 3; i++)
	{
		if (m_pvecToRing[i])
			free(m_pvecToRing[i]);
		m_pvecToRing[i] = nullptr;
		if (m_pvecFromRing[i])
			free(m_pvecFromRing[i]);
		m_pvecFromRing[i] = nullptr;
	}
	if (m_pflToAlpha)
		free(m_pflToAlpha);
	m_pflToAlpha = nullptr;
	if (m_pflFromAlpha)
		free(m_pflFromAlpha);
	m_pflFromAlpha = nullptr;
	if (m_pcolColors)
		free(m_pcolColors);
	m_pcolColors = nullptr;
	if (m_pflWidths)
		free(m_pflWidths);
	m_pflWidths = nullptr;
	if (m_pflSegLengths)
		free(m_pflSegLengths);
	m_pflSegLengths = nullptr;

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return true;

	if (pPlayer && pPlayer->index == pLocal->index)
	{
		V_StartColorFade(Vector(255, 255, 255), Vector(230, 230, 255), 240, 0, 0.45, 1);
		return true;
	}

	// Original bug, kept: with the player's entity gone this reads through a null pointer.
	Vector vecViewOfs, vecViewAngles, vecForward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecViewOfs);
	gEngfuncs.GetViewAngles(vecViewAngles);
	gEngfuncs.pfnAngleVectors(vecViewAngles, vecForward, nullptr, nullptr);
	Vector vecEye = pLocal->origin + vecViewOfs;

	if (DotProduct((pPlayer->origin - vecEye).Normalize(), vecForward) <= 0)
		return true;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_PlayerTrace(pPlayer->origin, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &tr);
	if (tr.fraction >= 1.0 || (tr.ent != 0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == pLocal->index))
		V_StartColorFade(Vector(255, 255, 255), Vector(230, 230, 255), 200, 0, 0.3, 0);

	gEngfuncs.pEventAPI->EV_PopPMStates();
	return true;
}
