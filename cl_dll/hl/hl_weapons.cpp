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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

#include "usercmd.h"
#include "entity_state.h"
#include "demo_api.h"
#include "pm_defs.h"
#include "event_api.h"
#include "r_efx.h"

#include "cl_dll.h"
#include "../com_weapons.h"
#include "../demo.h"
#include "hud.h"

extern int g_iUser1;

// Rocket Crowbar 2: the HUD's copies of what the server tells the client about players (names
// ours; the real ones are c2's)
extern int g_iLocalPlayerFrozen; // rc2/clte.cpp, orig client .bss 0x101f69f8: the local player is frozen
int PM_GetAbilityFlags(int iEnt);			// rc2/attributes.cpp, orig client 0x4f4c0: player iEnt's ability bits, 0 out of 1..32
unsigned char* PM_GetRideInfo(int iEnt);	// rc2/pm_client.cpp, orig client 0xaf980: player iEnt's riding byte
static void RC2_ApplyWeaponSync(CBasePlayerWeapon* pWeapon);
bool CanAttack(float attack_time, float curtime, bool isPredicted); // weapons_shared.cpp

// Pool of client side entities/entvars_t
static entvars_t ev[MAX_WEAPONS + 1];
static int num_ents = 0;

// The entity we'll use to represent the local client
static CBasePlayer player;

// Local version of game .dll global variables ( time, etc. )
static globalvars_t Globals;

static CBasePlayerWeapon* g_pWpns[MAX_WEAPONS];

float g_flApplyVel = 0.0;
bool g_irunninggausspred = false;

Vector previousorigin;

// HLDM Weapon placeholder entities.
CGlock g_Glock;
CCrowbar g_Crowbar;
CPython g_Python;
CMP5 g_Mp5;
CCrossbow g_Crossbow;
CShotgun g_Shotgun;
CRpg g_Rpg;
CGauss g_Gauss;
CEgon g_Egon;
CHgun g_HGun;
CHandGrenade g_HandGren;
CSatchel g_Satchel;
CTripmine g_Tripmine;
CSqueak g_Snark;


/*
======================
AlertMessage

Print debug messages to console
======================
*/
void AlertMessage(ALERT_TYPE atype, const char* szFmt, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, szFmt);
	vsprintf(string, szFmt, argptr);
	va_end(argptr);

	gEngfuncs.Con_Printf("cl:  ");
	gEngfuncs.Con_Printf(string);
}

//Returns if it's multiplayer.
//Mostly used by the client side weapons.
// orig client 0x39480
bool bIsMultiplayer()
{
	return gEngfuncs.GetMaxClients() != 1;
}
//Just loads a v_ model.
void LoadVModel(const char* szViewModel, CBasePlayer* m_pPlayer)
{
	gEngfuncs.CL_LoadModel(szViewModel, &m_pPlayer->pev->viewmodel);
}

/*
=====================
HUD_PrepEntity

Links the raw entity to an entvars_s holder.  If a player is passed in as the owner, then
we set up the m_pPlayer field.
=====================
*/
// orig client 0x394d0. RC2's is SDK 2.3's; halflife-updated's ItemInfoArray and ammo registry
// filling are kept (RC2's ItemPostFrame reads ItemInfoArray, which nothing else fills here).
void HUD_PrepEntity(CBaseEntity* pEntity, CBasePlayer* pWeaponOwner)
{
	memset(&ev[num_ents], 0, sizeof(entvars_t));
	pEntity->pev = &ev[num_ents++];

	pEntity->Precache();
	pEntity->Spawn();

	if (pWeaponOwner)
	{
		ItemInfo info;

		memset(&info, 0, sizeof(info));

		((CBasePlayerWeapon*)pEntity)->m_pPlayer = pWeaponOwner;

		((CBasePlayerWeapon*)pEntity)->GetItemInfo(&info);

		CBasePlayerItem::ItemInfoArray[info.iId] = info;

		const char* weaponName = ((info.iFlags & ITEM_FLAG_EXHAUSTIBLE) != 0) ? STRING(pEntity->pev->classname) : nullptr;

		if (info.pszAmmo1 && '\0' != *info.pszAmmo1)
		{
			AddAmmoNameToAmmoRegistry(info.pszAmmo1, weaponName);
		}

		if (info.pszAmmo2 && '\0' != *info.pszAmmo2)
		{
			AddAmmoNameToAmmoRegistry(info.pszAmmo2, weaponName);
		}

		g_pWpns[info.iId] = (CBasePlayerWeapon*)pEntity;
	}
}

/*
=====================
CBaseEntity:: Killed

If weapons code "kills" an entity, just set its effects to EF_NODRAW
=====================
*/
void CBaseEntity::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->effects |= EF_NODRAW;
}

/*
=====================
CBasePlayerWeapon:: DefaultDeploy

=====================
*/
// orig client 0x39730 (as SDK 2.3)
bool CBasePlayerWeapon::DefaultDeploy(const char* szViewModel, const char* szWeaponModel, int iAnim, const char* szAnimExt, int body)
{
	if (!CanDeploy())
		return false;

	gEngfuncs.CL_LoadModel(szViewModel, &m_pPlayer->pev->viewmodel);

	SendWeaponAnim(iAnim, body);

	g_irunninggausspred = false;
	m_pPlayer->m_flNextAttack = 0.5;
	m_flTimeWeaponIdle = 1.0;
	return true;
}

/*
=====================
CBasePlayerWeapon:: PlayEmptySound

=====================
*/
// orig client 0x397c0 (as SDK 2.3)
bool CBasePlayerWeapon::PlayEmptySound()
{
	if (m_iPlayEmptySound)
	{
		HUD_PlaySound("weapons/357_cock1.wav", 0.8);
		m_iPlayEmptySound = false;
		return false;
	}
	return false;
}

/*
=====================
CBasePlayerWeapon::Holster

Put away weapon
=====================
*/
// orig client 0x39820 (as SDK 2.3)
void CBasePlayerWeapon::Holster()
{
	m_fInReload = false; // cancel any reload in progress.
	g_irunninggausspred = false;
	m_pPlayer->pev->viewmodel = 0;
}

/*
=====================
CBasePlayerWeapon::SendWeaponAnim

Animate weapon model
=====================
*/
// orig client 0x39850 (as SDK 2.3; its skiplocal argument is halflife-updated's to drop)
void CBasePlayerWeapon::SendWeaponAnim(int iAnim, int body)
{
	m_pPlayer->pev->weaponanim = iAnim;

	HUD_SendWeaponAnim(iAnim, body, false);
}

/*
=====================
CBaseEntity::FireBulletsPlayer

Only produces random numbers to match the server ones.
=====================
*/
Vector CBaseEntity::FireBulletsPlayer(unsigned int cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t* pevAttacker, int shared_rand)
{
	float x = 0, y = 0, z;

	for (unsigned int iShot = 1; iShot <= cShots; iShot++)
	{
		if (pevAttacker == NULL)
		{
			// get circular gaussian spread
			do
			{
				x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);
		}
		else
		{
			//Use player's random seed.
			// get circular gaussian spread
			x = UTIL_SharedRandomFloat(shared_rand + iShot, -0.5, 0.5) + UTIL_SharedRandomFloat(shared_rand + (1 + iShot), -0.5, 0.5);
			y = UTIL_SharedRandomFloat(shared_rand + (2 + iShot), -0.5, 0.5) + UTIL_SharedRandomFloat(shared_rand + (3 + iShot), -0.5, 0.5);
			z = x * x + y * y;
		}
	}

	return Vector(x * vecSpread.x, y * vecSpread.y, 0.0);
}

/*
=====================
CBasePlayer::SelectItem

  Switch weapons
=====================
*/
void CBasePlayer::SelectItem(const char* pstr)
{
	if (!pstr)
		return;

	CBasePlayerItem* pItem = NULL;

	if (!pItem)
		return;


	if (pItem == m_pActiveItem)
		return;

	if (m_pActiveItem)
		m_pActiveItem->Holster();

	m_pLastItem = m_pActiveItem;
	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->Deploy();
	}
}

/*
=====================
CBasePlayer::Killed

=====================
*/
// orig client 0x39e90 (as SDK 2.3)
void CBasePlayer::Killed(entvars_t* pevAttacker, int iGib)
{
	// Holster weapon immediately, to allow it to cleanup
	if (m_pActiveItem)
		m_pActiveItem->Holster();

	g_irunninggausspred = false;
}

/*
=====================
CBasePlayer::Spawn

=====================
*/
// orig client 0x39ec0 (as SDK 2.3)
void CBasePlayer::Spawn()
{
	if (m_pActiveItem)
		m_pActiveItem->Deploy();

	g_irunninggausspred = false;
}

/*
=====================
UTIL_TraceLine

Don't actually trace, but act like the trace didn't hit anything.
=====================
*/
void UTIL_TraceLine(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, edict_t* pentIgnore, TraceResult* ptr)
{
	memset(ptr, 0, sizeof(*ptr));
	ptr->flFraction = 1.0;
}

/*
=====================
UTIL_ParticleBox

For debugging, draw a box around a player made out of particles
=====================
*/
void UTIL_ParticleBox(CBasePlayer* player, float* mins, float* maxs, float life, unsigned char r, unsigned char g, unsigned char b)
{
	int i;
	Vector mmin, mmax;

	for (i = 0; i < 3; i++)
	{
		mmin[i] = player->pev->origin[i] + mins[i];
		mmax[i] = player->pev->origin[i] + maxs[i];
	}

	gEngfuncs.pEfxAPI->R_ParticleBox((float*)&mmin, (float*)&mmax, 5.0, 0, 255, 0);
}

/*
=====================
UTIL_ParticleBoxes

For debugging, draw boxes for other collidable players
=====================
*/
void UTIL_ParticleBoxes()
{
	int idx;
	physent_t* pe;
	cl_entity_t* player;
	Vector mins, maxs;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	player = gEngfuncs.GetLocalPlayer();
	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(player->index - 1);

	for (idx = 1; idx < 100; idx++)
	{
		pe = gEngfuncs.pEventAPI->EV_GetPhysent(idx);
		if (!pe)
			break;

		if (pe->info >= 1 && pe->info <= gEngfuncs.GetMaxClients())
		{
			mins = pe->origin + pe->mins;
			maxs = pe->origin + pe->maxs;

			gEngfuncs.pEfxAPI->R_ParticleBox((float*)&mins, (float*)&maxs, 0, 0, 255, 2.0);
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

/*
=====================
UTIL_ParticleLine

For debugging, draw a line made out of particles
=====================
*/
void UTIL_ParticleLine(CBasePlayer* player, float* start, float* end, float life, unsigned char r, unsigned char g, unsigned char b)
{
	gEngfuncs.pEfxAPI->R_ParticleLine(start, end, r, g, b, life);
}

/*
=====================
HUD_InitClientWeapons

Set up weapons, player and functions needed to run weapons code client-side.
=====================
*/
// orig client 0x3a2a0 (as SDK 2.3)
void HUD_InitClientWeapons()
{
	static bool initialized = false;
	if (initialized)
		return;

	initialized = true;

	// Set up pointer ( dummy object )
	gpGlobals = &Globals;

	// Fill in current time ( probably not needed )
	gpGlobals->time = gEngfuncs.GetClientTime();

	// Fake functions
	g_engfuncs.pfnPrecacheModel = stub_PrecacheModel;
	g_engfuncs.pfnPrecacheSound = stub_PrecacheSound;
	g_engfuncs.pfnPrecacheEvent = stub_PrecacheEvent;
	g_engfuncs.pfnNameForFunction = stub_NameForFunction;
	g_engfuncs.pfnSetModel = stub_SetModel;
	g_engfuncs.pfnSetClientMaxspeed = HUD_SetMaxSpeed;
	// RC2's (orig client 0x9b8b0): its weapons call ENTINDEX on the client (the crowbar's and
	// hornetgun's charge events, the gauss); without it the crowbar's Deploy called through a null
	// pointer and crashed the client (found in play, 2026-11-07)
	g_engfuncs.pfnIndexOfEdict = stub_IndexOfEdict;
	// and INDEXENT (the gauss's pull loop): no entities on the client
	g_engfuncs.pfnPEntityOfEntIndex = stub_PEntityOfEntIndex;

	// Handled locally
	g_engfuncs.pfnPlaybackEvent = HUD_PlaybackEvent;
	g_engfuncs.pfnAlertMessage = AlertMessage;

	// Pass through to engine
	g_engfuncs.pfnPrecacheEvent = gEngfuncs.pfnPrecacheEvent;
	g_engfuncs.pfnRandomFloat = gEngfuncs.pfnRandomFloat;
	g_engfuncs.pfnRandomLong = gEngfuncs.pfnRandomLong;
	g_engfuncs.pfnCVarGetPointer = gEngfuncs.pfnGetCvarPointer;
	g_engfuncs.pfnCVarGetString = gEngfuncs.pfnGetCvarString;
	g_engfuncs.pfnCVarGetFloat = gEngfuncs.pfnGetCvarFloat;

	// Allocate a slot for the local player
	HUD_PrepEntity(&player, NULL);

	// Allocate slot(s) for each weapon that we are going to be predicting
	HUD_PrepEntity(&g_Glock, &player);
	HUD_PrepEntity(&g_Crowbar, &player);
	HUD_PrepEntity(&g_Python, &player);
	HUD_PrepEntity(&g_Mp5, &player);
	HUD_PrepEntity(&g_Crossbow, &player);
	HUD_PrepEntity(&g_Shotgun, &player);
	HUD_PrepEntity(&g_Rpg, &player);
	HUD_PrepEntity(&g_Gauss, &player);
	HUD_PrepEntity(&g_Egon, &player);
	HUD_PrepEntity(&g_HGun, &player);
	HUD_PrepEntity(&g_HandGren, &player);
	HUD_PrepEntity(&g_Satchel, &player);
	HUD_PrepEntity(&g_Tripmine, &player);
	HUD_PrepEntity(&g_Snark, &player);
}

/*
=====================
HUD_GetLastOrg

Retruns the last position that we stored for egon beam endpoint.
=====================
*/
void HUD_GetLastOrg(float* org)
{
	int i;

	// Return last origin
	for (i = 0; i < 3; i++)
	{
		org[i] = previousorigin[i];
	}
}

/*
=====================
HUD_SetLastOrg

Remember our exact predicted origin so we can draw the egon to the right position.
=====================
*/
void HUD_SetLastOrg()
{
	int i;

	// Offset final origin by view_offset
	for (i = 0; i < 3; i++)
	{
		previousorigin[i] = g_finalstate->playerstate.origin[i] + g_finalstate->client.view_ofs[i];
	}
}

/*
=====================
HUD_WeaponsPostThink

Run Weapon firing code on client
=====================
*/
// orig client 0x3a530. RC2's is SDK 2.3's with: the local player's energy and ability bits and
// the player it rides, from the HUD's copies; the tertiary and quaternary attack timers carried in
// weapon_data_t's m_flPumpTime and fuser4 (as the server's GetWeaponData sends them); a WeaponSync
// message applied to the weapon before it runs; no spectator (g_iUser1) check; no RPG laser spot
// state. halflife-updated's clock, frametime, v_angle and origin copies are kept; its per-weapon
// SetWeaponData/GetWeaponData/DecrementTimers hooks are left out, as RC2's server leaves them out.
void HUD_WeaponsPostThink(local_state_s* from, local_state_s* to, usercmd_t* cmd, double time, unsigned int random_seed)
{
	int i;
	int buttonsChanged;
	CBasePlayerWeapon* pWeapon = NULL;
	CBasePlayerWeapon* pCurrent;
	weapon_data_t nulldata, *pfrom, *pto;
	static int lasthealth;

	memset(&nulldata, 0, sizeof(nulldata));

	// Get current clock
	//Use actual time instead of prediction frame time because that time value breaks anything that uses absolute time values.
	gpGlobals->time = gEngfuncs.GetClientTime(); //time;

	//Lets weapons code use frametime to decrement timers and stuff.
	gpGlobals->frametime = cmd->msec / 1000.0f;

	// Fill in data based on selected weapon
	// FIXME, make this a method in each weapon?  where you pass in an entity_state_t *?
	switch (from->client.m_iId)
	{
	case WEAPON_CROWBAR:
		pWeapon = &g_Crowbar;
		break;

	case WEAPON_GLOCK:
		pWeapon = &g_Glock;
		break;

	case WEAPON_PYTHON:
		pWeapon = &g_Python;
		break;

	case WEAPON_MP5:
		pWeapon = &g_Mp5;
		break;

	case WEAPON_CROSSBOW:
		pWeapon = &g_Crossbow;
		break;

	case WEAPON_SHOTGUN:
		pWeapon = &g_Shotgun;
		break;

	case WEAPON_RPG:
		pWeapon = &g_Rpg;
		break;

	case WEAPON_GAUSS:
		pWeapon = &g_Gauss;
		break;

	case WEAPON_EGON:
		pWeapon = &g_Egon;
		break;

	case WEAPON_HORNETGUN:
		pWeapon = &g_HGun;
		break;

	case WEAPON_HANDGRENADE:
		pWeapon = &g_HandGren;
		break;

	case WEAPON_SATCHEL:
		pWeapon = &g_Satchel;
		break;

	case WEAPON_TRIPMINE:
		pWeapon = &g_Tripmine;
		break;

	case WEAPON_SNARK:
		pWeapon = &g_Snark;
		break;
	}

	// Store pointer to our destination entity_state_t so we can get our origin, etc. from it
	//  for setting up events on the client
	g_finalstate = to;

	// If we are running events/etc. go ahead and see if we
	//  managed to die between last frame and this one
	// If so, run the appropriate player killed or spawn function
	if (g_runfuncs)
	{
		if (to->client.health <= 0 && lasthealth > 0)
		{
			player.Killed(NULL, 0);
		}
		else if (to->client.health > 0 && lasthealth <= 0)
		{
			player.Spawn();
		}

		lasthealth = to->client.health;
	}

	// We are not predicting the current weapon, just bow out here.
	if (!pWeapon)
		return;

	// Rocket Crowbar 2: the local player's energy and attribute ability bits. VERIFY: read
	// literally (pEventAPI slot 4, EV_IsLocal, on number - 1); what sets the HUD's copies is c2's
	if (!gEngfuncs.pEventAPI->EV_IsLocal(from->playerstate.number - 1))
	{
		player.m_flEnergy = 100;
	}
	else
	{
		player.m_flEnergy = gHUD.m_Energy.GetEnergy(); // orig client .data 0x1a8004, the energy HUD's value
		player.m_iAbilities = PM_GetAbilityFlags(from->playerstate.number);
	}

	for (i = 0; i < MAX_WEAPONS; i++)
	{
		pCurrent = g_pWpns[i];
		if (!pCurrent)
		{
			continue;
		}

		pfrom = &from->weapondata[i];

		pCurrent->m_fInReload = 0 != pfrom->m_fInReload;
		pCurrent->m_fInSpecialReload = pfrom->m_fInSpecialReload;
		//		pCurrent->m_flPumpTime			= pfrom->m_flPumpTime;
		pCurrent->m_iClip = pfrom->m_iClip;
		pCurrent->m_flNextPrimaryAttack = pfrom->m_flNextPrimaryAttack;
		pCurrent->m_flNextSecondaryAttack = pfrom->m_flNextSecondaryAttack;
		pCurrent->m_flNextTertiaryAttack = pfrom->m_flPumpTime;
		pCurrent->m_flNextQuaternaryAttack = pfrom->fuser4;
		pCurrent->m_flTimeWeaponIdle = pfrom->m_flTimeWeaponIdle;
		pCurrent->pev->fuser1 = pfrom->fuser1;
		pCurrent->m_flStartThrow = pfrom->fuser2;
		pCurrent->m_flReleaseThrow = pfrom->fuser3;
		pCurrent->m_chargeReady = pfrom->iuser1;
		pCurrent->m_fInAttack = pfrom->iuser2;
		pCurrent->m_fireState = pfrom->iuser3;

		pCurrent->m_iSecondaryAmmoType = (int)from->client.vuser3[2];
		pCurrent->m_iPrimaryAmmoType = (int)from->client.vuser4[0];
		player.m_rgAmmo[pCurrent->m_iPrimaryAmmoType] = (int)from->client.vuser4[1];
		player.m_rgAmmo[pCurrent->m_iSecondaryAmmoType] = (int)from->client.vuser4[2];
	}

	// For random weapon events, use this seed to seed random # generator
	player.random_seed = random_seed;

	// Get old buttons from previous state.
	player.m_afButtonLast = from->playerstate.oldbuttons;

	// Which buttsons chave changed
	buttonsChanged = (player.m_afButtonLast ^ cmd->buttons); // These buttons have changed this frame

	// Debounced button codes for pressed/released
	// The changed ones still down are "pressed"
	player.m_afButtonPressed = buttonsChanged & cmd->buttons;
	// The ones not down are "released"
	player.m_afButtonReleased = buttonsChanged & (~cmd->buttons);
	player.pev->v_angle = cmd->viewangles;
	player.pev->origin = from->client.origin;

	// Set player variables that weapons code might check/alter
	player.pev->button = cmd->buttons;

	player.pev->velocity = from->client.velocity;
	player.pev->flags = from->client.flags;

	player.pev->deadflag = from->client.deadflag;
	player.pev->waterlevel = from->client.waterlevel;
	player.pev->maxspeed = from->client.maxspeed;
	player.pev->fov = from->client.fov; // RC2 (as SDK 2.3): pev->fov, which the crossbow's zoom reads
	player.pev->weaponanim = from->client.weaponanim;
	player.pev->viewmodel = from->client.viewmodel;
	player.m_flNextAttack = from->client.m_flNextAttack;
	player.m_flNextAmmoBurn = from->client.fuser2;
	player.m_flAmmoStartCharge = from->client.fuser3;

	//Stores all our ammo info, so the client side weapons can use them.
	player.ammo_9mm = (int)from->client.vuser1[0];
	player.ammo_357 = (int)from->client.vuser1[1];
	player.ammo_argrens = (int)from->client.vuser1[2];
	player.ammo_bolts = (int)from->client.ammo_nails; //is an int anyways...
	player.ammo_buckshot = (int)from->client.ammo_shells;
	player.ammo_uranium = (int)from->client.ammo_cells;
	player.ammo_hornets = (int)from->client.vuser2[0];
	player.ammo_rockets = (int)from->client.ammo_rockets;

	// Rocket Crowbar 2: the player this one rides (the HUD's per-player byte, its top bit dropped).
	// VERIFY: the byte table's meaning (orig client .data 0x20d5b8, written by a c2/c3 message handler)
	player.m_iRidingIndex = *PM_GetRideInfo(from->playerstate.number) & 0x7f;

	// Point to current weapon object
	if (WEAPON_NONE != from->client.m_iId)
	{
		player.m_pActiveItem = g_pWpns[from->client.m_iId];
	}

	// Don't go firing anything if we have died or are spectating
	// Or if we don't have a weapon model deployed
	if ((player.pev->deadflag != (DEAD_DISCARDBODY + 1)) &&
		!CL_IsDead() && 0 != player.pev->viewmodel)
	{
		RC2_ApplyWeaponSync(pWeapon);

		if (player.m_flNextAttack <= 0)
		{
			pWeapon->ItemPostFrame();
		}
	}

	// Assume that we are not going to switch weapons
	to->client.m_iId = from->client.m_iId;

	// Now see if we issued a changeweapon command ( and we're not dead )
	if (0 != cmd->weaponselect && (player.pev->deadflag != (DEAD_DISCARDBODY + 1)))
	{
		// Switched to a different weapon?
		if (from->weapondata[cmd->weaponselect].m_iId == cmd->weaponselect)
		{
			CBasePlayerWeapon* pNew = g_pWpns[cmd->weaponselect];
			if (pNew && (pNew != pWeapon))
			{
				// Put away old weapon
				if (player.m_pActiveItem)
					player.m_pActiveItem->Holster();

				player.m_pLastItem = player.m_pActiveItem;
				player.m_pActiveItem = pNew;

				// Deploy new weapon
				if (player.m_pActiveItem)
				{
					player.m_pActiveItem->Deploy();
				}

				// Update weapon id so we can predict things correctly.
				to->client.m_iId = cmd->weaponselect;
			}
		}
	}

	// Copy in results of prediction code
	to->client.viewmodel = player.pev->viewmodel;
	to->client.fov = player.pev->fov;
	to->client.weaponanim = player.pev->weaponanim;
	to->client.m_flNextAttack = player.m_flNextAttack;
	to->client.fuser2 = player.m_flNextAmmoBurn;
	to->client.fuser3 = player.m_flAmmoStartCharge;
	to->client.maxspeed = player.pev->maxspeed;

	//HL Weapons
	to->client.vuser1[0] = player.ammo_9mm;
	to->client.vuser1[1] = player.ammo_357;
	to->client.vuser1[2] = player.ammo_argrens;

	to->client.ammo_nails = player.ammo_bolts;
	to->client.ammo_shells = player.ammo_buckshot;
	to->client.ammo_cells = player.ammo_uranium;
	to->client.vuser2[0] = player.ammo_hornets;
	to->client.ammo_rockets = player.ammo_rockets;

	// Make sure that weapon animation matches what the game .dll is telling us
	//  over the wire ( fixes some animation glitches )
	if (g_runfuncs && (HUD_GetWeaponAnim() != to->client.weaponanim))
	{
		// RC2 (as SDK 2.3): body 2, 0 for the tripmine, 1 for the python in multiplayer
		int body = 2;

		if (pWeapon == &g_Tripmine)
			body = 0;

		if (pWeapon == &g_Python && bIsMultiplayer())
			body = 1;

		// Force a fixed anim down to viewmodel
		HUD_SendWeaponAnim(to->client.weaponanim, body, true);
	}

	for (i = 0; i < MAX_WEAPONS; i++)
	{
		pCurrent = g_pWpns[i];

		pto = &to->weapondata[i];

		if (!pCurrent)
		{
			memset(pto, 0, sizeof(weapon_data_t));
			continue;
		}

		pto->m_fInReload = static_cast<int>(pCurrent->m_fInReload);
		pto->m_fInSpecialReload = pCurrent->m_fInSpecialReload;
		//		pto->m_flPumpTime				= pCurrent->m_flPumpTime;
		pto->m_iClip = pCurrent->m_iClip;
		pto->m_flNextPrimaryAttack = pCurrent->m_flNextPrimaryAttack;
		pto->m_flNextSecondaryAttack = pCurrent->m_flNextSecondaryAttack;
		pto->m_flPumpTime = pCurrent->m_flNextTertiaryAttack;
		pto->fuser4 = pCurrent->m_flNextQuaternaryAttack;
		pto->m_flTimeWeaponIdle = pCurrent->m_flTimeWeaponIdle;
		pto->fuser1 = pCurrent->pev->fuser1;
		pto->fuser2 = pCurrent->m_flStartThrow;
		pto->fuser3 = pCurrent->m_flReleaseThrow;
		pto->iuser1 = pCurrent->m_chargeReady;
		pto->iuser2 = pCurrent->m_fInAttack;
		pto->iuser3 = pCurrent->m_fireState;

		// Decrement weapon counters, server does this at same time ( during post think, after doing everything else )
		pto->m_flNextReload -= cmd->msec / 1000.0;
		pto->m_fNextAimBonus -= cmd->msec / 1000.0;
		pto->m_flNextPrimaryAttack -= cmd->msec / 1000.0;
		pto->m_flNextSecondaryAttack -= cmd->msec / 1000.0;
		pto->m_flPumpTime -= cmd->msec / 1000.0;
		pto->fuser4 -= cmd->msec / 1000.0;
		pto->m_flTimeWeaponIdle -= cmd->msec / 1000.0;
		pto->fuser1 -= cmd->msec / 1000.0;

		to->client.vuser3[2] = pCurrent->m_iSecondaryAmmoType;
		to->client.vuser4[0] = pCurrent->m_iPrimaryAmmoType;
		to->client.vuser4[1] = player.m_rgAmmo[pCurrent->m_iPrimaryAmmoType];
		to->client.vuser4[2] = player.m_rgAmmo[pCurrent->m_iSecondaryAmmoType];

		/*		if ( pto->m_flPumpTime != -9999 )
		{
			pto->m_flPumpTime -= cmd->msec / 1000.0;
			if ( pto->m_flPumpTime < -0.001 )
				pto->m_flPumpTime = -0.001;
		}*/

		if (pto->m_fNextAimBonus < -1.0)
		{
			pto->m_fNextAimBonus = -1.0;
		}

		if (pto->m_flNextPrimaryAttack < -1.0)
		{
			pto->m_flNextPrimaryAttack = -1.0;
		}

		if (pto->m_flNextSecondaryAttack < -0.001)
		{
			pto->m_flNextSecondaryAttack = -0.001;
		}

		if (pto->m_flPumpTime < -0.001)
		{
			pto->m_flPumpTime = -0.001;
		}

		if (pto->fuser4 < -0.001)
		{
			pto->fuser4 = -0.001;
		}

		if (pto->m_flTimeWeaponIdle < -0.001)
		{
			pto->m_flTimeWeaponIdle = -0.001;
		}

		if (pto->m_flNextReload < -0.001)
		{
			pto->m_flNextReload = -0.001;
		}

		if (pto->fuser1 < -0.001)
		{
			pto->fuser1 = -0.001;
		}
	}

	// m_flNextAttack is now part of the weapons, but is part of the player instead
	to->client.m_flNextAttack -= cmd->msec / 1000.0;
	if (to->client.m_flNextAttack < -0.001)
	{
		to->client.m_flNextAttack = -0.001;
	}

	to->client.fuser2 -= cmd->msec / 1000.0;
	if (to->client.fuser2 < -0.001)
	{
		to->client.fuser2 = -0.001;
	}

	to->client.fuser3 -= cmd->msec / 1000.0;
	if (to->client.fuser3 < -0.001)
	{
		to->client.fuser3 = -0.001;
	}

	// Store off the last position from the predicted state.
	HUD_SetLastOrg();

	// Wipe it so we can't use it after this frame
	g_finalstate = NULL;
}

/*
=====================
HUD_PostRunCmd

Client calls this during prediction, after it has moved the player and updated any info changed into to->
time is the current client clock based on prediction
cmd is the command that caused the movement, etc
runfuncs is 1 if this is the first time we've predicted this command.  If so, sounds and effects should play, otherwise, they should
be ignored
=====================
*/
// orig client 0x3ba90 (as SDK 2.3; halflife-updated's HUD_InitClientWeapons call and weapon id kept)
void DLLEXPORT HUD_PostRunCmd(struct local_state_s* from, struct local_state_s* to, struct usercmd_s* cmd, int runfuncs, double time, unsigned int random_seed)
{
	//	RecClPostRunCmd(from, to, cmd, runfuncs, time, random_seed);

	g_runfuncs = runfuncs != 0;

	//Event code depends on this stuff, so always initialize it.
	HUD_InitClientWeapons();

#if defined(CLIENT_WEAPONS)
	if (cl_lw && 0 != cl_lw->value)
	{
		HUD_WeaponsPostThink(from, to, cmd, time, random_seed);
	}
	else
#endif
	{
		to->client.fov = g_lastFOV;
	}

	if (g_irunninggausspred)
	{
		Vector forward;
		gEngfuncs.pfnAngleVectors(v_angles, forward, NULL, NULL);
		to->client.velocity = to->client.velocity - forward * g_flApplyVel * 5;
		g_irunninggausspred = false;
	}

	// All games can use FOV state
	g_lastFOV = to->client.fov;
	g_CurrentWeaponId = to->client.m_iId;
}

/*
=====================
CBasePlayerWeapon::ItemPostFrame

The client's (orig client 0x39a40). SDK 2.3's client ItemPostFrame with RC2's two extra attacks
(Quaternary on +alt1, Tertiary on +cancel, before the secondary and the primary, each on its own
timer), WeaponFrame first every frame, a frozen player's attack buttons dropped, attacks held while
the HUD's hide bit 0x10 (shrunk or transformed) is set, and the auto-reload waiting for all four timers. A finished reload
fills the clip from the ammo (halflife-updated's fix of SDK 2.3's "m_iClip += 10"); the timers are
checked with halflife-updated's CanAttack (RC2's compares them with 0 directly).
=====================
*/
void CBasePlayerWeapon::ItemPostFrame()
{
	if (0 != g_iLocalPlayerFrozen)
	{
		m_pPlayer->pev->button &= ~(IN_ATTACK | IN_ATTACK2 | IN_CANCEL | IN_ALT1);
		WeaponFrame();
		return;
	}

	WeaponFrame();

	if ((m_fInReload) && (m_pPlayer->m_flNextAttack <= 0.0))
	{
		int j = V_min(iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;
		m_fInReload = false;
	}

	// HideWeapon bit 0x10, which RC2's server sets while the player is shrunk or transformed
	// (ammo.cpp's HIDEHUD_RC2_NOWEAPONS): nothing fires or idles
	if ((gHUD.m_iHideHUDDisplay & 0x10) != 0)
		return;

	if ((m_pPlayer->pev->button & IN_ALT1) != 0 && CanAttack(m_flNextQuaternaryAttack, gpGlobals->time, UseDecrement()))
	{
		QuaternaryAttack();
		m_pPlayer->pev->button &= ~IN_ALT1;
	}
	else if ((m_pPlayer->pev->button & IN_CANCEL) != 0 && CanAttack(m_flNextTertiaryAttack, gpGlobals->time, UseDecrement()))
	{
		TertiaryAttack();
		m_pPlayer->pev->button &= ~IN_CANCEL;
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK2) != 0 && CanAttack(m_flNextSecondaryAttack, gpGlobals->time, UseDecrement()))
	{
		if (pszAmmo2() && 0 == m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()])
		{
			m_fFireOnEmpty = true;
		}

		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK) != 0 && CanAttack(m_flNextPrimaryAttack, gpGlobals->time, UseDecrement()))
	{
		if ((m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && 0 == m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()]))
		{
			m_fFireOnEmpty = true;
		}

		PrimaryAttack();
	}
	else if ((m_pPlayer->pev->button & IN_RELOAD) != 0 && iMaxClip() != WEAPON_NOCLIP && !m_fInReload)
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ((m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2 | IN_CANCEL | IN_ALT1)) == 0)
	{
		// no fire buttons down
		m_fFireOnEmpty = false;

		// weapon is useable. Reload if empty and every attack has waited as long as it has to
		if (m_iClip == 0 && (iFlags() & ITEM_FLAG_NOAUTORELOAD) == 0 && m_flNextPrimaryAttack < 0.0 && m_flNextSecondaryAttack < 0.0 && m_flNextTertiaryAttack < 0.0 && m_flNextQuaternaryAttack < 0.0)
		{
			Reload();
			return;
		}

		WeaponIdle();
		return;
	}

	// catch all
	if (ShouldWeaponIdle())
	{
		WeaponIdle();
	}
}

// orig client 0x39f60 UTIL_ExtendTime: util.h's inline, which the client has out of line here.

// Rocket Crowbar 2's client copies of the server's direction helpers (util.cpp there), which the
// predicted weapons call. Bodies as the server's.
// orig client 0x3b400
int UTIL_PackNormal(Vector vecDir)
{
	const Vector n = vecDir.Normalize();

	int x = (int)(fabs(n.x) * 511.0);
	if (n.x < 0)
		x |= 0x200;
	int y = (int)(fabs(n.y) * 511.0);
	if (n.y < 0)
		y |= 0x200;
	int z = (int)(fabs(n.z) * 511.0);
	if (n.z < 0)
		z |= 0x200;

	return z << 20 | x | y << 10;
}

// orig client 0x3b560
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal)
{
	const float flDot = -DotProduct(vecDir, vecNormal);
	return vecNormal * 2 * flDot + vecDir;
}

// orig client 0x3b630
Vector UTIL_RandomSpreadDir(Vector vecDir, Vector vecRight, Vector vecUp, float flSpread)
{
	float x, y;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
	} while (x * x + y * y > 1);

	return (vecDir + vecRight * (x * flSpread) + vecUp * (y * flSpread)).Normalize();
}

// Rocket Crowbar 2: the WeaponSync message (the server's CBasePlayerWeapon::WeaponFrame sends it,
// the weapon id then its state), kept until the weapon it names runs. Names ours.
static unsigned char g_rgucRC2WeaponSync[256]; // orig client .data 0x17c44c
static int g_iRC2WeaponSyncSize = 0;		   // orig client .data 0x1843f8

// orig client 0x3b880: hand a pending WeaponSync to its weapon's ReadWeaponState
static void RC2_ApplyWeaponSync(CBasePlayerWeapon* pWeapon)
{
	if (!pWeapon || g_iRC2WeaponSyncSize <= 1)
		return;

	ItemInfo info;
	pWeapon->GetItemInfo(&info);

	if (g_rgucRC2WeaponSync[0] == info.iId)
	{
		pWeapon->ReadWeaponState(&g_rgucRC2WeaponSync[1], g_iRC2WeaponSyncSize - 1);
		g_iRC2WeaponSyncSize = 0;
	}
}

// orig client 0x3b900. Its HOOK_MESSAGE is in the HUD's init (orig client 0xa148c, c2's).
int __MsgFunc_WeaponSync(const char* pszName, int iSize, void* pbuf)
{
	// Original bug (crash), guarded: the original copies iSize bytes into its 256-byte buffer
	// with no bound (the server sends a few)
	if (iSize > (int)sizeof(g_rgucRC2WeaponSync))
		iSize = sizeof(g_rgucRC2WeaponSync);
	memcpy(g_rgucRC2WeaponSync, pbuf, iSize);
	g_iRC2WeaponSyncSize = iSize;
	return 1;
}

// The client's readers for ReadWeaponState (weapons.h; the server's return 0). Original oddity,
// kept: each checks *piSize but never lowers it, so a reader can run past the message.
// orig client 0x3b940
unsigned char RC2ReadStateByte(void** ppData, int* piSize)
{
	if (*piSize < 1)
		return 0;
	unsigned char* p = (unsigned char*)*ppData;
	*ppData = p + 1;
	return p[0];
}

// orig client 0x3b970
short RC2ReadStateShort(void** ppData, int* piSize)
{
	if (*piSize < 2)
		return 0;
	unsigned char* p = (unsigned char*)*ppData;
	*ppData = p + 2;
	return (short)(p[0] + (p[1] << 8));
}

// orig client 0x3b9b0
int RC2ReadStateInt(void** ppData, int* piSize)
{
	if (*piSize < 4)
		return 0;
	unsigned char* p = (unsigned char*)*ppData;
	*ppData = p + 4;
	return p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
}

// orig client 0x3ba20
float RC2ReadStateFloat(void** ppData, int* piSize)
{
	if ((unsigned int)*piSize < 4)
		return 0.0f;
	float* p = (float*)*ppData;
	*ppData = p + 1;
	return *p;
}

// orig client 0x3ba60: attribute flag 0x200 (the player may fire under water). The server's reads
// it by entity index (player.cpp, orig 0x95860); the client's from the predicted player.
bool PlayerHasAttrFlag200(CBaseEntity* pPlayer)
{
	return ((((CBasePlayer*)pPlayer)->m_iAbilities >> 9) & 1) != 0;
}

// Inline virtuals the original emitted as COMDATs here (bodies stock, from the headers):
//   orig client 0x3b190 CBaseMonster::Stop
//   orig client 0x3b360 CBaseMonster's RC2 slot 147 default (basemonster.h)
//   orig client 0x3bcf0 CTripmine::iItemSlot (5)
//   orig client 0x3be00 CTripmine::UseDecrement (true)
//   orig client 0x3be60 CBasePlayer::BodyTarget

