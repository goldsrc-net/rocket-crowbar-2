/***
*
*	Rocket Crowbar 2 (client): the attribute prices, the client's copy of what the server's
*	setattr checks (dlls/client.cpp, the same source in both libraries: client.dll's copies are
*	instruction for instruction rc2.dll's 0x1cd50-0x1d360). The attribute menu
*	(vgui_attributes.cpp) prices its choices with them. All names ours.
*
****/

#include <cmath>
#include "hud.h"
#include "cl_util.h"
#include "rc2prefs.h"
#include "rc2/vgui_attributes.h"

// orig client 0xd1830. Points for attribute i, held to [0, max] (jump .. reflection) or [1, max] (speed,
// energy regeneration).
int Attr_ClampPoints(int i, int iPoints)
{
	int iMax;
	switch (i)
	{
	case 0: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxJumpPoints).c; break;
	case 1:
		iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxSpeedPoints).c;
		if (iPoints > iMax)
			return iMax;
		return iPoints < 1 ? 1 : iPoints;
	case 2: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxAttackPoints).c; break;
	case 3: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxDefendPoints).c; break;
	case 4: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxHealthPoints).c; break;
	case 5: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxExtraHealthPoints).c; break;
	case 6: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxArmorPoints).c; break;
	case 7: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxExtraArmorPoints).c; break;
	case 8:
		iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxEnergyRegenPoints).c;
		if (iPoints > iMax)
			return iMax;
		return iPoints < 1 ? 1 : iPoints;
	case 9: iMax = (unsigned char)RC2Pref(RC2P_Attributes_MaxDamageReflectionPoints).c; break;
	default: return iPoints;
	}
	if (iPoints > iMax)
		return iMax;
	return iPoints < 0 ? 0 : iPoints;
}

// orig client 0xd16d0. The value iPoints buy of attribute i.
float Attr_PointsToValue(int i, int iPoints)
{
	switch (i)
	{
	case 0: return iPoints / RC2Pref(RC2P_Attributes_JumpCost).f;
	case 1: return iPoints / RC2Pref(RC2P_Attributes_SpeedCost).f;
	case 2: return exp(iPoints * RC2Pref(RC2P_Attributes_AttackConstant).f) / RC2Pref(RC2P_Attributes_AttackScaler).f;
	case 3: return exp(-(iPoints * RC2Pref(RC2P_Attributes_DefendConstant).f)) * RC2Pref(RC2P_Attributes_DefendScaler).f;
	case 4: return iPoints / RC2Pref(RC2P_Attributes_HealthCost).f + RC2Pref(RC2P_Attributes_MinHealth).f;
	case 5: return iPoints / RC2Pref(RC2P_Attributes_ExtraHealthCost).f;
	case 6: return iPoints / RC2Pref(RC2P_Attributes_ArmorCost).f;
	case 7: return iPoints / RC2Pref(RC2P_Attributes_ExtraArmorCost).f;
	case 8: return iPoints / RC2Pref(RC2P_Attributes_EnergyRegenCost).f;
	case 9: return iPoints / RC2Pref(RC2P_Attributes_DamageReflectionCost).f;
	}
	return 0;
}

// orig client 0xd1430. The points a value of attribute i costs (the inverse, rounded, never below 0).
int Attr_ValueToPoints(int i, float flValue)
{
	double flPoints;
	switch (i)
	{
	case 0: flPoints = RC2Pref(RC2P_Attributes_JumpCost).f * flValue; break;
	case 1: flPoints = RC2Pref(RC2P_Attributes_SpeedCost).f * flValue; break;
	case 2: flPoints = log(RC2Pref(RC2P_Attributes_AttackScaler).f * flValue) / RC2Pref(RC2P_Attributes_AttackConstant).f; break;
	case 3: flPoints = log(flValue / RC2Pref(RC2P_Attributes_DefendScaler).f) / -RC2Pref(RC2P_Attributes_DefendConstant).f; break;
	case 4: flPoints = (flValue - RC2Pref(RC2P_Attributes_MinHealth).f) * RC2Pref(RC2P_Attributes_HealthCost).f; break;
	case 5: flPoints = RC2Pref(RC2P_Attributes_ExtraHealthCost).f * flValue; break;
	case 6: flPoints = RC2Pref(RC2P_Attributes_ArmorCost).f * flValue; break;
	case 7: flPoints = RC2Pref(RC2P_Attributes_ExtraArmorCost).f * flValue; break;
	case 8: flPoints = RC2Pref(RC2P_Attributes_EnergyRegenCost).f * flValue; break;
	case 9: flPoints = RC2Pref(RC2P_Attributes_DamageReflectionCost).f * flValue; break;
	default: return 0;
	}
	if (!(flPoints > 0.0))
		flPoints = 0.0;
	return static_cast<int>(flPoints + 0.5);
}

// orig client 0xd1940. The price of one ability bit.
int Attr_AbilityCost(unsigned int iBit)
{
	switch (iBit)
	{
	case 0x1: return (unsigned short)RC2Pref(RC2P_Attributes_WallhopPointCost).s;
	case 0x2: return (unsigned short)RC2Pref(RC2P_Attributes_AqualungPointCost).s;
	case 0x4: return (unsigned short)RC2Pref(RC2P_Attributes_JetboostPointCost).s;
	case 0x8: return (unsigned short)RC2Pref(RC2P_Attributes_JetpackPointCost).s;
	case 0x10: return (unsigned short)RC2Pref(RC2P_Attributes_EnergyDrawPointCost).s;
	case 0x20: return (unsigned short)RC2Pref(RC2P_Attributes_CreateHealthPointCost).s;
	case 0x40: return (unsigned short)RC2Pref(RC2P_Attributes_CrowbarControlPointCost).s;
	case 0x80: return (unsigned short)RC2Pref(RC2P_Attributes_ShieldPointCost).s;
	case 0x100: return (unsigned short)RC2Pref(RC2P_Attributes_LongJumpPointCost).s;
	case 0x200: return (unsigned short)RC2Pref(RC2P_Attributes_SubmersibleWeaponsPointCost).s;
	}
	return 0;
}

// orig client 0xd1a20: the ten ability bits' prices
int Attr_AbilitiesCost(unsigned int iBits)
{
	int iCost = 0;
	for (int i = 0; i < 10; i++)
	{
		if ((iBits & (1u << i)) != 0)
			iCost += Attr_AbilityCost(1u << i);
	}
	return iCost;
}

// orig client 0xd1a60. The price of one weapon bit.
int Attr_WeaponCost(unsigned int iBit)
{
	switch (iBit)
	{
	case 0x1: return (unsigned short)RC2Pref(RC2P_Attributes_GlockPointCost).s;
	case 0x2: return (unsigned short)RC2Pref(RC2P_Attributes_PythonPointCost).s;
	case 0x4: return (unsigned short)RC2Pref(RC2P_Attributes_MP5PointCost).s;
	case 0x8: return (unsigned short)RC2Pref(RC2P_Attributes_ShotgunPointCost).s;
	case 0x10: return (unsigned short)RC2Pref(RC2P_Attributes_CrossbowPointCost).s;
	case 0x20: return (unsigned short)RC2Pref(RC2P_Attributes_RPGPointCost).s;
	case 0x40: return (unsigned short)RC2Pref(RC2P_Attributes_EgonPointCost).s;
	case 0x80: return (unsigned short)RC2Pref(RC2P_Attributes_GaussPointCost).s;
	case 0x100: return (unsigned short)RC2Pref(RC2P_Attributes_HornetgunPointCost).s;
	}
	return 0;
}

// orig client 0xd1b30: the nine weapon bits' prices
int Attr_WeaponsCost(unsigned int iBits)
{
	int iCost = 0;
	for (int i = 0; i < 9; i++)
	{
		if ((iBits & (1u << i)) != 0)
			iCost += Attr_WeaponCost(1u << i);
	}
	return iCost;
}

// orig client 0xd1b70. The price of a transform (0-4; anything else is free).
int Attr_TransformCost(int iTransform)
{
	switch (iTransform)
	{
	case 0: return (unsigned short)RC2Pref(RC2P_Attributes_TransformAssassinPointCost).s;
	case 1: return (unsigned short)RC2Pref(RC2P_Attributes_TransformHoundeyePointCost).s;
	case 2: return (unsigned short)RC2Pref(RC2P_Attributes_TransformControllerPointCost).s;
	case 3: return (unsigned short)RC2Pref(RC2P_Attributes_TransformBatPointCost).s;
	case 4: return (unsigned short)RC2Pref(RC2P_Attributes_TransformHeadcrabPointCost).s;
	}
	return 0;
}

// orig client 0xd1c00. Attribute i's value when the points run out: 1 for jump, speed, attack, defend
// and energy regeneration, 100 health, nothing extra.
float Attr_DefaultValue(int i)
{
	switch (i)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 8:
		return 1.0;
	case 4:
		return 100.0;
	}
	return 0.0;
}
