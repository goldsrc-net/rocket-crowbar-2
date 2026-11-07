/***
*
*	Rocket Crowbar 2 client: a transformed houndeye charging its beam (CLTE 0x41).
*
*	Sent by the server (thoundeye.cpp) when the houndeye starts charging, killed with CLTE
*	0xfe/0x41 when it fires or stops. Over the charge time (rc2.ini TransformHoundeye
*	BeamChargeTime) a glow grows in front of its head, particles are sucked in toward it, four
*	green arcs spin around it ever faster and green sparks stream in from ahead, while
*	weapons/egon_run3.wav rises in pitch.
*
*	The original's functions 0x8f460-0x91490 (and 0x9ae40) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"
#include "rc2prefs.h"

#include <cmath>
#include <cstdint>

// c3 needs:
// orig .bss 0x10210aa8: the client's copy of the rc2.ini values (8 bytes a key, in the server's
// key order), read through rc2prefs.h's RC2Pref() over the client's g_rc2PrefValues.

// The factory (the dispatcher's case 0x41; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line (
// orig client 0x9ae40). The dispatcher always passes null.
CClientTent* RC2_NewHoundeyeChargeTent(const CClientTent* pCopy) // orig client 0x8f460
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CHoundeyeChargeTent* pTent = pCopy ? new CHoundeyeChargeTent(*static_cast<const CHoundeyeChargeTent*>(pCopy)) : new CHoundeyeChargeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x8f460): reads the message.
CHoundeyeChargeTent::CHoundeyeChargeTent()
{
	m_iEntIndex = READ_BYTE();
	m_pParticleSprite = nullptr;
	m_pArcSprite = nullptr;
	m_pGlow = nullptr;
	m_flChargeTime = RC2Pref(RC2P_TransformHoundeye_BeamChargeTime).f;
	m_flTimeLeft = RC2Pref(RC2P_TransformHoundeye_BeamChargeTime).f;
	m_bDrawInner = true;
	m_flNextParticle = 0;
	m_flArcPhase = 0;
	m_flArcSpin = (gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1) * 0.1;

	for (int i = 0; i < HOUNDEYE_ARC_POINTS; i++)
	{
		m_colArc[i] = Vector(0.15, 1.0, 0.1);
		m_flArcAlpha[i] = 0.01;
		m_flArcWidth[i] = 0.5;
		for (int j = HOUNDEYE_ARCS - 1; j >= 0; j--)
			m_vecArc[j][i] = Vector(0, 0, 0);
	}

	m_iSparks = 0;
	for (int i = 0; i < HOUNDEYE_SPARKS; i++)
		m_flSparkLife[i] = 0;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;

	int iModelIndex;
	m_pParticleSprite = gEngfuncs.CL_LoadModel("sprites/heflare2.spr", &iModelIndex);
	m_pArcSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke3.spr", &iModelIndex);
	m_pSparkSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke.spr", &iModelIndex);

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_WEAPON, "ambience/particle_suck1.wav", 0.7, 0.5, 0, 110);
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "weapons/egon_run3.wav", 0.4, 0.5, 0, 60);

	model_s* pGlowSprite = gEngfuncs.CL_LoadModel("sprites/heflare1.spr", &iModelIndex);
	if (pGlowSprite)
	{
		m_pGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pGlowSprite);
		if (m_pGlow)
		{
			m_pGlow->flags |= FTENT_PERSIST;
			m_pGlow->entity.curstate.scale = 2.0;
			m_pGlow->entity.curstate.renderamt = 1;
			m_pGlow->entity.curstate.rendermode = kRenderGlow;
			m_pGlow->entity.curstate.renderfx = kRenderFxNoDissipation;
			m_pGlow->entity.curstate.rendercolor.r = 255;
			m_pGlow->entity.curstate.rendercolor.g = 255;
			m_pGlow->entity.curstate.rendercolor.b = 255;
			m_pGlow->entity.curstate.owner = m_iEntIndex; // VERIFY: +0x38c of the tempent is curstate.owner by SDK 2.3's layout
			m_pGlow->die += 10.0;
		}
	}
}

bool CHoundeyeChargeTent::Update(float flFrametime) // orig client 0x8fa00
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		return true;
	}

	if (m_pGlow)
		m_pGlow->die = gEngfuncs.GetClientTime() + 10.0;

	m_flTimeLeft -= flFrametime;
	m_bVisible = true;
	float flFrac = m_flTimeLeft / m_flChargeTime;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;
	const float flDone = 1.0 - flFrac;
	float flGlow = flDone * 1.2;
	if (flGlow < 0)
		flGlow = 0;
	else if (flGlow > 1)
		flGlow = 1;

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "weapons/egon_run3.wav", (double)flDone * 0.6 + 0.4, 0.5, 0xc0 /* SND_CHANGE_VOL | SND_CHANGE_PITCH */, (int)((flDone + 1.0f) * 60));

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal)
	{
		if (pLocal->curstate.messagenum > pEntity->curstate.messagenum && pLocal->index != pEntity->index)
		{
			m_bVisible = false;
			if (m_pGlow)
				m_pGlow->entity.origin = Vector(4096, 4096, 4096);
			return false;
		}
		if (pEntity->index == pLocal->index)
			m_bDrawInner = CL_IsThirdPerson();
	}

	m_vecOrigin = CLTE_GetOrigin(pEntity->index);
	Vector vecAngles = pEntity->angles;
	vecAngles.x = 0;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);
	m_vecOrigin = vecUp * 8 + (vecForward * 30 + m_vecOrigin);

	if (m_pGlow)
	{
		m_pGlow->entity.origin = m_vecOrigin;
		m_pGlow->entity.curstate.renderamt = (int)(flGlow * 140 + 60);
	}

	// particles sucked in, faster as the charge grows
	m_flNextParticle -= flFrametime;
	if (m_flNextParticle <= 0)
	{
		if (g_iClientDetail < 0)
			m_flNextParticle = 0.15 - flGlow * 0.1;
		else if (g_iClientDetail == 0)
			m_flNextParticle = 0.1 - flGlow * 0.075;
		else
			m_flNextParticle = 0.1 - flGlow * 0.0999;

		const float flAround = gEngfuncs.pfnRandomFloat(-2.2779, 2.2779);
		const float flOff = gEngfuncs.pfnRandomFloat(-0.175, 0.175);
		const double flCosOff = cos(flOff);
		Vector vecDir = vecForward * sin(flOff);
		vecDir = vecRight * (sin(flAround) * flCosOff) + vecDir;
		vecDir = vecUp * (cos(flAround) * flCosOff) + vecDir;
		m_Particles.Add(m_vecOrigin + vecDir * gEngfuncs.pfnRandomFloat(64, 72), Vector(0, 0, 0), Vector(0, 0, 0), Vector(1, 1, 1), 0.01, 0.05, 1.0, 1.0, this, Particle_Seek); // the user pointer is the effect: Particle_Seek flies to m_vecOrigin
	}
	m_Particles.Update(flFrametime);

	// the arcs spin up over the charge time
	const float flSpinUp = (12.566371 / m_flChargeTime) * flFrametime;
	if (m_flArcSpin < 0)
		m_flArcSpin = m_flArcSpin - flSpinUp;
	else
		m_flArcSpin = flSpinUp + m_flArcSpin;
	if (m_flArcSpin < -12.566371f)
		m_flArcSpin = -12.566371f;
	else if (m_flArcSpin > 12.566371f)
		m_flArcSpin = 12.566371f;
	m_flArcPhase = m_flArcSpin * flFrametime + m_flArcPhase;
	while (m_flArcPhase < 0)
		m_flArcPhase = m_flArcPhase + 6.283185307179586;
	while (m_flArcPhase > 6.283185307179586)
		m_flArcPhase = m_flArcPhase - 6.283185307179586;

	float flAngle = m_flArcPhase - 0.2617993877991494;
	const float flInner = (double)flDone * 4 + 16;
	const float flOuter = (double)flDone * 6 + 18;
	const float flArcAlpha = flGlow * 0.55 + 0.2;
	const float flArcWidth = flGlow * 0.8 + 0.5;
	for (int i = 0; i < HOUNDEYE_ARC_POINTS; i++)
	{
		const Vector vecRadial = vecUp * sin(flAngle) + vecRight * cos(flAngle);
		const Vector vecNear = vecForward * 12 + m_vecOrigin;
		m_vecArc[0][i] = vecRadial * flInner + vecNear;
		m_vecArc[1][i] = vecNear - vecRadial * flInner;

		const Vector vecRadial2 = vecUp * sin(-flAngle) + vecRight * cos(-flAngle);
		const Vector vecFar = vecForward * 32 + m_vecOrigin;
		m_vecArc[2][i] = vecRadial2 * flOuter + vecFar;
		m_vecArc[3][i] = vecFar - vecRadial2 * flOuter;

		flAngle = flAngle + 0.08726646259971647;
		m_flArcAlpha[i] = flArcAlpha;
		m_flArcWidth[i] = flArcWidth;
	}
	m_flArcWidth[0] = m_flArcWidth[0] * 0.5;
	m_flArcAlpha[HOUNDEYE_ARC_POINTS - 1] = 0;
	m_flArcAlpha[0] = 0;
	m_flArcWidth[HOUNDEYE_ARC_POINTS - 1] = m_flArcWidth[HOUNDEYE_ARC_POINTS - 1] * 0.5;

	// sparks streaming in from ahead, more as the charge grows
	m_iSparks = (int)ceil(flGlow * 16); // VERIFY: libcmt 0xf0650 read as ceil (its error path rounds up)
	for (int i = 0; i < m_iSparks; i++)
	{
		Vector& vecLocal = m_vecSpark[i][1];
		if (m_flSparkLife[i] < 0.05)
		{
			vecLocal.x = gEngfuncs.pfnRandomFloat(120, 140) + flDone * 24;
			vecLocal.y = gEngfuncs.pfnRandomFloat(-24, 24);
			vecLocal.z = gEngfuncs.pfnRandomFloat(-24, 24);
			m_vecSpark[i][0] = vecUp * vecLocal.z + (vecRight * vecLocal.y + (vecForward * vecLocal.x + m_vecOrigin));
			m_flSparkLife[i] = 0.7;
			const float flPale = gEngfuncs.pfnRandomFloat(0.1, 0.2);
			m_colSpark[i] = Vector(flPale, 1.0, flPale);
		}
		else
		{
			const Vector vecPull = vecLocal * 0.44;
			vecLocal = Vector(vecLocal.x - vecPull.x * flFrametime, vecLocal.y - vecPull.y * flFrametime, vecLocal.z - vecPull.z * flFrametime);
			m_flSparkLife[i] = m_flSparkLife[i] - flFrametime * 0.9333333333333333;
			m_vecSpark[i][0] = vecForward * vecLocal.x + m_vecOrigin + vecRight * vecLocal.y + vecUp * vecLocal.z;
		}

		if (m_flSparkLife[i] < 0)
			m_flSparkLife[i] = 0;
		else if (m_flSparkLife[i] > 1)
			m_flSparkLife[i] = 1;
	}
	return false;
}

void CHoundeyeChargeTent::DrawTransparent() // orig client 0x91290
{
	if (!m_bVisible)
		return;

	bool bParticles = false;
	bool bSparks = false;
	if (m_bDrawInner)
		bParticles = m_Particles.HasAlive();
	if (g_iClientDetail >= 0 && m_bDrawInner)
		bSparks = m_iSparks > 0;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	if (bParticles)
	{
		if (m_pParticleSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pParticleSprite, 0);
		m_Particles.Draw();
	}

	if (m_pArcSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pArcSprite, 0);
	for (int i = 0; i < HOUNDEYE_ARCS; i++)
		CLTE_DrawStrip(m_vecArc[i], m_flArcWidth, m_colArc, m_flArcAlpha, HOUNDEYE_ARC_POINTS, true, true, m_flArcSegLen);

	if (bSparks)
	{
		if (m_pSparkSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pSparkSprite, 0);
		for (int i = 0; i < m_iSparks; i++)
			CLTE_DrawBeamQuad(m_vecOrigin, m_vecSpark[i][0], 3.0, 0, m_colSpark[i], m_flSparkLife[i], m_colSpark[i], 0);
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The beam fired or the charge stopped (CLTE 0xfe/0x41). The suck sound plays out.
bool CHoundeyeChargeTent::KillEntity(int iEntIndex) // orig client 0x91490
{
	if (iEntIndex != m_iEntIndex)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_STATIC, "weapons/egon_run3.wav");
	if (m_pGlow)
	{
		m_pGlow->die = 0;
		m_pGlow = nullptr;
	}
	m_iEntIndex = 0;
	return true;
}
