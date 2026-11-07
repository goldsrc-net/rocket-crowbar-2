/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "client.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "pm_shared.h"
#include "rc2prefs.h"
#include "player.h"

void EntvarsKeyvalue(entvars_t* pev, KeyValueData* pkvd);

void OnFreeEntPrivateData(edict_s* pEdict);

extern Vector VecBModelOrigin(entvars_t* pevBModel);

static DLL_FUNCTIONS gFunctionTable =
	{
		GameDLLInit,			   //pfnGameInit
		DispatchSpawn,			   //pfnSpawn
		DispatchThink,			   //pfnThink
		DispatchUse,			   //pfnUse
		DispatchTouch,			   //pfnTouch
		DispatchBlocked,		   //pfnBlocked
		DispatchKeyValue,		   //pfnKeyValue
		DispatchSave,			   //pfnSave
		DispatchRestore,		   //pfnRestore
		DispatchObjectCollsionBox, //pfnAbsBox

		SaveWriteFields, //pfnSaveWriteFields
		SaveReadFields,	 //pfnSaveReadFields

		SaveGlobalState,	//pfnSaveGlobalState
		RestoreGlobalState, //pfnRestoreGlobalState
		ResetGlobalState,	//pfnResetGlobalState

		ClientConnect,		   //pfnClientConnect
		ClientDisconnect,	   //pfnClientDisconnect
		ClientKill,			   //pfnClientKill
		ClientPutInServer,	   //pfnClientPutInServer
		ClientCommand,		   //pfnClientCommand
		ClientUserInfoChanged, //pfnClientUserInfoChanged
		ServerActivate,		   //pfnServerActivate
		ServerDeactivate,	   //pfnServerDeactivate

		PlayerPreThink,	 //pfnPlayerPreThink
		PlayerPostThink, //pfnPlayerPostThink

		StartFrame,		  //pfnStartFrame
		ParmsNewLevel,	  //pfnParmsNewLevel
		ParmsChangeLevel, //pfnParmsChangeLevel

		GetGameDescription,	 //pfnGetGameDescription    Returns string describing current .dll game.
		PlayerCustomization, //pfnPlayerCustomization   Notifies .dll of new customization for player.

		SpectatorConnect,	 //pfnSpectatorConnect      Called when spectator joins server
		SpectatorDisconnect, //pfnSpectatorDisconnect   Called when spectator leaves the server
		SpectatorThink,		 //pfnSpectatorThink        Called when spectator sends a command packet (usercmd_t)

		Sys_Error, //pfnSys_Error				Called when engine has encountered an error

		PM_Move,			//pfnPM_Move
		PM_Init,			//pfnPM_Init				Server version of player movement initialization
		PM_FindTextureType, //pfnPM_FindTextureType

		SetupVisibility,		  //pfnSetupVisibility        Set up PVS and PAS for networking for this client
		UpdateClientData,		  //pfnUpdateClientData       Set up data sent only to specific client
		AddToFullPack,			  //pfnAddToFullPack
		CreateBaseline,			  //pfnCreateBaseline			Tweak entity baseline for network encoding, allows setup of player baselines, too.
		RegisterEncoders,		  //pfnRegisterEncoders		Callbacks for network encoding
		GetWeaponData,			  //pfnGetWeaponData
		CmdStart,				  //pfnCmdStart
		CmdEnd,					  //pfnCmdEnd
		ConnectionlessPacket,	  //pfnConnectionlessPacket
		GetHullBounds,			  //pfnGetHullBounds
		CreateInstancedBaselines, //pfnCreateInstancedBaselines
		InconsistentFile,		  //pfnInconsistentFile
		AllowLagCompensation,	  //pfnAllowLagCompensation
};

NEW_DLL_FUNCTIONS gNewDLLFunctions =
	{
		OnFreeEntPrivateData, //pfnOnFreeEntPrivateData
		GameDLLShutdown,
};

static void SetObjectCollisionBox(entvars_t* pev);

extern "C" {

int GetEntityAPI(DLL_FUNCTIONS* pFunctionTable, int interfaceVersion)
{
	if (!pFunctionTable || interfaceVersion != INTERFACE_VERSION)
	{
		return 0;
	}

	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return 1;
}

int GetEntityAPI2(DLL_FUNCTIONS* pFunctionTable, int* interfaceVersion)
{
	if (!pFunctionTable || *interfaceVersion != INTERFACE_VERSION)
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return 0;
	}

	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return 1;
}

int GetNewDLLFunctions(NEW_DLL_FUNCTIONS* pFunctionTable, int* interfaceVersion)
{
	if (!pFunctionTable || *interfaceVersion != NEW_DLL_FUNCTIONS_VERSION)
	{
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return 0;
	}

	memcpy(pFunctionTable, &gNewDLLFunctions, sizeof(gNewDLLFunctions));
	return 1;
}
}


int DispatchSpawn(edict_t* pent)
{
	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pent);

	if (pEntity)
	{
		// Initialize these or entities who don't link to the world won't have anything in here
		pEntity->pev->absmin = pEntity->pev->origin - Vector(1, 1, 1);
		pEntity->pev->absmax = pEntity->pev->origin + Vector(1, 1, 1);

		pEntity->Spawn();

		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.
		pEntity = (CBaseEntity*)GET_PRIVATE(pent);

		if (pEntity)
		{
			if (g_pGameRules && !g_pGameRules->IsAllowedToSpawn(pEntity))
				return -1; // return that this entity should be deleted
			if ((pEntity->pev->flags & FL_KILLME) != 0)
				return -1;
		}


		// Handle global stuff here
		if (pEntity && !FStringNull(pEntity->pev->globalname))
		{
			const globalentity_t* pGlobal = gGlobalState.EntityFromTable(pEntity->pev->globalname);
			if (pGlobal)
			{
				// Already dead? delete
				if (pGlobal->state == GLOBAL_DEAD)
					return -1;
				else if (!FStrEq(STRING(gpGlobals->mapname), pGlobal->levelName))
					pEntity->MakeDormant(); // Hasn't been moved to this level yet, wait but stay alive
											// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd(pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON);
				//				ALERT( at_console, "Added global entity %s (%s)\n", STRING(pEntity->pev->classname), STRING(pEntity->pev->globalname) );
			}
		}
	}

	return 0;
}

void DispatchKeyValue(edict_t* pentKeyvalue, KeyValueData* pkvd)
{
	if (!pkvd || !pentKeyvalue)
		return;

	EntvarsKeyvalue(VARS(pentKeyvalue), pkvd);

	// If the key was an entity variable, or there's no class set yet, don't look for the object, it may
	// not exist yet.
	if (0 != pkvd->fHandled || pkvd->szClassName == NULL)
		return;

	// Get the actualy entity object
	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pentKeyvalue);

	if (!pEntity)
		return;

	pkvd->fHandled = static_cast<int32>(pEntity->KeyValue(pkvd));
}

// orig 0x194f0. RC2: the touch lands on each side's GetEffectedEnt, so does the engine's
// trace_ent, and a player doesn't touch what counts as its own view (IsViewOfPlayer).
void DispatchTouch(edict_t* pentTouched, edict_t* pentOther)
{
	if (gTouchDisabled)
		return;

	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pentTouched);
	CBaseEntity* pOther = (CBaseEntity*)GET_PRIVATE(pentOther);

	if (!pEntity || !pOther)
		return;

	if (((pEntity->pev->flags | pOther->pev->flags) & FL_KILLME) == 0)
	{
		if (UTIL_IsValidEntity(gpGlobals->trace_ent))
			gpGlobals->trace_ent = CBaseEntity::Instance(gpGlobals->trace_ent)->GetEffectedEnt()->edict();

		if (pEntity->IsPlayer() && pOther->IsViewOfPlayer(pEntity))
			return;
		if (pOther->IsPlayer() && pEntity->IsViewOfPlayer(pOther))
			return;

		pEntity->GetEffectedEnt()->Touch(pOther->GetEffectedEnt());
	}
}


// orig 0x19600 (unlisted: no function at it in the decompile). RC2: between the two sides'
// GetEffectedEnt. pOther isn't checked (stock passes it on, RC2 calls through it).
void DispatchUse(edict_t* pentUsed, edict_t* pentOther)
{
	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pentUsed);
	CBaseEntity* pOther = (CBaseEntity*)GET_PRIVATE(pentOther);

	if (pEntity && (pEntity->pev->flags & FL_KILLME) == 0)
		pEntity->GetEffectedEnt()->Use(pOther->GetEffectedEnt(), pOther->GetEffectedEnt(), USE_TOGGLE, 0);
}

void DispatchThink(edict_t* pent)
{
	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pent);
	if (pEntity)
	{
		if (FBitSet(pEntity->pev->flags, FL_DORMANT))
			ALERT(at_error, "Dormant entity %s is thinking!!\n", STRING(pEntity->pev->classname));

		pEntity->Think();
	}
}

void DispatchBlocked(edict_t* pentBlocked, edict_t* pentOther)
{
	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pentBlocked);
	CBaseEntity* pOther = (CBaseEntity*)GET_PRIVATE(pentOther);

	// orig 0x196c0. RC2: between the two sides' GetEffectedEnt (pOther not checked)
	if (pEntity)
		pEntity->GetEffectedEnt()->Blocked(pOther->GetEffectedEnt());
}

void DispatchSave(edict_t* pent, SAVERESTOREDATA* pSaveData)
{
	gpGlobals->time = pSaveData->time;

	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pent);

	if (pEntity && CSaveRestoreBuffer::IsValidSaveRestoreData(pSaveData))
	{
		ENTITYTABLE* pTable = &pSaveData->pTable[pSaveData->currentIndex];

		if (pTable->pent != pent)
			ALERT(at_error, "ENTITY TABLE OR INDEX IS WRONG!!!!\n");

		if ((pEntity->ObjectCaps() & FCAP_DONT_SAVE) != 0)
			return;

		// These don't use ltime & nextthink as times really, but we'll fudge around it.
		if (pEntity->pev->movetype == MOVETYPE_PUSH)
		{
			float delta = pEntity->pev->nextthink - pEntity->pev->ltime;
			pEntity->pev->ltime = gpGlobals->time;
			pEntity->pev->nextthink = pEntity->pev->ltime + delta;
		}

		pTable->location = pSaveData->size;			 // Remember entity position for file I/O
		pTable->classname = pEntity->pev->classname; // Remember entity class for respawn

		CSave saveHelper(*pSaveData);
		pEntity->Save(saveHelper);

		pTable->size = pSaveData->size - pTable->location; // Size of entity block is data size written to block
	}
}

void OnFreeEntPrivateData(edict_s* pEdict)
{
	if (pEdict && pEdict->pvPrivateData)
	{
		auto entity = reinterpret_cast<CBaseEntity*>(pEdict->pvPrivateData);

		delete entity;

		//Zero this out so the engine doesn't try to free it again.
		pEdict->pvPrivateData = nullptr;
	}
}

// Find the matching global entity.  Spit out an error if the designer made entities of
// different classes with the same global name
// orig 0x197e0 (stock)
CBaseEntity* FindGlobalEntity(string_t classname, string_t globalname)
{
	edict_t* pent = FIND_ENTITY_BY_STRING(NULL, "globalname", STRING(globalname));
	CBaseEntity* pReturn = CBaseEntity::Instance(pent);
	if (pReturn)
	{
		if (!FClassnameIs(pReturn->pev, STRING(classname)))
		{
			ALERT(at_console, "Global entity found %s, wrong class %s\n", STRING(globalname), STRING(pReturn->pev->classname));
			pReturn = NULL;
		}
	}

	return pReturn;
}


int DispatchRestore(edict_t* pent, SAVERESTOREDATA* pSaveData, int globalEntity)
{
	gpGlobals->time = pSaveData->time;

	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pent);

	if (pEntity && CSaveRestoreBuffer::IsValidSaveRestoreData(pSaveData))
	{
		entvars_t tmpVars;
		Vector oldOffset;

		CRestore restoreHelper(*pSaveData);
		if (0 != globalEntity)
		{
			CRestore tmpRestore(*pSaveData);
			tmpRestore.PrecacheMode(false);
			tmpRestore.ReadEntVars("ENTVARS", &tmpVars);

			// HACKHACK - reset the save pointers, we're going to restore for real this time
			pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
			pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;
			// -------------------


			const globalentity_t* pGlobal = gGlobalState.EntityFromTable(tmpVars.globalname);

			// Don't overlay any instance of the global that isn't the latest
			// pSaveData->szCurrentMapName is the level this entity is coming from
			// pGlobla->levelName is the last level the global entity was active in.
			// If they aren't the same, then this global update is out of date.
			if (!FStrEq(pSaveData->szCurrentMapName, pGlobal->levelName))
				return 0;

			// Compute the new global offset
			oldOffset = pSaveData->vecLandmarkOffset;
			CBaseEntity* pNewEntity = FindGlobalEntity(tmpVars.classname, tmpVars.globalname);
			if (pNewEntity)
			{
				//				ALERT( at_console, "Overlay %s with %s\n", STRING(pNewEntity->pev->classname), STRING(tmpVars.classname) );
				// Tell the restore code we're overlaying a global entity from another level
				restoreHelper.SetGlobalMode(true); // Don't overwrite global fields
				pSaveData->vecLandmarkOffset = (pSaveData->vecLandmarkOffset - pNewEntity->pev->mins) + tmpVars.mins;
				pEntity = pNewEntity; // we're going to restore this data OVER the old entity
				pent = ENT(pEntity->pev);
				// Update the global table to say that the global definition of this entity should come from this level
				gGlobalState.EntityUpdate(pEntity->pev->globalname, gpGlobals->mapname);
			}
			else
			{
				// This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
				// or call EntityUpdate() to move it to this level, we haven't changed global state at all.
				return 0;
			}
		}

		if ((pEntity->ObjectCaps() & FCAP_MUST_SPAWN) != 0)
		{
			pEntity->Restore(restoreHelper);
			pEntity->Spawn();
		}
		else
		{
			pEntity->Restore(restoreHelper);
			pEntity->Precache();
		}

		// Again, could be deleted, get the pointer again.
		pEntity = (CBaseEntity*)GET_PRIVATE(pent);

#if 0
		if ( pEntity && !FStringNull(pEntity->pev->globalname) && 0 != globalEntity ) 
		{
			ALERT( at_console, "Global %s is %s\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->model) );
		}
#endif

		// Is this an overriding global entity (coming over the transition), or one restoring in a level
		if (0 != globalEntity)
		{
			//			ALERT( at_console, "After: %f %f %f %s\n", pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z, STRING(pEntity->pev->model) );
			pSaveData->vecLandmarkOffset = oldOffset;
			if (pEntity)
			{
				UTIL_SetOrigin(pEntity->pev, pEntity->pev->origin);
				pEntity->OverrideReset();
			}
		}
		else if (pEntity && !FStringNull(pEntity->pev->globalname))
		{
			const globalentity_t* pGlobal = gGlobalState.EntityFromTable(pEntity->pev->globalname);
			if (pGlobal)
			{
				// Already dead? delete
				if (pGlobal->state == GLOBAL_DEAD)
					return -1;
				else if (!FStrEq(STRING(gpGlobals->mapname), pGlobal->levelName))
				{
					pEntity->MakeDormant(); // Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				ALERT(at_error, "Global Entity %s (%s) not in table!!!\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->classname));
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd(pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON);
			}
		}
	}
	return 0;
}


void DispatchObjectCollsionBox(edict_t* pent)
{
	CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE(pent);
	if (pEntity)
	{
		pEntity->SetObjectCollisionBox();
	}
	else
		SetObjectCollisionBox(&pent->v);
}


void SaveWriteFields(SAVERESTOREDATA* pSaveData, const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount)
{
	if (!CSaveRestoreBuffer::IsValidSaveRestoreData(pSaveData))
	{
		return;
	}

	CSave saveHelper(*pSaveData);
	saveHelper.WriteFields(pname, pBaseData, pFields, fieldCount);
}


void SaveReadFields(SAVERESTOREDATA* pSaveData, const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount)
{
	if (!CSaveRestoreBuffer::IsValidSaveRestoreData(pSaveData))
	{
		return;
	}

	// Always check if the player is stuck when loading a save game.
	g_CheckForPlayerStuck = true;

	CRestore restoreHelper(*pSaveData);
	restoreHelper.ReadFields(pname, pBaseData, pFields, fieldCount);
}


edict_t* EHANDLE::Get()
{
	if (m_pent)
	{
		if (m_pent->serialnumber == m_serialnumber)
			return m_pent;
		else
			return NULL;
	}
	return NULL;
};

edict_t* EHANDLE::Set(edict_t* pent)
{
	m_pent = pent;
	if (pent)
		m_serialnumber = m_pent->serialnumber;
	return pent;
};


// orig 0x19ce0 (stock). RC2 has a second, identical copy at orig 0x19cf0 that it uses to read a
// player's members through an EHANDLE (CImpailer::PullingThink); a cast here.
EHANDLE::operator CBaseEntity*()
{
	return (CBaseEntity*)GET_PRIVATE(Get());
};


CBaseEntity* EHANDLE::operator=(CBaseEntity* pEntity)
{
	if (pEntity)
	{
		m_pent = ENT(pEntity->pev);
		if (m_pent)
			m_serialnumber = m_pent->serialnumber;
	}
	else
	{
		m_pent = NULL;
		m_serialnumber = 0;
	}
	return pEntity;
}

// orig 0x19d40 (stock). orig 0x19d30 is SDK 2.3's operator int (stock), which halflife-updated
// dropped: the conversion to CBaseEntity* does its job.
CBaseEntity* EHANDLE::operator->()
{
	return (CBaseEntity*)GET_PRIVATE(Get());
}


// give health
bool CBaseEntity::TakeHealth(float flHealth, int bitsDamageType)
{
	if (0 == pev->takedamage)
		return false;

	// heal
	if (pev->health >= pev->max_health)
		return false;

	pev->health += flHealth;

	if (pev->health > pev->max_health)
		pev->health = pev->max_health;

	return true;
}

// inflict damage on this entity.  bitsDamageType indicates type of damage inflicted, ie: DMG_CRUSH

// orig 0x19dc0. RC2: damage to an entity that has a GetEffectedEnt goes to that.
bool CBaseEntity::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	Vector vecTemp;

	CBaseEntity* pEffected = GetEffectedEnt();
	if (pEffected->pev != pev)
		return pEffected->TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);

	if (0 == pev->takedamage)
		return false;

	// UNDONE: some entity types may be immune or resistant to some bitsDamageType

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin).
	if (pevAttacker == pevInflictor)
	{
		vecTemp = pevInflictor->origin - (VecBModelOrigin(pev));
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - (VecBModelOrigin(pev));
	}

	// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();

	// save damage based on the target's armor level

	// figure momentum add (don't let hurt brushes or other triggers move player)
	if ((!FNullEnt(pevInflictor)) && (pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP) && (pevAttacker->solid != SOLID_TRIGGER))
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();

		float flForce = flDamage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;

		if (flForce > 1000.0)
			flForce = 1000.0;
		pev->velocity = pev->velocity + vecDir * flForce;
	}

	// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed(pevAttacker, GIB_NORMAL);
		return false;
	}

	return true;
}


void CBaseEntity::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove(this);
}


// orig 0x1a1d0 (stock)
CBaseEntity* CBaseEntity::GetNextTarget()
{
	if (FStringNull(pev->target))
		return NULL;
	edict_t* pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->target));
	if (FNullEnt(pTarget))
		return NULL;

	return Instance(pTarget);
}

// Global Savedata for Delay
TYPEDESCRIPTION CBaseEntity::m_SaveData[] =
	{
		DEFINE_FIELD(CBaseEntity, m_pGoalEnt, FIELD_CLASSPTR),
		DEFINE_FIELD(CBaseEntity, m_EFlags, FIELD_CHARACTER),

		DEFINE_FIELD(CBaseEntity, m_pfnThink, FIELD_FUNCTION), // UNDONE: Build table of these!!!
		DEFINE_FIELD(CBaseEntity, m_pfnTouch, FIELD_FUNCTION),
		DEFINE_FIELD(CBaseEntity, m_pfnUse, FIELD_FUNCTION),
		DEFINE_FIELD(CBaseEntity, m_pfnBlocked, FIELD_FUNCTION),
};


bool CBaseEntity::Save(CSave& save)
{
	if (save.WriteEntVars("ENTVARS", pev))
		return save.WriteFields("BASE", this, m_SaveData, ARRAYSIZE(m_SaveData));

	return false;
}

bool CBaseEntity::Restore(CRestore& restore)
{
	bool status;

	status = restore.ReadEntVars("ENTVARS", pev);
	if (status)
		status = restore.ReadFields("BASE", this, m_SaveData, ARRAYSIZE(m_SaveData));

	if (pev->modelindex != 0 && !FStringNull(pev->model))
	{
		Vector mins, maxs;
		mins = pev->mins; // Set model is about to destroy these
		maxs = pev->maxs;


		PRECACHE_MODEL((char*)STRING(pev->model));
		SET_MODEL(ENT(pev), STRING(pev->model));
		UTIL_SetSize(pev, mins, maxs); // Reset them
	}

	return status;
}


// Initialize absmin & absmax to the appropriate box
void SetObjectCollisionBox(entvars_t* pev)
{
	if ((pev->solid == SOLID_BSP) &&
		(pev->angles != g_vecZero))
	{ // expand for rotation
		float max, v;
		int i;

		max = 0;
		for (i = 0; i < 3; i++)
		{
			v = fabs(((float*)pev->mins)[i]);
			if (v > max)
				max = v;
			v = fabs(((float*)pev->maxs)[i]);
			if (v > max)
				max = v;
		}
		for (i = 0; i < 3; i++)
		{
			((float*)pev->absmin)[i] = ((float*)pev->origin)[i] - max;
			((float*)pev->absmax)[i] = ((float*)pev->origin)[i] + max;
		}
	}
	else
	{
		pev->absmin = pev->origin + pev->mins;
		pev->absmax = pev->origin + pev->maxs;
	}

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}


// orig 0x1a520 (stock)
void CBaseEntity::SetObjectCollisionBox()
{
	::SetObjectCollisionBox(pev);
}


bool CBaseEntity::Intersects(CBaseEntity* pOther)
{
	if (pOther->pev->absmin.x > pev->absmax.x ||
		pOther->pev->absmin.y > pev->absmax.y ||
		pOther->pev->absmin.z > pev->absmax.z ||
		pOther->pev->absmax.x < pev->absmin.x ||
		pOther->pev->absmax.y < pev->absmin.y ||
		pOther->pev->absmax.z < pev->absmin.z)
		return false;
	return true;
}

void CBaseEntity::MakeDormant()
{
	SetBits(pev->flags, FL_DORMANT);

	// Don't touch
	pev->solid = SOLID_NOT;
	// Don't move
	pev->movetype = MOVETYPE_NONE;
	// Don't draw
	SetBits(pev->effects, EF_NODRAW);
	// Don't think
	pev->nextthink = 0;
	// Relink
	UTIL_SetOrigin(pev, pev->origin);
}

// orig 0x1a620 (stock)
bool CBaseEntity::IsDormant()
{
	return FBitSet(pev->flags, FL_DORMANT);
}

// orig 0x1a630. RC2: the position only.
bool CBaseEntity::IsInWorld()
{
	// position
	if (pev->origin.x >= 4096)
		return false;
	if (pev->origin.y >= 4096)
		return false;
	if (pev->origin.z >= 4096)
		return false;
	if (pev->origin.x <= -4096)
		return false;
	if (pev->origin.y <= -4096)
		return false;
	if (pev->origin.z <= -4096)
		return false;
	// RC2 dropped SDK 2.3's speed limits (velocity beyond +-2000 counted as out of the world)

	return true;
}

bool CBaseEntity::ShouldToggle(USE_TYPE useType, bool currentState)
{
	if (useType != USE_TOGGLE && useType != USE_SET)
	{
		if ((currentState && useType == USE_ON) || (!currentState && useType == USE_OFF))
			return false;
	}
	return true;
}


int CBaseEntity::DamageDecal(int bitsDamageType)
{
	if (pev->rendermode == kRenderTransAlpha)
		return -1;

	if (pev->rendermode != kRenderNormal)
		return DECAL_BPROOF1;

	return DECAL_GUNSHOT1 + RANDOM_LONG(0, 4);
}



// The client commands that create entities set this (client.cpp's ClientCommand); while it is
// set Create refuses what isn't on the precache list and the classnames that must come from
// the map. RC 1.9's name.
extern bool g_bUnsafeCreate;		  // orig 0x18f3f0

// NOTE: szName must be a pointer to constant memory, e.g. "monster_class" because the entity
// will keep a pointer to it after this call.
// orig 0x1a710. RC2: the g_bUnsafeCreate gate.
CBaseEntity* CBaseEntity::Create(const char* szName, const Vector& vecOrigin, const Vector& vecAngles, edict_t* pentOwner)
{
	edict_t* pent;
	CBaseEntity* pEntity;

	if (g_bUnsafeCreate && (!IsClassnamePrecachedNoInvalid(szName) || IsClassnameMapOnly(szName)))
		return NULL;

	pent = CREATE_NAMED_ENTITY(MAKE_STRING(szName));
	if (FNullEnt(pent))
	{
		ALERT(at_console, "NULL Ent in Create!\n");
		return NULL;
	}
	pEntity = Instance(pent);
	pEntity->pev->owner = pentOwner;
	pEntity->pev->origin = vecOrigin;
	pEntity->pev->angles = vecAngles;
	DispatchSpawn(pEntity->edict());
	return pEntity;
}


// Rocket Crowbar 2: the defaults of CBaseEntity's added virtuals that aren't inline in cbase.h
// (the original has them inline; here they read rc2.ini, which the client doesn't link).
// reference/BASE_VIRTUALS.md

// orig 0x4c50
float CBaseEntity::TransphasicImpactDamage() { return RC2Pref(RC2P_Glock_TransphasicImpactDamage).f; }
// orig 0x4c60
float CBaseEntity::GlobalBeamDamage() { return RC2Pref(RC2P_Tripmine_GlobalBeamDamage).f; }
// orig 0x4c70
float CBaseEntity::FartDamageInflict() { return RC2Pref(RC2P_MP5_FartDamageInflict).f; }
// orig 0x4c80
float CBaseEntity::ZoomedPrimaryDamage() { return RC2Pref(RC2P_Crossbow_ZoomedPrimaryDamage).f; }
// orig 0x4c90
float CBaseEntity::ZoomedSecondaryDamage() { return RC2Pref(RC2P_Crossbow_ZoomedSecondaryDamage).f; }
// orig 0x4ca0
float CBaseEntity::BeamCoreDamage() { return RC2Pref(RC2P_TransformHoundeye_BeamCoreDamage).f; }
// orig 0x4cb0
float CBaseEntity::BeamRadialMaxDamage() { return RC2Pref(RC2P_TransformHoundeye_BeamRadialMaxDamage).f; }
// orig 0x4cc0
float CBaseEntity::BeamViewDamage() { return RC2Pref(RC2P_TransformHoundeye_BeamViewDamage).f; }

// orig 0x4d00: the gravity grenade's kill, as blast + energy beam that always gibs
void CBaseEntity::GravGrenAnnihilate(CBaseEntity* pInflictor, CBaseEntity* pAttacker)
{
	if (pev->takedamage != DAMAGE_NO)
		TakeDamage(pInflictor->pev, pAttacker->pev, RC2Pref(RC2P_Grenade_GravGrenAnnihilateDamage).f, DMG_ALWAYSGIB | DMG_ENERGYBEAM | DMG_BLAST);
}

#include "rc2/pullers.h" // p6: ApplyExternalForce's pull sources

// Integrates what pulls this entity into pev->velocity over flInterval (0: since the last
// call, kept in m_iRC2Base54 as a float): every GetPullOrg (RC2_GetPullForce), the egon
// tractor point, and when GetRCCaps has 0x400 the global pull points. Walking monsters on
// the ground only lift off for a strong enough pull. True when it lifted the entity, tractored
// it, or a pull point had it in range. Called from think functions.
bool CBaseEntity::ApplyExternalForce(float flInterval) // orig 0xa9ba0
{
	Vector vecForce = RC2_GetPullForce(this);
	bool bPulled = false;
	float& flLastTime = reinterpret_cast<float&>(m_iRC2Base54); // VERIFY: m_iRC2Base54 is a float, the last call's time

	if (flInterval == 0)
	{
		if (flLastTime == 0)
		{
			flLastTime = gpGlobals->time;
			return false;
		}
		flInterval = gpGlobals->time - flLastTime;
	}
	flLastTime = gpGlobals->time;

	if (vecForce != g_vecZero)
	{
		bool bDrag = true;
		if (pev->movetype == MOVETYPE_STEP)
		{
			if ((pev->flags & FL_ONGROUND) != 0)
			{
				bDrag = false;
				float flForce = sqrt(vecForce.x * vecForce.x + vecForce.y * vecForce.y + vecForce.z * vecForce.z);
				if (GetForceScale() * 150 < flForce || GetForceScale() * 10 < vecForce.z)
				{
					pev->velocity = vecForce * flInterval + pev->velocity;
					pev->flags &= ~FL_ONGROUND;
					UTIL_SetOrigin(pev, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 1.0));
					bPulled = true;
				}
			}
			else
			{
				pev->velocity = vecForce * flInterval + pev->velocity;
			}
		}
		else if (pev->movetype == MOVETYPE_TOSS)
		{
			Vector vecAdd(vecForce.x * flInterval, vecForce.y * flInterval, vecForce.z * flInterval);
			pev->velocity = Vector(pev->velocity.x + vecAdd.x, pev->velocity.y + vecAdd.y, pev->velocity.z + vecAdd.z);
			if ((pev->flags & FL_ONGROUND) != 0)
			{
				pev->flags &= ~FL_ONGROUND;
				UTIL_SetOrigin(pev, Vector(pev->origin.x, pev->origin.y, pev->origin.z + 1.0));
			}
		}
		else
		{
			Vector vecAdd(vecForce.x * flInterval, vecForce.y * flInterval, vecForce.z * flInterval);
			pev->velocity = Vector(pev->velocity.x + vecAdd.x, pev->velocity.y + vecAdd.y, pev->velocity.z + vecAdd.z);
		}

		// air drag while pulled: a third of the speed per second
		if (bDrag && (pev->flags & FL_ONGROUND) == 0)
		{
			float flSpeed = sqrt(pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y + pev->velocity.z * pev->velocity.z);
			Vector vecDrag = pev->velocity.Normalize() * flSpeed * 0.33f * flInterval;
			pev->velocity = pev->velocity - vecDrag;
		}
	}

	// the egon's tractor beam: straight at its point, harder further out, dropped past
	// [Egon] TractorDropDist
	Vector vecTractor;
	if (PM_GetPlayerPullVec(ENTINDEX(edict()), vecTractor) && !(vecTractor == g_vecZero) && flInterval > 0)
	{
		Vector vecTo = vecTractor - Center();
		float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);
		if (flDist > RC2Pref(RC2P_Egon_TractorDropDist).f)
		{
			PM_ClearPlayerPull(ENTINDEX(edict()));
		}
		else
		{
			if (flDist < 0)
				flDist = 0;
			float flFrac = flDist / RC2Pref(RC2P_Egon_TractorDropDist).f * 0.95 + 0.05;
			pev->velocity = vecTo.Normalize() * flFrac * RC2Pref(RC2P_Egon_TractorMaxAccel).f * flInterval;
			bPulled = true;
			pev->flags &= ~FL_ONGROUND;

			float flSpeed = sqrt(pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y + pev->velocity.z * pev->velocity.z);
			Vector vecDrag = pev->velocity.Normalize() * flSpeed * 0.5f * flInterval;
			pev->velocity = pev->velocity - vecDrag;
		}
	}

	// the gravity and suck grenades' pull points: inverse square, snapping in inside 32
	if ((GetRCCaps(0, NULL) & 0x400) != 0)
	{
		for (int i = 0; i < 64; i++)
		{
			if (g_vecPullPointOrg[i] == g_vecZero)
				continue;

			Vector vecTo = g_vecPullPointOrg[i] - Center();
			float flDist = sqrt(vecTo.x * vecTo.x + vecTo.y * vecTo.y + vecTo.z * vecTo.z);
			if (!(flDist < g_flPullPointRadius[i]))
				continue;

			bPulled = true;
			if (flDist < 32)
			{
				if (flInterval != 0)
					pev->velocity = Vector(vecTo.x / flInterval, vecTo.y / flInterval, vecTo.z / flInterval);
				if (pev->velocity.z > 0 && ((pev->flags & FL_ONGROUND) != 0 || pev->movetype == MOVETYPE_TOSS))
					UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
			}
			else
			{
				pev->velocity = pev->velocity + vecTo / (flDist * flDist) * g_flPullPointMonsterForce[i] * flInterval;
				if ((pev->flags & FL_ONGROUND) != 0 || pev->movetype == MOVETYPE_TOSS)
					UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
			}
			pev->flags &= ~FL_ONGROUND;
		}
	}

	return bPulled;
}

// orig 0x1a7e0. True when pPlayer is a player and this entity's owner is the player it rides
// or one riding its shoulders (CBasePlayer's RC2 members at +0x69c, +0x6a4, +0x6a8; p6's names).
bool CBaseEntity::IsViewOfPlayer(CBaseEntity* pPlayer)
{
	if (!pPlayer->IsPlayer())
		return false;
	if (0 == ENTINDEX(edict()))
		return false;
	if (!UTIL_IsValidEntity(pev->owner))
		return false;
	const int iOwner = ENTINDEX(pev->owner);
	if (0 == iOwner)
		return false;
	CBasePlayer* pThePlayer = static_cast<CBasePlayer*>(pPlayer);
	return iOwner == pThePlayer->m_iRidingIndex || iOwner == pThePlayer->m_iShoulderRider[0] || iOwner == pThePlayer->m_iShoulderRider[1];
}
