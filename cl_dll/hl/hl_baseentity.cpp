/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

/*
==========================
This file contains "stubs" of class member implementations so that we can predict certain
 weapons client side.  From time to time you might find that you need to implement part of the
 these functions.  If so, cut it from here, paste it in hl_weapons.cpp or somewhere else and
 add in the functionality you need.
==========================
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "skill.h"
#include "rc2prefs.h"

// Rocket Crowbar 2's client.dll has this file's stubs in SDK 2.3's order (each its own function,
// the client was linked incrementally), with RC2's own after CBasePlayer::AddPointsToTeam and at
// the end. The ones tagged orig client were matched to SDK 2.3's list by position, argument bytes
// and return value (reference/dossiers/c1.md); their bodies are the stock stubs.

void EMIT_SOUND_DYN(edict_t* entity, int channel, const char* sample, float volume, float attenuation, int flags, int pitch) {} // orig client 0x36310

// CBaseEntity Stubs
bool CBaseEntity::TakeHealth(float flHealth, int bitsDamageType) { return true; } // orig client 0x36330
bool CBaseEntity::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) { return true; } // orig client 0x36350
CBaseEntity* CBaseEntity::GetNextTarget() { return NULL; } // orig client 0x36370
bool CBaseEntity::Save(CSave& save) { return true; }
bool CBaseEntity::Restore(CRestore& restore) { return true; }
void CBaseEntity::SetObjectCollisionBox() {}
bool CBaseEntity::Intersects(CBaseEntity* pOther) { return false; }
void CBaseEntity::MakeDormant() {}
bool CBaseEntity::IsDormant() { return false; }
bool CBaseEntity::IsInWorld() { return true; } // orig client 0x36450
bool CBaseEntity::ShouldToggle(USE_TYPE useType, bool currentState) { return false; }
int CBaseEntity::DamageDecal(int bitsDamageType) { return -1; } // orig client 0x36490
CBaseEntity* CBaseEntity::Create(const char* szName, const Vector& vecOrigin, const Vector& vecAngles, edict_t* pentOwner) { return NULL; }
void CBaseEntity::SUB_Remove() {} // orig client 0x364d0

// CBaseDelay Stubs
bool CBaseDelay::KeyValue(struct KeyValueData_s*) { return false; }
bool CBaseDelay::Restore(class CRestore&) { return true; }
bool CBaseDelay::Save(class CSave&) { return true; }

// CBaseAnimating Stubs
bool CBaseAnimating::Restore(class CRestore&) { return true; }
bool CBaseAnimating::Save(class CSave&) { return true; }

// DEBUG Stubs
edict_t* DBG_EntOfVars(const entvars_t* pev) { return NULL; } // orig client 0x365b0
void DBG_AssertFunction(bool fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage) {}

// UTIL_* Stubs
void UTIL_PrecacheOther(const char* szClassname) {} // orig client 0x365f0
void UTIL_BloodDrips(const Vector& origin, const Vector& direction, int color, int amount) {}
void UTIL_DecalTrace(TraceResult* pTrace, int decalNumber) {}
void UTIL_GunshotDecalTrace(TraceResult* pTrace, int decalNumber) {}
void UTIL_MakeVectors(const Vector& vecAngles) {} // orig client 0x36670
bool UTIL_IsValidEntity(edict_t* pent) { return true; } // orig client 0x36690
void UTIL_SetOrigin(entvars_t*, const Vector& org) {} // orig client 0x366b0
void UTIL_LogPrintf(char*, ...) {}
void UTIL_ClientPrintAll(int, char const*, char const*, char const*, char const*, char const*) {}
void ClientPrint(entvars_t* client, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4) {}

// CBaseToggle Stubs
bool CBaseToggle::Restore(class CRestore&) { return true; }
bool CBaseToggle::Save(class CSave&) { return true; }
bool CBaseToggle::KeyValue(struct KeyValueData_s*) { return false; }
void CBaseToggle::PlaySentence(const char* pszSentence, float duration, float volume, float attenuation) {}
void CBaseToggle::PlaySentenceCore(const char* pszSentence, float duration, float volume, float attenuation) {}
void CBaseToggle::PlayScriptedSentence(const char* pszSentence, float duration, float volume, float attenuation, bool bConcurrent, CBaseEntity* pListener) {}
void CBaseToggle::SentenceStop() {}

// CGrenade Stubs
void CGrenade::BounceSound() {}
void CGrenade::Explode(Vector, Vector) {}
void CGrenade::Explode(TraceResult*, int) {}
void CGrenade::Killed(entvars_t*, int) {}
void CGrenade::Spawn() {}
CGrenade* CGrenade::ShootTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time) { return 0; }
CGrenade* CGrenade::ShootContact(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity) { return 0; }
void CGrenade::DetonateUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) {} // orig client 0x36890

void UTIL_Remove(CBaseEntity* pEntity) {}
void UTIL_SetSize(entvars_t* pev, const Vector& vecMin, const Vector& vecMax) {} // orig client 0x368d0
CBaseEntity* UTIL_FindEntityInSphere(CBaseEntity* pStartEntity, const Vector& vecCenter, float flRadius) { return 0; }

Vector UTIL_VecToAngles(const Vector& vec) { return 0; }
CSprite* CSprite::SpriteCreate(const char* pSpriteName, const Vector& origin, bool animate) { return 0; }
void CBeam::PointEntInit(const Vector& start, int endIndex) {}
CBeam* CBeam::BeamCreate(const char* pSpriteName, int width) { return NULL; }
void CSprite::Expand(float scaleSpeed, float fadeSpeed) {}


CBaseEntity* CBaseMonster::CheckTraceHullAttack(float flDist, int iDamage, int iDmgType) { return NULL; }
void CBaseMonster::Eat(float flFullDuration) {}
bool CBaseMonster::FShouldEat() { return true; }
void CBaseMonster::BarnacleVictimBitten(entvars_t* pevBarnacle) {}
void CBaseMonster::BarnacleVictimReleased() {}
void CBaseMonster::Listen() {}
float CBaseMonster::FLSoundVolume(CSound* pSound) { return 0.0; }
bool CBaseMonster::FValidateHintType(short sHint) { return false; }
void CBaseMonster::Look(int iDistance) {}
int CBaseMonster::ISoundMask() { return 0; }
CSound* CBaseMonster::PBestSound() { return NULL; }
CSound* CBaseMonster::PBestScent() { return NULL; }
float CBaseAnimating::StudioFrameAdvance(float flInterval) { return 0.0; }
void CBaseMonster::MonsterThink() {}
void CBaseMonster::MonsterUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) {} // orig client 0x36b80
int CBaseMonster::IgnoreConditions() { return 0; }
void CBaseMonster::RouteClear() {}
void CBaseMonster::RouteNew() {}
bool CBaseMonster::FRouteClear() { return false; }
bool CBaseMonster::FRefreshRoute() { return false; }
bool CBaseMonster::MoveToEnemy(Activity movementAct, float waitTime) { return false; }
// p6 needs: client stub for RC2's CBaseMonster slot 147 (basemonster.h)
bool CBaseMonster::MoveToLocation(Activity movementAct, float waitTime, const Vector& goal) { return false; }
bool CBaseMonster::MoveToTarget(Activity movementAct, float waitTime) { return false; }
bool CBaseMonster::MoveToNode(Activity movementAct, float waitTime, const Vector& goal) { return false; }
bool ShouldSimplify(int routeType) { return true; }
void CBaseMonster::RouteSimplify(CBaseEntity* pTargetEnt) {}
bool CBaseMonster::FBecomeProne() { return true; }
bool CBaseMonster::CheckRangeAttack1(float flDot, float flDist) { return false; }
bool CBaseMonster::CheckRangeAttack2(float flDot, float flDist) { return false; }
bool CBaseMonster::CheckMeleeAttack1(float flDot, float flDist) { return false; }
bool CBaseMonster::CheckMeleeAttack2(float flDot, float flDist) { return false; }
void CBaseMonster::CheckAttacks(CBaseEntity* pTarget, float flDist) {}
bool CBaseMonster::FCanCheckAttacks() { return false; }
bool CBaseMonster::CheckEnemy(CBaseEntity* pEnemy) { return false; }
void CBaseMonster::PushEnemy(CBaseEntity* pEnemy, Vector& vecLastKnownPos) {}
bool CBaseMonster::PopEnemy() { return false; }
void CBaseMonster::SetActivity(Activity NewActivity) {}
void CBaseMonster::SetSequenceByName(const char* szSequence) {}
int CBaseMonster::CheckLocalMove(const Vector& vecStart, const Vector& vecEnd, CBaseEntity* pTarget, float* pflDist) { return 0; }
float CBaseMonster::OpenDoorAndWait(entvars_t* pevDoor) { return 0.0; }
void CBaseMonster::AdvanceRoute(float distance) {}
int CBaseMonster::RouteClassify(int iMoveFlag) { return 0; }
bool CBaseMonster::BuildRoute(const Vector& vecGoal, int iMoveFlag, CBaseEntity* pTarget) { return false; }
void CBaseMonster::InsertWaypoint(Vector vecLocation, int afMoveFlags) {}
bool CBaseMonster::FTriangulate(const Vector& vecStart, const Vector& vecEnd, float flDist, CBaseEntity* pTargetEnt, Vector* pApex) { return false; }
void CBaseMonster::Move(float flInterval) {}
bool CBaseMonster::ShouldAdvanceRoute(float flWaypointDist) { return false; }
void CBaseMonster::MoveExecute(CBaseEntity* pTargetEnt, const Vector& vecDir, float flInterval) {}
void CBaseMonster::MonsterInit() {}
void CBaseMonster::MonsterInitThink() {} // orig client 0x36fe0
void CBaseMonster::StartMonster() {}
void CBaseMonster::MovementComplete() {}
bool CBaseMonster::TaskIsRunning() { return false; }
int CBaseMonster::IRelationship(CBaseEntity* pTarget) { return 0; }
bool CBaseMonster::FindCover(Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist) { return false; }
bool CBaseMonster::BuildNearestRoute(Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist) { return false; }
CBaseEntity* CBaseMonster::BestVisibleEnemy() { return NULL; }
bool CBaseMonster::FInViewCone(CBaseEntity* pEntity) { return false; }
bool CBaseMonster::FInViewCone(Vector* pOrigin) { return false; }
bool CBaseEntity::FVisible(CBaseEntity* pEntity) { return false; }
bool CBaseEntity::FVisible(const Vector& vecOrigin) { return false; }
// Rocket Crowbar 2's added CBaseEntity virtuals (dlls/cbase.cpp on the server). The client's are
// the same bodies, inline in the original's cbase.h (emitted into crossbow.cpp's object): they read
// the client's copy of rc2.ini.
float CBaseEntity::TransphasicImpactDamage() { return RC2Pref(RC2P_Glock_TransphasicImpactDamage).f; }		  // orig client 0xa410
float CBaseEntity::GlobalBeamDamage() { return RC2Pref(RC2P_Tripmine_GlobalBeamDamage).f; }				  // orig client 0xa430
float CBaseEntity::FartDamageInflict() { return RC2Pref(RC2P_MP5_FartDamageInflict).f; }				  // orig client 0xa450
float CBaseEntity::ZoomedPrimaryDamage() { return RC2Pref(RC2P_Crossbow_ZoomedPrimaryDamage).f; }		  // orig client 0xa470
float CBaseEntity::ZoomedSecondaryDamage() { return RC2Pref(RC2P_Crossbow_ZoomedSecondaryDamage).f; }	  // orig client 0xa490
float CBaseEntity::BeamCoreDamage() { return RC2Pref(RC2P_TransformHoundeye_BeamCoreDamage).f; }		  // orig client 0xa4b0
float CBaseEntity::BeamRadialMaxDamage() { return RC2Pref(RC2P_TransformHoundeye_BeamRadialMaxDamage).f; } // orig client 0xa4d0
float CBaseEntity::BeamViewDamage() { return RC2Pref(RC2P_TransformHoundeye_BeamViewDamage).f; }		  // orig client 0xa4f0
// orig client 0xa570
void CBaseEntity::GravGrenAnnihilate(CBaseEntity* pInflictor, CBaseEntity* pAttacker)
{
	if (pev->takedamage != DAMAGE_NO)
		TakeDamage(pInflictor->pev, pAttacker->pev, RC2Pref(RC2P_Grenade_GravGrenAnnihilateDamage).f, DMG_ALWAYSGIB | DMG_ENERGYBEAM | DMG_BLAST);
}
bool CBaseEntity::ApplyExternalForce(float flInterval) { return false; } // orig client 0x38880
bool CBaseEntity::IsViewOfPlayer(CBaseEntity* pPlayer) { return false; }
void CBaseMonster::MakeIdealYaw(Vector vecTarget) {}
float CBaseMonster::FlYawDiff() { return 0.0; }
float CBaseMonster::ChangeYaw(int yawSpeed) { return 0; }
float CBaseMonster::VecToYaw(Vector vecDir) { return 0.0; }
int CBaseAnimating::LookupActivity(int activity) { return 0; }
int CBaseAnimating::LookupActivityHeaviest(int activity) { return 0; }
void CBaseMonster::SetEyePosition() {}
int CBaseAnimating::LookupSequence(const char* label) { return 0; }
void CBaseAnimating::ResetSequenceInfo() {}
int CBaseAnimating::GetSequenceFlags() { return 0; }
void CBaseAnimating::DispatchAnimEvents(float flInterval) {}
void CBaseMonster::HandleAnimEvent(MonsterEvent_t* pEvent) {}
float CBaseAnimating::SetBoneController(int iController, float flValue) { return 0.0; }
void CBaseAnimating::InitBoneControllers() {}
float CBaseAnimating::SetBlending(int iBlender, float flValue) { return 0; }
void CBaseAnimating::GetBonePosition(int iBone, Vector& origin, Vector& angles) {}
void CBaseAnimating::GetAttachment(int iAttachment, Vector& origin, Vector& angles) {}
int CBaseAnimating::FindTransition(int iEndingSequence, int iGoalSequence, int* piDir) { return -1; }
void CBaseAnimating::GetAutomovement(Vector& origin, Vector& angles, float flInterval) {}
void CBaseAnimating::SetBodygroup(int iGroup, int iValue) {}
int CBaseAnimating::GetBodygroup(int iGroup) { return 0; }
Vector CBaseMonster::GetGunPosition() { return g_vecZero; }
void CBaseEntity::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) {} // orig client 0x37440
void CBaseEntity::FireBullets(unsigned int cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t* pevAttacker) {}
void CBaseEntity::TraceBleed(float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) {} // orig client 0x37480
void CBaseMonster::MakeDamageBloodDecal(int cCount, float flNoise, TraceResult* ptr, const Vector& vecDir) {}
bool CBaseMonster::FGetNodeRoute(Vector vecDest) { return true; }
int CBaseMonster::FindHintNode() { return NO_NODE; }
void CBaseMonster::ReportAIState() {}
bool CBaseMonster::KeyValue(KeyValueData* pkvd) { return false; }
bool CBaseMonster::FCheckAITrigger() { return false; }
bool CBaseMonster::CanPlaySequence(bool fDisregardMonsterState, int interruptLevel) { return false; }
bool CBaseMonster::FindLateralCover(const Vector& vecThreat, const Vector& vecViewOffset) { return false; }
Vector CBaseMonster::ShootAtEnemy(const Vector& shootOrigin) { return g_vecZero; }
bool CBaseMonster::FacingIdeal() { return false; }
bool CBaseMonster::FCanActiveIdle() { return false; }
void CBaseMonster::CorpseFallThink() {} // orig client 0x37680
void CBaseMonster::MonsterInitDead() {}
bool CBaseMonster::BBoxFlat() { return true; }
bool CBaseMonster::GetEnemy() { return false; }
void CBaseMonster::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) {}
CBaseEntity* CBaseMonster::DropItem(const char* pszItemName, const Vector& vecPos, const Vector& vecAng) { return NULL; }
bool CBaseMonster::ShouldFadeOnDeath() { return false; }
void CBaseMonster::RadiusDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType) {}
void CBaseMonster::RadiusDamage(Vector vecSrc, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType) {}
void CBaseMonster::FadeMonster() {}
void CBaseMonster::GibMonster() {}
bool CBaseMonster::HasHumanGibs() { return false; }
bool CBaseMonster::HasAlienGibs() { return false; }
Activity CBaseMonster::GetDeathActivity() { return ACT_DIE_HEADSHOT; }
MONSTERSTATE CBaseMonster::GetIdealState() { return MONSTERSTATE_ALERT; }
Schedule_t* CBaseMonster::GetScheduleOfType(int Type) { return NULL; }
Schedule_t* CBaseMonster::GetSchedule() { return NULL; }
void CBaseMonster::RunTask(Task_t* pTask) {}
void CBaseMonster::StartTask(Task_t* pTask) {}
Schedule_t* CBaseMonster::ScheduleFromName(const char* pName) { return NULL; }
void CBaseMonster::BecomeDead() {}
void CBaseMonster::RunAI() {}
void CBaseMonster::Killed(entvars_t* pevAttacker, int iGib) {}
bool CBaseMonster::TakeHealth(float flHealth, int bitsDamageType) { return false; }
bool CBaseMonster::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) { return false; }
bool CBaseMonster::Restore(class CRestore&) { return true; }
bool CBaseMonster::Save(class CSave&) { return true; }

int TrainSpeed(int iSpeed, int iMax) { return 0; }
void CBasePlayer::DeathSound() {}
bool CBasePlayer::TakeHealth(float flHealth, int bitsDamageType) { return false; }
void CBasePlayer::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) {}
bool CBasePlayer::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) { return false; }
void CBasePlayer::PackDeadPlayerItems() {}
void CBasePlayer::RemoveAllItems(bool removeSuit) {}
void CBasePlayer::SetAnimation(PLAYER_ANIM playerAnim) {} // orig client 0x37ac0
void CBasePlayer::WaterMove() {}
bool CBasePlayer::IsOnLadder() { return false; }
void CBasePlayer::PlayerDeathThink() {} // orig client 0x37b20
void CBasePlayer::StartDeathCam() {}
void CBasePlayer::StartObserver(Vector vecPosition, Vector vecViewAngle) {}
void CBasePlayer::PlayerUse() {}
void CBasePlayer::Jump() {}
void CBasePlayer::Duck() {}
int CBasePlayer::Classify() { return 0; }
void CBasePlayer::PreThink() {}
void CBasePlayer::CheckTimeBasedDamage() {}
void CBasePlayer::UpdateGeigerCounter() {}
void CBasePlayer::CheckSuitUpdate() {}
void CBasePlayer::SetSuitUpdate(const char* name, bool fgroup, int iNoRepeatTime) {} // orig client 0x37c80
void CBasePlayer::UpdatePlayerSound() {}
void CBasePlayer::PostThink() {}
void CBasePlayer::Precache() {}
bool CBasePlayer::Save(CSave& save) { return false; }
void CBasePlayer::RenewItems() {}
bool CBasePlayer::Restore(CRestore& restore) { return false; }
void CBasePlayer::SelectNextItem(int iItem) {}
bool CBasePlayer::HasWeapons() { return false; }
void CBasePlayer::SelectPrevItem(int iItem) {}
bool CBasePlayer::FlashlightIsOn() { return false; }
void CBasePlayer::FlashlightTurnOn() {}
void CBasePlayer::FlashlightTurnOff() {}
void CBasePlayer::ForceClientDllUpdate() {}
void CBasePlayer::ImpulseCommands() {}
void CBasePlayer::CheatImpulseCommands(int iImpulse) {}
bool CBasePlayer::AddPlayerItem(CBasePlayerItem* pItem) { return false; }
bool CBasePlayer::RemovePlayerItem(CBasePlayerItem* pItem) { return false; }
void CBasePlayer::ItemPreFrame() {}
void CBasePlayer::ItemPostFrame() {}
int CBasePlayer::AmmoInventory(int iAmmoIndex) { return -1; }
int CBasePlayer::GetAmmoIndex(const char* psz) { return -1; }
void CBasePlayer::SendAmmoUpdate() {}
void CBasePlayer::UpdateClientData() {}
bool CBasePlayer::FBecomeProne() { return true; }
void CBasePlayer::BarnacleVictimBitten(entvars_t* pevBarnacle) {}
void CBasePlayer::BarnacleVictimReleased() {}
int CBasePlayer::Illumination() { return 0; }
void CBasePlayer::EnableControl(bool fControl) {}
Vector CBasePlayer::GetAutoaimVector(float flDelta) { return g_vecZero; } // orig client 0x38040
Vector CBasePlayer::AutoaimDeflection(Vector& vecSrc, float flDist, float flDelta) { return g_vecZero; }
void CBasePlayer::ResetAutoaim() {}
void CBasePlayer::SetCustomDecalFrames(int nFrames) {}
int CBasePlayer::GetCustomDecalFrames() { return -1; }
void CBasePlayer::DropPlayerItem(char* pszItemName) {}
bool CBasePlayer::HasPlayerItem(CBasePlayerItem* pCheckItem) { return false; }
bool CBasePlayer::SwitchWeapon(CBasePlayerItem* pWeapon) { return false; }
Vector CBasePlayer::GetGunPosition() { return g_vecZero; }
const char* CBasePlayer::TeamID() { return ""; }
int CBasePlayer::GiveAmmo(int iCount, const char* szName, int iMax) { return 0; }
// p5 needs: RC2's CBasePlayer virtuals the client's copy of the class carries
int CBasePlayer::GiveAmmo(int iCount, const char* szName, int iMax, int* piGiven) { return 0; }
bool CBasePlayer::IsViewOfPlayer(CBaseEntity* pPlayer) { return false; }
void CBasePlayer::AddPoints(int score, bool bAllowNegativeScore) {}
void CBasePlayer::AddPointsToTeam(int score, bool bAllowNegativeScore) {}
void CBasePlayer::TabulateAmmo() {}

void ClearMultiDamage() {}
void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker) {}
void AddMultiDamage(entvars_t* pevInflictor, CBaseEntity* pEntity, float flDamage, int bitsDamageType) {}
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage) {}
int DamageDecal(CBaseEntity* pEntity, int bitsDamageType) { return 0; }
void DecalGunshot(TraceResult* pTrace, int iBulletType) {} // orig client 0x38400
void EjectBrass(const Vector& vecOrigin, const Vector& vecVelocity, float rotation, int model, int soundtype) {}
bool CBasePlayerItem::Restore(class CRestore&) { return true; }
bool CBasePlayerItem::Save(class CSave&) { return true; }
bool CBasePlayerWeapon::Restore(class CRestore&) { return true; } // orig client 0x384a0
bool CBasePlayerWeapon::Save(class CSave&) { return true; } // orig client 0x384c0
float CBasePlayerWeapon::GetNextAttackDelay(float flTime) { return flTime; }
void CBasePlayerItem::SetObjectCollisionBox() {}
void CBasePlayerItem::FallInit() {} // orig client 0x38500
void CBasePlayerItem::FallThink() {} // orig client 0x38520
void CBasePlayerItem::Materialize() {} // orig client 0x38540
void CBasePlayerItem::AttemptToMaterialize() {} // orig client 0x38560
void CBasePlayerItem::CheckRespawn() {}
CBaseEntity* CBasePlayerItem::Respawn() { return NULL; }
void CBasePlayerItem::DefaultTouch(CBaseEntity* pOther) {} // orig client 0x385c0
void CBasePlayerItem::DestroyItem() {} // orig client 0x385e0
void CBasePlayerItem::AddToPlayer(CBasePlayer* pPlayer) {} // orig client 0x38600
void CBasePlayerItem::Drop() {}
void CBasePlayerItem::Kill() {}
void CBasePlayerItem::Holster() {}
void CBasePlayerItem::AttachToPlayer(CBasePlayer* pPlayer) {}
bool CBasePlayerWeapon::AddDuplicate(CBasePlayerItem* pOriginal) { return false; }
void CBasePlayerWeapon::AddToPlayer(CBasePlayer* pPlayer) {} // orig client 0x386c0
bool CBasePlayerWeapon::UpdateClientData(CBasePlayer* pPlayer) { return false; }
bool CBasePlayerWeapon::IsUseable() { return true; }
int CBasePlayerWeapon::PrimaryAmmoIndex() { return m_iPrimaryAmmoType; }
int CBasePlayerWeapon::SecondaryAmmoIndex() { return m_iSecondaryAmmoType; }
void CBasePlayerAmmo::Spawn() {} // orig client 0x387a0
CBaseEntity* CBasePlayerAmmo::Respawn() { return this; } // orig client 0x387c0
void CBasePlayerAmmo::Materialize() {} // orig client 0x387e0
void CBasePlayerAmmo::DefaultTouch(CBaseEntity* pOther) {} // orig client 0x38800
bool CBasePlayerWeapon::ExtractAmmo(CBasePlayerWeapon* pWeapon) { return false; }
bool CBasePlayerWeapon::ExtractClipAmmo(CBasePlayerWeapon* pWeapon) { return false; }
void CBasePlayerWeapon::RetireWeapon() {}
void CBasePlayerWeapon::DoRetireWeapon() {}

// Rocket Crowbar 2 (p3, weapons): client stubs for what weapons.cpp adds (server only there)
bool CBasePlayerItem::KeyValue(KeyValueData* pkvd) { return false; } // orig client 0x388c0
void RC2RandomizeItem(CBaseEntity* pItem) {}
bool g_fRC2Randomize = true;
unsigned short g_usExplodeEvent;
// RC2ReadState*: the client's real ones are in hl_weapons.cpp (c1)
void CSoundEnt::InsertSound(int iType, const Vector& vecOrigin, int iVolume, float flDuration) {}
void RadiusDamage(Vector vecSrc, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType) {}

// Rocket Crowbar 2 (c1): the client's LINK_ENTITY_TO_CLASS calls this too, after GetClassPtr
// (the client keeps no precache list). util.h's client macro leaves the call out: an empty call.
void AddToPrecachedList(const char* pszClassname) {} // orig client 0x38900
