/***
*
*	Rocket Crowbar 2: the gauss beacon (gaussbecon.cpp). Shared with gauss.cpp, which finds,
*	teleports to and retrieves it.
*
****/

#pragma once

class CGaussBecon : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override;

	void EXPORT FlyingThink();
	void EXPORT AttachTouch(CBaseEntity* pOther);
	void EXPORT AttachedThink();

	// Not saved (RC2 has no save table for it), so the names are ours except RC 1.9's.
	bool m_blAttached;			 // orig +0x70, RC 1.9's: stuck to a surface
	bool m_blAttachedToWorld;	 // orig +0x71, ours: stuck to the world, not to a brush entity
	Vector m_vecAttachAngles;	 // orig +0x74, ours: on the world its angles, on an entity their offset from the entity's
	Vector m_vecAttachOrigin;	 // orig +0x80, ours: on the world its origin, on an entity its offset in the entity's forward/right/up
	Vector m_vecPlaneNormal;	 // orig +0x8c, RC 1.9's: the surface's normal (on an entity, its forward each think)
};
