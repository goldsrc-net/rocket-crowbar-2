/***
*
*	Rocket Crowbar 2: the hornet gun's freeze grenade, and the freeze damage it and the hornet
*	gun deal (freezegrenade.cpp).
*
****/

#pragma once

class CFreezeGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	// the capability bits the RC2 effects ask of it (slot 75)
	// orig 0x63900
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x400; }

	void EXPORT FlyingThink();
	void EXPORT AttachTouch(CBaseEntity* pOther);
	void EXPORT AttachedThink();
	void EXPORT Detonate();

	// Not saved, so the names are ours.
	Vector m_vecAttachAngles;	   // orig +0x2b8: its angles when stuck, relative to what it stuck on
	Vector m_vecAttachOffset;	   // orig +0x2c4: where it is stuck, in the frame of what it stuck on (on the world: the point itself)
	bool m_fOnWorld;			   // orig +0x2d0: stuck on the world (entity 0)
	unsigned short m_usFreezeDet; // orig +0x2d2: events/hgungrenadedet.sc
};

// Freezes pEntity for flTime more seconds through its freeze_timer (made if it has none).
// Name ours.
void FreezeEntity(CBaseEntity* pEntity, float flTime);

// Damage that freezes: TakeDamage(DMG_FREEZE | DMG_NEVERGIB) after a player's shield takes its
// share when bShield, then freezes what is left with health at or under flFreezeDamage for
// flFreezeTime (both scaled by the share that got through). ptr may be NULL. Name ours.
void FreezeDamage(CBaseEntity* pEntity, entvars_t* pevAttacker, entvars_t* pevInflictor, float flDamage, float flFreezeDamage, float flFreezeTime, TraceResult* ptr, Vector vecDir, bool bShield);
