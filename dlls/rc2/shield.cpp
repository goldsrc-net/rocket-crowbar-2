/***
*
*	Rocket Crowbar 2: the player shield's deflection ([Shield] keys). RC2's code between
*	scripted.cpp and shotgun.cpp in link order, before the shotgun scientists; names ours.
*
*	The shield is an ellipsoid round the player (20 across and 40 high standing, 26 and 30
*	ducking). A hit on it costs energy in place of damage, shows a flash where it struck
*	(CLTE type 38) and, for projectiles, gives the point and direction to bounce off.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "rc2prefs.h"
#include "p7_extern.h"
#include <algorithm>

#define CLTE_SHIELDHIT 38 // name ours: byte player, byte yaw, byte height, byte effect

// Where a shot coming along vecDir meets the shield of a player, as an offset from the
// player's origin; g_vecZero for a miss. vecRel is the hit position relative to the origin.
static Vector ShieldIntersect(Vector vecRel, Vector vecDir, bool bDucking) // orig 0xc42c0
{
	// from 1024 units back along the shot, towards the player
	const Vector d = vecDir * -1;
	const Vector p = d * 1024 + vecRel;

	float a, b;
	if (!bDucking)
	{
		a = 20.0;
		b = 40.0;
	}
	else
	{
		a = 26.0;
		b = 30.0;
	}
	const float a2 = a * a;
	const float b2 = b * b;

	const float flDenom = a2 * d.z * d.z + (d.y * d.y + d.x * d.x) * b2;
	if (flDenom != 0)
	{
		const float f6 = p.z * a2 * d.z;
		const float f8 = d.y * p.y * b2;
		const float f5 = a2 * p.z * p.z;
		const float f4 = a2 - p.x * p.x;
		const float f3 = p.z * a2 * d.z * d.y * p.y + (f8 + f6) * d.x * p.x;
		const float flDisc = (f4 - p.y * p.y) * a2 * d.z * d.z + (f4 * b2 - f5) * d.y * d.y + ((a2 - p.y * p.y) * b2 - f5) * d.x * d.x + f3 + f3;
		if (flDisc >= 0)
		{
			const float t = (((sqrt(flDisc) * b - d.x * p.x * b2) - f8) - f6) / flDenom;
			return Vector(d.x * t + p.x, d.y * t + p.y, t * d.z + p.z);
		}
	}
	return g_vecZero;
}

// A point on a sphere-like shell: height flZ, flScale times the circle's relative radius
// there, round flYaw. The caller passes flYaw in degrees; cos/sin take radians.
static Vector ShieldShellPoint(float flYaw, float flZ, float flRadius, float flScale) // orig 0xc4530
{
	float r = 0;
	if (-flRadius < flZ && flZ < flRadius)
		r = (sqrt(flRadius * flRadius - flZ * flZ) / flRadius) * flScale;
	return Vector(r * cos(flYaw), r * sin(flYaw), flZ);
}

float ShieldDeflect(CBaseEntity* pEntity, float flDamage, float flEnergyScale, Vector vecHitPos, Vector vecDir, Vector* pvecDeflectPos, Vector* pvecDeflectDir, int iEffect) // orig 0xc45b0
{
	if (pvecDeflectPos != NULL)
		*pvecDeflectPos = vecHitPos;

	if (!pEntity->GetEffectedEnt()->IsActivePlayer())
		return flDamage;

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pEntity->GetEffectedEnt());
	if (!pPlayer->m_fShieldOn)
		return flDamage;

	Vector vecOffset = g_vecZero;
	if (vecHitPos != g_vecZero)
	{
		if (vecDir == g_vecZero)
			vecDir = (vecHitPos - pPlayer->pev->origin).Normalize();
		if (vecDir != g_vecZero)
			vecOffset = ShieldIntersect(vecHitPos - pPlayer->pev->origin, vecDir, (pPlayer->pev->flags & FL_DUCKING) != 0);
	}

	iEffect = std::clamp(iEffect, 0, 15);
	int iYaw = 0;
	int iHeight = 0;

	if (vecOffset == g_vecZero)
	{
		iEffect |= 0x10; // missed the shell: a plain flash
		if (pvecDeflectPos != NULL)
			*pvecDeflectPos = vecHitPos;
	}
	else
	{
		const Vector vecNormal = vecOffset.Normalize();
		const float flYaw = UTIL_VecToAngles(vecNormal).y;
		iYaw = (int)(flYaw * (255.0 / 360.0) + 0.5);
		iHeight = (int)(vecOffset.z * (127.0 / 40.0) + 127.0);

		if (pvecDeflectPos != NULL)
			*pvecDeflectPos = vecOffset + pPlayer->pev->origin;

		if (pvecDeflectDir != NULL)
		{
			float a = 20.0;
			float b = 40.0;
			if ((pPlayer->pev->flags & FL_DUCKING) != 0)
			{
				a = 26.0;
				b = 30.0;
			}
			const float flScale = a + 1.0;
			const float flSign = vecOffset.z <= 0 ? -1.0 : 1.0;
			// VERIFY: flYaw is in degrees and ShieldShellPoint's cos/sin read radians (as the original)
			const Vector vecShell = ShieldShellPoint(flYaw, (b / a) * (1.0 - sqrt(b * b - vecOffset.z * vecOffset.z) / b) * flSign + vecOffset.z, b / a + b, flScale);
			*pvecDeflectDir = (vecShell - vecOffset).Normalize();
		}
	}

	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, pPlayer->pev->origin);
	WRITE_BYTE(CLTE_SHIELDHIT);
	WRITE_BYTE(ENTINDEX(pEntity->edict()));
	WRITE_BYTE(std::clamp(iYaw, 0, 255));
	WRITE_BYTE(std::clamp(iHeight, 0, 255));
	WRITE_BYTE(iEffect);
	MESSAGE_END();

	float flCost = RC2Pref(RC2P_Shield_BaseEnergyScale).f * flEnergyScale;
	if (flCost == 0)
		flCost = 1.0;

	float flLeft = flDamage;
	if (pPlayer->m_flEnergy < flCost * flDamage)
	{
		if (flEnergyScale != 0)
			flLeft = flDamage - pPlayer->m_flEnergy / flCost;
		pPlayer->m_flEnergy = 0;
	}
	else
	{
		flLeft = 0;
		pPlayer->m_flEnergy -= flCost * flDamage;
	}

	if (pPlayer->m_flEnergy < 0)
		pPlayer->m_flEnergy = 0;
	if (pPlayer->m_flEnergy < RC2Pref(RC2P_Shield_MinRunEnergy).f)
		pPlayer->m_fShieldOn = false;
	if (flLeft < 0)
		flLeft = 0;

	return flLeft;
}
