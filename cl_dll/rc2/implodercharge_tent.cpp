/***
*
*	Rocket Crowbar 2 client: the egon's imploder charging (CLTE 0x2c).
*
*	byte the player. While the imploder charges ([Egon] ImploderChargeTime, the client's copy)
*	the player's egon hums (ambience/zapmachine.wav, louder and higher as it charges) and a
*	spark sprite (sprites/xspark4.spr) grows on its muzzle; for the player himself the spark
*	sits in front of his view and a yellow beam runs to it from the gun. The server ends it
*	with CLTE 0xff/0x2c (egon.cpp, when the charge is fired or the egon holstered); who
*	starts it isn't in the server (VERIFY: likely the client's own egon prediction).
*
*	The original's functions 0x77cc0-0x78800 (and 0x9a2b0) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "rc2prefs.h"

// c3 needs:
// orig .bss 0x10210aa8 (owner: the client's RCCFG handler, outside c3): the client's copy of
// rc2.ini's NETWORK_SYNC keys, one 8-byte slot per key in the server's order
// (0x10211888 is RC2P_Egon_ImploderChargeTime), read through rc2prefs.h's RC2Pref() over the
// client's g_rc2PrefValues.

class CImploderChargeTent : public CClientTent // name ours
{
public:
	CImploderChargeTent();

	bool Update(float flFrametime) override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;	   // orig +0x8
	float m_flLife;		   // orig +0xc: charge time left
	float m_flMaxLife;	   // orig +0x10
	float m_flForward;	   // orig +0x14: the spark's offset in the local player's view: 16 forward,
	float m_flRight;	   // orig +0x18: 3 right,
	float m_flUp;		   // orig +0x1c: -6 up
	TEMPENTITY* m_pSpark;  // orig +0x20
	BEAM* m_pBeam;		   // orig +0x24: the local player's only
};

// orig 0x78300: a vector times a scale into *pOut. Out of line in the original, inline here.
static void ScaleVector(float* pOut, float flScale, const float* pIn) // orig client 0x78300
{
	pOut[0] = flScale * pIn[0];
	pOut[1] = flScale * pIn[1];
	pOut[2] = flScale * pIn[2];
}

// The factory (the dispatcher's case 0x2c; c3_tents.h). pCopy: a copy through the implicit
// copy constructor, out of line in the original (orig client 0x9a2b0).
CClientTent* RC2_NewImploderChargeTent(const CClientTent* pCopy) // orig client 0x77cc0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CImploderChargeTent* pTent = pCopy ? new CImploderChargeTent(*static_cast<const CImploderChargeTent*>(pCopy)) : new CImploderChargeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x77cc0).
CImploderChargeTent::CImploderChargeTent()
{
	m_iEntIndex = READ_BYTE();
	m_flLife = RC2Pref(RC2P_Egon_ImploderChargeTime).f;
	m_flMaxLife = RC2Pref(RC2P_Egon_ImploderChargeTime).f;
	m_pBeam = nullptr;
	m_pSpark = nullptr;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_WEAPON, "ambience/zapmachine.wav", 0.5, 0.8, 0, 90);

	int iModelIndex;
	if (gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
	{
		Vector vecSrc = CLTE_GetOrigin(m_iEntIndex);
		m_flForward = 16;
		m_flRight = 3;
		m_flUp = -6;
		Vector vecViewOfs;
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecViewOfs);
		Vector vecUp, vecRight, vecForward;
		ScaleVector(vecUp, m_flUp, g_vecTentUp);
		ScaleVector(vecRight, m_flRight, g_vecTentRight);
		ScaleVector(vecForward, m_flForward, g_vecTentForward);
		vecSrc = vecSrc + vecViewOfs + vecForward + vecRight + vecUp;

		if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModelIndex))
		{
			m_pBeam = gEngfuncs.pEfxAPI->R_BeamEntPoint(m_iEntIndex | 0x1000, vecSrc, iModelIndex, 0.0, 0.7, 0.75, 0.5, 1.0, 0, 0.0, 1.0, 0.8, 0.0);
			if (m_pBeam)
				m_pBeam->flags |= FBEAM_SHADEIN;
		}

		model_s* pSpark = gEngfuncs.CL_LoadModel("sprites/xspark4.spr", &iModelIndex);
		if (!pSpark)
			return;
		m_pSpark = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecSrc, pSpark);
		if (!m_pSpark)
			return;
		m_pSpark->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
		m_pSpark->entity.curstate.scale = 0.2;
		m_pSpark->entity.curstate.renderamt = 100;
		m_pSpark->entity.curstate.rendermode = kRenderGlow;
		m_pSpark->entity.curstate.renderfx = kRenderFxNoDissipation;
		m_pSpark->entity.curstate.rendercolor.g = 255;
		m_pSpark->entity.curstate.rendercolor.r = 255;
		m_pSpark->entity.curstate.rendercolor.b = 255;
		m_pSpark->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 10);
		m_pSpark->entity.curstate.framerate = 15;
		m_pSpark->frameMax = 11;
	}
	else
	{
		// on everyone else's egon: follows the muzzle (attachment 1)
		model_s* pSpark = gEngfuncs.CL_LoadModel("sprites/xspark4.spr", &iModelIndex);
		if (!pSpark)
			return;
		m_pSpark = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEntity->attachment[0], pSpark);
		if (!m_pSpark)
			return;
		m_pSpark->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
		m_pSpark->entity.curstate.scale = 0.2;
		m_pSpark->entity.curstate.renderamt = 100;
		m_pSpark->entity.curstate.rendermode = kRenderGlow;
		m_pSpark->entity.curstate.renderfx = kRenderFxNoDissipation;
		m_pSpark->entity.curstate.rendercolor.g = 255;
		m_pSpark->entity.curstate.rendercolor.r = 255;
		m_pSpark->entity.curstate.rendercolor.b = 255;
		m_pSpark->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 10);
		m_pSpark->entity.curstate.framerate = 15;
		m_pSpark->frameMax = 11;
		m_pSpark->entity.curstate.movetype = MOVETYPE_FOLLOW;
		m_pSpark->entity.curstate.aiment = pEntity->index;
		m_pSpark->entity.curstate.body = 1;
		m_pSpark->entity.curstate.skin = (short)pEntity->index;
	}
	m_pSpark->die += 10;
}

bool CImploderChargeTent::Update(float flFrametime) // orig client 0x78350
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	m_flLife = m_flLife - flFrametime;
	if (m_flLife < 0)
		m_flLife = 0;

	float flCharge = 1.0 - m_flLife / m_flMaxLife;
	if (flCharge < 0)
		flCharge = 0;
	else if (flCharge > 1)
		flCharge = 1;
	const double flScale = flCharge + 1.0;

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_WEAPON, "ambience/zapmachine.wav", flScale * 0.5, 0.8, 0xc0, (int)(flCharge * 40 + 90)); // 0xc0: SND_CHANGE_VOL | SND_CHANGE_PITCH

	if (m_pBeam)
		m_pBeam->brightness = flCharge * 0.6 + 0.4;

	if (m_pSpark)
	{
		m_pSpark->entity.curstate.renderamt = (int)(flCharge * 100 + 150);
		m_pSpark->entity.curstate.scale = flScale * 0.2;
		m_pSpark->die = gEngfuncs.GetClientTime() + 10.0;
	}

	if (!gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
	{
		if (m_pSpark)
			m_pSpark->entity.origin = pEntity->attachment[0];
	}
	else
	{
		Vector vecSrc = CLTE_GetOrigin(m_iEntIndex);
		Vector vecViewOfs;
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecViewOfs);
		const Vector vecUp = g_vecTentUp * m_flUp;
		const Vector vecRight = g_vecTentRight * m_flRight;
		const Vector vecForward = g_vecTentForward * m_flForward;
		vecSrc = vecSrc + vecViewOfs + vecForward + vecRight + vecUp;
		if (m_pBeam)
			m_pBeam->target = vecSrc;
		if (m_pSpark)
			m_pSpark->entity.origin = vecSrc;
	}
	return false;
}

bool CImploderChargeTent::KillEntity(int iEntIndex) // orig client 0x78800
{
	if (iEntIndex != m_iEntIndex)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_WEAPON, "ambience/zapmachine.wav");
	if (m_pBeam)
	{
		m_pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pBeam->t = 1.0;
		m_pBeam->die = 0.1;
		m_pBeam->endEntity = 0;
		m_pBeam->startEntity = 0;
		m_pBeam = nullptr;
	}
	if (m_pSpark)
	{
		m_pSpark->die = 0;
		m_pSpark = nullptr;
	}
	m_iEntIndex = 0;
	return true;
}
