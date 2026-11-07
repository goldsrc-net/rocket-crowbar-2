/***
*
*	Rocket Crowbar 2 client: afterimages of a fast-moving entity (CLTE 0x44).
*
*	No server file in this tree sends type 0x44. Four copies of the entity's model, as
*	temp entities, trail it while it moves at 1500 units a second or more: each frame each copy
*	takes the place, angles and frame of the one ahead (the first the entity's of last frame),
*	fainter down the line. Not drawn below cl_detail 0, nor while the entity isn't in the
*	packet, nor for the local player's own. Killed with CLTE 0xfe/0x44 (or 0xff).
*
*	The original's functions 0x92ac0-0x931d0 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

#define AFTERIMAGES 4 // name ours

//
// CLTE 0x44: short the entity.
//
class CAfterimageTent : public CClientTent // name ours
{
public:
	CAfterimageTent();

	bool Update(float flFrametime) override;
	bool KillEntity(int iEntIndex) override;

	void HideAll();

	int m_iEntIndex;					// orig +0x8
	TEMPENTITY* m_pImage[AFTERIMAGES];	// orig +0xc
	Vector m_vecLastOrigin;				// orig +0x1c: the entity's, last frame
};

// The factory (the dispatcher's case 0x44; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor (inline here in the original). The dispatcher always
// passes null.
CClientTent* RC2_NewAfterimageTent(const CClientTent* pCopy) // orig client 0x92ac0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CAfterimageTent* pTent = pCopy ? new CAfterimageTent(*static_cast<const CAfterimageTent*>(pCopy)) : new CAfterimageTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x92ac0): reads the message and makes the copies.
CAfterimageTent::CAfterimageTent()
{
	m_iEntIndex = READ_SHORT();
	for (int i = 0; i < AFTERIMAGES; i++)
		m_pImage[i] = nullptr;
	m_vecLastOrigin = Vector(0, 0, 0);

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		// (the original keeps m_iEntIndex; Update then finds no entity and ends it)
		for (int i = 0; i < AFTERIMAGES; i++)
		{
			if (m_pImage[i])
			{
				m_pImage[i]->die = 0;
				m_pImage[i] = nullptr;
			}
		}
		return;
	}

	m_vecLastOrigin = pEntity->origin;

	// the entity's model by name (a model_s starts with its name)
	int iModelIndex;
	model_s* pModel = gEngfuncs.CL_LoadModel(pEntity->model->name, &iModelIndex);
	if (!pModel)
		return;

	for (int i = 0; i < AFTERIMAGES; i++)
	{
		m_pImage[i] = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEntity->origin, pModel);
		if (!m_pImage[i])
			continue;
		m_pImage[i]->flags |= FTENT_PERSIST;
		m_pImage[i]->entity.curstate.renderamt = 1;
		m_pImage[i]->entity.curstate.rendermode = kRenderTransTexture;
		m_pImage[i]->entity.curstate.renderfx = kRenderFxNoDissipation;
		m_pImage[i]->entity.curstate.rendercolor.r = 255;
		m_pImage[i]->entity.curstate.rendercolor.g = 255;
		m_pImage[i]->entity.curstate.rendercolor.b = 255;
		m_pImage[i]->entity.curstate.owner = m_iEntIndex; // VERIFY: +0x38c of the tempent is curstate.owner by SDK 2.3's layout
		m_pImage[i]->die += 10.0;
	}
}

// Inline four times in Update (orig 0x92e70): every copy invisible. name ours
void CAfterimageTent::HideAll()
{
	for (int i = 0; i < AFTERIMAGES; i++)
	{
		if (m_pImage[i])
			m_pImage[i]->entity.curstate.renderamt = 0;
	}
}

bool CAfterimageTent::Update(float flFrametime) // orig client 0x92e70
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		return true;
	}

	if (g_iClientDetail < 0)
	{
		HideAll();
		return false;
	}

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && (pLocal->curstate.messagenum > pEntity->curstate.messagenum || pLocal->index == pEntity->index))
	{
		HideAll();
		return false;
	}

	const Vector vecMove = pEntity->origin - m_vecLastOrigin;
	const Vector vecLast = m_vecLastOrigin;
	const float flTime = flFrametime != 0 ? flFrametime : 1.0;
	m_vecLastOrigin = pEntity->origin;
	if (sqrt(vecMove.z * vecMove.z + vecMove.y * vecMove.y + vecMove.x * vecMove.x) / flTime < 1500)
	{
		HideAll();
		return false;
	}

	// Original bugs, kept: the copies are dereferenced without a null check, and walking them
	// front to back makes each take the one ahead's place as just updated, so the three behind the
	// first all sit where the first was a frame ago instead of strung out behind it.
	for (int i = 1; i < AFTERIMAGES; i++)
	{
		m_pImage[i]->entity.origin = m_pImage[i - 1]->entity.origin;
		m_pImage[i]->entity.angles = m_pImage[i - 1]->entity.angles;
		m_pImage[i]->entity.curstate.frame = m_pImage[i - 1]->entity.curstate.frame;
		m_pImage[i]->entity.curstate.framerate = 0;
		if (m_pImage[i - 1]->entity.curstate.renderamt > 0)
			m_pImage[i]->entity.curstate.renderamt = (int)((0.8 - pow((double)i, 0.6) * 0.3) * 255);
		else
			m_pImage[i]->entity.curstate.renderamt = 0;
	}

	m_pImage[0]->entity.curstate.renderamt = 204;
	m_pImage[0]->entity.origin = vecLast;
	m_pImage[0]->entity.angles = pEntity->curstate.angles;
	m_pImage[0]->entity.curstate.frame = pEntity->curstate.frame;
	m_pImage[0]->entity.curstate.framerate = 0;
	return false;
}

// The copies die at once. (The original leaves m_iEntIndex set.)
bool CAfterimageTent::KillEntity(int iEntIndex) // orig client 0x931d0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	for (int i = 0; i < AFTERIMAGES; i++)
	{
		if (m_pImage[i])
		{
			m_pImage[i]->die = 0;
			m_pImage[i] = nullptr;
		}
	}
	return true;
}
