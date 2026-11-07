/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// rat - environmental monster
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CRat : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
};
LINK_ENTITY_TO_CLASS(monster_rat, CRat); // orig 0xae8d0

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CRat::Classify() // orig 0xae940: as SDK 2.3 (RC2's layout moved its bytes)
{
	return CLASS_INSECT;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CRat::SetYawSpeed() // orig 0xae950: as SDK 2.3
{
	int ys;

	switch (m_Activity)
	{
	case ACT_IDLE:
	default:
		ys = 45;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// Spawn
//=========================================================
void CRat::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/bigrat.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = 8;
	pev->view_ofs = Vector(0, 0, 6); // position of the eyes relative to monster's origin.
	m_flFieldOfView = 0.5;			 // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CRat::Precache() // orig 0xaea50: as SDK 2.3
{
	PRECACHE_MODEL("models/bigrat.mdl");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
