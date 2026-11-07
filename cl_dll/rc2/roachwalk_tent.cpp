/***
*
*	Rocket Crowbar 2 client: the roach charge (CLTE 0x3b, CLTE_ROACHWALK).
*
*	Server dlls/satchel.cpp: short the charge, byte armed. c2's CLTE dispatcher (orig 0x510b0)
*	does the message: the first one (armed 0) creates this effect through the factory here
*	and sets m_iEntIndex; a later one with armed set finds it by entity and arms it: below
*	cl_detail 0 a red sprites/smoke.spr follow beam (R_BeamFollow, FBEAM_FOREVER) into m_pBeam,
*	otherwise m_vecOrigin from the entity, a first trail point and sprites/gradsmoke.spr into
*	m_pTrailSprite; then m_bArmed and both arm times to [Satchel] RoachArmTime. Armed, the roach
*	scuttles louder and higher as the arming runs out, glows red, and leaves a red trail.
*
*	The original's functions 0x97b40-0x981b0 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

// the engine's sound flags (dlls/util.h; the client has no header with them)
#ifndef SND_CHANGE_VOL
#define SND_CHANGE_VOL (1 << 6)
#define SND_CHANGE_PITCH (1 << 7)
#endif

class CRoachWalkTent : public CClientTent // name ours
{
public:
	CRoachWalkTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;		 // orig +0x8: the charge (the dispatcher sets it)
	float m_flTrailTime;	 // orig +0xc: until the next trail point
	bool m_bVisible;		 // orig +0x10: in the PVS this frame
	Vector m_vecOrigin;		 // orig +0x14
	model_s* m_pTrailSprite; // orig +0x20: sprites/gradsmoke.spr (the dispatcher loads it)
	CRC2Trail m_Trail;		 // orig +0x24
	BEAM* m_pBeam;			 // orig +0x4c: the follow beam below cl_detail 0 (the dispatcher makes it)
	bool m_bArmed;			 // orig +0x50 (the dispatcher sets it)
	float m_flArmTimeLeft;	 // orig +0x54
	float m_flArmTime;		 // orig +0x58
};

// The factory; reads nothing (the dispatcher read the message). The copy is inline here.
CClientTent* RC2_NewRoachWalkTent(const CClientTent* pCopy) // orig client 0x97b40
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CRoachWalkTent* pTent = pCopy ? new CRoachWalkTent(*static_cast<const CRoachWalkTent*>(pCopy)) : new CRoachWalkTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x97b40).
CRoachWalkTent::CRoachWalkTent()
{
	m_bVisible = false;
	m_bArmed = false;
	m_iEntIndex = 0;
	m_flArmTime = 1.0;
	m_flArmTimeLeft = 1.0;
	if (g_iClDetail < 0)
		m_flTrailTime = 0.1;
	else if (g_iClDetail == 0)
		m_flTrailTime = 0.075;
	else
		m_flTrailTime = 0.05;
	m_pTrailSprite = nullptr;
	m_pBeam = nullptr;
}

bool CRoachWalkTent::Think(float flFrametime) // orig client 0x97d70
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		return true;
	}

	// how far the arming has run, 0-1
	float flArm = 0;
	if (m_bArmed)
	{
		m_flArmTimeLeft = m_flArmTimeLeft - flFrametime;
		flArm = 1.0 - m_flArmTimeLeft / m_flArmTime;
		if (flArm < 0)
			flArm = 0;
		else if (flArm > 1)
			flArm = 1;
	}

	m_bVisible = true;
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pEntity->curstate.messagenum < pLocal->curstate.messagenum)
	{
		// out of the PVS: only the trail fades
		if (g_iClDetail >= 0 && m_bArmed)
			m_Trail.Update(flFrametime);
		m_bVisible = false;
		return false;
	}

	const float flPitch = flArm * 150 + 100;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, 6, "roach/rch_walk.wav", flArm * 0.75 + 0.25, 0.9 - flArm * 0.4, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)flPitch);

	if (g_iClDetail >= 0)
	{
		if (!m_bArmed)
			return false;

		m_vecOrigin = RC2_GetEntityOrigin(m_iEntIndex);
		m_flTrailTime = m_flTrailTime - flFrametime;
		if (m_flTrailTime <= 0)
		{
			if (g_iClDetail < 0)
				m_flTrailTime = 0.15;
			else if (g_iClDetail == 0)
				m_flTrailTime = 0.1;
			else
				m_flTrailTime = 0.075;
			m_Trail.AddPoint(m_vecOrigin, Vector(0, 0, 20), Vector(0, 0, 0), Vector(1, 0, 0), 0.8, 2, 0.8, 0, nullptr);
		}
		m_Trail.Update(flFrametime);
	}

	if (m_bArmed)
	{
		// the red shell thickens and brightens as the arming runs out
		pEntity->curstate.renderfx = kRenderFxGlowShell;
		pEntity->curstate.renderamt = (int)(flArm * 8);
		pEntity->curstate.rendercolor.r = (int)flPitch;
		pEntity->curstate.rendercolor.b = 0;
		pEntity->curstate.rendercolor.g = 0;
	}
	return false;
}

void CRoachWalkTent::DrawTransparent() // orig client 0x980c0
{
	if (!m_bVisible || g_iClDetail < 0 || !m_bArmed)
		return;
	if (!m_Trail.CanDraw())
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pTrailSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pTrailSprite, 0);
	m_Trail.DrawToHead(m_vecOrigin, Vector(1, 0, 0), 0.8, 2.0, true);
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The charge gone (or killed with CLTE 0xfe/0x3b): the follow beam ends and the scuttle stops.
// m_iEntIndex is left as it is.
bool CRoachWalkTent::EntityGone(int iEntIndex) // orig client 0x981b0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	if (m_pBeam)
	{
		m_pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pBeam->endEntity = 0;
		m_pBeam->startEntity = 0;
		m_pBeam = nullptr;
	}
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, 6, "roach/rch_walk.wav");
	return true;
}
