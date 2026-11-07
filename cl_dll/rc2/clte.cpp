/***
*
*	Rocket Crowbar 2: the client temp entity manager (see clte.h): the "CLTE" message, the update
*	and draw passes, the origin cache, the per-player overrides, the loopback buffer and the shield
*	intersection the bullet events use.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "event_api.h"
#include "r_efx.h"
#include "Exports.h"
#include "clte.h"
#include "clte_types.h"

#include <stdio.h>
#include <string.h>

void VectorAngles(const float* forward, float* angles);

CClientTent* g_pClientTents[MAX_CLIENT_TENTS]; // orig .bss 0x101ee07c
unsigned int g_iClientFrame;				   // orig .bss 0x101f6a08
unsigned int g_iShieldPlayers;				   // orig .bss 0x101f69f4
int g_iLocalPlayerFrozen;					   // orig .bss 0x101f69f8 (clte.h says who uses these)
int g_iRC2LocalAntiFriction;				   // orig .bss 0x101f69fc
int g_bLocalTransforming;					   // orig .bss 0x101f6a00
unsigned char g_bEntityFrozen[1024];		   // orig .bss 0x101f24c4
unsigned char g_abRC2AntiFriction[1024];	   // orig .bss 0x101edc7c

Vector g_vecTentForward; // orig .bss 0x101f3948
Vector g_vecTentRight;	 // orig .bss 0x101f69e8
Vector g_vecTentUp;		 // orig .bss 0x101ee9e0

// The view the effects face: the camera's angles in third person (pitch, yaw, the third
// component zeroed or not as each caller does), v_angles otherwise
static void CLTE_SetupView(bool bClearRoll)
{
	Vector vecAngles;
	if (0 != CL_IsThirdPerson())
	{
		vecAngles = Vector(0, 0, 0);
		CL_CameraOffset(vecAngles);
		if (bClearRoll)
			vecAngles.z = 0;
	}
	else
	{
		vecAngles = v_angles;
	}
	gEngfuncs.pfnAngleVectors(vecAngles, g_vecTentForward, g_vecTentRight, g_vecTentUp);
}

// Every effect of iType or, iType 0, every effect
static inline void CLTE_Delete(int i)
{
	delete g_pClientTents[i];
	g_pClientTents[i] = NULL;
}

// orig client 0x510b0. "CLTE": byte type, then the type's own data
int __MsgFunc_CLTE(const char* pszName, int iSize, void* pbuf)
{
	CClientTent* pTent;
	int i;

	BEGIN_READ(pbuf, iSize);
	const int iType = READ_BYTE();

	switch (iType)
	{
	case 1: pTent = CLTE_CreateRocketTrail(NULL); break;
	case 2: pTent = CLTE_CreateJetboost(NULL); break;
	case 3: pTent = CLTE_CreateJetpack(NULL); break;

	case 4:
	{
		// byte player, byte splash: the visible jetpack on that player goes, with a swim sound
		const int iEnt = READ_BYTE();
		const int iSplash = READ_BYTE();
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CTentJetpack* pJetpack = static_cast<CTentJetpack*>(g_pClientTents[i]);
			if (!pJetpack || pJetpack->m_iType != 3 || iEnt != pJetpack->m_iEntity)
				continue;

			cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(pJetpack->m_iEntity);
			if (pEnt && 0 != iSplash)
			{
				switch (gEngfuncs.pfnRandomLong(0, 3))
				{
				case 0: gEngfuncs.pEventAPI->EV_PlaySound(pJetpack->m_iEntity, pEnt->origin, CHAN_BODY, "player/pl_swim1.wav", 0.8, 0.8, 0, 110); break;
				case 1: gEngfuncs.pEventAPI->EV_PlaySound(pJetpack->m_iEntity, pEnt->origin, CHAN_BODY, "player/pl_swim2.wav", 0.8, 0.8, 0, 110); break;
				case 2: gEngfuncs.pEventAPI->EV_PlaySound(pJetpack->m_iEntity, pEnt->origin, CHAN_BODY, "player/pl_swim3.wav", 0.8, 0.8, 0, 110); break;
				case 3: gEngfuncs.pEventAPI->EV_PlaySound(pJetpack->m_iEntity, pEnt->origin, CHAN_BODY, "player/pl_swim4.wav", 0.8, 0.8, 0, 110); break;
				}
			}
			pJetpack->KillEntity(iEnt);
			pJetpack->m_iEntity = 0;
			CLTE_Delete(i);
		}
		return 1;
	}

	case 5: pTent = CLTE_CreateEnergyDraw(NULL); break;

	case 6:
	{
		// byte player: the energy draw's beams go
		const int iEnt = READ_BYTE();
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CTentEnergyDraw* pDraw = static_cast<CTentEnergyDraw*>(g_pClientTents[i]);
			if (!pDraw || pDraw->m_iType != 5 || iEnt != pDraw->m_iEntity)
				continue;

			for (int j = 0; j < CTentEnergyDraw::NUM_BEAMS; j++)
			{
				BEAM* pBeam = pDraw->m_pBeams[j];
				if (!pBeam)
					continue;
				pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
				pBeam->t = 1.0;
				pBeam->die = 0.1;
				pBeam->endEntity = 0;
				pBeam->startEntity = 0;
				pDraw->m_pBeams[j] = NULL;
			}
			pDraw->m_iEntity = 0;
			CLTE_Delete(i);
		}
		return 1;
	}

	case 7: pTent = CLTE_CreateSciMineIdle(NULL); break;
	case 8: pTent = CLTE_CreateSciMineSpin(NULL); break;
	case 9: pTent = CLTE_CreateCreateHealth(NULL); break;
	case 10: pTent = CLTE_CreateGibBleed(NULL); break;
	case 11: pTent = CLTE_CreateStickyGib(NULL); break;
	case 12: pTent = CLTE_CreateTransphasic(NULL); break;
	case 13: pTent = CLTE_CreateChargeGlow(NULL); break;
	case 14: pTent = CLTE_CreatePythonCharge(NULL); break;
	case 15: pTent = CLTE_CreateBallLightning(NULL); break;
	case 16: pTent = CLTE_CreateMP5Bolt(NULL); break;
	case 17: pTent = CLTE_CreateMP5Pulse(NULL); break;
	case 18: pTent = CLTE_CreateMP5Bouncy(NULL); break;
	case 19: pTent = CLTE_CreateCrateCharge(NULL); break;
	case 20: pTent = CLTE_CreateHEVFlame(NULL); break;
	case 21: pTent = CLTE_CreateCrowbarangGlint(NULL); break;
	case 22: pTent = CLTE_CreateTracer(NULL); break;
	case 23: pTent = CLTE_CreateBoltTrail(NULL); break;
	case 24: pTent = CLTE_CreateImpailer(NULL); break;
	case 25: pTent = CLTE_CreateAttach(NULL); break;
	case 26: pTent = CLTE_CreateBoltRope(NULL); break;
	case 27: pTent = CLTE_CreateColorTracer(NULL); break;
	case 28: pTent = CLTE_CreateCrossbowCharge(NULL); break;
	case 29: pTent = CLTE_CreateBoltGlow(NULL); break;
	case 30: pTent = CLTE_CreateBoltArmed(NULL); break;
	case 31: pTent = CLTE_CreateSuckMineRope(NULL); break;
	case 32: pTent = CLTE_CreateSphereMine(NULL); break;
	case 33: pTent = CLTE_CreateLightningFlash(NULL); break;
	case 34: pTent = CLTE_CreateBeaconStuck(NULL); break;
	case 35: pTent = CLTE_CreateChargeHum(NULL); break;
	case 36: pTent = CLTE_CreateBlueLightning(NULL); break;
	case 37: pTent = CLTE_CreateShield(NULL); break;
	case 0x26: pTent = RC2_NewShieldHitTent(NULL); break;
	case 0x27: pTent = RC2_NewShrinkerGlowTent(NULL); break;
	case 0x28: pTent = RC2_NewShrinkTent(NULL); break;

	case 0x29:
	{
		// byte entity, short value (0x4000: unchanged), coord x3: update that entity's effect, or
		// start one: two beams from the entity to the point
		const int iEnt = READ_BYTE();
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CEgonTractorTent* p29 = static_cast<CEgonTractorTent*>(g_pClientTents[i]);
			if (p29 && p29->m_iType == 0x29 && iEnt == p29->m_iEntIndex)
			{
				const int iValue = READ_SHORT();
				if (iValue != 0x4000)
					p29->m_iHeld = iValue;
				p29->m_vecEnd.x = READ_COORD();
				p29->m_vecEnd.y = READ_COORD();
				p29->m_vecEnd.z = READ_COORD();
				return 1;
			}
		}

		CEgonTractorTent* p29 = static_cast<CEgonTractorTent*>(RC2_NewEgonTractorTent(NULL));
		if (!p29)
			return 1;

		p29->m_iEntIndex = iEnt;
		if (!gEngfuncs.GetEntityByIndex(iEnt))
		{
			p29->m_iEntIndex = 0;
			p29->m_iType = 0x29;
		}
		else
		{
			int iModel;
			if (gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel))
			{
				p29->m_pLightning = gEngfuncs.pEfxAPI->R_BeamEntPoint(p29->m_iEntIndex | 0x1000, p29->m_vecEnd, iModel,
					0, 3.5, 0.2, 0.9, 3, (int)gEngfuncs.pfnRandomFloat(0, 2), 30, 0, 1, 0);
				if (p29->m_pLightning)
					p29->m_pLightning->flags |= FBEAM_SINENOISE;

				if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModel))
				{
					p29->m_pLaser = gEngfuncs.pEfxAPI->R_BeamEntPoint(p29->m_iEntIndex | 0x1000, p29->m_vecEnd, iModel,
						0, 0.9, 0.5, 0.9, 0.5, 0, 0, 1, 1, 1);
					gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &p29->m_iLaserModel);
				}
			}
			p29->m_iType = 0x29;
		}
		pTent = p29;
		break;
	}

	case 0x2a: pTent = RC2_NewNukeTent(NULL); break;
	case 0x2b: pTent = RC2_NewBlastSphereTent(NULL); break;
	case 0x2c: pTent = RC2_NewImploderChargeTent(NULL); break;
	case 0x2d: pTent = RC2_NewImploderShotTent(NULL); break;
	case 0x2e: pTent = RC2_NewImplodeTent(NULL); break;
	case 0x2f: pTent = RC2_NewFrozenTent(NULL); break;
	case 0x30: pTent = RC2_NewFreezeRayTent(NULL); break;
	case 0x31: pTent = RC2_NewFreezeTrailTent(NULL); break;
	case 0x32: pTent = RC2_NewGravGrenadeTent(NULL); break;
	case 0x33: pTent = RC2_NewBlueShockRingTent(NULL); break;
	case 0x34: pTent = RC2_NewSuckGrenadeTent(NULL); break;
	case 0x35: pTent = RC2_NewAntiFrictionTent(NULL); break;
	case 0x36: pTent = RC2_NewPurpleShockRingTent(NULL); break;
	case 0x37: pTent = RC2_NewUltimaTent(NULL); break;
	case 0x38: pTent = RC2_NewEnergyBlastTent(NULL); break;
	case 0x39: pTent = RC2_NewTripmineChargeTent(NULL); break;

	case 0x3a:
	{
		// short entity: the tripmine beams of type 0x39 on it go out with a sound, then a 0x3a effect
		const int iEnt = READ_SHORT();
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CTripmineChargeTent* p39 = static_cast<CTripmineChargeTent*>(g_pClientTents[i]);
			if (p39 && p39->m_iType == 0x39 && iEnt == p39->m_iEntIndex)
			{
				cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p39->m_iEntIndex);
				if (pEnt)
					gEngfuncs.pEventAPI->EV_PlaySound(p39->m_iEntIndex, pEnt->origin, CHAN_WEAPON, "debris/beamstart9.wav", 1.0, 0.8, 0, 120);
				p39->m_flCharge = 0;
			}
		}
		pTent = RC2_NewTripmineZapTent(NULL);
		break;
	}

	case 0x3b:
	{
		// short entity, byte armed: arm that entity's roach charge effect, or start one
		const int iEnt = READ_SHORT();
		if (0 != READ_BYTE())
		{
			for (i = 0; i < MAX_CLIENT_TENTS; i++)
			{
				CRoachWalkTent* pRoach = static_cast<CRoachWalkTent*>(g_pClientTents[i]);
				if (!pRoach || pRoach->m_iType != 0x3b || iEnt != pRoach->m_iEntIndex)
					continue;

				int iModel;
				if (g_iClientDetail < 0)
				{
					if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
					{
						pRoach->m_pBeam = gEngfuncs.pEfxAPI->R_BeamFollow(pRoach->m_iEntIndex, iModel, 0.65, 2, 1, 0, 0, 0.6);
						if (pRoach->m_pBeam)
							pRoach->m_pBeam->flags |= FBEAM_FOREVER;
					}
				}
				else
				{
					pRoach->m_vecOrigin = CLTE_GetOrigin(pRoach->m_iEntIndex);
					pRoach->m_Trail.AddPoint(pRoach->m_vecOrigin, Vector(0, 0, 20), Vector(0, 0, 0), Vector(1, 0, 0), 0.8, 2, 0.8, 0, NULL);
					pRoach->m_pTrailSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke.spr", &iModel);
				}
				pRoach->m_bArmed = true;
				pRoach->m_flArmTimeLeft = RC2Pref(RC2P_Satchel_RoachArmTime).f;
				pRoach->m_flArmTime = RC2Pref(RC2P_Satchel_RoachArmTime).f;
			}
			return 1;
		}

		CRoachWalkTent* pRoach = static_cast<CRoachWalkTent*>(RC2_NewRoachWalkTent(NULL));
		if (!pRoach)
			return 1;
		pRoach->m_iEntIndex = iEnt;
		pRoach->m_iType = 0x3b;
		pTent = pRoach;
		break;
	}

	case 0x3c: pTent = RC2_NewConcChargeTent(NULL); break;
	case 0x3d: pTent = RC2_NewExpandingSphereTent(NULL); break;
	case 0x3e: pTent = RC2_NewSnarkTrailTent(NULL); break;
	case 0x3f: pTent = RC2_NewForkliftLightsTent(NULL); break;
	case 0x40: pTent = RC2_NewTransformTent(NULL); break;
	case 0x41: pTent = RC2_NewHoundeyeChargeTent(NULL); break;
	case 0x42: pTent = RC2_NewCurvedBeamTent(NULL); break;
	case 0x43: pTent = RC2_NewRingBlastTent(NULL); break;
	case 0x44: pTent = RC2_NewAfterimageTent(NULL); break;
	case 0x45: pTent = RC2_NewControllerThrowGlowTent(NULL); break;
	case 0x46: pTent = RC2_NewControllerThrowBallTent(NULL); break;
	case 0x47: pTent = RC2_NewControllerSparksTent(NULL); break;
	case 0x48: pTent = RC2_NewControllerChargeGlowTent(NULL); break;
	case 0x49: pTent = RC2_NewControllerChargeBallTent(NULL); break;

	case 0xfc:
		// every effect lets go of every player, then goes
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			// Original bug (crash), guarded: the original calls KillEntity through empty slots
			// too, a crash once g_iHighestEntIndex is past 1 (any player seen)
			for (int j = 1; j < g_iHighestEntIndex && g_pClientTents[i]; j++)
				g_pClientTents[i]->KillEntity(j);
			CLTE_Delete(i);
		}
		return 1;

	case 0xfd:
	{
		// byte type, byte command, short value
		const int iCmdType = READ_BYTE();
		const int iCmd = READ_BYTE();
		const int iValue = READ_SHORT();
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CClientTent* p = g_pClientTents[i];
			if (p && p->m_iType == iCmdType && static_cast<CClientTentCmd*>(p)->Command(iCmd, iValue))
				CLTE_Delete(i);
		}
		return 1;
	}

	case 0xfe:
	{
		// short entity, byte type: the effects of that type let go of it
		const int iEnt = READ_SHORT();
		const int iKillType = READ_BYTE();
		if (iEnt == 0)
			return 1;
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CClientTent* p = g_pClientTents[i];
			if (p && p->m_iType == iKillType && p->KillEntity(iEnt))
				CLTE_Delete(i);
		}
		return 1;
	}

	case 0xff:
	{
		// short entity: every effect lets go of it
		const int iEnt = READ_SHORT();
		if (iEnt == 0)
			return 1;
		for (i = 0; i < MAX_CLIENT_TENTS; i++)
		{
			CClientTent* p = g_pClientTents[i];
			if (p && p->KillEntity(iEnt))
				CLTE_Delete(i);
		}
		return 1;
	}

	default:
	{
		char sz[256];
		sprintf(sz, "Unreconized client temp ent: %d\n", iType);
		gEngfuncs.pfnConsolePrint(sz);
		return 1;
	}
	}

	if (pTent)
		pTent->m_iType = iType;
	return 1;
}

// orig client 0x52280. The effects of iType let go of iEnt (0: nothing)
void CLTE_KillByEntity(int iEnt, int iType)
{
	if (iEnt == 0)
		return;
	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		CClientTent* p = g_pClientTents[i];
		if (p && p->m_iType == iType && p->KillEntity(iEnt))
			CLTE_Delete(i);
	}
}

// orig client 0x522f0. From HUD_Init, HUD_VidInit and HUD_Reset: the slots are forgotten, not freed
void CLTE_Reset()
{
	CLTE_InitShapes();
	CLTE_ClearPlayerOverrides();
	CLTE_ClearOrigins();
	memset(g_pClientTents, 0, sizeof(g_pClientTents));
	memset(g_bEntityFrozen, 0, sizeof(g_bEntityFrozen));
	memset(g_abRC2AntiFriction, 0, sizeof(g_abRC2AntiFriction));
	g_iShieldPlayers = 0;
	g_iLocalPlayerFrozen = 0;
	g_bLocalTransforming = 0;
	g_iRC2LocalAntiFriction = 0;
}

// orig client 0x52370. From HUD_Shutdown: every effect deleted
void CLTE_Shutdown()
{
	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (g_pClientTents[i])
			CLTE_Delete(i);
	}
	g_iShieldPlayers = 0;
	memset(g_bEntityFrozen, 0, sizeof(g_bEntityFrozen));
	memset(g_abRC2AntiFriction, 0, sizeof(g_abRC2AntiFriction));
	g_iLocalPlayerFrozen = 0;
	g_bLocalTransforming = 0;
	g_iRC2LocalAntiFriction = 0;
}

// orig client 0x523f0. From HUD_TempEntUpdate
void CLTE_Update(float flFrametime)
{
	if (!(flFrametime > 0))
		return;

	CLTE_SetupView(true);

	// the local player's angles follow the view while playing
	if (0 == gEngfuncs.IsSpectateOnly())
	{
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (pLocal)
			pLocal->angles = v_angles;
	}

	CLTE_UpdateShapes(flFrametime);

	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (g_pClientTents[i] && g_pClientTents[i]->Update(flFrametime))
			CLTE_Delete(i);
	}
}

// orig client 0x52550. Nothing in client.dll calls it
void CLTE_Think()
{
	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (g_pClientTents[i])
			g_pClientTents[i]->Think();
	}
}

// orig client 0x52580. From HUD_DrawNormalTriangles
void CLTE_DrawNormal()
{
	CLTE_SetupView(true);

	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (g_pClientTents[i])
			g_pClientTents[i]->DrawNormal();
	}
}

// orig client 0x52650. From HUD_DrawTransparentTriangles: the effects, then the disks and quads.
// In third person the camera's third component is left as CL_CameraOffset gives it here
void CLTE_DrawTransparent()
{
	CLTE_SetupView(false);

	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (g_pClientTents[i])
			g_pClientTents[i]->DrawTransparent();
	}

	CLTE_DrawShapes();
}

// --- per-player overrides (orig .bss 0x101edc58 on): a point and a value each ---

static bool s_bPlayerOverride[MAX_PLAYERS + 1];		 // orig .bss 0x101edc58
static Vector s_vecPlayerOverride[MAX_PLAYERS + 1]; // orig .bss 0x101f6458
static float s_flPlayerOverride[MAX_PLAYERS + 1];	 // orig .bss 0x101f38c4

// orig client 0x59830
void CLTE_ClearPlayerOverrides()
{
	memset(s_bPlayerOverride, 0, sizeof(s_bPlayerOverride));
}

// orig client 0x59860
void CLTE_SetPlayerOverride(int iPlayer, float x, float y, float z, float flValue)
{
	s_bPlayerOverride[iPlayer] = true;
	s_flPlayerOverride[iPlayer] = flValue;
	s_vecPlayerOverride[iPlayer] = Vector(x, y, z);
}

// orig client 0x598b0
void CLTE_ClearPlayerOverride(int iPlayer)
{
	s_bPlayerOverride[iPlayer] = false;
}

// orig client 0x598d0
bool CLTE_GetPlayerOverride(int iPlayer, float* vecOut, float* pflValue)
{
	if (!s_bPlayerOverride[iPlayer])
		return false;
	vecOut[0] = s_vecPlayerOverride[iPlayer].x;
	vecOut[1] = s_vecPlayerOverride[iPlayer].y;
	vecOut[2] = s_vecPlayerOverride[iPlayer].z;
	*pflValue = s_flPlayerOverride[iPlayer];
	return true;
}

// --- the loopback: a CLTE message the client builds and hands itself ---

static unsigned char s_LoopbackBuf[1024]; // orig .bss 0x101f65e4
static int s_iLoopbackSize;				  // orig .bss 0x101f6a04

// orig client 0x59940
void CLTE_LoopbackBegin()
{
	memset(s_LoopbackBuf, 0, sizeof(s_LoopbackBuf));
	s_iLoopbackSize = 0;
}

// orig client 0x59970
void CLTE_LoopbackSend()
{
	__MsgFunc_CLTE("loopback", s_iLoopbackSize, s_LoopbackBuf);
}

// orig client 0x599a0
void CLTE_LoopbackByte(int iValue)
{
	s_LoopbackBuf[s_iLoopbackSize] = (unsigned char)iValue;
	s_iLoopbackSize++;
}

// orig client 0x599d0
void CLTE_LoopbackShort(int iValue)
{
	*(short*)&s_LoopbackBuf[s_iLoopbackSize] = (short)iValue;
	s_iLoopbackSize += 2;
}

// orig client 0x59a00
void CLTE_LoopbackLong(int iValue)
{
	*(int*)&s_LoopbackBuf[s_iLoopbackSize] = iValue;
	s_iLoopbackSize += 4;
}

// orig client 0x59a30. As WRITE_COORD: eighths in a short
void CLTE_LoopbackCoord(float flValue)
{
	*(short*)&s_LoopbackBuf[s_iLoopbackSize] = (short)(int)(flValue * 8);
	s_iLoopbackSize += 2;
}

// orig client 0x59a70. The entity's scale, 1 if it has none
float CLTE_GetEntityScale(int iEnt)
{
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(iEnt);
	if (!pEnt)
		return 1.0;
	if (pEnt->curstate.scale <= 0)
		return 1.0;
	return pEnt->curstate.scale;
}

// --- the origin cache: where an effect says an entity is this frame ---

#define CLTE_MAX_ORIGINS 1024 // name ours

static unsigned int s_iOriginFrame[CLTE_MAX_ORIGINS]; // orig .bss 0x101f28c4
static Vector s_vecOrigins[CLTE_MAX_ORIGINS];		   // orig .bss 0x101ee9f0

// orig client 0x59b00
void CLTE_ClearOrigins()
{
	memset(s_iOriginFrame, 0, sizeof(s_iOriginFrame));
	g_iClientFrame = 0;
	for (int i = 0; i < CLTE_MAX_ORIGINS; i++)
		s_vecOrigins[i] = Vector(0, 0, 0);
}

// orig client 0x59b90
void CLTE_SetOrigin(int iEnt, float x, float y, float z)
{
	if (iEnt < 0 || iEnt >= CLTE_MAX_ORIGINS)
		return;
	s_vecOrigins[iEnt].x = x;
	s_vecOrigins[iEnt].y = y;
	s_vecOrigins[iEnt].z = z;
	s_iOriginFrame[iEnt] = g_iClientFrame;
}

// orig client 0x59be0. The cached origin if it was set this frame or the last and the entity moves
// as a monster or projectile would (walk, step, fly, toss, bounce; not a BSP model); the entity's
// own origin otherwise. No NULL test for an index in range, as the original; GetEntityByIndex
// returns NULL only past the engine's entity list, and callers pass entity indices
Vector CLTE_GetOrigin(int iEnt)
{
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(iEnt);
	if (!pEnt && (iEnt < 0 || iEnt >= CLTE_MAX_ORIGINS))
		return Vector(0, 0, 0);

	if (iEnt >= 0 && iEnt < CLTE_MAX_ORIGINS &&
		(s_vecOrigins[iEnt].x != 0 || s_vecOrigins[iEnt].y != 0 || s_vecOrigins[iEnt].z != 0))
	{
		const int iMove = pEnt->curstate.movetype;
		if ((iMove == MOVETYPE_WALK || iMove == MOVETYPE_TOSS || iMove == MOVETYPE_BOUNCE || iMove == MOVETYPE_STEP || iMove == MOVETYPE_FLY) &&
			pEnt->curstate.solid != SOLID_BSP)
		{
			if (s_iOriginFrame[iEnt] >= g_iClientFrame - 1)
				return s_vecOrigins[iEnt];
			s_iOriginFrame[iEnt] = 0;
		}
	}

	return pEnt->origin;
}

// orig client 0x59d50
void CLTE_SetOriginV(int iEnt, const float* vecOrigin)
{
	if (iEnt < 0 || iEnt >= CLTE_MAX_ORIGINS)
		return;
	s_vecOrigins[iEnt].x = vecOrigin[0];
	s_vecOrigins[iEnt].y = vecOrigin[1];
	s_vecOrigins[iEnt].z = vecOrigin[2];
	s_iOriginFrame[iEnt] = g_iClientFrame;
}

// orig client 0x59dc0. Nothing in client.dll calls it
void CLTE_GetOriginV(int iEnt, float* vecOut)
{
	if (iEnt < 0 || iEnt >= CLTE_MAX_ORIGINS)
		return;
	vecOut[0] = CLTE_GetOrigin(iEnt).x;
	vecOut[1] = CLTE_GetOrigin(iEnt).y;
	vecOut[2] = CLTE_GetOrigin(iEnt).z;
}

// orig client 0x59e40. A point at height z on an ellipse of the given radius around the z axis,
// scaled out by flScale, at flAngle (radians)
Vector CLTE_EllipsePoint(float flAngle, float z, float flRadius, float flScale)
{
	float flDist = 0;
	if (-flRadius < z && z < flRadius)
		flDist = (sqrt(flRadius * flRadius - z * z) / flRadius) * flScale;

	Vector vecOut;
	vecOut.z = z;
	vecOut.x = flDist * cos(flAngle);
	vecOut.y = flDist * sin(flAngle);
	return vecOut;
}

// orig client 0x59ee0. Where a shot from vecPoint along vecDir meets iPlayer's shield (an ellipsoid
// round his origin, 20 wide and 40 high, 26 and 30 ducked), and the surface normal there; vecPoint
// when the player has no shield up (*pbHit 0; the original writes the flag as a dword). The original solves along the reversed
// direction from 1024 units past the point. VERIFY: literal reading of the original's quadratic,
// and of its result, which adds the step along the ray twice when vecNormal is asked for (its
// variable for the point relative to the origin becomes the hit point first)
Vector CLTE_ShieldPoint(int iPlayer, Vector vecPoint, Vector vecDir, float* vecNormal, int* pbHit)
{
	if (pbHit)
		*pbHit = 0;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(iPlayer);
	if (!pEnt || 0 == pEnt->player || (g_iShieldPlayers & (1 << (iPlayer - 1))) == 0)
		return vecPoint;

	if (pbHit)
		*pbHit = 1;

	const Vector vecBack = vecDir * -1.0;
	const Vector vecOrg = CLTE_GetOrigin(iPlayer);
	const Vector vecFar = vecBack * 1024.0;
	Vector vecRel = vecPoint - vecOrg;
	const Vector vecStart = vecFar + vecRel;

	float flWidth, flHeight;
	if (pEnt->curstate.usehull == 1)
	{
		flWidth = 26;
		flHeight = 30;
	}
	else
	{
		flWidth = 20;
		flHeight = 40;
	}

	const float flSx2 = vecStart.x * vecStart.x;
	const float flSy2 = vecStart.y * vecStart.y;
	const float flSz2 = vecStart.z * vecStart.z;
	const float flA = flWidth * flWidth;
	const float flB = flHeight * flHeight;
	const float flDenom = flA * vecBack.z * vecBack.z + (vecBack.y * vecBack.y + vecBack.x * vecBack.x) * flB;
	if (flDenom == 0)
		return vecPoint;

	const float flZTerm = vecStart.z * flA * vecBack.z;
	const float flYTerm = vecBack.y * vecStart.y * flB;
	const float flCross = flZTerm * vecBack.y * vecStart.y + (flYTerm + flZTerm) * vecBack.x * vecStart.x;
	const float flDisc = ((flA - flSx2) - flSy2) * flA * vecBack.z * vecBack.z +
						 ((flA - flSx2) * flB - flA * flSz2) * vecBack.y * vecBack.y +
						 ((flA - flSy2) * flB - flA * flSz2) * vecBack.x * vecBack.x +
						 flCross + flCross;
	if (flDisc < 0)
		return vecPoint;

	const float t = (((sqrt(flDisc) * flHeight - vecBack.x * vecStart.x * flB) - flYTerm) - flZTerm) / flDenom;

	if (vecNormal)
	{
		vecRel = vecStart + vecBack * t;

		Vector vecAngles;
		VectorAngles(vecRel.Normalize(), vecAngles);

		const float flScale = flWidth + 1.0;
		const float flRadius = flHeight / flWidth + flHeight;
		const float flSign = vecRel.z > 0 ? 1.0 : -1.0;
		const float z = (flHeight / flWidth) * (1.0 - sqrt(flB - vecRel.z * vecRel.z) / flHeight) * flSign + vecRel.z;
		const Vector vecSurface = CLTE_EllipsePoint(vecAngles.y, z, flRadius, flScale);

		Vector vecN = vecSurface - vecRel;
		vecN = vecN.Normalize();
		vecNormal[0] = vecN.x;
		vecNormal[1] = vecN.y;
		vecNormal[2] = vecN.z;
	}

	return vecRel + vecOrg + vecBack * t;
}
