/***
*
*	Rocket Crowbar 2: the player transforms' common code (transform.cpp, the original's own
*	file name, from its __FILE__ string): starting and finishing a change into the chosen
*	monster and back, the dispatchers from the player's code to each transform, and the
*	CBasePlayer overrides that depend on the transform.
*	Each transform is in its own file: tassassin, thoundeye, tcontroller, tbat, theadcrab.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "p8_needs_transform.h"

// The original's __FILE__, which its warnings print
#define TRANSFORM_FILE "C:\\SIERRA\\Half-Life\\RC2dev\\dlls\\transform.cpp"

// orig 0xe1fa0
void CBasePlayer::Transform_SetAnimation(PLAYER_ANIM playerAnim)
{
	switch (m_iTransformType)
	{
	case 1:
		TAssassin_SetAnimation(playerAnim);
		break;
	case 2:
		THoundeye_SetAnimation(playerAnim);
		break;
	case 3:
		TController_SetAnimation(playerAnim);
		break;
	case 4:
		TBat_SetAnimation(playerAnim);
		break;
	case 5:
		THeadcrab_SetAnimation(playerAnim);
		break;
	}
}

// orig 0xe2000
void CBasePlayer::Transform_Think()
{
	switch (m_iTransformType)
	{
	case 1:
		TAssassin_Think();
		break;
	case 2:
		THoundeye_Think();
		break;
	case 3:
		TController_Think();
		break;
	case 4:
		TBat_Think();
		break;
	case 5:
		THeadcrab_Think();
		break;
	}
}

// orig 0xe2040
void CBasePlayer::Transform_FixAngle()
{
	switch (m_iTransformType)
	{
	case 2:
		THoundeye_FixAngle();
		break;
	case 5:
		THeadcrab_FixAngle();
		break;
	}
}

// Called from CBasePlayer::Killed when transformed; empty in the original.
// orig 0xe2070
void CBasePlayer::Transform_Killed()
{
}

// orig 0xe2080
void CBasePlayer::Transform_DeathSound()
{
	switch (m_iTransformType)
	{
	case 2:
		THoundeye_DeathSound();
		break;
	case 3:
		TController_DeathSound();
		break;
	case 4:
		TBat_DeathSound();
		break;
	case 5:
		THeadcrab_DeathSound();
		break;
	}
}

// orig 0xe20c0
void CBasePlayer::Transform_PainSound()
{
	if (pev->health <= 0)
		return;

	switch (m_iTransformType)
	{
	case 2:
		THoundeye_PainSound();
		break;
	case 3:
		TController_PainSound();
		break;
	case 4:
		TBat_PainSound();
		break;
	case 5:
		THeadcrab_PainSound();
		break;
	}
}

// Rocket Crowbar 2: only transformed players handle animation events (the houndeye its own).
// orig 0xe2150
void CBasePlayer::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (m_iTransformType)
	{
	case 1:
	case 3:
	case 4:
	case 5:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	case 2:
		THoundeye_HandleAnimEvent(pEvent);
		break;
	}
}

// Rocket Crowbar 2: the assassin and humans bleed red, the rest yellow.
// orig 0xe21a0
int CBasePlayer::BloodColor()
{
	switch (m_iTransformType)
	{
	case 1:
		return BLOOD_COLOR_RED;
	case 2:
	case 3:
	case 4:
	case 5:
		return BLOOD_COLOR_YELLOW;
	}
	return BLOOD_COLOR_RED;
}

// Start the change into m_iChosenTransform: needs a clear spot of the new hull at the origin
// and costs TransformEnergyCost; the player is held for that transform's TransformParalyzeTime.
// orig 0xe21f0
void CBasePlayer::StartTransform()
{
	if (m_bTransforming || 0 != m_iTransformType)
		return;

	int iType;
	switch (m_iChosenTransform)
	{
	case 0:
		iType = 1;
		break;
	case 1:
		iType = 2;
		break;
	case 2:
		iType = 3;
		break;
	case 3:
		iType = 4;
		break;
	case 4:
		iType = 5;
		break;
	default:
		ALERT(at_console, "BUGBUG: Unsupported transform/wierdness (%s:%i)\n", TRANSFORM_FILE, 137);
		return;
	}

	Vector vecAngles(0, pev->v_angle.y, 0);
	UTIL_MakeVectors(vecAngles);

	float flParalyze = 1.0;
	TraceResult tr;
	bool bTraced = true;
	Vector vecUp(pev->origin.x, pev->origin.y, pev->origin.z + 1);

	// Original bug: only the assassin and controller branches test the chosen type; the others
	// test m_iTransformType, which is 0 here, so the houndeye, bat and headcrab skip the room test
	// and are held 1 second instead of their TransformParalyzeTime.
	if (iType == 1)
	{
		UTIL_TraceHull(pev->origin, vecUp, dont_ignore_monsters, human_hull, ENT(pev), &tr);
		flParalyze = RC2Pref(RC2P_TransformAssassin_TransformParalyzeTime).f;
	}
	else if (m_iTransformType == 2)
	{
		UTIL_TraceHull(pev->origin, vecUp, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		flParalyze = RC2Pref(RC2P_TransformHoundeye_TransformParalyzeTime).f;
	}
	else if (iType == 3)
	{
		UTIL_TraceHull(pev->origin, vecUp, dont_ignore_monsters, human_hull, ENT(pev), &tr);
		flParalyze = RC2Pref(RC2P_TransformController_TransformParalyzeTime).f;
	}
	else if (m_iTransformType == 4)
	{
		UTIL_TraceHull(pev->origin, vecUp, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		flParalyze = RC2Pref(RC2P_TransformBat_TransformParalyzeTime).f;
	}
	else if (m_iTransformType == 5)
	{
		UTIL_TraceHull(pev->origin, vecUp, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		flParalyze = RC2Pref(RC2P_TransformHeadcrab_TransformParalyzeTime).f;
	}
	else
	{
		bTraced = false;
	}

	if (bTraced && (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid))
	{
		CLIENT_PRINTF(edict(), print_console, "Not enough room to transform\n");
		return;
	}

	pev->v_angle = vecAngles;
	pev->angles = pev->v_angle;
	pev->fixangle = 1;
	m_vecTransformAngles = vecAngles;
	AddPhysFlags(0x20);
	m_iHideHUD |= 0x10;
	m_fShieldOn = false;

	if (m_bRC2State650)
	{
		m_bRC2State650 = false;
		RemovePhysFlags(1);
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(4);
		WRITE_BYTE(ENTINDEX(edict()));
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	if (m_pActiveItem)
	{
		m_pActiveItem->Holster();
		pev->weaponmodel = 0;
		pev->viewmodel = 0;
	}

	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}

	if (FlashlightIsOn())
		FlashlightTurnOff();
	pev->effects &= ~EF_DIMLIGHT;

	SetTouch(NULL);
	m_bTransforming = true;
	m_flTransformDoneTime = flParalyze + gpGlobals->time;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
	pev->rendercolor = g_vecZero;
	pev->skin = 0;
	m_iTransformIdleAnim = 0;
	m_flTransformIdleTime = gpGlobals->time - 120;

	m_flEnergy -= RC2Pref(RC2P_Attributes_TransformEnergyCost).f;
	UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_Attributes_TransformEnergyRegenDelay).f);
	if (m_flEnergy < 0)
		m_flEnergy = 0;

	m_pRC2Player79c->m_iRC2Flags = 8;
	m_pRC2Player79c->pev->effects &= ~EF_NODRAW;

	// the change effect: player, its entity, from 0 to the new type
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x40);
	WRITE_BYTE(ENTINDEX(edict()));
	WRITE_SHORT(ENTINDEX(m_pRC2Player79c->edict()));
	WRITE_BYTE(0);
	WRITE_BYTE(iType);
	MESSAGE_END();
}

static void Transform_RestartAnimation(CBasePlayer* pPlayer) // ours: the original has this twice, inline
{
	pPlayer->pev->sequence = -1;
	pPlayer->SetAnimation(PLAYER_WALK);
	if (pPlayer->pev->sequence == -1)
	{
		pPlayer->pev->sequence = 0;
		pPlayer->ResetSequenceInfo();
	}
}

// Become the chosen monster: model, view height, health scaled to its MaxHealth, every
// transform state cleared.
// orig 0xe2830
void CBasePlayer::FinishTransform()
{
	int iType;
	switch (m_iChosenTransform)
	{
	case 0:
		iType = 1;
		break;
	case 1:
		iType = 2;
		break;
	case 2:
		iType = 3;
		break;
	case 3:
		iType = 4;
		break;
	case 4:
		iType = 5;
		break;
	default:
		ALERT(at_console, "BUGBUG: Unsupported transform/wierdness (%s:%i)\n", TRANSFORM_FILE, 258);
		FinishUntransform();
		return;
	}

	m_flPreTransformHealth = pev->health;
	m_flPreTransformMaxHealth = pev->max_health;
	float flOldMax = pev->max_health;
	const char* pszModel = NULL;
	float flMax;

	switch (iType)
	{
	case 1:
		pszModel = "models/hassassin.mdl";
		flMax = RC2Pref(RC2P_TransformAssassin_MaxHealth).f;
		pev->view_ofs = Vector(0, 0, 32);
		break;
	case 2:
		pszModel = "models/houndeye.mdl";
		flMax = RC2Pref(RC2P_TransformHoundeye_MaxHealth).f;
		pev->view_ofs = Vector(0, 0, 8);
		break;
	case 3:
		pszModel = "models/controller.mdl";
		flMax = RC2Pref(RC2P_TransformController_MaxHealth).f;
		pev->view_ofs = Vector(0, 0, 16);
		pev->flags &= ~FL_ONGROUND;
		break;
	case 4:
		pszModel = "models/stukabat.mdl";
		flMax = RC2Pref(RC2P_TransformBat_MaxHealth).f;
		pev->view_ofs = Vector(0, 0, 8);
		break;
	case 5:
		pszModel = "models/headcrab.mdl";
		flMax = RC2Pref(RC2P_TransformHeadcrab_MaxHealth).f;
		pev->view_ofs = Vector(0, 0, 4);
		break;
	}
	// the health keeps its fraction; view_ofs is set after max_health in the original, which
	// changes nothing
	pev->health = (m_flPreTransformHealth / flOldMax) * flMax;
	pev->max_health = flMax;
	SET_MODEL(ENT(pev), pszModel);

	if (pev->health < 1)
		pev->health = 1;

	pev->v_angle = m_vecTransformAngles;
	pev->angles = pev->v_angle;
	pev->fixangle = 1;
	RemovePhysFlags(0x20);
	m_bTransforming = false;
	m_iTransformType = iType;
	pev->playerclass = iType;
	pev->skin = 0;

	float flSoon = gpGlobals->time + 0.25;
	m_flTransformLastAttack = -1;
	m_flTransformNextAttack4 = flSoon;
	m_flTransformNextAttack3 = flSoon;
	m_flTransformNextAttack2 = flSoon;
	m_flTransformNextAttack1 = flSoon;
	m_flTransformVar1 = 0;
	m_flTransformVar2 = 0;
	m_flTransformVar3 = 0;
	m_flTransformVar4 = 0;
	m_iTransformState1 = 0;
	m_iTransformState2 = 0;
	m_iTransformState3 = 0;
	m_iTransformState4 = 0;
	m_flTransformTimer1 = -1;
	m_flTransformTimer2 = -1;
	m_flTransformTimer3 = -1;
	m_flTransformTimer4 = -1;
	m_vecTransformVec = g_vecZero;
	m_pentRaceController = NULL;

	Transform_RestartAnimation(this);

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x40);
	MESSAGE_END();

	m_pRC2Player79c->m_iRC2Flags = 0;
	m_pRC2Player79c->pev->effects |= EF_NODRAW;
}

// Start changing back: needs room for the human hull (the houndeye and headcrab rise 20 units
// first), costs TransformEnergyCost.
// orig 0xe2cc0
void CBasePlayer::StartUntransform()
{
	if (m_bTransforming || 0 == m_iTransformType)
		return;

	Vector vecSrc = pev->origin;
	if (m_iTransformType == 2 || m_iTransformType == 5)
		vecSrc.z += 20;
	Vector vecEnd(vecSrc.x, vecSrc.y, vecSrc.z + 1);

	TraceResult tr;
	UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);
	if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
	{
		CLIENT_PRINTF(edict(), print_console, "Not enough room to transform\n");
		return;
	}

	if (vecSrc != pev->origin)
		UTIL_SetOrigin(pev, vecSrc);

	float flParalyze = 1.0;
	switch (m_iTransformType)
	{
	case 1:
	case 2: // original bug: the houndeye uses the assassin's TransformParalyzeTime here
		flParalyze = RC2Pref(RC2P_TransformAssassin_TransformParalyzeTime).f;
		break;
	case 3:
		flParalyze = RC2Pref(RC2P_TransformController_TransformParalyzeTime).f;
		break;
	case 4:
		flParalyze = RC2Pref(RC2P_TransformBat_TransformParalyzeTime).f;
		break;
	case 5:
		flParalyze = RC2Pref(RC2P_TransformHeadcrab_TransformParalyzeTime).f;
		break;
	}

	SetTouch(NULL);
	AddPhysFlags(0x20);
	m_bTransforming = true;
	m_flTransformDoneTime = flParalyze + gpGlobals->time;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
	pev->rendercolor = g_vecZero;
	pev->skin = 0;

	m_flEnergy -= RC2Pref(RC2P_Attributes_TransformEnergyCost).f;
	UTIL_SetMaxTime(m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_Attributes_TransformEnergyRegenDelay).f);
	if (m_flEnergy < 0)
		m_flEnergy = 0;

	m_pRC2Player79c->m_iRC2Flags = 8;
	m_pRC2Player79c->pev->effects &= ~EF_NODRAW;

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x40);
	WRITE_BYTE(ENTINDEX(edict()));
	WRITE_SHORT(ENTINDEX(m_pRC2Player79c->edict()));
	WRITE_BYTE(m_iTransformType);
	WRITE_BYTE(0);
	MESSAGE_END();
}

// orig 0xe2f90
void CBasePlayer::FinishUntransform()
{
	RemovePhysFlags(0x20);
	m_iHideHUD &= ~0x10;
	SET_MODEL(ENT(pev), "models/player.mdl");
	m_bTransforming = false;
	m_iTransformType = 0;
	pev->playerclass = 0;

	if (m_pActiveItem)
		m_pActiveItem->Deploy();

	pev->health = (pev->health / pev->max_health) * m_flPreTransformMaxHealth;
	if (pev->health < 1)
		pev->health = 1;
	pev->max_health = m_flPreTransformMaxHealth;
	pev->view_ofs = VEC_VIEW;

	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
	pev->rendercolor = g_vecZero;
	pev->skin = 0;
	SetTouch(NULL);

	Transform_RestartAnimation(this);

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x40);
	MESSAGE_END();

	m_pRC2Player79c->m_iRC2Flags = 0;
	m_pRC2Player79c->pev->effects |= EF_NODRAW;
}

// Rocket Crowbar 2: the player's RC caps by transform (a human has all 14 bits).
// orig 0xe3150
int CBasePlayer::GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker)
{
	switch (m_iTransformType)
	{
	case 0:
		return 0x3fff;
	case 6:
		return 0;
	case 2:
		return ~0x1000 & 0x3fff;
	case 4:
		return ~((m_iTransformState1 == 2) ? 0x1cbb : 0x12) & 0x3fff;
	}
	return 0x3fff;
}

// orig 0xe31b0
int CBasePlayer::GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker)
{
	switch (m_iTransformType)
	{
	case 6:
		return 6;
	case 1:
		if (0 != m_iTransformState2) // cloaked
			return 8;
		break;
	case 4:
		return (m_iTransformState1 == 2) ? 0x22 : 0x20;
	}
	return 0;
}

// The direction a transformed player dies in, as CBaseMonster::GetDeathActivity reasons:
// 1 shot in the head, 2 in the stomach, 3 forward (room ahead), 4 backward (room behind), 0
// otherwise.
// orig 0xe3200
int Transform_DeathDirection(CBasePlayer* pPlayer)
{
	Vector vecSrc = pPlayer->Center();
	UTIL_MakeVectors(Vector(0, pPlayer->pev->v_angle.y, 0));
	float flDot = DotProduct(g_vecAttackDir * -1, gpGlobals->v_forward);

	switch (pPlayer->m_LastHitGroup)
	{
	case HITGROUP_HEAD:
		return 1;
	case HITGROUP_STOMACH:
		return 2;
	}

	int iDir;
	Vector vecEnd;
	if (flDot > 0.3)
	{
		iDir = 3;
		vecEnd = vecSrc + gpGlobals->v_forward * 64;
	}
	else if (flDot < -0.3)
	{
		iDir = 4;
		vecEnd = vecSrc - gpGlobals->v_forward * 64;
	}
	else
	{
		return 0;
	}

	TraceResult tr;
	UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, pPlayer->edict(), &tr);
	if (tr.flFraction == 1.0)
		return iDir;
	return 0;
}

// orig 0xe3400
int Transform_OfPlayer(int iPlayer)
{
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return 0;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer)
		return 0;
	return pPlayer->m_iTransformType;
}

// From the world's precache.
// orig 0xe3450
void Transform_Precache()
{
	PRECACHE_SOUND("zombie/claw_strike1.wav");
	PRECACHE_SOUND("zombie/claw_strike2.wav");
	PRECACHE_SOUND("zombie/claw_strike3.wav");
	PRECACHE_SOUND("zombie/claw_miss1.wav");
	PRECACHE_SOUND("zombie/claw_miss2.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
	PRECACHE_SOUND("ambience/labgear.wav");

	// assassin
	PRECACHE_MODEL("models/hassassin.mdl");
	PRECACHE_MODEL("models/shell.mdl");
	PRECACHE_SOUND("weapons/pl_gun1.wav");
	PRECACHE_SOUND("weapons/pl_gun2.wav");
	PRECACHE_SOUND("debris/beamstart1.wav");

	// houndeye
	PRECACHE_MODEL("models/houndeye.mdl");
	PRECACHE_MODEL("sprites/heflare1.spr");
	PRECACHE_MODEL("sprites/heflare2.spr");
	PRECACHE_MODEL("sprites/heflare3.spr");
	PRECACHE_MODEL("sprites/heflare4.spr");
	PRECACHE_MODEL("sprites/glow01.spr");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	PRECACHE_SOUND("weapons/egon_run3.wav");
	PRECACHE_SOUND("houndeye/he_attack1.wav");
	PRECACHE_SOUND("houndeye/he_attack3.wav");
	PRECACHE_SOUND("houndeye/he_die1.wav");
	PRECACHE_SOUND("houndeye/he_die2.wav");
	PRECACHE_SOUND("houndeye/he_die3.wav");
	PRECACHE_SOUND("houndeye/he_pain1.wav");
	PRECACHE_SOUND("houndeye/he_pain3.wav");
	PRECACHE_SOUND("houndeye/he_pain4.wav");
	PRECACHE_SOUND("houndeye/he_pain5.wav");
	PRECACHE_SOUND("houndeye/he_hunt1.wav");
	PRECACHE_SOUND("houndeye/he_hunt2.wav");
	PRECACHE_SOUND("houndeye/he_hunt3.wav");
	PRECACHE_SOUND("houndeye/he_blast1.wav");
	PRECACHE_SOUND("houndeye/he_blast2.wav");
	PRECACHE_SOUND("houndeye/he_blast3.wav");

	// controller
	PRECACHE_MODEL("models/controller.mdl");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_MODEL("sprites/nhth1.spr");
	PRECACHE_MODEL("sprites/zbeam2.spr");
	PRECACHE_SOUND("controller/con_die1.wav");
	PRECACHE_SOUND("controller/con_die2.wav");
	PRECACHE_SOUND("controller/con_pain1.wav");
	PRECACHE_SOUND("controller/con_pain2.wav");
	PRECACHE_SOUND("controller/con_pain3.wav");
	PRECACHE_SOUND("controller/con_attack1.wav");
	PRECACHE_SOUND("controller/con_attack2.wav");
	PRECACHE_SOUND("controller/con_attack3.wav");
	UTIL_PrecacheOther("tcontroller_tball");
	UTIL_PrecacheOther("tcontroller_cball");

	// bat
	PRECACHE_MODEL("models/stukabat.mdl");
	PRECACHE_SOUND("headcrab/hc_die2.wav");
	PRECACHE_SOUND("zombie/zo_pain1.wav");
	PRECACHE_SOUND("zombie/zo_pain2.wav");

	// headcrab
	PRECACHE_MODEL("models/headcrab.mdl");
	PRECACHE_SOUND("headcrab/hc_die1.wav");
	PRECACHE_SOUND("headcrab/hc_die2.wav");
	PRECACHE_SOUND("headcrab/hc_pain1.wav");
	PRECACHE_SOUND("headcrab/hc_pain2.wav");
	PRECACHE_SOUND("headcrab/hc_pain3.wav");
	PRECACHE_SOUND("headcrab/hc_attack1.wav");
	PRECACHE_SOUND("headcrab/hc_attack2.wav");

	PRECACHE_EVENT(1, "events/tassassingun.sc");
	PRECACHE_EVENT(1, "events/tassassinkick.sc");
	PRECACHE_EVENT(1, "events/thoundeyebeam.sc");
	PRECACHE_EVENT(1, "events/thoundeyesonic.sc");
	PRECACHE_EVENT(1, "events/tcontrollerthrow.sc");
	PRECACHE_EVENT(1, "events/tbatbite.sc");
}

// CBasePlayer::Classify's answer for a transformed player.
// orig 0xe3750
int Transform_Classify(int iTransform)
{
	switch (iTransform)
	{
	case 1:
		return CLASS_HUMAN_MILITARY;
	case 2:
	case 3:
	case 4:
		return CLASS_ALIEN_MONSTER;
	case 5:
		return CLASS_ALIEN_PREY;
	}
	return CLASS_PLAYER;
}

// Rocket Crowbar 2: a houndeye takes none of the houndeye beam's damage; anyone else the
// rc2.ini values.
// orig 0xe3790
float CBasePlayer::BeamCoreDamage()
{
	if (m_iTransformType != 2)
		return RC2Pref(RC2P_TransformHoundeye_BeamCoreDamage).f;
	return 0;
}

// orig 0xe37b0
float CBasePlayer::BeamRadialMaxDamage()
{
	if (m_iTransformType != 2)
		return RC2Pref(RC2P_TransformHoundeye_BeamRadialMaxDamage).f;
	return 0;
}

// orig 0xe37d0
float CBasePlayer::BeamViewDamage()
{
	if (m_iTransformType != 2)
		return RC2Pref(RC2P_TransformHoundeye_BeamViewDamage).f;
	return 0;
}
