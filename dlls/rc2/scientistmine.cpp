/***
*
*	Rocket Crowbar 2: scientistmine.cpp (file name ours), the RPG's scientist mine
*	(scientist_mine). Original functions 0xb34d0-0xb4510 of rc2.dll.
*
*	A scientist (or, half the time, a barney) that the RPG lobs. Once it lands and arms it
*	stands around, looks at whoever is near and chats with other scientist mines; when a live
*	target it may attack comes into [RPG] SciMineTriggerRadius in sight, it says hello, spins up
*	into the air and explodes [RPG] SciMineDetonateDelay later.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "p6stubs.h"

extern int gmsgCLTE; // RC2's "CLTE" user message (orig .bss 0x18f4b8); TEMP STUB in dlls/rc2/playerride.cpp

#define CLTE_SCIMINE_IDLE 7		  // ours: the client's idle effect on the mine
#define CLTE_SCIMINE_TRIGGERED 8 // ours: the client's effect once it spins up

class CScientistMine : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_PLAYER_ALLY; } // orig 0xb3570
	int BloodColor() override { return BLOOD_COLOR_RED; } // orig 0xb3560
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x17fd; } // orig 0xb3550

	void EXPORT SciMineActivate(CBaseEntity* pOther);
	void EXPORT SciMineThink();
	void EXPORT PreExplodeThink();
	void EXPORT ExplodeThink();

	void SetHeadTarget(Vector vecTarget); // ours
	void Talk();						  // ours
	void LookAround();					  // ours

	bool m_bBarney;			// orig +0x2b8, name ours: the barney model and sentences
	bool m_bActive;			// orig +0x2b9, name ours: armed and looking for targets
	float m_flArmTime;		// orig +0x2bc, name ours: arms by itself 5 s after Spawn
	int m_iVoicePitch;		// orig +0x2c0, name ours
	float m_flNextTalk;		// orig +0x2c4, name ours
	float m_flTalkUntil;	// orig +0x2c8, name ours: busy talking until then
	bool m_bAnswer;			// orig +0x2cc, name ours: was asked, answers on its next Talk
	float m_flNextLook;		// orig +0x2d0, name ours
	EHANDLE m_hLookTarget;	// orig +0x2d4, name ours
	float m_flNextIdle;		// orig +0x2dc, name ours: next CLTE idle effect
	float m_flDetonateTime; // orig +0x2e0, name ours
	float m_flHeadYawGoal;	// orig +0x2e4, name ours
	float m_flHeadYaw;		// orig +0x2e8, name ours: bone controller 0
};

// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("scientist_mine") (0xf5a20); the shared macro doesn't yet
LINK_ENTITY_TO_CLASS(scientist_mine, CScientistMine); // orig 0xb34d0

void CScientistMine::Precache() // orig 0xb3580
{
	PRECACHE_MODEL("models/scientist.mdl");
	PRECACHE_MODEL("models/barney.mdl");
	PRECACHE_MODEL("sprites/lgtning.spr");
}

void CScientistMine::Spawn() // orig 0xb35b0
{
	Precache();
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_AIM;
	pev->deadflag = DEAD_NO;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = RC2Pref(RC2P_RPG_SciMineHealth).f;
	pev->view_ofs = Vector(0, 0, 50);
	pev->gravity = RC2Pref(RC2P_RPG_SciMineGravity).f;

	m_bBarney = RANDOM_LONG(0, 100) < 50;
	if (!m_bBarney)
	{
		SET_MODEL(ENT(pev), "models/scientist.mdl");
		pev->skin = 0;
		pev->body = RANDOM_LONG(0, 3);
		if (pev->body == 2)
			pev->skin = 1;
		pev->sequence = LookupSequence("idle1");
		ResetSequenceInfo();
		m_fSequenceLoops = true;
		if (RANDOM_FLOAT(0, 1) < 0.5)
			m_iVoicePitch = RANDOM_LONG(100, 115);
		else
			m_iVoicePitch = 100;
	}
	else
	{
		SET_MODEL(ENT(pev), "models/barney.mdl");
		pev->body = 2;
		pev->skin = 0;
		pev->sequence = LookupSequence("relaxstand");
		ResetSequenceInfo();
		m_fSequenceLoops = true;
		m_iVoicePitch = RANDOM_LONG(95, 105);
	}

	m_bAnswer = false;
	m_flTalkUntil = gpGlobals->time;
	m_flNextTalk = RANDOM_FLOAT(9, 20) + gpGlobals->time;
	m_flNextLook = gpGlobals->time;
	m_hLookTarget = NULL;
	InitBoneControllers();
	m_flHeadYawGoal = 0;
	m_flHeadYaw = 0;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	UTIL_SetOrigin(pev, pev->origin);
	pev->dmg = 250;
	SetTouch(&CScientistMine::SciMineActivate);
	SetThink(&CScientistMine::SciMineThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_bActive = false;
	m_flArmTime = gpGlobals->time + 5.0;
	m_flNextIdle = RANDOM_FLOAT(3, 4) + gpGlobals->time;
	pev->dmg_inflictor = pev->owner; // the thrower, kept for the kill credit
}

// Landing (touching the world, or any BSP model) arms it.
void CScientistMine::SciMineActivate(CBaseEntity* pOther) // orig 0xb38a0
{
	if (pOther->edict() == pev->owner || !pOther->IsBSPModel())
		return;

	pev->owner = NULL;
	m_bActive = true;
	SetTouch(NULL);
	m_flNextIdle = RANDOM_FLOAT(1, 2) + gpGlobals->time;
}

void CScientistMine::SciMineThink() // orig 0xb3910
{
	float flInterval = StudioFrameAdvance(0);
	pev->nextthink = gpGlobals->time + 0.1;
	ApplyExternalForce(flInterval);

	if (!m_bActive)
	{
		if (!(m_flArmTime < gpGlobals->time))
			return;
		m_bActive = true;
	}

	// CBaseEntity byte +0x66, m_bFrozen: while set the mine doesn't look for targets or idle
	if (m_bFrozen)
		return;

	Vector vecCenter = Center();
	CBaseEntity* pOwner = NULL;
	if (pev->dmg_inflictor && UTIL_IsValidEntity(pev->dmg_inflictor))
		pOwner = CBaseEntity::Instance(pev->dmg_inflictor);

	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecCenter, RC2Pref(RC2P_RPG_SciMineTriggerRadius).f)) != NULL)
	{
		if (!pEnt->pev || !UTIL_IsValidEntity(pEnt->edict()))
			continue;
		if ((pEnt->GetEffectedEnt()->GetRCCaps(this, pOwner) & 2) == 0)
			continue;
		if ((pEnt->GetRCCaps2(pOwner, this) & 8) != 0)
			continue;
		if (!pEnt->GetEffectedEnt()->IsAlive() || pEnt->GetEffectedEnt()->pev->deadflag != DEAD_NO)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecCenter, pEnt->Center(), ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		// a target: say hello, spin up and go
		m_bActive = false;
		InitBoneControllers();
		SetThink(&CScientistMine::PreExplodeThink);
		SetTouch(NULL);
		pev->nextthink = gpGlobals->time + 0.1;
		m_flDetonateTime = RC2Pref(RC2P_RPG_SciMineDetonateDelay).f + gpGlobals->time;
		SENTENCEG_PlayRndSz(edict(), m_bBarney ? "BA_HELLO" : "SC_HELLO", 1.0, 0.9, 0, ScaledPitchShift() + m_iVoicePitch);

		pev->velocity = g_vecZero;
		pev->movetype = MOVETYPE_FLY;
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin(pev, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 1.0));
		pev->flags &= ~FL_ONGROUND;
		pev->avelocity.y = RANDOM_FLOAT(700, 900);
		pev->avelocity.z = 0;
		pev->avelocity.x = 0;

		MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, Center());
		WRITE_BYTE(CLTE_SCIMINE_TRIGGERED);
		WRITE_SHORT(ENTINDEX(edict()));
		MESSAGE_END();

		// the HLTV event of SDK 2.2 (svc 50, DRC_EVENT 3), not SDK 2.3's SVC_DIRECTOR form
		MESSAGE_BEGIN(MSG_SPEC, 50);
		WRITE_BYTE(3);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_SHORT(ENTINDEX(pEnt->edict()));
		WRITE_LONG(6 | 0x20); // priority 6 | DRC_FLAG_DRAMATIC
		MESSAGE_END();

		CSoundEnt::InsertSound(bits_SOUND_DANGER, Center(), 1000, 0.75);
		return;
	}

	Talk();
	LookAround();

	m_flHeadYaw = UTIL_Approach(m_flHeadYawGoal, m_flHeadYaw, flInterval * 180);
	SetBoneController(0, m_flHeadYaw);

	if (m_flNextIdle < gpGlobals->time)
	{
		m_flNextIdle = RANDOM_FLOAT(3, 4) + gpGlobals->time;
		MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, Center());
		WRITE_BYTE(CLTE_SCIMINE_IDLE);
		WRITE_SHORT(ENTINDEX(edict()));
		MESSAGE_END();
	}
}

void CScientistMine::PreExplodeThink() // orig 0xb3dd0
{
	float flInterval = StudioFrameAdvance(0);
	pev->nextthink = gpGlobals->time + 0.1;
	ApplyExternalForce(flInterval);
	if (m_flDetonateTime < gpGlobals->time)
		ExplodeThink();
}

void CScientistMine::ExplodeThink() // orig 0xb3e30
{
	pev->takedamage = DAMAGE_NO;
	EMIT_SOUND_DYN(edict(), CHAN_VOICE, "common/null.wav", 1.0, 0.8, 0, 100); // cuts the hello off
	pev->avelocity = g_vecZero;
	if (pev->scale > 0)
		pev->dmg = pev->dmg * pev->scale;
	pev->owner = pev->dmg_inflictor;
	Detonate();
}

void CScientistMine::Killed(entvars_t* pevAttacker, int iGib) // orig 0xb3ed0
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	ExplodeThink();
}

// Turns the head (bone controller 0, through m_flHeadYawGoal) towards vecTarget.
void CScientistMine::SetHeadTarget(Vector vecTarget) // orig 0xb3f00
{
	float flYaw = VecToYaw(vecTarget - pev->origin) - pev->angles.y;
	if (flYaw > 180)
		flYaw -= 360;
	if (flYaw < -180)
		flYaw += 360;
	m_flHeadYawGoal = flYaw;
}

// Answers when asked; otherwise, every so often, asks the nearest free mine of its kind in sight
// (in front preferred) a question, and both look at each other.
void CScientistMine::Talk() // orig 0xb3f70
{
	if (m_flNextTalk > gpGlobals->time)
		return;

	const char* pszSentence;
	if (m_bAnswer)
	{
		m_flNextTalk = RANDOM_FLOAT(6, 12) + gpGlobals->time;
		m_flTalkUntil = RANDOM_FLOAT(2.8, 3.2) + gpGlobals->time;
		m_bAnswer = false;
		pszSentence = m_bBarney ? "BA_ANSWER" : "SC_ANSWER";
	}
	else
	{
		Vector vecEyes = pev->origin + pev->view_ofs;
		float flBest = 99999;
		UTIL_MakeVectors(pev->angles);
		Vector vecForward = gpGlobals->v_forward;
		CScientistMine* pBest = NULL;

		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecEyes, 512)) != NULL)
		{
			if (!pEnt->pev || !UTIL_IsValidEntity(pEnt->edict()) || pEnt->edict() == edict())
				continue;
			if (strcmp(STRING(pEnt->pev->classname), STRING(pev->classname)) != 0)
				continue;
			CScientistMine* pOther = (CScientistMine*)pEnt;
			if (pOther->m_bAnswer || pOther->m_flTalkUntil > gpGlobals->time || !pOther->m_bActive)
				continue;
			if (pBest && RANDOM_FLOAT(0, 1) < 0.2)
				continue;

			Vector vecOtherEyes = pOther->pev->origin + pOther->pev->view_ofs;
			TraceResult tr;
			UTIL_TraceLine(vecEyes, vecOtherEyes, ignore_monsters, edict(), &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pOther->edict())
				continue;

			// it won't start a talk of its own for a while
			if (gpGlobals->time > pOther->m_flNextTalk - 6)
				pOther->m_flNextTalk = RANDOM_FLOAT(6, 8) + gpGlobals->time;

			Vector vecTo = vecOtherEyes - vecEyes;
			float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);
			if (flDist > flBest)
				continue;

			if (pBest)
			{
				Vector vecDir = vecTo.Normalize();
				if (DotProduct(vecDir, vecForward) < 0 && RANDOM_FLOAT(0, 1) < 0.5)
					continue;
			}
			pBest = pOther;
			flBest = flDist;
		}

		if (!pBest)
			return;

		m_flNextTalk = RANDOM_FLOAT(8, 15) + gpGlobals->time;
		m_flTalkUntil = RANDOM_FLOAT(2.8, 3.2) + gpGlobals->time;
		SetHeadTarget(pBest->pev->origin + pBest->pev->view_ofs);
		pBest->m_flNextTalk = pBest->m_flTalkUntil = m_flTalkUntil;
		pBest->SetHeadTarget(pev->origin + pev->view_ofs);
		pBest->m_bAnswer = true;

		if (m_bBarney)
			pszSentence = "BA_QUESTION";
		else if (RANDOM_FLOAT(0, 1) > 0.1)
			pszSentence = "SC_QUESTION";
		else
			pszSentence = "SC_PQUEST";
	}

	SENTENCEG_PlayRndSz(edict(), pszSentence, 1.0, 1.0, 0, ScaledPitchShift() + m_iVoicePitch);
}

// Every half second, picks what to look at in 768: the best scored by closeness and by being
// in front, live things it could attack only, the current one counting double.
void CScientistMine::LookAround() // orig 0xb4510
{
	if (m_flTalkUntil > gpGlobals->time)
		return;

	if (m_flNextLook > gpGlobals->time)
	{
		if (m_hLookTarget)
			SetHeadTarget(m_hLookTarget->pev->origin + m_hLookTarget->pev->view_ofs);
		return;
	}

	Vector vecEyes = pev->origin + pev->view_ofs;
	float flBest = -9999;
	CBaseEntity* pBest = NULL;
	UTIL_MakeVectors(pev->angles);
	Vector vecForward = gpGlobals->v_forward;

	CBaseEntity* pOwner = NULL;
	if (pev->dmg_inflictor && UTIL_IsValidEntity(pev->dmg_inflictor))
		pOwner = CBaseEntity::Instance(pev->dmg_inflictor);

	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecEyes, 768)) != NULL)
	{
		if (!pEnt->pev || !UTIL_IsValidEntity(pEnt->edict()) || pEnt->edict() == edict())
			continue;
		if ((pEnt->GetRCCaps(this, pOwner) & 2) == 0) // VERIFY: as in SciMineThink
			continue;
		if ((pEnt->GetRCCaps2(pOwner, this) & 8) != 0)
			continue;
		if (!pEnt->IsAlive() || pEnt->pev->deadflag != DEAD_NO)
			continue;

		Vector vecTheirEyes = pEnt->pev->origin + pEnt->pev->view_ofs;
		Vector vecTo = vecTheirEyes - vecEyes;
		float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);
		Vector vecDir = vecTo.Normalize();
		float flDot = DotProduct(vecDir, vecForward);
		if (flDot < 0)
			continue;

		float flNear = flDist * (1.0 / 512);
		if (flNear < 0)
			flNear = 0;
		else if (flNear > 1)
			flNear = 1;
		float flScore = (1 - flNear) + flDot * 0.5;
		if (m_hLookTarget && m_hLookTarget->edict() == pEnt->edict())
			flScore = flScore + flScore;
		if (flScore < flBest)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecEyes, vecTheirEyes, ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		flBest = flScore;
		pBest = pEnt;
	}

	if (pBest)
	{
		m_flNextLook = gpGlobals->time + 0.5;
		m_hLookTarget = pBest;
		SetHeadTarget(m_hLookTarget->pev->origin + m_hLookTarget->pev->view_ofs);
	}
	else
	{
		m_flNextLook = gpGlobals->time + 0.3;
	}
}

// TEMP STUB (p6): real one belongs in monsters.cpp (rc2.dll 0x74d0, CBaseMonster's slot 147);
// this is the reading of the original's body, for its package to check
int CBaseMonster::ScaledPitchShift()
{
	if (pev->scale == 0 || pev->scale == 1)
		return 0;
	float flShift = (1.0 - pev->scale) * 1.25 * 40;
	if (flShift < 0)
		flShift = 0;
	else if (flShift > 40)
		flShift = 40;
	return (int)flShift;
}
