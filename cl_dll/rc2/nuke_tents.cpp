/***
*
*	Rocket Crowbar 2 client: the nuclear missile's flight (CLTE 0x2a).
*
*	Sent by the missile when it launches (server nuke.cpp: short entity, byte the player who
*	fired it, byte 0) and killed with CLTE 0xfe/0x2a. The client rolls the missile as it flies,
*	pushes its model back 20 units along its heading, hides it from the player steering it,
*	plays its looping engine sound, trails three smoke beams from its attachments, puts glow
*	sprites and two dynamic lights on it, blows bubbles under water and, for everyone but the
*	steering player, draws crossed flame quads (sprites/nukef1.spr) from each exhaust.
*
*	The original's functions 0x75a80-0x76c40 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"

// c3 needs:
// orig 0xdd80 ("new: start .. ev_hldm", c1): the height of the water surface above vecPos
// between flMinZ and flMaxZ, by bisecting PM_PointContents down to a unit (flMinZ when vecPos
// isn't in water at flMinZ, flMaxZ when it is still water there). Only x and y of vecPos are
// used. name ours
float RC2_WaterLevel(const float* vecPos, float flMinZ, float flMaxZ);

class CNukeTent : public CClientTent // name ours
{
public:
	CNukeTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	void FlameVertex(Vector vecBase, float flForward, float flRight, float flUp, float s, float t);

	BEAM* m_pTrails[3];		 // orig +0x8: smoke beams following attachments 1-3
	TEMPENTITY* m_pFlare;	 // orig +0x14: sprites/bigflare1.spr on the missile
	TEMPENTITY* m_pGlows[3]; // orig +0x18: sprites/animglow01.spr on each attachment
	dlight_t* m_pLight;		 // orig +0x24
	dlight_t* m_pLight2;	 // orig +0x28
	model_s* m_pFlame;		 // orig +0x2c: sprites/nukef1.spr
	int m_iBubbleModel;		 // orig +0x30: sprites/bubble.spr's index
	Vector m_vecForward;	 // orig +0x34
	Vector m_vecRight;		 // orig +0x40
	Vector m_vecUp;			 // orig +0x4c
	Vector m_vecOrigin;		 // orig +0x58: (4096,4096,4096) while the missile isn't in this client's view
	Vector m_vecAttachment[3]; // orig +0x64: the missile's attachments 0-2
	int m_iSmokeModel;		 // orig +0x88: sprites/smoke.spr's index
	int m_iEntIndex;		 // orig +0x8c
	int m_iOwner;			 // orig +0x90: the player steering it
	float m_flBubbles;		 // orig +0x94: bubbles owed, 8 a second
	float m_flRollSpeed;	 // orig +0x98: 260-280 degrees a second, either way
	float m_flRoll;			 // orig +0x9c
	bool m_bVisible;		 // orig +0xa0
};

// The factory (the dispatcher's case 0x2a; c3_tents.h). The copy is inline here.
CClientTent* RC2_NewNukeTent(const CClientTent* pCopy) // orig client 0x75a80
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CNukeTent* pTent = pCopy ? new CNukeTent(*static_cast<const CNukeTent*>(pCopy)) : new CNukeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// short entity, byte the player who fired it (0: none), byte 0. The original never sets the
// lights below cl_detail 0, nor m_flBubbles at all: they keep whatever the heap held (kept).
CNukeTent::CNukeTent() // orig client 0x76c40
{
	m_iEntIndex = READ_SHORT();
	m_iOwner = READ_BYTE();
	m_vecUp = Vector(0, 0, 0);
	m_pTrails[2] = nullptr;
	m_pTrails[1] = nullptr;
	m_pTrails[0] = nullptr;
	m_pFlare = nullptr;
	m_pGlows[2] = nullptr;
	m_pGlows[1] = nullptr;
	m_pGlows[0] = nullptr;
	m_vecRight = Vector(0, 0, 0);
	m_bVisible = false;
	m_pFlame = nullptr;
	m_vecForward = Vector(0, 0, 0);

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
		return;

	m_vecOrigin = pEntity->origin;
	m_bVisible = true;
	gEngfuncs.pfnAngleVectors(pEntity->angles, m_vecForward, m_vecRight, m_vecUp);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pEntity->curstate.messagenum < pLocal->curstate.messagenum)
	{
		m_vecOrigin = Vector(4096, 4096, 4096);
		m_bVisible = false;
	}

	m_flRollSpeed = gEngfuncs.pfnRandomFloat(260, 280);
	m_flRollSpeed = ((gEngfuncs.pfnRandomLong(0, 1) != 0) ? 1 : -1) * m_flRollSpeed;
	m_flRoll = gEngfuncs.pfnRandomFloat(0, 360);

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, 6, "weapons/nuker.wav", 1.0, 0.5, 0, 100);

	if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &m_iSmokeModel))
	{
		m_pTrails[0] = gEngfuncs.pEfxAPI->R_BeamFollow(m_iEntIndex | 0x1000, m_iSmokeModel, 4.0, 1.5, 1.0, 1.0, 1.0, 0.9);
		if (m_pTrails[0])
			m_pTrails[0]->flags |= FBEAM_FOREVER;
		m_pTrails[1] = gEngfuncs.pEfxAPI->R_BeamFollow(m_iEntIndex | 0x2000, m_iSmokeModel, 4.0, 1.5, 1.0, 1.0, 1.0, 0.9);
		if (m_pTrails[1])
			m_pTrails[1]->flags |= FBEAM_FOREVER;
		m_pTrails[2] = gEngfuncs.pEfxAPI->R_BeamFollow(m_iEntIndex | 0x3000, m_iSmokeModel, 4.0, 1.5, 1.0, 1.0, 1.0, 0.9);
		if (m_pTrails[2])
			m_pTrails[2]->flags |= FBEAM_FOREVER;
	}

	int iModelIndex;
	m_pFlame = gEngfuncs.CL_LoadModel("sprites/nukef1.spr", &iModelIndex);
	gEngfuncs.CL_LoadModel("sprites/bubble.spr", &m_iBubbleModel);

	// the glows are for everyone but the player steering it
	if (m_iOwner == 0 || !gEngfuncs.pEventAPI->EV_IsLocal(m_iOwner - 1))
	{
		model_s* pFlare = gEngfuncs.CL_LoadModel("sprites/bigflare1.spr", &iModelIndex);
		if (pFlare)
		{
			m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pFlare);
			if (m_pFlare)
			{
				m_pFlare->flags |= FTENT_PERSIST;
				m_pFlare->entity.curstate.scale = 1.5;
				m_pFlare->entity.curstate.renderamt = 200;
				m_pFlare->entity.curstate.rendermode = kRenderGlow;
				m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
				m_pFlare->entity.curstate.rendercolor.r = 255;
				m_pFlare->entity.curstate.rendercolor.g = 180;
				m_pFlare->entity.curstate.rendercolor.b = 130;
				m_pFlare->entity.curstate.frame = 0;
				m_pFlare->die += 10;
			}
		}

		model_s* pGlow = gEngfuncs.CL_LoadModel("sprites/animglow01.spr", &iModelIndex);
		if (pGlow)
		{
			for (int i = 0; i < 3; i++)
			{
				m_pGlows[i] = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pGlow);
				if (m_pGlows[i])
				{
					m_pGlows[i]->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
					m_pGlows[i]->entity.curstate.scale = 0.9;
					m_pGlows[i]->entity.curstate.renderamt = 210;
					m_pGlows[i]->entity.curstate.rendermode = kRenderGlow;
					m_pGlows[i]->entity.curstate.renderfx = kRenderFxNoDissipation;
					m_pGlows[i]->entity.curstate.rendercolor.r = 255;
					m_pGlows[i]->entity.curstate.rendercolor.g = 180;
					m_pGlows[i]->entity.curstate.rendercolor.b = 130;
					m_pGlows[i]->entity.curstate.framerate = 10;
					m_pGlows[i]->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 6);
					m_pGlows[i]->frameMax = 7;
					m_pGlows[i]->die += 10;
				}
			}
		}
	}

	if (g_iClientDetail >= 0)
	{
		m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pLight)
		{
			m_pLight->origin = m_vecOrigin;
			m_pLight->radius = 300;
			m_pLight->color.r = 255;
			m_pLight->color.g = 200;
			m_pLight->color.b = 150;
			m_pLight->die = gEngfuncs.GetClientTime() + 10.0;
		}
		m_pLight2 = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pLight2)
		{
			m_pLight2->origin = m_vecOrigin;
			m_pLight2->radius = 200;
			m_pLight2->color.r = 255;
			m_pLight2->color.g = 200;
			m_pLight2->color.b = 150;
			m_pLight2->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}
}

bool CNukeTent::Update(float flFrametime) // orig client 0x75d70
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

	m_bVisible = true;
	m_vecOrigin = pEntity->origin;
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pEntity->curstate.messagenum < pLocal->curstate.messagenum)
	{
		m_vecOrigin = Vector(4096, 4096, 4096);
		m_bVisible = false;
	}

	m_flRoll = flFrametime * m_flRollSpeed + m_flRoll;
	if (m_bVisible)
	{
		Vector vecAngles(-pEntity->angles.x, pEntity->angles.y, pEntity->angles.z - m_flRoll);
		gEngfuncs.pfnAngleVectors(vecAngles, m_vecForward, m_vecRight, m_vecUp);
	}

	m_vecAttachment[0] = pEntity->attachment[0];
	m_vecAttachment[1] = pEntity->attachment[1];
	m_vecAttachment[2] = pEntity->attachment[2];

	// the player steering it sees through it
	if (m_iOwner != 0 && gEngfuncs.pEventAPI->EV_IsLocal(m_iOwner - 1))
	{
		pEntity->curstate.rendermode = kRenderTransTexture;
		pEntity->curstate.renderamt = 1;
	}

	if (m_bVisible)
	{
		// roll the model, and draw it 20 units back along its heading
		Vector vecAngles(pEntity->angles.x, pEntity->angles.y, pEntity->angles.z - m_flRoll);
		pEntity->angles = vecAngles;
		pEntity->curstate.angles = vecAngles;
		pEntity->prevstate.angles = Vector(pEntity->prevstate.angles.x, pEntity->prevstate.angles.y, pEntity->prevstate.angles.z - m_flRoll);

		pEntity->origin = pEntity->origin - m_vecForward * 20;
		pEntity->curstate.origin = pEntity->origin;
		pEntity->prevstate.origin = Vector(pEntity->prevstate.origin) - m_vecForward * 20;
		m_vecOrigin = m_vecOrigin - m_vecForward * 20;

		if (m_bVisible)
		{
			gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, 6, "weapons/nuker.wav", 1.0, 0.5, 0x40, 100); // 0x40: SND_CHANGE_VOL

			if (gEngfuncs.PM_PointContents(pEntity->origin, nullptr) == CONTENTS_WATER)
			{
				m_flBubbles = flFrametime * 8.0 + m_flBubbles;
				if (m_flBubbles >= 1.0)
				{
					const int iBubbles = (int)m_flBubbles;
					m_flBubbles = m_flBubbles - iBubbles;
					Vector vecMins, vecMaxs;
					const Vector vecOne(1, 1, 1);
					VectorMA(pEntity->origin, -16, vecOne, vecMins);
					VectorMA(pEntity->origin, 16, vecOne, vecMaxs);
					Vector vecPos = pEntity->origin;
					// VERIFY: the height is read as the water level less the box's bottom (the stack slot is mins.z by its offset)
					const float flHeight = RC2_WaterLevel(vecPos, pEntity->origin.z, pEntity->origin.z + 1024) - vecMins.z;
					gEngfuncs.pEfxAPI->R_Bubbles(vecMins, vecMaxs, flHeight, m_iBubbleModel, iBubbles, 8.0);
				}
			}
			else
			{
				m_flBubbles = 0;
			}
		}
	}

	// the glows brighten as the missile flies away from the viewer
	float flFacing = (g_vecTentForward.y * m_vecForward.y + g_vecTentForward.z * m_vecForward.z + g_vecTentForward.x * m_vecForward.x + 1.0) * 0.5;
	if (flFacing < 0)
		flFacing = 0;
	else if (flFacing > 1)
		flFacing = 1;

	if (m_pFlare)
	{
		m_pFlare->entity.origin = m_vecOrigin;
		m_pFlare->die = gEngfuncs.GetClientTime() + 10.0;
		m_pFlare->entity.curstate.renderamt = (int)(flFacing * 60 + 150);
	}
	for (int i = 0; i < 3; i++)
	{
		if (m_pGlows[i])
		{
			m_pGlows[i]->entity.curstate.renderamt = (int)(flFacing * 30 + 190);
			m_pGlows[i]->entity.origin = m_vecAttachment[i];
			m_pGlows[i]->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}
	if (m_pLight)
	{
		m_pLight->origin = m_vecOrigin;
		m_pLight->die = gEngfuncs.GetClientTime() + 10.0;
	}
	if (m_pLight2)
	{
		m_pLight2->origin = m_vecOrigin;
		m_pLight2->die = gEngfuncs.GetClientTime() + 10.0;
	}
	return false;
}

// Two crossed flame quads from each exhaust, flickering, for everyone but the steering player
// and only at cl_detail 0 and up.
void CNukeTent::DrawTransparent() // orig client 0x76620
{
	if (!m_bVisible)
		return;
	if (m_iOwner != 0 && gEngfuncs.pEventAPI->EV_IsLocal(m_iOwner - 1))
		return;
	if (g_iClientDetail < 0)
		return;

	const float flWidth = gEngfuncs.pfnRandomFloat(-1, 1) + 9;
	const float flLength = gEngfuncs.pfnRandomFloat(-6, 6) + 63;
	const float flHeight = gEngfuncs.pfnRandomFloat(-2, 2) + 9;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pFlame)
		gEngfuncs.pTriAPI->SpriteTexture(m_pFlame, 0);
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	for (int i = 0; i < 3; i++)
	{
		const Vector vecBase = m_vecAttachment[i];
		gEngfuncs.pTriAPI->Color4f(1, 1, 1, gEngfuncs.pfnRandomFloat(0.9, 0.95));
		gEngfuncs.pTriAPI->Brightness(gEngfuncs.pfnRandomFloat(0.9, 0.95));

		FlameVertex(vecBase, flWidth, flHeight, 0, 1, 0);
		FlameVertex(vecBase, -flLength, flHeight, 0, 1, 1);
		FlameVertex(vecBase, -flLength, -flHeight, 0, 0, 1);
		FlameVertex(vecBase, flWidth, -flHeight, 0, 0, 0);

		FlameVertex(vecBase, flWidth, 0, flHeight, 1, 0);
		FlameVertex(vecBase, -flLength, 0, flHeight, 1, 1);
		FlameVertex(vecBase, -flLength, 0, -flHeight, 0, 1);
		FlameVertex(vecBase, flWidth, 0, -flHeight, 0, 0);
	}

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// One vertex at vecBase moved along the missile's axes, with its texture coordinates.
void CNukeTent::FlameVertex(Vector vecBase, float flForward, float flRight, float flUp, float s, float t) // orig client 0x769b0
{
	const Vector vecPoint = vecBase + m_vecForward * flForward + m_vecRight * flRight + m_vecUp * flUp;
	gEngfuncs.pTriAPI->TexCoord2f(s, t);
	gEngfuncs.pTriAPI->Vertex3f(vecPoint.x, vecPoint.y, vecPoint.z);
}

// Lets its beams, glows and lights die and stops the engine sound. Returns false even for its
// own entity (Update then ends it).
bool CNukeTent::KillEntity(int iEntIndex) // orig client 0x76b30
{
	if (iEntIndex != m_iEntIndex)
		return false;

	for (int i = 0; i < 3; i++)
	{
		if (m_pTrails[i])
		{
			m_pTrails[i]->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pTrails[i]->endEntity = 0;
			m_pTrails[i]->startEntity = 0;
			m_pTrails[i] = nullptr;
		}
		if (m_pGlows[i])
		{
			m_pGlows[i]->die = 0;
			m_pGlows[i]->flags &= ~FTENT_SPRANIMATELOOP;
			m_pGlows[i] = nullptr;
		}
	}
	if (m_pFlare)
	{
		m_pFlare->die = 0;
		m_pFlare = nullptr;
	}
	if (m_pLight2)
	{
		m_pLight2->die = gEngfuncs.GetClientTime() + 0.01;
		m_pLight2 = nullptr;
	}
	if (m_pLight)
	{
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pLight = nullptr;
	}
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, 6, "weapons/nuker.wav");
	m_iEntIndex = 0;
	return false;
}
