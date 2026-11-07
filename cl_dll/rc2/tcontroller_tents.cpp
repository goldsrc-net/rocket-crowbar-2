/***
*
*	Rocket Crowbar 2 client: the controller transform's effects (CLTE 0x45-0x49).
*
*	Server dlls/rc2/tcontroller.cpp. CLTE 0x45 is the throw wind-up (the glow in the hand,
*	TController_ThrowStart), 0x46 the thrown ball in flight, 0x48 the charge-ball charging
*	(both hands glow, a beam arcs between them, a glow in front of the face), 0x49 the charged
*	ball in flight: it leeches with beams from the players it passes and, once it is gone,
*	streams their health back to the thrower as trails. 0x47 is a burst of orange sparks along
*	a direction; nothing on the server sends it.
*
*	The original's functions 0x93220-0x97510 of client.dll's run "new: cdll_int .. com_weapons"
*	(and the copy constructors 0x9b320, 0x9b370).
*
****/

#include "c3_needs.h"
#include "c3_tents.h"
#include "rc2prefs.h"
#include "pm_defs.h"
#include "pmtrace.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// c3 needs:
// .bss 0x10214200 (c4, StudioModelRenderer.cpp): each player's four attachments as last drawn,
// by player index. The controller's hands are attachments 2 and 3. name c4's
extern Vector g_rgvecPlayerAttachment[32 + 1][4];
// orig 0xa9fd0 (in_camera, a changed SDK file): the camera is in third person. name fork E's
bool CAM_IsThirdPerson();
// .bss 0x10206918 (c2): one past the highest player/normal entity index HUD_AddEntity saw. name c2's
extern int g_iMaxPlayerEnt;
// orig 0x59ee0 (c2): where a ray from vecSrc along -vecDir meets the shield (CLTE 0x25) of
// entity iEntIndex, when that entity's bit is set in c2's shield mask (0x101f69f4; *pbHit = 1
// then), else (it seems) vecSrc; pflUnused is passed null here. name ours. VERIFY: the
// function is c2's; read only as far as its entry
Vector RC2_ShieldIntercept(int iEntIndex, Vector vecSrc, Vector vecDir, float* pflUnused, int* pbHit);

// The flags RC2 puts on its looping sprite tempents (FTENT_SPRANIMATE | FTENT_PERSIST |
// FTENT_SPRANIMATELOOP). name ours
#define RC2_FTENT_LOOPSPRITE (FTENT_SPRANIMATE | FTENT_PERSIST | FTENT_SPRANIMATELOOP)

// A white additive glow sprite tempent at vecOrigin, as every effect here makes it (inline
// in the original at each use). name ours
static void ControllerGlowSetup(TEMPENTITY* pTent, int iRenderAmt)
{
	pTent->flags |= RC2_FTENT_LOOPSPRITE;
	pTent->entity.curstate.framerate = 20;
	pTent->entity.curstate.scale = 0.9;
	pTent->entity.curstate.renderamt = iRenderAmt;
	pTent->entity.curstate.rendermode = kRenderGlow;
	pTent->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTent->entity.curstate.rendercolor.r = 255;
	pTent->entity.curstate.rendercolor.g = 255;
	pTent->entity.curstate.rendercolor.b = 255;
	pTent->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 10);
}

// A dynamic or entity light at vecOrigin for ten seconds (Think keeps it alive frame by frame).
static void ControllerLightSetup(dlight_t* pLight, const Vector& vecOrigin, float flRadius, int r, int g, int b)
{
	pLight->origin = vecOrigin;
	pLight->radius = flRadius;
	pLight->color.r = r;
	pLight->color.g = g;
	pLight->color.b = b;
	pLight->die = gEngfuncs.GetClientTime() + 10.0;
}

// The end of a tempent: it dies now and stops looping.
static void KillLoopTent(TEMPENTITY*& pTent)
{
	pTent->die = 0;
	pTent->flags &= ~FTENT_SPRANIMATELOOP;
	pTent = nullptr;
}

// The end of a light: it dies a hundredth of a second from now.
static void KillLight(dlight_t*& pLight)
{
	pLight->die = gEngfuncs.GetClientTime() + 0.01;
	pLight = nullptr;
}

// The end of a beam that may follow entities.
static void KillEntityBeam(BEAM*& pBeam)
{
	pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
	pBeam->endEntity = 0;
	pBeam->startEntity = 0;
	pBeam = nullptr;
}

static const char* const s_szControllerAttack[3] = {"controller/con_attack1.wav", "controller/con_attack2.wav", "controller/con_attack3.wav"};

//
// CLTE 0x45: byte the player.
//
class CControllerThrowGlowTent : public CClientTent // name ours
{
public:
	CControllerThrowGlowTent();

	bool Think(float flFrametime) override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;	  // orig +0x8: the player (1-32)
	float m_flCharge;	  // orig +0xc: 0.01 up to 1 over 0.6 s
	TEMPENTITY* m_pGlow;  // orig +0x10: sprites/xspark4.spr in the hand
	dlight_t* m_pDlight;  // orig +0x14
	dlight_t* m_pElight;  // orig +0x18
};

// The factory. The copy constructor is out of line in the original (orig client 0x9b320).
CClientTent* RC2_NewControllerThrowGlowTent(const CClientTent* pCopy) // orig client 0x93220
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CControllerThrowGlowTent* pTent = pCopy ? new CControllerThrowGlowTent(*static_cast<const CControllerThrowGlowTent*>(pCopy)) : new CControllerThrowGlowTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x93220).
CControllerThrowGlowTent::CControllerThrowGlowTent()
{
	m_iEntIndex = READ_BYTE();
	if (m_iEntIndex < 1 || m_iEntIndex > 32)
	{
		// the rest stays unset, as in the original; Think ends it at once
		m_iEntIndex = 0;
		return;
	}

	m_pGlow = nullptr;
	m_flCharge = 0.01;
	m_pDlight = nullptr;
	m_pElight = nullptr;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	const int iSound = gEngfuncs.pfnRandomLong(0, 2);
	if (iSound >= 0 && iSound <= 2)
		gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_WEAPON, s_szControllerAttack[iSound], 1.0, 0.7, 0, gEngfuncs.pfnRandomLong(95, 105));

	if (g_iClDetail >= 0)
	{
		m_pDlight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pDlight)
			ControllerLightSetup(m_pDlight, pEntity->origin, 100, 1, 1, 1);
		m_pElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pElight)
			ControllerLightSetup(m_pElight, pEntity->origin, 80, 1, 1, 1);
	}

	int iModelIndex;
	model_s* pModel = gEngfuncs.CL_LoadModel("sprites/xspark4.spr", &iModelIndex);
	if (!pModel)
		return;
	m_pGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEntity->origin, pModel);
	if (!m_pGlow)
		return;
	ControllerGlowSetup(m_pGlow, 1);
	m_pGlow->frameMax = 11;
	m_pGlow->die += 5;
}

// The glow brightens in the hand (attachment 2), unseen by the player himself in first person.
bool CControllerThrowGlowTent::Think(float flFrametime) // orig client 0x93710
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pEntity->curstate.messagenum < pLocal->curstate.messagenum)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	m_flCharge = flFrametime * 1.6666666666666667 + m_flCharge;
	if (m_flCharge < 0.01)
		m_flCharge = 0.01;
	else if (m_flCharge > 1)
		m_flCharge = 1;

	const Vector& vecHand = g_rgvecPlayerAttachment[m_iEntIndex][2];

	if (m_pGlow)
	{
		m_pGlow->entity.origin = vecHand;
		if (pLocal->index == pEntity->index && !CAM_IsThirdPerson())
			m_pGlow->entity.curstate.renderamt = 0;
		else
			m_pGlow->entity.curstate.renderamt = (int)(m_flCharge * 240);
		m_pGlow->entity.curstate.framerate = m_flCharge * 15 + 5;
		m_pGlow->die = flFrametime + m_pGlow->die;
	}

	if (m_pDlight)
	{
		m_pDlight->origin = vecHand;
		m_pDlight->color.r = (int)(m_flCharge * 255);
		m_pDlight->color.g = (int)(m_flCharge * 192);
		m_pDlight->color.b = (int)(m_flCharge * 64);
		m_pDlight->die = flFrametime + m_pDlight->die;
	}

	if (m_pElight)
	{
		m_pElight->origin = vecHand;
		m_pElight->color.r = (int)(m_flCharge * 255);
		m_pElight->color.g = (int)(m_flCharge * 192);
		m_pElight->color.b = (int)(m_flCharge * 64);
		m_pElight->die = flFrametime + m_pElight->die;
	}

	return false;
}

bool CControllerThrowGlowTent::EntityGone(int iEntIndex) // orig client 0x939c0
{
	if (m_iEntIndex != iEntIndex)
		return false;

	if (m_pGlow)
		KillLoopTent(m_pGlow);
	if (m_pDlight)
		KillLight(m_pDlight);
	if (m_pElight)
		KillLight(m_pElight);
	m_iEntIndex = 0;
	return true;
}

//
// CLTE 0x46: short the ball.
//
class CControllerThrowBallTent : public CClientTent // name ours
{
public:
	CControllerThrowBallTent();

	bool Think(float flFrametime) override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;	  // orig +0x8: the ball
	float m_flGrow;		  // orig +0xc: 0 up to 1.2, twice the frame time a frame
	float m_flSpin;		  // orig +0x10: degrees a second, 200-250 either way
	TEMPENTITY* m_pGlow;  // orig +0x14: sprites/xspark4.spr on the ball
	dlight_t* m_pDlight;  // orig +0x18
	dlight_t* m_pElight;  // orig +0x1c
};

// The factory. The original's copy is inline here, through the base class's out-of-line copy
// constructor (orig 0x9b430).
CClientTent* RC2_NewControllerThrowBallTent(const CClientTent* pCopy) // orig client 0x93a50
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CControllerThrowBallTent* pTent = pCopy ? new CControllerThrowBallTent(*static_cast<const CControllerThrowBallTent*>(pCopy)) : new CControllerThrowBallTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x93a50).
CControllerThrowBallTent::CControllerThrowBallTent()
{
	m_iEntIndex = READ_SHORT();
	m_pGlow = nullptr;
	m_pDlight = nullptr;
	m_pElight = nullptr;
	m_flGrow = 0;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	if (g_iClDetail >= 0)
	{
		m_pDlight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pDlight)
			ControllerLightSetup(m_pDlight, pEntity->origin, 100, 255, 192, 64);
		m_pElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pElight)
			ControllerLightSetup(m_pElight, pEntity->origin, 80, 255, 192, 64);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, 6, "ambience/crtnoise.wav", 0.5, 0.7, 0, 150);

	const float flSign = gEngfuncs.pfnRandomLong(0, 1) != 0 ? 1.0 : -1.0;
	m_flSpin = gEngfuncs.pfnRandomFloat(200, 250) * flSign;

	int iModelIndex;
	model_s* pModel = gEngfuncs.CL_LoadModel("sprites/xspark4.spr", &iModelIndex);
	if (!pModel)
		return;
	m_pGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEntity->origin, pModel);
	if (!m_pGlow)
		return;
	ControllerGlowSetup(m_pGlow, 240);
	m_pGlow->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 360);
	m_pGlow->frameMax = 11;
	m_pGlow->die += 5;
}

// The glow swells and spins on the ball; the lights follow it.
bool CControllerThrowBallTent::Think(float flFrametime) // orig client 0x93eb0
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	m_flGrow = flFrametime + flFrametime + m_flGrow;
	if (m_flGrow < 0)
		m_flGrow = 0;
	else if (m_flGrow > 1.2)
		m_flGrow = 1.2;

	if (m_pGlow)
	{
		m_pGlow->entity.curstate.scale = m_flGrow + 0.9;
		m_pGlow->entity.origin = pEntity->origin;
		m_pGlow->entity.angles.z = flFrametime * m_flSpin + m_pGlow->entity.angles.z;
		m_pGlow->die = flFrametime + m_pGlow->die;
	}

	if (m_pDlight)
	{
		m_pDlight->origin = pEntity->origin;
		m_pDlight->die = flFrametime + m_pDlight->die;
	}

	if (m_pElight)
	{
		m_pElight->origin = pEntity->origin;
		m_pElight->die = flFrametime + m_pElight->die;
	}

	return false;
}

bool CControllerThrowBallTent::EntityGone(int iEntIndex) // orig client 0x94030
{
	if (m_iEntIndex != iEntIndex)
		return false;

	if (m_pGlow)
		KillLoopTent(m_pGlow);
	if (m_pDlight)
		KillLight(m_pDlight);
	if (m_pElight)
		KillLight(m_pElight);
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, 6, "ambience/crtnoise.wav");
	m_iEntIndex = 0;
	return true;
}

//
// CLTE 0x47: coord x3 the origin, short x3 the direction (in 1/32767ths).
//
class CControllerSparksTent : public CClientTent // name ours
{
public:
	CControllerSparksTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;

	Vector m_vecOrigin;		 // orig +0x8
	Vector m_vecDir;		 // orig +0x14: normalized
	Vector m_vecRight;		 // orig +0x20: a random axis square to m_vecDir
	Vector m_vecUp;			 // orig +0x2c
	model_s* m_pSprite;		 // orig +0x38: sprites/glow04.spr
	CRC2Particles m_Sparks;	 // orig +0x3c
};

// The factory. The copy constructor is out of line in the original (orig client 0x9b370).
CClientTent* RC2_NewControllerSparksTent(const CClientTent* pCopy) // orig client 0x940f0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CControllerSparksTent* pTent = pCopy ? new CControllerSparksTent(*static_cast<const CControllerSparksTent*>(pCopy)) : new CControllerSparksTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x940f0): 25-35, 55-65 or 100-120 sparks by cl_detail, spread
// up to 0.9 around the direction, flung out at 150-160 and pulled back along it at 300-320.
CControllerSparksTent::CControllerSparksTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_vecDir.x = READ_SHORT() * (1.0f / 32767);
	m_vecDir.y = READ_SHORT() * (1.0f / 32767);
	m_vecDir.z = READ_SHORT() * (1.0f / 32767);
	m_vecDir = m_vecDir.Normalize();

	do
	{
		m_vecRight.x = gEngfuncs.pfnRandomFloat(-1, 1);
		m_vecRight.y = gEngfuncs.pfnRandomFloat(-1, 1);
		m_vecRight.z = gEngfuncs.pfnRandomFloat(-1, 1);
	} while (m_vecRight.x * m_vecRight.x + m_vecRight.y * m_vecRight.y + m_vecRight.z * m_vecRight.z == 0);

	// orig 0x922e0 (fork E's range) is a cross product, as mathlib's CrossProduct
	Vector vecCross;
	CrossProduct(m_vecRight, m_vecDir, vecCross);
	m_vecUp = vecCross.Normalize();
	CrossProduct(m_vecUp, m_vecDir, vecCross);
	m_vecRight = vecCross.Normalize();

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/glow04.spr", &iModelIndex);

	int iCount;
	if (g_iClDetail < 0)
		iCount = gEngfuncs.pfnRandomLong(25, 35);
	else if (g_iClDetail == 0)
		iCount = gEngfuncs.pfnRandomLong(55, 65);
	else
		iCount = gEngfuncs.pfnRandomLong(100, 120);

	for (int i = iCount; i > 0; i--)
	{
		const Vector vecSpread = RC2_SpreadVector(m_vecDir, m_vecRight, m_vecUp, 0.9);
		const Vector vecUpOffset = m_vecUp * gEngfuncs.pfnRandomFloat(-16, 16);
		const Vector vecRightOffset = m_vecRight * gEngfuncs.pfnRandomFloat(-16, 16);
		const Vector vecSparkOrigin = m_vecOrigin + vecRightOffset + vecUpOffset;
		const float flLife = gEngfuncs.pfnRandomFloat(1.0, 1.1);
		const float flAlpha = gEngfuncs.pfnRandomFloat(0.8, 0.9);
		const float flSize = gEngfuncs.pfnRandomFloat(3.0, 3.5);
		const Vector vecAccel = -m_vecDir * gEngfuncs.pfnRandomFloat(300, 320);
		const Vector vecVelocity = vecSpread * gEngfuncs.pfnRandomFloat(150, 160);
		m_Sparks.Add(vecSparkOrigin, vecVelocity, vecAccel, Vector(1.0, 0.75, 0.25), flAlpha, flSize, flAlpha / flLife, flLife, 0, nullptr);
	}
}

bool CControllerSparksTent::Think(float flFrametime) // orig client 0x946e0
{
	return m_Sparks.Update(flFrametime) < 1;
}

void CControllerSparksTent::DrawTransparent() // orig client 0x94710
{
	if (!m_Sparks.HasLive())
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);
	m_Sparks.Draw();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

//
// CLTE 0x48: byte the player.
//
class CControllerChargeGlowTent : public CClientTent // name ours
{
public:
	CControllerChargeGlowTent();

	bool Think(float flFrametime) override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;		   // orig +0x8: the player (1-32)
	float m_flCharge;		   // orig +0xc: 0.01 up to 1 over five seconds
	TEMPENTITY* m_pGlows[2];   // orig +0x10: sprites/xspark4.spr in each hand
	dlight_t* m_pDlights[2];   // orig +0x18
	dlight_t* m_pElights[2];   // orig +0x20
	BEAM* m_pBeams[8];		   // orig +0x28: only the first is made, hand to hand
	TEMPENTITY* m_pFaceGlow;   // orig +0x48: sprites/nhth1.spr in front of the face
	dlight_t* m_pFaceDlight;   // orig +0x4c
	dlight_t* m_pFaceElight;   // orig +0x50
};

CClientTent* RC2_NewControllerChargeGlowTent(const CClientTent* pCopy) // orig client 0x94790
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CControllerChargeGlowTent* pTent = pCopy ? new CControllerChargeGlowTent(*static_cast<const CControllerChargeGlowTent*>(pCopy)) : new CControllerChargeGlowTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

CControllerChargeGlowTent::CControllerChargeGlowTent() // orig client 0x95590
{
	m_iEntIndex = READ_BYTE();
	if (m_iEntIndex < 1 || m_iEntIndex > 32)
	{
		// the rest stays unset, as in the original; Think ends it at once
		m_iEntIndex = 0;
		return;
	}

	m_pGlows[1] = nullptr;
	m_pGlows[0] = nullptr;
	m_flCharge = 0.01;
	m_pDlights[1] = nullptr;
	m_pDlights[0] = nullptr;
	m_pElights[1] = nullptr;
	m_pElights[0] = nullptr;
	m_pFaceGlow = nullptr;
	m_pFaceDlight = nullptr;
	m_pFaceElight = nullptr;
	for (int i = 0; i < 8; i++)
		m_pBeams[i] = nullptr;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
	{
		m_iEntIndex = 0;
		return;
	}

	const int iSound = gEngfuncs.pfnRandomLong(0, 2);
	if (iSound >= 0 && iSound <= 2)
		gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_WEAPON, s_szControllerAttack[iSound], 0.8, 0.7, 0, gEngfuncs.pfnRandomLong(110, 115));

	if (g_iClDetail >= 0)
	{
		m_pFaceDlight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pFaceDlight)
			ControllerLightSetup(m_pFaceDlight, pEntity->origin, 100, 1, 1, 1);
		m_pFaceElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pFaceElight)
			ControllerLightSetup(m_pFaceElight, pEntity->origin, 100, 1, 1, 1);

		for (int i = 0; i < 2; i++)
		{
			m_pDlights[i] = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
			if (m_pDlights[i])
				ControllerLightSetup(m_pDlights[i], pEntity->origin, 110, 1, 1, 1);
			m_pElights[i] = gEngfuncs.pEfxAPI->CL_AllocElight(0);
			if (m_pElights[i])
				ControllerLightSetup(m_pElights[i], pEntity->origin, 90, 1, 1, 1);
		}
	}

	int iModelIndex;
	model_s* pModel = gEngfuncs.CL_LoadModel("sprites/xspark4.spr", &iModelIndex);
	if (!pModel)
		return;
	for (int i = 0; i < 2; i++)
	{
		m_pGlows[i] = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEntity->origin, pModel);
		if (m_pGlows[i])
		{
			ControllerGlowSetup(m_pGlows[i], 1);
			m_pGlows[i]->frameMax = 11;
			m_pGlows[i]->die += 5;
		}
	}

	int iFaceIndex;
	pModel = gEngfuncs.CL_LoadModel("sprites/nhth1.spr", &iFaceIndex);
	if (!pModel)
		return;
	m_pFaceGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEntity->origin, pModel);
	if (m_pFaceGlow)
	{
		ControllerGlowSetup(m_pFaceGlow, 1);
		m_pFaceGlow->frameMax = 11;
		m_pFaceGlow->die += 5;
	}

	int iBeamIndex;
	if (!gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iBeamIndex))
		return;
	// from hand to hand (attachments 3 and 4 in beam terms)
	m_pBeams[0] = gEngfuncs.pEfxAPI->R_BeamEnts(pEntity->index | 0x3000, pEntity->index | 0x4000, iBeamIndex, 0, 0.8, 0.9, 0.1, 2.0, 0, 0, 1.0, 0.75, 0.25);
	// the original then loads both entities' messagenum and drops them
}

// Everything brightens as the charge builds; out of the PVS it is all parked far away.
bool CControllerChargeGlowTent::Think(float flFrametime) // orig client 0x94970
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	if (pEntity->curstate.messagenum < pLocal->curstate.messagenum)
	{
		const Vector vecFar(8192, 8192, 8192);
		TEMPENTITY* pTents[3] = {m_pGlows[0], m_pGlows[1], m_pFaceGlow};
		for (int i = 0; i < 3; i++)
		{
			if (pTents[i])
			{
				pTents[i]->entity.origin = vecFar;
				pTents[i]->entity.curstate.renderamt = 0;
				pTents[i]->die = flFrametime + pTents[i]->die;
			}
		}
		dlight_t* pLights[6] = {m_pDlights[0], m_pDlights[1], m_pElights[0], m_pElights[1], m_pFaceDlight, m_pFaceElight};
		for (int i = 0; i < 6; i++)
		{
			if (pLights[i])
			{
				pLights[i]->origin = vecFar;
				pLights[i]->die = flFrametime + pLights[i]->die;
			}
		}
		for (int i = 0; i < 8; i++)
		{
			if (m_pBeams[i])
				m_pBeams[i]->brightness = 0.01;
		}
		return false;
	}

	m_flCharge = flFrametime * 0.2 + m_flCharge;
	if (m_flCharge < 0.01)
		m_flCharge = 0.01;
	else if (m_flCharge > 1)
		m_flCharge = 1;

	const bool bOwnView = pLocal->index == pEntity->index;
	for (int i = 0; i < 2; i++)
	{
		TEMPENTITY* pGlow = m_pGlows[i];
		if (!pGlow)
			continue;
		pGlow->entity.origin = g_rgvecPlayerAttachment[m_iEntIndex][2 + i];
		if (bOwnView && !CAM_IsThirdPerson())
			pGlow->entity.curstate.renderamt = 0;
		else
			pGlow->entity.curstate.renderamt = (int)(m_flCharge * 240);
		pGlow->entity.curstate.framerate = m_flCharge * 15 + 5;
		pGlow->die = flFrametime + pGlow->die;
	}

	// the dynamic and entity lights, hand by hand
	dlight_t* pHandLights[4] = {m_pDlights[0], m_pDlights[1], m_pElights[0], m_pElights[1]};
	for (int i = 0; i < 4; i++)
	{
		dlight_t* pLight = pHandLights[i];
		if (!pLight)
			continue;
		pLight->origin = g_rgvecPlayerAttachment[m_iEntIndex][2 + (i & 1)];
		pLight->color.r = (int)(m_flCharge * 255);
		pLight->color.g = (int)(m_flCharge * 192);
		pLight->color.b = (int)(m_flCharge * 64);
		pLight->die = flFrametime + pLight->die;
	}

	for (int i = 0; i < 8; i++)
	{
		if (m_pBeams[i])
		{
			m_pBeams[i]->brightness = m_flCharge * 0.95;
			m_pBeams[i]->amplitude = m_flCharge * 0.7;
			m_pBeams[i]->r = (m_flCharge + 1) * 0.5;
			m_pBeams[i]->g = (m_flCharge + 1) * 0.375;
			m_pBeams[i]->b = (m_flCharge + 1) * 0.125;
		}
	}

	// the face glow: 32 units in front of him, 8 up
	Vector vecForward;
	gEngfuncs.pfnAngleVectors(pEntity->angles, vecForward, nullptr, nullptr);
	const Vector vecOrigin = RC2_GetEntityOrigin(pEntity->index);
	Vector vecFace;
	vecFace.x = vecForward.x * 32 + vecOrigin.x;
	vecFace.y = vecOrigin.y + vecForward.y * 32;
	vecFace.z = vecOrigin.z + vecForward.z * 32 + 8;

	if (m_pFaceGlow)
	{
		m_pFaceGlow->entity.origin = vecFace;
		// VERIFY: read as the local player against this one, as for the hand glows
		if (bOwnView && !CAM_IsThirdPerson())
			m_pFaceGlow->entity.curstate.renderamt = 0;
		else
			m_pFaceGlow->entity.curstate.renderamt = (int)(m_flCharge * 240);
		m_pFaceGlow->entity.curstate.framerate = m_flCharge * 15 + 5;
		m_pFaceGlow->die = flFrametime + m_pFaceGlow->die;
	}

	if (m_pFaceDlight)
	{
		m_pFaceDlight->origin = vecFace;
		m_pFaceDlight->color.r = (int)(m_flCharge * 192);
		m_pFaceDlight->color.g = 0;
		m_pFaceDlight->color.b = (int)(m_flCharge * 255);
		m_pFaceDlight->die = flFrametime + m_pFaceDlight->die;
	}

	if (m_pFaceElight)
	{
		m_pFaceElight->origin = vecFace;
		m_pFaceElight->color.r = (int)(m_flCharge * 129);
		m_pFaceElight->color.g = 0;
		m_pFaceElight->color.b = (int)(m_flCharge * 255);
		m_pFaceElight->die = flFrametime + m_pFaceElight->die;
	}

	return false;
}

bool CControllerChargeGlowTent::EntityGone(int iEntIndex) // orig client 0x95400
{
	if (m_iEntIndex != iEntIndex)
		return false;

	if (m_pGlows[0])
		KillLoopTent(m_pGlows[0]);
	if (m_pGlows[1])
		KillLoopTent(m_pGlows[1]);
	if (m_pFaceGlow)
		KillLoopTent(m_pFaceGlow);
	if (m_pDlights[0])
		KillLight(m_pDlights[0]);
	if (m_pDlights[1])
		KillLight(m_pDlights[1]);
	if (m_pElights[0])
		KillLight(m_pElights[0]);
	if (m_pElights[1])
		KillLight(m_pElights[1]);
	if (m_pFaceDlight)
		KillLight(m_pFaceDlight);
	if (m_pFaceElight)
		KillLight(m_pFaceElight);
	for (int i = 0; i < 8; i++)
	{
		if (m_pBeams[i])
			KillEntityBeam(m_pBeams[i]);
	}
	m_iEntIndex = 0;
	return true;
}

//
// CLTE 0x49: short the ball, byte the thrower, byte 0-255 the charge.
//
class CControllerChargeBallTent : public CClientTent // name ours
{
public:
	CControllerChargeBallTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;		 // orig +0x8: the ball
	int m_iThrower;			 // orig +0xc
	float m_flCharge;		 // orig +0x10: 0-1
	TEMPENTITY* m_pGlow;	 // orig +0x14: sprites/nhth1.spr on the ball
	dlight_t* m_pDlight;	 // orig +0x18
	dlight_t* m_pElight;	 // orig +0x1c
	BEAM* m_pBeams[8];		 // orig +0x20: sprites/zbeam2.spr from the ball to the players it leeches
	Vector m_vecLastOrigin;	 // orig +0x40: the ball's, when it last moved
	Vector m_vecDir;		 // orig +0x4c: its direction of travel
	bool m_bBallGone;		 // orig +0x58: the ball hit; only the streams back are left
	int m_cStreams;			 // orig +0x5c: (int)(2 * [TransformController] ChargeBallGiveMaxTime), 1-600
	float* m_pflStreamFrac;	 // orig +0x60: by stream, how far it still has to go (1 to 0; 0 free)
	Vector* m_pvecStreamStart; // orig +0x64: where it started (the ball)
	Vector* m_pvecStreamPos; // orig +0x68: where its head is
	CRC2Trail* m_pTrails;	 // orig +0x6c: by stream (new[])
	float m_flTrailTime;	 // orig +0x70: until the trails get their next points
	float m_flStreamTime;	 // orig +0x74: until the next stream may start (0.5 s)
	float m_flHits;			 // orig +0x78: leeches since the last stream started
	model_s* m_pTrailSprite; // orig +0x7c: sprites/gradsmoke2.spr
};

CClientTent* RC2_NewControllerChargeBallTent(const CClientTent* pCopy) // orig client 0x95c20
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CControllerChargeBallTent* pTent = pCopy ? new CControllerChargeBallTent(*static_cast<const CControllerChargeBallTent*>(pCopy)) : new CControllerChargeBallTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

CControllerChargeBallTent::CControllerChargeBallTent() // orig client 0x97510
{
	m_iEntIndex = READ_SHORT();
	m_iThrower = READ_BYTE();
	m_flCharge = READ_BYTE() * (1.0f / 255);
	if (m_flCharge < 0)
		m_flCharge = 0;
	else if (m_flCharge > 1)
		m_flCharge = 1;

	m_vecLastOrigin = Vector(0, 0, 0);
	m_pGlow = nullptr;
	m_pDlight = nullptr;
	m_pElight = nullptr;
	m_pflStreamFrac = nullptr;
	m_pvecStreamStart = nullptr;
	m_pvecStreamPos = nullptr;
	m_pTrails = nullptr;
	m_vecDir = Vector(0, 0, 0);
	m_cStreams = 0;
	m_flTrailTime = 0;
	m_flStreamTime = 0.5;
	m_flHits = 0;
	m_pTrailSprite = nullptr;
	m_bBallGone = false;
	for (int i = 0; i < 8; i++)
		m_pBeams[i] = nullptr;

	cl_entity_t* pBall = RC2_GetEntity(m_iEntIndex);
	if (!pBall)
	{
		m_iEntIndex = 0;
		m_iThrower = 0;
		return;
	}

	if (g_iClDetail >= 0)
	{
		m_pDlight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pDlight)
			ControllerLightSetup(m_pDlight, pBall->origin, m_flCharge * 50 + 100, 192, 0, 255);
		m_pElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pElight)
			ControllerLightSetup(m_pElight, pBall->origin, m_flCharge * 40 + 80, 192, 0, 255);
	}

	m_vecLastOrigin = pBall->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pBall->origin, 6, "ambience/crtnoise.wav", m_flCharge * 0.3 + 0.5, 0.7, 0, 130);

	int iModelIndex;
	model_s* pModel = gEngfuncs.CL_LoadModel("sprites/nhth1.spr", &iModelIndex);
	if (!pModel)
		return;
	m_pGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pBall->origin, pModel);
	if (!m_pGlow)
		return;
	ControllerGlowSetup(m_pGlow, (int)(m_flCharge * 50 + 180));
	m_pGlow->entity.curstate.scale = m_flCharge * 0.5 + 0.9;
	m_pGlow->entity.angles.z = gEngfuncs.pfnRandomFloat(0, 360);
	m_pGlow->frameMax = 11;
	m_pGlow->die += 5;

	int iBeamIndex;
	if (!gEngfuncs.CL_LoadModel("sprites/zbeam2.spr", &iBeamIndex))
		return;
	for (int i = 0; i < 8; i++)
	{
		Vector vecEnd = pBall->origin;
		m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamEntPoint(pBall->index, vecEnd, iBeamIndex, 0, 1.3, 0, 0, -5, 0, 0, 0.75, 0, 1.0);
	}

	int iTrailIndex;
	m_pTrailSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke2.spr", &iTrailIndex);

	m_cStreams = (int)(RC2Pref(RC2P_TransformController_ChargeBallGiveMaxTime).f + RC2Pref(RC2P_TransformController_ChargeBallGiveMaxTime).f);
	if (m_cStreams < 1)
		m_cStreams = 1;
	else if (m_cStreams > 600)
		m_cStreams = 600;
	m_pflStreamFrac = (float*)malloc(m_cStreams * sizeof(float));
	m_pvecStreamStart = (Vector*)malloc(m_cStreams * sizeof(Vector));
	m_pvecStreamPos = (Vector*)malloc(m_cStreams * sizeof(Vector));
	m_pTrails = new CRC2Trail[m_cStreams];
	memset(m_pflStreamFrac, 0, m_cStreams * sizeof(float));
	memset(m_pvecStreamStart, 0, m_cStreams * sizeof(Vector));
	memset(m_pvecStreamPos, 0, m_cStreams * sizeof(Vector));
}

// The thrower's side: each stream's head moves from where the ball hit toward 32 units in
// front of him at [TransformController] ChargeBallGiveTravelVel, its trail behind it. While the
// ball flies: it leeches with a beam each from up to eight live players in
// ChargeBallLeechRadius and in front of it (ChargeBallLeechMinDot) with nothing between, and
// every half second a leech starts a stream back from the ball. Done when the ball is gone and
// every trail has faded.
bool CControllerChargeBallTent::Think(float flFrametime) // orig client 0x95e50
{
	if (!m_iThrower)
		return true;

	cl_entity_t* pThrower = RC2_GetEntity(m_iThrower);
	if (!pThrower)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		if (m_pGlow)
			KillLoopTent(m_pGlow);
		if (m_pDlight)
			KillLight(m_pDlight);
		if (m_pElight)
			KillLight(m_pElight);
		for (int i = 0; i < 8; i++)
		{
			if (m_pBeams[i])
				KillEntityBeam(m_pBeams[i]);
		}
		gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, 6, "ambience/crtnoise.wav");
		if (m_pflStreamFrac)
			free(m_pflStreamFrac);
		if (m_pvecStreamStart)
			free(m_pvecStreamStart);
		if (m_pvecStreamPos)
			free(m_pvecStreamPos);
		if (m_pTrails)
		{
			// VERIFY: the original frees only the first trail's arrays, then passes the array
			// itself (new[]'s block + 4) to plain operator delete, which MSVC6's release heap
			// refuses without a word: the trails leak. Freeing that pointer here would abort,
			// so the leak is kept and the bad delete left out
			m_pTrails->Free();
		}
		m_pflStreamFrac = nullptr;
		m_pvecStreamStart = nullptr;
		m_pvecStreamPos = nullptr;
		m_pTrails = nullptr;
		m_cStreams = 0;
		return true;
	}

	const Vector vecThrower = RC2_GetEntityOrigin(pThrower->index);

	bool bTrailsLeft = false;
	for (int i = 0; i < m_cStreams; i++)
	{
		if (m_pTrails[i].Update(flFrametime))
			bTrailsLeft = true;

		if (m_pflStreamFrac[i] <= 0)
			continue;

		Vector vecDelta = m_pvecStreamStart[i] - vecThrower;
		Vector vecToStart;
		const float flLength = vecDelta.Length();
		if (flLength == 0)
			vecToStart = Vector(0, 0, 1);
		else
			vecToStart = vecDelta * (1.0f / flLength);

		const Vector vecTarget = vecToStart * 32 + vecThrower;
		m_pvecStreamPos[i] = vecTarget + (m_pvecStreamStart[i] - vecTarget) * m_pflStreamFrac[i];

		const Vector vecToThrower = vecThrower - m_pvecStreamStart[i];
		m_pflStreamFrac[i] = m_pflStreamFrac[i] - (RC2Pref(RC2P_TransformController_ChargeBallGiveTravelVel).f / vecToThrower.Length()) * flFrametime;
		if (m_pflStreamFrac[i] <= 0)
			m_pflStreamFrac[i] = 0;
	}

	m_flTrailTime -= flFrametime;
	if (m_flTrailTime < 0)
	{
		if (g_iClDetail < 0)
			m_flTrailTime = 0.4;
		else if (g_iClDetail == 0)
			m_flTrailTime = 0.25;
		else
			m_flTrailTime = 0.1;

		for (int i = 0; i < m_cStreams; i++)
		{
			if (m_pflStreamFrac[i] > 0)
			{
				bTrailsLeft = true;
				m_pTrails[i].AddPoint(m_pvecStreamPos[i], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.75, 0, 1.0), 0.8, 1.5, 1.6, 0, nullptr);
			}
		}
	}

	if (m_bBallGone)
		return !bTrailsLeft;

	if (!m_iEntIndex)
		return true;

	cl_entity_t* pBall = RC2_GetEntity(m_iEntIndex);
	if (!pBall)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return false;
	}
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return false;
	}

	// orig 0x6f610 (c2) is Vector::operator== out of line
	if (!(m_vecLastOrigin == pBall->origin))
	{
		const Vector vecMove = pBall->origin - m_vecLastOrigin;
		const float flLength = vecMove.Length();
		if (flLength == 0)
			m_vecDir = Vector(0, 0, 1);
		else
			m_vecDir = vecMove * (1.0f / flLength);
		m_vecLastOrigin = pBall->origin;
	}

	if (m_pGlow)
	{
		m_pGlow->entity.origin = pBall->origin;
		m_pGlow->die = flFrametime + m_pGlow->die;
	}
	if (m_pDlight)
	{
		m_pDlight->origin = pBall->origin;
		m_pDlight->die = flFrametime + m_pDlight->die;
	}
	if (m_pElight)
	{
		m_pElight->origin = pBall->origin;
		m_pElight->die = flFrametime + m_pElight->die;
	}

	const Vector vecBall = pBall->origin;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	int iBeam = 0;
	for (int iEnt = 1; iEnt < g_iMaxPlayerEnt; iEnt++)
	{
		if (iBeam >= 8)
			goto streams;

		cl_entity_t* pVictim = RC2_GetEntity(iEnt);
		if (!pVictim)
			continue;
		if (pVictim->curstate.messagenum < pLocal->curstate.messagenum)
			continue;
		if (pVictim->index == m_iThrower || pVictim->index == m_iEntIndex)
			continue;
		// VERIFY: curstate.team (+0xa4) == 2 is what the original tests; what RC2 keeps there is open
		if (pVictim->curstate.team != 2)
			continue;

		const Vector vecCenter = (pVictim->curstate.mins + pVictim->curstate.maxs) * 0.5 + pVictim->curstate.origin;
		const Vector vecToVictim = vecCenter - vecBall;
		if (vecToVictim.Length() > RC2Pref(RC2P_TransformController_ChargeBallLeechRadius).f)
			continue;

		// the original normalizes it three times over, and takes one component from each
		const float flLength = vecToVictim.Length();
		const Vector vecNorm = flLength == 0 ? Vector(0, 0, 1) : vecToVictim * (1.0f / flLength);
		if (vecNorm.x * m_vecDir.x + vecNorm.y * m_vecDir.y + vecNorm.z * m_vecDir.z < RC2Pref(RC2P_TransformController_ChargeBallLeechMinDot).f)
			continue;

		pmtrace_t tr;
		Vector vecStart = vecBall;
		Vector vecEnd = vecCenter;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_NORMAL, m_iEntIndex, &tr);
		if (tr.fraction < 1.0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) != pVictim->index)
			continue;

		// VERIFY: the original passes the ball's entity index, not the victim's, to the shield
		// intercept (see RC2_ShieldIntercept)
		int bShielded = 0;
		const Vector vecHit = RC2_ShieldIntercept(pBall->index, vecCenter, (vecCenter - vecBall).Normalize(), nullptr, &bShielded);
		if (!bShielded)
			m_flHits = m_flHits + 1.0;

		if (m_pBeams[iBeam])
		{
			m_pBeams[iBeam]->brightness = m_flCharge * 0.75 + 0.25;
			m_pBeams[iBeam]->target = vecHit;
		}
		iBeam++;
	}

	// the beams nobody holds go dark
	// (original bug: no null check; a beam missing because zbeam2.spr didn't load crashes here)
	for (; iBeam < 8; iBeam++)
		m_pBeams[iBeam]->brightness = 0;

streams:
	m_flStreamTime -= flFrametime;
	if (m_flStreamTime <= 0)
	{
		m_flStreamTime = 0.5;
		if (m_flHits > 0)
		{
			// one new stream, in the first free slot
			for (int i = 0; i < m_cStreams; i++)
			{
				if (m_pflStreamFrac[i] <= 0)
				{
					m_pflStreamFrac[i] = 1.0;
					m_pvecStreamStart[i] = vecBall;
					m_pvecStreamPos[i] = m_pvecStreamStart[i];
					break;
				}
			}
		}
		m_flHits = 0;
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
	return false;
}

// The streams' trails, each led by its head.
void CControllerChargeBallTent::DrawTransparent() // orig client 0x970a0
{
	int i;
	for (i = 0; i < m_cStreams; i++)
	{
		if (m_pTrails[i].CanDraw())
			break;
	}
	if (i >= m_cStreams)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pTrailSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pTrailSprite, 0);
	for (i = 0; i < m_cStreams; i++)
		m_pTrails[i].DrawToHead(m_pvecStreamPos[i], Vector(0.75, 0, 1.0), 0.8, 1.5, true);
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The thrower gone: everything ends. The ball gone (it hit): its lights, glow and beams end,
// and if it leeched anything every free stream starts back from it at once; the trails play out.
bool CControllerChargeBallTent::EntityGone(int iEntIndex) // orig client 0x971c0
{
	if (m_iThrower == 0 || m_iThrower == iEntIndex)
	{
		if (m_pGlow)
			KillLoopTent(m_pGlow);
		if (m_pDlight)
			KillLight(m_pDlight);
		if (m_pElight)
			KillLight(m_pElight);
		for (int i = 0; i < 8; i++)
		{
			if (m_pBeams[i])
				KillEntityBeam(m_pBeams[i]);
		}
		gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, 6, "ambience/crtnoise.wav");
		if (m_pflStreamFrac)
			free(m_pflStreamFrac);
		if (m_pvecStreamStart)
			free(m_pvecStreamStart);
		if (m_pvecStreamPos)
			free(m_pvecStreamPos);
		if (m_pTrails)
		{
			// VERIFY: as in Think, the original's operator delete of the array is left out
			m_pTrails->Free();
		}
		m_pflStreamFrac = nullptr;
		m_pvecStreamStart = nullptr;
		m_pvecStreamPos = nullptr;
		m_pTrails = nullptr;
		m_cStreams = 0;
		return true;
	}

	if (m_iEntIndex != iEntIndex)
		return false;

	if (m_iEntIndex != 0 && m_pflStreamFrac && m_pvecStreamStart && m_pvecStreamPos)
	{
		cl_entity_t* pBall = RC2_GetEntity(m_iEntIndex);
		if (pBall && m_flHits > 0)
		{
			for (int i = 0; i < m_cStreams; i++)
			{
				if (m_pflStreamFrac[i] <= 0)
				{
					m_pflStreamFrac[i] = 1.0;
					m_pvecStreamStart[i] = pBall->origin;
					m_pvecStreamPos[i] = m_pvecStreamStart[i];
				}
			}
		}
		m_flHits = 0;
	}

	if (m_pGlow)
		KillLoopTent(m_pGlow);
	if (m_pDlight)
		KillLight(m_pDlight);
	if (m_pElight)
		KillLight(m_pElight);
	for (int i = 0; i < 8; i++)
	{
		if (m_pBeams[i])
			KillEntityBeam(m_pBeams[i]);
	}
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, 6, "ambience/crtnoise.wav");
	m_iEntIndex = 0;
	m_bBallGone = true;
	return false;
}
