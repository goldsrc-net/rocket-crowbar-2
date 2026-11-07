// studio_model.cpp
// routines for setting up to draw 3DStudio models

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "pm_rc2.h"
#include "rc2/studio_rc2.h"
#include "event_api.h"
#include "Exports.h"

extern cvar_t* tfc_newmodels;

extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS_HUD + 1];

// team colors for old TFC models
#define TEAM1_COLOR 150
#define TEAM2_COLOR 250
#define TEAM3_COLOR 45
#define TEAM4_COLOR 100

int m_nPlayerGaitSequences[MAX_PLAYERS];

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

// RC2 (all names ours): where a rider sits on each player, [0] the right shoulder and [1] the
// left (.bss 0x10218f48, by player index), and the client frame each was last worked out
// (.bss 0x10216a90). StudioCalcAttachments fills them, StudioDrawPlayer seats the riders.
static Vector g_rgvecRiderSeat[2][MAX_PLAYERS + 1];
static unsigned int g_rgiRiderSeatFrame[MAX_PLAYERS + 1];

// RC2: each player's attachments as last drawn (by player index; read by client.dll 0x93710,
// c2). VERIFY: the base, 0x10214200, is worked out from StudioCalcAttachments' writes
// (0x10214230 + (index * 4 + i - 4) * 12) and 0x93710's read at 0x10214218.
Vector g_rgvecPlayerAttachment[MAX_PLAYERS + 1][4]; // name ours

/////////////////////
// Implementation of CStudioModelRenderer.h

/*
====================
Init

====================
*/
// orig client 0xc0640. RC2: also loads the shell sprites and the jetpack and transform models
void CStudioModelRenderer::Init()
{
	int iIndex;

	// Set up some variables shared with engine
	m_pCvarHiModels = IEngineStudio.GetCvar("cl_himodels");
	m_pCvarDeveloper = IEngineStudio.GetCvar("developer");
	m_pCvarDrawEntities = IEngineStudio.GetCvar("r_drawentities");

	m_pChromeSprite = IEngineStudio.GetChromeSprite();

	m_pWhiteSprite = gEngfuncs.CL_LoadModel("sprites/white.spr", &iIndex);
	m_pAftexSprite = gEngfuncs.CL_LoadModel("sprites/aftex.spr", &iIndex);
	m_pJetpackModel = gEngfuncs.CL_LoadModel("models/jetpack.mdl", &iIndex);
	m_pAssassinModel = gEngfuncs.CL_LoadModel("models/hassassin.mdl", &iIndex);
	m_pHoundeyeModel = gEngfuncs.CL_LoadModel("models/houndeye.mdl", &iIndex);
	m_pControllerModel = gEngfuncs.CL_LoadModel("models/controller.mdl", &iIndex);
	m_pStukabatModel = gEngfuncs.CL_LoadModel("models/stukabat.mdl", &iIndex);
	m_pHeadcrabModel = gEngfuncs.CL_LoadModel("models/headcrab.mdl", &iIndex);

	IEngineStudio.GetModelCounters(&m_pStudioModelCount, &m_pModelsDrawn);

	// Get pointers to engine data structures
	m_pbonetransform = (float(*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetBoneTransform();
	m_plighttransform = (float(*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetLightTransform();
	m_paliastransform = (float(*)[3][4])IEngineStudio.StudioGetAliasTransform();
	m_protationmatrix = (float(*)[3][4])IEngineStudio.StudioGetRotationMatrix();
}

/*
====================
CStudioModelRenderer

====================
*/
// orig client 0xc07b0. RC2: also clears the shell sprites and the assassin's model (not the
// other models; Init loads them all)
CStudioModelRenderer::CStudioModelRenderer()
{
	m_fDoInterp = true;
	m_fGaitEstimation = true;
	m_pCurrentEntity = NULL;
	m_pCvarHiModels = NULL;
	m_pCvarDeveloper = NULL;
	m_pCvarDrawEntities = NULL;
	m_pChromeSprite = NULL;
	m_pWhiteSprite = NULL;
	m_pAftexSprite = NULL;
	m_pStudioModelCount = NULL;
	m_pModelsDrawn = NULL;
	m_protationmatrix = NULL;
	m_paliastransform = NULL;
	m_pbonetransform = NULL;
	m_plighttransform = NULL;
	m_pStudioHeader = NULL;
	m_pBodyPart = NULL;
	m_pSubModel = NULL;
	m_pPlayerInfo = NULL;
	m_pRenderModel = NULL;
	m_pAssassinModel = NULL;
}

/*
====================
~CStudioModelRenderer

====================
*/
// orig client 0xc0870. Its deleting destructor is orig client 0xc0840 (compiler-made)
CStudioModelRenderer::~CStudioModelRenderer()
{
}

/*
====================
StudioCalcBoneAdj

====================
*/
// orig client 0xc0890. Not byte-identical to SDK 2.3's (RC2's members moved), but it reads the same
void CStudioModelRenderer::StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen)
{
	int i, j;
	float value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if ((pbonecontroller[j].type & STUDIO_RLOOP) != 0)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
				else
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0)
					value = 0;
				if (value > 1.0)
					value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
		}
		else
		{
			value = mouthopen / 64.0;
			if (value > 1.0)
				value = 1.0;
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}


/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q)
{
	int j, k;
	vec4_t q1, q2;
	Vector angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j + 3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
	{
		AngleQuaternion(angle1, q);
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos)
{
	int j, k;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j]);
			/*
			if (i == 0 && j == 0)
				Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			*/

			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if (pbone->bonecontroller[j] != -1 && adj)
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
// orig client 0xc0f60. Not byte-identical to SDK 2.3's (RC2's members moved), but it reads the same
void CStudioModelRenderer::StudioSlerpBones(vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s)
{
	int i;
	vec4_t q3;
	float s1;

	if (s < 0)
		s = 0;
	else if (s > 1.0)
		s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
StudioGetAnim

====================
*/
// orig client 0xc10f0. RC2: no "loading %s" developer message
mstudioanim_t* CStudioModelRenderer::StudioGetAnim(model_t* m_pSubModel, mstudioseqdesc_t* pseqdesc)
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t*)((byte*)m_pStudioHeader + pseqdesc->animindex);
	}

	paSequences = (cache_user_t*)m_pSubModel->submodels;

	if (paSequences == NULL)
	{
		paSequences = (cache_user_t*)IEngineStudio.Mem_Calloc(16, sizeof(cache_user_t)); // UNDONE: leak!
		m_pSubModel->submodels = (dmodel_t*)paSequences;
	}

	if (!IEngineStudio.Cache_Check((struct cache_user_s*)&(paSequences[pseqdesc->seqgroup])))
	{
		IEngineStudio.LoadCacheFile(pseqgroup->name, (struct cache_user_s*)&paSequences[pseqdesc->seqgroup]);
	}
	return (mstudioanim_t*)((byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch)
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);
	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
// orig client 0xc12e0. RC2: the model origin is kept in m_vecModelPos
void CStudioModelRenderer::StudioSetUpTransform(bool trivial_accept)
{
	int i;
	Vector angles;
	Vector& modelpos = m_vecModelPos; // RC2: a member

	// tweek model origin
	//for (i = 0; i < 3; i++)
	//	modelpos[i] = m_pCurrentEntity->origin[i];

	VectorCopy(m_pCurrentEntity->origin, modelpos);

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	angles[ROLL] = m_pCurrentEntity->curstate.angles[ROLL];
	angles[PITCH] = m_pCurrentEntity->curstate.angles[PITCH];
	angles[YAW] = m_pCurrentEntity->curstate.angles[YAW];

	//Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], m_pCurrentEntity->index);
	//Con_DPrintf("movetype %d %d\n", m_pCurrentEntity->movetype, m_pCurrentEntity->aiment );
	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP)
	{
		float f = 0;
		float d;

		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ((m_clTime < m_pCurrentEntity->curstate.animtime + 1.0f) &&
			(m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime))
		{
			f = (m_clTime - m_pCurrentEntity->curstate.animtime) / (m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime);
			//Con_DPrintf("%4.2f %.2f %.2f\n", f, m_pCurrentEntity->curstate.animtime, m_clTime);
		}

		if (m_fDoInterp)
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		const auto pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

		if ((pseqdesc->motiontype & STUDIO_LX) != 0 || (m_pCurrentEntity->curstate.eflags & EFLAG_SLERP) != 0)
		{
			for (i = 0; i < 3; i++)
			{
				modelpos[i] += (m_pCurrentEntity->origin[i] - m_pCurrentEntity->latched.prevorigin[i]) * f;
			}
		}

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		//if (f > -1.0 && f < 1.5) {}

		//			Con_DPrintf("%.0f %.0f\n",m_pCurrentEntity->msg_angles[0][YAW], m_pCurrentEntity->msg_angles[1][YAW] );
		for (i = 0; i < 3; i++)
		{
			float ang1, ang2;

			ang1 = m_pCurrentEntity->angles[i];
			ang2 = m_pCurrentEntity->latched.prevangles[i];

			d = ang1 - ang2;
			if (d > 180)
			{
				d -= 360;
			}
			else if (d < -180)
			{
				d += 360;
			}

			angles[i] += d * f;
		}
		//Con_DPrintf("%.3f \n", f );
	}
	else if (m_pCurrentEntity->curstate.movetype != MOVETYPE_NONE)
	{
		VectorCopy(m_pCurrentEntity->angles, angles);
	}

	//Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
	//Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );

	angles[PITCH] = -angles[PITCH];
	AngleMatrix(angles, (*m_protationmatrix));

	if (0 == IEngineStudio.IsHardware())
	{
		static float viewmatrix[3][4];

		VectorCopy(m_vRight, viewmatrix[0]);
		VectorCopy(m_vUp, viewmatrix[1]);
		VectorInverse(viewmatrix[1]);
		VectorCopy(m_vNormal, viewmatrix[2]);

		(*m_protationmatrix)[0][3] = modelpos[0] - m_vRenderOrigin[0];
		(*m_protationmatrix)[1][3] = modelpos[1] - m_vRenderOrigin[1];
		(*m_protationmatrix)[2][3] = modelpos[2] - m_vRenderOrigin[2];

		ConcatTransforms(viewmatrix, (*m_protationmatrix), (*m_paliastransform));

		// do the scaling up of x and y to screen coordinates as part of the transform
		// for the unclipped case (it would mess up clipping in the clipped case).
		// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
		// correspondingly so the projected x and y come out right
		// FIXME: make this work for clipped case too?
		if (trivial_accept)
		{
			for (i = 0; i < 4; i++)
			{
				(*m_paliastransform)[0][i] *= m_fSoftwareXScale *
											  (1.0 / (ZISCALE * 0x10000));
				(*m_paliastransform)[1][i] *= m_fSoftwareYScale *
											  (1.0 / (ZISCALE * 0x10000));
				(*m_paliastransform)[2][i] *= 1.0 / (ZISCALE * 0x10000);
			}
		}
	}

	(*m_protationmatrix)[0][3] = modelpos[0];
	(*m_protationmatrix)[1][3] = modelpos[1];
	(*m_protationmatrix)[2][3] = modelpos[2];
}


/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer::StudioEstimateInterpolant()
{
	float dadt = 1.0;

	if (m_fDoInterp && (m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01))
	{
		dadt = (m_clTime - m_pCurrentEntity->curstate.animtime) / 0.1;
		if (dadt > 2.0)
		{
			dadt = 2.0;
		}
	}
	return dadt;
}

/*
====================
StudioCalcRotations

====================
*/
// orig client 0xc1720. Not byte-identical to SDK 2.3's (RC2's members moved), but it reads the same
void CStudioModelRenderer::StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f)
{
	int i;
	int frame;
	mstudiobone_t* pbone;

	float s;
	float adj[MAXSTUDIOCONTROLLERS];
	float dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0; // bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if (f < -0.01)
	{
		f = -0.01;
	}

	frame = (int)f;

	// Con_DPrintf("%d %.4f %.4f %.4f %.4f %d\n", m_pCurrentEntity->curstate.sequence, m_clTime, m_pCurrentEntity->animtime, m_pCurrentEntity->frame, f, frame );

	// Con_DPrintf( "%f %f %f\n", m_pCurrentEntity->angles[ROLL], m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->angles[YAW] );

	// Con_DPrintf("frame %d %d\n", frame1, frame2 );


	dadt = StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	StudioCalcBoneAdj(dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen);

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(frame, s, pbone, panim, adj, q[i]);

		StudioCalcBonePosition(frame, s, pbone, panim, adj, pos[i]);
		// if (0 && i == 0)
		//	Con_DPrintf("%d %d %d %d\n", m_pCurrentEntity->curstate.sequence, frame, j, k );
	}

	if ((pseqdesc->motiontype & STUDIO_X) != 0)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if ((pseqdesc->motiontype & STUDIO_Y) != 0)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if ((pseqdesc->motiontype & STUDIO_Z) != 0)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * m_pCurrentEntity->curstate.framerate;

	if ((pseqdesc->motiontype & STUDIO_LX) != 0)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if ((pseqdesc->motiontype & STUDIO_LY) != 0)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if ((pseqdesc->motiontype & STUDIO_LZ) != 0)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}

/*
====================
Studio_FxTransform

====================
*/
// orig client 0xc1950. RC2: then scales the model by curstate.scale
void CStudioModelRenderer::StudioFxTransform(cl_entity_t* ent, float transform[3][4])
{
	switch (ent->curstate.renderfx)
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if (gEngfuncs.pfnRandomLong(0, 49) == 0)
		{
			int axis = gEngfuncs.pfnRandomLong(0, 1);
			if (axis == 1) // Choose between x & z
				axis = 2;
			VectorScale(transform[axis], gEngfuncs.pfnRandomFloat(1, 1.484), transform[axis]);
		}
		else if (gEngfuncs.pfnRandomLong(0, 49) == 0)
		{
			float offset;
			int axis = gEngfuncs.pfnRandomLong(0, 1);
			if (axis == 1) // Choose between x & z
				axis = 2;
			offset = gEngfuncs.pfnRandomFloat(-10, 10);
			transform[gEngfuncs.pfnRandomLong(0, 2)][3] += offset;
		}
		break;
	case kRenderFxExplode:
	{
		float scale;

		scale = 1.0 + (m_clTime - ent->curstate.animtime) * 10.0;
		if (scale > 2) // Don't blow up more than 200%
			scale = 2;
		transform[0][1] *= scale;
		transform[1][1] *= scale;
		transform[2][1] *= scale;
	}
	break;
	}

	// RC2: the entity's scale
	if (ent->curstate.scale != 1.0 && ent->curstate.scale > 0)
	{
		for (int i = 0; i < 3; i++)
		{
			transform[0][i] *= ent->curstate.scale;
			transform[1][i] *= ent->curstate.scale;
			transform[2][i] *= ent->curstate.scale;
		}
	}
}

/*
====================
StudioEstimateFrame

====================
*/
// orig client 0xc1b20. Not byte-identical to SDK 2.3's (RC2's members moved), but it reads the same
float CStudioModelRenderer::StudioEstimateFrame(mstudioseqdesc_t* pseqdesc)
{
	double dfdt, f;

	if (m_fDoInterp)
	{
		if (m_clTime < m_pCurrentEntity->curstate.animtime)
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (m_clTime - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;
		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
	}

	f += dfdt;

	if ((pseqdesc->flags & STUDIO_LOOPING) != 0)
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
		}
		if (f < 0)
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else
	{
		if (f >= pseqdesc->numframes - 1.001)
		{
			f = pseqdesc->numframes - 1.001;
		}
		if (f < 0.0)
		{
			f = 0.0;
		}
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
// orig client 0xc1c60. RC2: the root bone follows curstate.scale
void CStudioModelRenderer::StudioSetupBones()
{
	int i;
	double f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	float bonematrix[3][4];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	// always want new gait sequences to start on frame zero
	/*	if ( m_pPlayerInfo )
	{
		int playerNum = m_pCurrentEntity->index - 1;

		// new jump gaitsequence?  start from frame zero
		if ( m_nPlayerGaitSequences[ playerNum ] != m_pPlayerInfo->gaitsequence )
		{
	//		m_pPlayerInfo->gaitframe = 0.0;
			gEngfuncs.Con_Printf( "Setting gaitframe to 0\n" );
		}

		m_nPlayerGaitSequences[ playerNum ] = m_pPlayerInfo->gaitsequence;
//		gEngfuncs.Con_Printf( "index: %d     gaitsequence: %d\n",playerNum, m_pPlayerInfo->gaitsequence);
	}
*/
	f = StudioEstimateFrame(pseqdesc);

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim(m_pRenderModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float s;
		float dadt;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations(pos2, q2, pseqdesc, panim, f);

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos3, q3, pseqdesc, panim, f);

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos4, q4, pseqdesc, panim, f);

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if (m_fDoInterp &&
		0 != m_pCurrentEntity->latched.sequencetime &&
		(m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime) &&
		(m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq))
	{
		// blend from last sequence
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		if (m_pCurrentEntity->latched.prevsequence >= m_pStudioHeader->numseq)
		{
			m_pCurrentEntity->latched.prevsequence = 0;
		}

		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		// clip prevframe
		StudioCalcRotations(pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
				StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0;
				StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0 - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		//Con_DPrintf("prevframe = %4.2f\n", f);
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	// bounds checking
	if (m_pPlayerInfo)
	{
		if (m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq)
		{
			m_pPlayerInfo->gaitsequence = 0;
		}
	}

	// calc gait animation
	if (m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0)
	{
		if (m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq)
		{
			m_pPlayerInfo->gaitsequence = 0;
		}

		bool copy = true;

		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe);

		for (i = 0; i < m_pStudioHeader->numbones; i++)
		{
			auto bone = &pbones[i];

			if (0 == strcmp(bone->name, "Bip01 Spine"))
			{
				copy = false;
			}
			else if (bone->parent >= 0 &&
					 bone->parent < m_pStudioHeader->numbones &&
					 0 == strcmp(pbones[bone->parent].name, "Bip01 Pelvis"))
			{
				copy = true;
			}

			if (copy)
			{
				memcpy(pos[i], pos2[i], sizeof(pos[i]));
				memcpy(q[i], q2[i], sizeof(q[i]));
			}
		}
	}

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		const int parent = pbones[i].parent;

		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
			// RC2: a scaled model is scaled about its origin, and a player's about the player
			// origin, 36 units above the model's
			if (m_pCurrentEntity->curstate.scale != 1.0 && m_pCurrentEntity->curstate.scale > 0)
			{
				bonematrix[2][3] *= m_pCurrentEntity->curstate.scale;
				if (0 != m_pCurrentEntity->player)
					bonematrix[2][3] += m_pCurrentEntity->curstate.scale * 36.0;
			}

			if (0 != IEngineStudio.IsHardware())
			{
				ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);
			}
			else
			{
				ConcatTransforms((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
			}
			ConcatTransforms((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);

			// Apply client-side effects to the transformation matrix. RC2 (as SDK 2.3): to the light
			// transform too, so the attachments follow the scale
			StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
			StudioFxTransform(m_pCurrentEntity, (*m_plighttransform)[i]);
		}
		else if (parent >= 0 && parent < m_pStudioHeader->numbones)
		{
			ConcatTransforms((*m_pbonetransform)[parent], bonematrix, (*m_pbonetransform)[i]);
			ConcatTransforms((*m_plighttransform)[parent], bonematrix, (*m_plighttransform)[i]);
		}
	}
}


/*
====================
StudioSaveBones

====================
*/
// orig client 0xc2540. Not byte-identical to SDK 2.3's (RC2's members moved), but it reads the same
void CStudioModelRenderer::StudioSaveBones()
{
	int i;

	mstudiobone_t* pbones;
	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_nCachedBones = m_pStudioHeader->numbones;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		strcpy(m_nCachedBoneNames[i], pbones[i].name);
		MatrixCopy((*m_pbonetransform)[i], m_rgCachedBoneTransform[i]);
		MatrixCopy((*m_plighttransform)[i], m_rgCachedLightTransform[i]);
	}
}


/*
====================
StudioMergeBones

====================
*/
// orig client 0xc2660. RC2: the root bone follows curstate.scale
void CStudioModelRenderer::StudioMergeBones(model_t* m_pSubModel)
{
	int i, j;
	double f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	float bonematrix[3][4];
	static vec4_t q[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame(pseqdesc);

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim(m_pSubModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);


	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		for (j = 0; j < m_nCachedBones; j++)
		{
			if (stricmp(pbones[i].name, m_nCachedBoneNames[j]) == 0)
			{
				MatrixCopy(m_rgCachedBoneTransform[j], (*m_pbonetransform)[i]);
				MatrixCopy(m_rgCachedLightTransform[j], (*m_plighttransform)[i]);
				break;
			}
		}
		if (j >= m_nCachedBones)
		{
			QuaternionMatrix(q[i], bonematrix);

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if (pbones[i].parent == -1)
			{
				// RC2: a scaled model is scaled about its origin, and a player's about the player
				// origin, 36 units above the model's
				if (m_pCurrentEntity->curstate.scale != 1.0 && m_pCurrentEntity->curstate.scale > 0)
				{
					bonematrix[2][3] *= m_pCurrentEntity->curstate.scale;
					if (0 != m_pCurrentEntity->player)
						bonematrix[2][3] += m_pCurrentEntity->curstate.scale * 36.0;
				}

				if (0 != IEngineStudio.IsHardware())
				{
					ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);
				}
				else
				{
					ConcatTransforms((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
				}
				ConcatTransforms((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);

				// Apply client-side effects to the transformation matrix. RC2 (as SDK 2.3): to the light
				// transform too, so the attachments follow the scale
				StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
				StudioFxTransform(m_pCurrentEntity, (*m_plighttransform)[i]);
			}
			else
			{
				ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
				ConcatTransforms((*m_plighttransform)[pbones[i].parent], bonematrix, (*m_plighttransform)[i]);
			}
		}
	}
}


/*
====================
StudioDrawModel

====================
*/
// orig client 0xc29d0. RC2: the attachments always; no interpolation for an entity a temporary
// entity holds; a dead player keeps his sequence, frame and playerclass; an entity flagged in
// playerclass (0x40000000) is drawn as the player in curstate.owner; a TransTexture model at
// renderamt 1 isn't drawn; and the draw origin is remembered (CLTE_SetOrigin)
bool CStudioModelRenderer::StudioDrawModel(int flags)
{
	alight_t lighting;
	Vector dir;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	m_pPlayerInfo = NULL;
	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);
	IEngineStudio.GetAliasScale(&m_fSoftwareXScale, &m_fSoftwareYScale);

	m_fDoInterp = 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	if (m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer)
	{
		entity_state_t deadplayer;

		bool result;
		bool save_interp;

		if (m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > gEngfuncs.GetMaxClients())
			return false;

		// get copy of player
		deadplayer = *(IEngineStudio.GetPlayerState(m_pCurrentEntity->curstate.renderamt - 1)); //cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[m_pCurrentEntity->curstate.renderamt-1];

		// clear weapon, movement state
		deadplayer.number = m_pCurrentEntity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy(m_pCurrentEntity->curstate.angles, deadplayer.angles);
		VectorCopy(m_pCurrentEntity->curstate.origin, deadplayer.origin);

		// RC2: the corpse's own sequence, frame and playerclass (a transformed corpse keeps its
		// form), and a negative health
		deadplayer.sequence = m_pCurrentEntity->curstate.sequence;
		deadplayer.frame = m_pCurrentEntity->curstate.frame;
		deadplayer.playerclass = m_pCurrentEntity->curstate.playerclass;
		deadplayer.health = -1;

		save_interp = m_fDoInterp;
		m_fDoInterp = false;

		// draw as though it were a player
		result = StudioDrawPlayer(flags, &deadplayer);

		m_fDoInterp = save_interp;
		return result;
	}

	// RC2: an entity that stands in for a player (playerclass bit 30) is drawn as the player
	// in curstate.owner, in the entity's place and pose
	if ((m_pCurrentEntity->curstate.playerclass & 0x40000000) != 0)
	{
		entity_state_t asplayer; // name ours

		bool result;
		bool save_interp;
		const int iOwner = m_pCurrentEntity->curstate.owner;

		if (iOwner <= 0 || iOwner > gEngfuncs.GetMaxClients())
			return false;

		asplayer = *(IEngineStudio.GetPlayerState(iOwner - 1));

		// the bit is clear while drawing, so the copy doesn't see it either
		m_pCurrentEntity->curstate.playerclass &= ~0x40000000;

		asplayer.number = iOwner;
		asplayer.weaponmodel = 0;
		asplayer.gaitsequence = 0;
		asplayer.movetype = MOVETYPE_NONE;
		VectorCopy(m_pCurrentEntity->curstate.angles, asplayer.angles);
		VectorCopy(m_pCurrentEntity->curstate.origin, asplayer.origin);
		asplayer.sequence = m_pCurrentEntity->curstate.sequence;
		asplayer.frame = m_pCurrentEntity->curstate.frame;
		asplayer.playerclass = m_pCurrentEntity->curstate.playerclass;

		save_interp = m_fDoInterp;
		m_fDoInterp = false;

		result = StudioDrawPlayer(flags, &asplayer);

		m_fDoInterp = save_interp;
		m_pCurrentEntity->curstate.playerclass |= 0x40000000;
		return result;
	}

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	StudioSetUpTransform(false);

	if ((flags & STUDIO_RENDER) != 0)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (0 == IEngineStudio.StudioCheckBBox())
			return false;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return true;
	}

	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW)
	{
		StudioMergeBones(m_pRenderModel);
	}
	else
	{
		StudioSetupBones();
	}
	StudioSaveBones();
	StudioCalcAttachments(); // RC2: always, not only for STUDIO_EVENTS

	if ((flags & STUDIO_EVENTS) != 0)
	{
		IEngineStudio.StudioClientEvents();
		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);

			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	// RC2: a TransTexture model at renderamt 1 isn't drawn (but still counts as drawn here)
	if (m_pCurrentEntity->curstate.rendermode == kRenderTransTexture && m_pCurrentEntity->curstate.renderamt == 1)
	{
		if ((flags & STUDIO_RENDER) == 0)
			return true;
	}
	else
	{
		if ((flags & STUDIO_RENDER) == 0)
			return true;

		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);

		IEngineStudio.StudioEntityLight(&lighting);

		// model and frame independant
		IEngineStudio.StudioSetupLighting(&lighting);

		// get remap colors

		m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;


		IEngineStudio.StudioSetRemapColors(m_nTopColor, m_nBottomColor);

		StudioRenderModel(false);
	}

	CLTE_SetOrigin(m_pCurrentEntity->index, m_vecModelPos.x, m_vecModelPos.y, m_vecModelPos.z);
	return true;
}

/*
====================
StudioEstimateGait

====================
*/
// orig client 0xc2f40. RC2: no gait without interpolation; a transformed player (playerclass)
// moves flat unless flying; the movement is measured from the velocity when the origin didn't
// move, and divided by the player's scale; a rider doesn't turn his legs to his movement
void CStudioModelRenderer::StudioEstimateGait(entity_state_t* pplayer)
{
	float dt;
	Vector est_velocity;

	dt = (m_clTime - m_clOldTime);
	if (dt < 0 || !m_fDoInterp)
	{
		m_flGaitMovement = 0;
		return;
	}
	if (dt > 1.0)
		dt = 1;
	else if (dt == 0)
	{
		m_flGaitMovement = 0;
		return;
	}

	if (m_pPlayerInfo->renderframe == m_nFrameCount)
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->velocity, pplayer->prediction_error, est_velocity );
	if (m_fGaitEstimation)
	{
		VectorSubtract(m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin, est_velocity);
		VectorCopy(m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin);
		if (0 != pplayer->playerclass && pplayer->movetype != MOVETYPE_FLY)
			est_velocity[2] = 0;
		m_flGaitMovement = Length(est_velocity);
		if (dt <= 0 || m_flGaitMovement / dt < 5)
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}

		// RC2: no movement measured, take the velocity
		if (m_flGaitMovement == 0)
		{
			VectorCopy(pplayer->velocity, est_velocity);
			if (0 != pplayer->playerclass)
				est_velocity[2] = 0;
			m_flGaitMovement = Length(est_velocity) * dt;
		}
	}
	else
	{
		VectorCopy(pplayer->velocity, est_velocity);
		if (0 != pplayer->playerclass)
			est_velocity[2] = 0;
		m_flGaitMovement = Length(est_velocity) * dt;
	}

	if (pplayer->scale > 0)
		m_flGaitMovement = m_flGaitMovement / pplayer->scale;

	// RC2: a rider's legs don't follow his movement
	const int iIndex = m_pCurrentEntity->index;
	if (iIndex >= 1 && iIndex <= 32 && 0 != m_pCurrentEntity->player && (*PM_GetRideInfo(iIndex) & 0x7f) != 0)
	{
		m_flGaitMovement = 0;
		est_velocity[0] = 0;
		est_velocity[1] = 0;
	}
	else if (est_velocity[1] != 0 || est_velocity[0] != 0)
	{
		m_pPlayerInfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (m_pPlayerInfo->gaityaw > 180)
			m_pPlayerInfo->gaityaw = 180;
		if (m_pPlayerInfo->gaityaw < -180)
			m_pPlayerInfo->gaityaw = -180;
		return;
	}

	float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
	if (flYawDiff > 180)
		flYawDiff -= 360;
	if (flYawDiff < -180)
		flYawDiff += 360;

	if (dt < 0.25)
		flYawDiff *= dt * 4;
	else
		flYawDiff *= dt;

	m_pPlayerInfo->gaityaw += flYawDiff;
	m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)(m_pPlayerInfo->gaityaw / 360) * 360;

	m_flGaitMovement = 0;
}

/*
====================
StudioProcessGait

====================
*/
// orig client 0xc3380. RC2: the pitch wraps over the top and is held to 30 degrees
void CStudioModelRenderer::StudioProcessGait(entity_state_t* pplayer)
{
	mstudioseqdesc_t* pseqdesc;
	float dt;
	int iBlend;
	float flYaw; // view direction relative to movement

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	// RC2: a pitch past straight up or down turns the player round instead
	while (m_pCurrentEntity->angles[PITCH] < -90.0)
	{
		m_pCurrentEntity->angles[PITCH] += 180.0;
		m_pCurrentEntity->angles[YAW] += 180.0f;
	}
	while (m_pCurrentEntity->angles[PITCH] > 90.0)
	{
		m_pCurrentEntity->angles[PITCH] -= 180.0;
		m_pCurrentEntity->angles[YAW] += 180.0f;
	}

	StudioPlayerBlend(pseqdesc, &iBlend, &m_pCurrentEntity->angles[PITCH]);

	// RC2: the model pitches at most 30 degrees
	if (m_pCurrentEntity->angles[PITCH] < -30.0)
		m_pCurrentEntity->angles[PITCH] = -30;
	else if (m_pCurrentEntity->angles[PITCH] > 30.0)
		m_pCurrentEntity->angles[PITCH] = 30;

	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[0] = iBlend;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

	// Con_DPrintf("%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	StudioEstimateGait(pplayer);

	// Con_DPrintf("%f %f\n", m_pCurrentEntity->angles[YAW], m_pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
	m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
	m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
	m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
	if (m_pCurrentEntity->angles[YAW] < -0)
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

	if (pplayer->gaitsequence >= m_pStudioHeader->numseq)
	{
		pplayer->gaitsequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
	{
		m_pPlayerInfo->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)(m_pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (m_pPlayerInfo->gaitframe < 0)
		m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}




/*
====================
StudioDrawPlayer

====================
*/
// orig client 0xc3880. RC2: a transformed player (playerclass 1-5) is drawn by his monster's
// draw; a rider sits on the shoulder of the player he rides (and is hidden from that player
// and the other rider in first person); no interpolation while a temporary entity holds the
// player; the attachments always; the weapon model may take the player's coloured shell; and
// the jetpack model (cl_visiblejetpack) is drawn on players wearing one
bool CStudioModelRenderer::StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
	alight_t lighting;
	Vector dir;
	int iRide = 0;		   // the player this one rides (name ours)
	bool bRideLeft = false; // on the left shoulder (name ours)
	int iIndex;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);
	IEngineStudio.GetAliasScale(&m_fSoftwareXScale, &m_fSoftwareYScale);

	m_nPlayerIndex = pplayer->number - 1;

	if (m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients())
		return false;

	// RC2: the transforms
	switch (m_pCurrentEntity->curstate.playerclass)
	{
	case 1:
		if (!m_pAssassinModel && !(m_pAssassinModel = gEngfuncs.CL_LoadModel("models/hassassin.mdl", &iIndex)))
			return false;
		return StudioDrawAssassin(flags, pplayer);
	case 2:
		if (!m_pHoundeyeModel && !(m_pHoundeyeModel = gEngfuncs.CL_LoadModel("models/houndeye.mdl", &iIndex)))
			return false;
		return StudioDrawHoundeye(flags, pplayer);
	case 3:
		if (!m_pControllerModel && !(m_pControllerModel = gEngfuncs.CL_LoadModel("models/controller.mdl", &iIndex)))
			return false;
		return StudioDrawController(flags, pplayer);
	case 4:
		if (!m_pStukabatModel && !(m_pStukabatModel = gEngfuncs.CL_LoadModel("models/stukabat.mdl", &iIndex)))
			return false;
		return StudioDrawStukabat(flags, pplayer);
	case 5:
		if (!m_pHeadcrabModel && !(m_pHeadcrabModel = gEngfuncs.CL_LoadModel("models/headcrab.mdl", &iIndex)))
			return false;
		return StudioDrawHeadcrab(flags, pplayer);
	}

	m_pRenderModel = IEngineStudio.SetupPlayerModel(m_nPlayerIndex);


	if (m_pRenderModel == NULL)
		return false;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	m_fDoInterp = m_pCurrentEntity->index < 1 || 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	// RC2: riders
	iIndex = m_pCurrentEntity->index;
	if (iIndex > 0 && iIndex <= 32 && 0 != m_pCurrentEntity->player)
	{
		const unsigned char ucRide = *PM_GetRideInfo(iIndex);
		iRide = ucRide & 0x7f;
		bRideLeft = (ucRide & 0x80) != 0;
		if (iRide == 0 || iRide > 32)
			iRide = 0;
	}

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	bool bSkipRide = false;
	if (pLocal)
	{
		const int iLocalRide = *PM_GetRideInfo(pLocal->index) & 0x7f;
		if (iLocalRide != 0)
		{
			// in first person, the player we ride and whoever else rides him are hidden
			if (iLocalRide == m_pCurrentEntity->index && 0 == CL_IsThirdPerson() && !g_bViewEntityOverride)
				return false;
			if (iRide == 0)
				bSkipRide = true;
			else if (iRide == iLocalRide && 0 == CL_IsThirdPerson() && !g_bViewEntityOverride)
				return false;
		}
	}

	if (!bSkipRide && iRide != 0)
	{
		// and so are the riders on us
		if (0 != gEngfuncs.pEventAPI->EV_IsLocal(iRide - 1) && 0 == CL_IsThirdPerson() && !g_bViewEntityOverride)
			return false;

		cl_entity_t* pRidden = gEngfuncs.GetEntityByIndex(iRide);
		Vector& vecSeat = g_rgvecRiderSeat[bRideLeft ? 1 : 0][iRide];

		// the seat wasn't worked out this frame or the last: work it out from the ridden player
		if ((g_rgiRiderSeatFrame[iRide] < g_iClientFrame - 1 || g_iClientFrame < g_rgiRiderSeatFrame[iRide]) && pRidden)
		{
			float flScale = 1.0;
			if (pRidden->curstate.scale > 0)
				flScale = pRidden->curstate.scale;

			Vector vecAngles(0, pRidden->angles[YAW], pRidden->angles[ROLL]);
			Vector forward, right, up;
			gEngfuncs.pfnAngleVectors(vecAngles, forward, right, up);

			const int iSide = bRideLeft ? -1 : 1;
			VectorMA(pRidden->origin, iSide * flScale * 6.0, right, vecSeat);
			vecSeat[2] = flScale * 24.0 + vecSeat[2];
		}

		VectorCopy(vecSeat, m_pCurrentEntity->origin);

		// a rider is drawn at 0.3 of the ridden player's size
		m_pCurrentEntity->curstate.scale = 0.3f;
		if (pRidden && pRidden->curstate.scale > 0)
			m_pCurrentEntity->curstate.scale = pRidden->curstate.scale * m_pCurrentEntity->curstate.scale;
	}

	if (0 != pplayer->gaitsequence)
	{
		Vector orig_angles;
		m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);

		VectorCopy(m_pCurrentEntity->angles, orig_angles);

		StudioProcessGait(pplayer);

		m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
		m_pPlayerInfo = NULL;

		StudioSetUpTransform(false);
		VectorCopy(orig_angles, m_pCurrentEntity->angles);
	}
	else
	{
		m_pCurrentEntity->curstate.controller[0] = 127;
		m_pCurrentEntity->curstate.controller[1] = 127;
		m_pCurrentEntity->curstate.controller[2] = 127;
		m_pCurrentEntity->curstate.controller[3] = 127;
		m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
		m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
		m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
		m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

		m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
		m_pPlayerInfo->gaitsequence = 0;

		StudioSetUpTransform(false);
	}

	if ((flags & STUDIO_RENDER) != 0)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (0 == IEngineStudio.StudioCheckBBox())
			return false;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return true;
	}

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
	StudioSetupBones();
	StudioSaveBones();
	m_pPlayerInfo->renderframe = m_nFrameCount;

	m_pPlayerInfo = NULL;

	StudioCalcAttachments(); // RC2: always, not only for STUDIO_EVENTS

	if ((flags & STUDIO_EVENTS) != 0)
	{
		IEngineStudio.StudioClientEvents();
		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);

			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	if ((flags & STUDIO_RENDER) != 0)
	{
		if (0 != m_pCvarHiModels->value && m_pRenderModel != m_pCurrentEntity->model)
		{
			// show highest resolution multiplayer model
			m_pCurrentEntity->curstate.body = 255;
		}

		if (!(m_pCvarDeveloper->value == 0 && gEngfuncs.GetMaxClients() == 1) && (m_pRenderModel == m_pCurrentEntity->model))
		{
			m_pCurrentEntity->curstate.body = 1; // force helmet
		}

		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);

		IEngineStudio.StudioEntityLight(&lighting);

		// model and frame independant
		IEngineStudio.StudioSetupLighting(&lighting);

		m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);

		// get remap colors
		m_nTopColor = m_pPlayerInfo->topcolor;
		m_nBottomColor = m_pPlayerInfo->bottomcolor;


		// bounds check
		if (m_nTopColor < 0)
			m_nTopColor = 0;
		if (m_nTopColor > 360)
			m_nTopColor = 360;
		if (m_nBottomColor < 0)
			m_nBottomColor = 0;
		if (m_nBottomColor > 360)
			m_nBottomColor = 360;

		IEngineStudio.StudioSetRemapColors(m_nTopColor, m_nBottomColor);

		StudioRenderModel(false);
		m_pPlayerInfo = NULL;

		// RC2: the draw origin, when this is the player's own model
		if (m_pRenderModel == m_pCurrentEntity->model)
			CLTE_SetOrigin(m_pCurrentEntity->index, m_vecModelPos.x, m_vecModelPos.y, m_vecModelPos.z);

		bool bDrawJetpack = true; // name ours
		const int iPlayer = m_pCurrentEntity->index;

		if (0 != pplayer->weaponmodel)
		{
			cl_entity_t saveent = *m_pCurrentEntity;

			model_t* pweaponmodel = IEngineStudio.GetModelByIndex(pplayer->weaponmodel);

			m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(pweaponmodel);
			IEngineStudio.StudioSetHeader(m_pStudioHeader);

			// RC2: the egon has its own backpack
			if (m_pStudioHeader && strstr(m_pStudioHeader->name, "p_egon"))
				bDrawJetpack = false;

			StudioMergeBones(pweaponmodel);

			IEngineStudio.StudioSetupLighting(&lighting);

			StudioRenderModel(true);

			StudioCalcAttachments();

			*m_pCurrentEntity = saveent;
		}

		// RC2: the jetpack
		if (0 != (int)cl_visiblejetpack->value && bDrawJetpack && iPlayer >= 1 && iPlayer <= 33 && (PM_GetAbilityFlags(iPlayer) & 8) != 0)
		{
			if (!m_pJetpackModel && !(m_pJetpackModel = gEngfuncs.CL_LoadModel("models/jetpack.mdl", &iIndex)))
				return true;

			cl_entity_t saveent = *m_pCurrentEntity;

			m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pJetpackModel);
			IEngineStudio.StudioSetHeader(m_pStudioHeader);

			StudioMergeBones(m_pJetpackModel);

			IEngineStudio.StudioSetupLighting(&lighting);

			StudioRenderModel(false);

			*m_pCurrentEntity = saveent;
		}
	}

	return true;
}

/*
====================
StudioCalcAttachments

====================
*/
// orig client 0xc5f90. RC2: for a player, first where a rider sits on each shoulder (the arm
// bones, or worked out from the angles when the model has none), then the attachments are kept
// per player too; no "Too many attachments" message before the exit
void CStudioModelRenderer::StudioCalcAttachments()
{
	int i;
	mstudioattachment_t* pattachment;

	const int iIndex = m_pCurrentEntity->index;
	if (0 != m_pCurrentEntity->player && iIndex > 0 && iIndex <= 32)
	{
		float flScale = 1.0;
		if (m_pCurrentEntity->curstate.scale > 0)
			flScale = m_pCurrentEntity->curstate.scale;

		mstudiobone_t* pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);
		const int iNumBones = m_pStudioHeader->numbones;

		// right shoulder
		for (i = 0; i < iNumBones; i++)
		{
			if (0 == strcmp(pbones[i].name, "Bip01 R Arm"))
				break;
		}
		if (i < iNumBones)
		{
			Vector vecZero(0, 0, 0);
			VectorTransform(vecZero, (*m_plighttransform)[i], g_rgvecRiderSeat[0][iIndex]);
		}
		else if (g_rgiRiderSeatFrame[iIndex] != g_iClientFrame)
		{
			Vector forward, right, up;
			gEngfuncs.pfnAngleVectors(m_pCurrentEntity->angles, forward, right, up);
			VectorMA(m_pCurrentEntity->origin, flScale * 6.0, right, g_rgvecRiderSeat[0][iIndex]);
			g_rgvecRiderSeat[0][iIndex][2] += flScale * 24.0;
		}

		// left shoulder
		for (i = 0; i < iNumBones; i++)
		{
			if (0 == strcmp(pbones[i].name, "Bip01 L Arm"))
				break;
		}
		if (i < iNumBones)
		{
			Vector vecZero(0, 0, 0);
			VectorTransform(vecZero, (*m_plighttransform)[i], g_rgvecRiderSeat[1][iIndex]);
		}
		else if (g_rgiRiderSeatFrame[iIndex] != g_iClientFrame)
		{
			Vector forward, right, up;
			gEngfuncs.pfnAngleVectors(m_pCurrentEntity->angles, forward, right, up);
			VectorMA(m_pCurrentEntity->origin, flScale * -6.0, right, g_rgvecRiderSeat[1][iIndex]);
			g_rgvecRiderSeat[1][iIndex][2] += flScale * 24.0;
		}

		g_rgiRiderSeatFrame[iIndex] = g_iClientFrame;
	}

	if (m_pStudioHeader->numattachments > 4)
	{
		exit(-1);
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t*)((byte*)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		VectorTransform(pattachment[i].org, (*m_plighttransform)[pattachment[i].bone], m_pCurrentEntity->attachment[i]);

		// RC2: and per player
		if (0 != m_pCurrentEntity->player && m_pCurrentEntity->index > 0 && m_pCurrentEntity->index <= 32)
		{
			VectorCopy(m_pCurrentEntity->attachment[i], g_rgvecPlayerAttachment[m_pCurrentEntity->index][i]);
		}
	}
}

/*
====================
StudioRenderModel

====================
*/
// orig client 0xc6710. RC2: bShellColor draws the model a second time in the player's coloured
// shell (CLTE_GetPlayerOverride), and the RC2 shells (StudioRenderShells) come last
void CStudioModelRenderer::StudioRenderModel(bool bShellColor)
{
	IEngineStudio.SetChromeOrigin();
	IEngineStudio.SetForceFaceFlags(0);

	if (bShellColor)
	{
		const byte r = m_pCurrentEntity->curstate.rendercolor.r;
		const byte g = m_pCurrentEntity->curstate.rendercolor.g;
		const byte b = m_pCurrentEntity->curstate.rendercolor.b;
		const int renderamt = m_pCurrentEntity->curstate.renderamt;
		float vecColor[3];
		float flAmount;

		if (CLTE_GetPlayerOverride(m_pCurrentEntity->index, vecColor, &flAmount))
		{
			StudioRenderFinal();

			if (0 == IEngineStudio.IsHardware())
			{
				gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
			}

			IEngineStudio.SetForceFaceFlags(STUDIO_NF_CHROME);

			gEngfuncs.pTriAPI->SpriteTexture(m_pChromeSprite, 0);

			const int renderfx = m_pCurrentEntity->curstate.renderfx;
			m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
			m_pCurrentEntity->curstate.rendercolor.r = (int)vecColor[0];
			m_pCurrentEntity->curstate.rendercolor.g = (int)vecColor[1];
			m_pCurrentEntity->curstate.rendercolor.b = (int)vecColor[2];
			int iAmount = (int)flAmount;
			if (iAmount < 0)
				iAmount = 0;
			else if (iAmount > 255)
				iAmount = 255;
			m_pCurrentEntity->curstate.renderamt = iAmount;

			StudioRenderFinal();
			if (0 == IEngineStudio.IsHardware())
			{
				gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
			}

			m_pCurrentEntity->curstate.renderfx = renderfx;
			m_pCurrentEntity->curstate.rendercolor.r = r;
			m_pCurrentEntity->curstate.rendercolor.g = g;
			m_pCurrentEntity->curstate.rendercolor.b = b;
			m_pCurrentEntity->curstate.renderamt = renderamt;

			StudioRenderShells();
			return;
		}
	}

	if (m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell)
	{
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
		StudioRenderFinal();

		if (0 == IEngineStudio.IsHardware())
		{
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		}

		IEngineStudio.SetForceFaceFlags(STUDIO_NF_CHROME);

		gEngfuncs.pTriAPI->SpriteTexture(m_pChromeSprite, 0);
		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;

		StudioRenderFinal();
		if (0 == IEngineStudio.IsHardware())
		{
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		}
	}
	else
	{
		StudioRenderFinal();
	}

	StudioRenderShells();
}

/*
====================
StudioRenderFinal_Software

====================
*/
// orig client 0xc69c0. Not byte-identical to SDK 2.3's (RC2's members moved), but it reads the same
void CStudioModelRenderer::StudioRenderFinal_Software()
{
	int i;

	// Note, rendermode set here has effect in SW
	IEngineStudio.SetupRenderer(0);

	if (m_pCvarDrawEntities->value == 2)
	{
		IEngineStudio.StudioDrawBones();
	}
	else if (m_pCvarDrawEntities->value == 3)
	{
		IEngineStudio.StudioDrawHulls();
	}
	else
	{
		for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
		{
			IEngineStudio.StudioSetupModel(i, (void**)&m_pBodyPart, (void**)&m_pSubModel);
			IEngineStudio.StudioDrawPoints();
		}
	}

	if (m_pCvarDrawEntities->value == 4)
	{
		gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		IEngineStudio.StudioDrawHulls();
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	}

	if (m_pCvarDrawEntities->value == 5)
	{
		IEngineStudio.StudioDrawAbsBBox();
	}

	IEngineStudio.RestoreRenderer();
}

/*
====================
StudioRenderFinal_Hardware

====================
*/
// orig client 0xc6ab0 (not in files-client.tsv: Ghidra missed it). Reads as SDK 2.3's; members moved
void CStudioModelRenderer::StudioRenderFinal_Hardware()
{
	int i;
	int rendermode;

	rendermode = 0 != IEngineStudio.GetForceFaceFlags() ? kRenderTransAdd : m_pCurrentEntity->curstate.rendermode;
	IEngineStudio.SetupRenderer(rendermode);

	if (m_pCvarDrawEntities->value == 2)
	{
		IEngineStudio.StudioDrawBones();
	}
	else if (m_pCvarDrawEntities->value == 3)
	{
		IEngineStudio.StudioDrawHulls();
	}
	else
	{
		for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
		{
			IEngineStudio.StudioSetupModel(i, (void**)&m_pBodyPart, (void**)&m_pSubModel);

			if (m_fDoInterp)
			{
				// interpolation messes up bounding boxes.
				m_pCurrentEntity->trivial_accept = 0;
			}

			IEngineStudio.GL_SetRenderMode(rendermode);
			IEngineStudio.StudioDrawPoints();
			IEngineStudio.GL_StudioDrawShadow();
		}
	}

	if (m_pCvarDrawEntities->value == 4)
	{
		gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		IEngineStudio.StudioDrawHulls();
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	}

	IEngineStudio.RestoreRenderer();
}

/*
====================
StudioRenderFinal

====================
*/
// orig client 0xc6bf0. Not byte-identical to SDK 2.3's (RC2's vtable is longer), but it reads the same
void CStudioModelRenderer::StudioRenderFinal()
{
	if (0 != IEngineStudio.IsHardware())
	{
		StudioRenderFinal_Hardware();
	}
	else
	{
		StudioRenderFinal_Software();
	}
}

/*
====================
StudioRenderShells

====================
*/
// orig client 0xc6420, name ours. RC2: a white glow shell on an entity a temporary entity
// holds, and a magenta one (sprites/aftex.spr) on an entity flagged for it
void CStudioModelRenderer::StudioRenderShells()
{
	int iIndex;

	if (0 != g_bEntityFrozen[m_pCurrentEntity->index])
	{
		if (0 == IEngineStudio.IsHardware())
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		IEngineStudio.SetForceFaceFlags(STUDIO_NF_CHROME);

		const byte r = m_pCurrentEntity->curstate.rendercolor.r;
		const byte g = m_pCurrentEntity->curstate.rendercolor.g;
		const byte b = m_pCurrentEntity->curstate.rendercolor.b;
		const int renderamt = m_pCurrentEntity->curstate.renderamt;
		const int renderfx = m_pCurrentEntity->curstate.renderfx;

		if (!m_pWhiteSprite)
			m_pWhiteSprite = gEngfuncs.CL_LoadModel("sprites/white.spr", &iIndex);
		if (m_pWhiteSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pWhiteSprite, 0);

		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
		m_pCurrentEntity->curstate.rendercolor.r = 60;
		m_pCurrentEntity->curstate.rendercolor.g = 60;
		m_pCurrentEntity->curstate.rendercolor.b = 76;
		m_pCurrentEntity->curstate.renderamt = 4;
		StudioRenderFinal();

		if (0 == IEngineStudio.IsHardware())
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

		m_pCurrentEntity->curstate.renderfx = renderfx;
		m_pCurrentEntity->curstate.rendercolor.r = r;
		m_pCurrentEntity->curstate.rendercolor.g = g;
		m_pCurrentEntity->curstate.rendercolor.b = b;
		m_pCurrentEntity->curstate.renderamt = renderamt;
	}

	if (0 != g_abRC2AntiFriction[m_pCurrentEntity->index])
	{
		const byte g = m_pCurrentEntity->curstate.rendercolor.g;
		const byte b = m_pCurrentEntity->curstate.rendercolor.b;
		const int r = m_pCurrentEntity->curstate.rendercolor.r;
		const int renderamt = m_pCurrentEntity->curstate.renderamt;
		const int renderfx = m_pCurrentEntity->curstate.renderfx;

		if (0 == IEngineStudio.IsHardware())
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		IEngineStudio.SetForceFaceFlags(STUDIO_NF_CHROME);

		if (!m_pAftexSprite)
			m_pAftexSprite = gEngfuncs.CL_LoadModel("sprites/aftex.spr", &iIndex);
		if (m_pAftexSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pAftexSprite, 0);

		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
		m_pCurrentEntity->curstate.rendercolor.r = 255;
		m_pCurrentEntity->curstate.rendercolor.g = 0;
		m_pCurrentEntity->curstate.rendercolor.b = 255;
		m_pCurrentEntity->curstate.renderamt = 2;
		StudioRenderFinal();

		if (0 == IEngineStudio.IsHardware())
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

		m_pCurrentEntity->curstate.renderfx = renderfx;
		m_pCurrentEntity->curstate.rendercolor.r = r;
		m_pCurrentEntity->curstate.rendercolor.g = g;
		m_pCurrentEntity->curstate.rendercolor.b = b;
		m_pCurrentEntity->curstate.renderamt = renderamt;
	}
}

/*
====================
StudioSetupTransformBones

====================
*/
// orig client 0xc6c20, name ours. RC2: StudioSetupBones for a transformed player's monster
// model: no gait blend, the root bone lowered by the mode's offset (the monster's origin is
// the player hull's centre), and for the assassin (iMode 1) the spine turned by flYaw through
// three bone controllers written into the model's header
void CStudioModelRenderer::StudioSetupTransformBones(int iMode, entity_state_t* pplayer, float flYaw)
{
	int i;
	double f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	float bonematrix[3][4];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	// the assassin's spine controllers (.data 0x10214e30, name ours)
	static mstudiobonecontroller_t s_rgAssassinControllers[3];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	// sequence 5 always loops (written into the model data)
	if (m_pCurrentEntity->curstate.sequence == 5)
		pseqdesc->flags |= STUDIO_LOOPING;

	f = StudioEstimateFrame(pseqdesc);

	pbones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);

	if (iMode == 1 && m_pStudioHeader->numbones > 16)
	{
		const int seq = m_pCurrentEntity->curstate.sequence;
		if (seq == 0 || seq == 1 || seq == 3 || seq == 4)
		{
			// VERIFY: the header keeps an offset to a static array, as the original's does; on a
			// 64-bit build that offset may not fit in an int (the client is 32-bit for now)
			m_pStudioHeader->bonecontrollerindex = (int)((byte*)s_rgAssassinControllers - (byte*)m_pStudioHeader);
			mstudiobonecontroller_t* pcontrollers = (mstudiobonecontroller_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

			pbones[14].bonecontroller[3] = 0;
			pcontrollers[0].bone = 14;
			pbones[15].bonecontroller[3] = 1;
			pcontrollers[1].bone = 15;
			pbones[16].bonecontroller[3] = 2;
			pcontrollers[2].bone = 16;
			for (i = 0; i < 3; i++)
			{
				pcontrollers[i].type = STUDIO_YR;
				pcontrollers[i].start = -40;
				pcontrollers[i].end = 40;
				pcontrollers[i].rest = 127;
				pcontrollers[i].index = i;
			}
			m_pStudioHeader->numbonecontrollers = 3;

			const byte value = (int)((flYaw * (1.0 / 3.0) + 40.0) * 3.1875);
			m_pCurrentEntity->curstate.controller[0] = value;
			m_pCurrentEntity->curstate.controller[1] = value;
			m_pCurrentEntity->curstate.controller[2] = value;
			m_pCurrentEntity->curstate.controller[3] = 127;
			m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
			m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
			m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
			m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];
		}
		else
		{
			pbones[14].bonecontroller[3] = -1;
			pbones[15].bonecontroller[3] = -1;
			pbones[16].bonecontroller[3] = -1;
		}
	}

	panim = StudioGetAnim(m_pRenderModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float s;
		float dadt;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations(pos2, q2, pseqdesc, panim, f);

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos3, q3, pseqdesc, panim, f);

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos4, q4, pseqdesc, panim, f);

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if (m_fDoInterp &&
		0 != m_pCurrentEntity->latched.sequencetime &&
		(m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime) &&
		(m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq))
	{
		// blend from last sequence
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		// clip prevframe
		StudioCalcRotations(pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
				StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0;
				StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0 - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		m_pCurrentEntity->latched.prevframe = f;
	}

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
			// the root bone sits below the player origin by the mode's offset
			float flOffset;
			switch (iMode)
			{
			default:
			case 1:
				flOffset = -36;
				break;
			case 3:
				flOffset = -30;
				break;
			case 2:
			case 4:
			case 5:
				flOffset = -18;
				break;
			}
			if (iMode == 4 && pplayer->health < 0)
				flOffset = -18;
			else if (iMode == 3 && pplayer->health < 0)
				flOffset = -36;

			if (m_pCurrentEntity->curstate.scale != 1.0 && m_pCurrentEntity->curstate.scale > 0)
			{
				bonematrix[2][3] *= m_pCurrentEntity->curstate.scale;
				bonematrix[2][3] = (flOffset + 36.0) * m_pCurrentEntity->curstate.scale + bonematrix[2][3];
			}
			else
			{
				bonematrix[2][3] = flOffset + bonematrix[2][3];
			}

			if (0 != IEngineStudio.IsHardware())
			{
				ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);
			}
			else
			{
				ConcatTransforms((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
			}
			ConcatTransforms((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);

			StudioFxTransform(m_pCurrentEntity, (*m_pbonetransform)[i]);
			StudioFxTransform(m_pCurrentEntity, (*m_plighttransform)[i]);
		}
		else
		{
			ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
			ConcatTransforms((*m_plighttransform)[pbones[i].parent], bonematrix, (*m_plighttransform)[i]);
		}
	}
}

/*
====================
StudioDrawTransformFinish

====================
*/
// Not a function of the original (name ours): the end every transformed player's draw shares,
// written out in each of them there (bones, attachments, events, lighting and the model; no
// remap colours, weapon or jetpack)
bool CStudioModelRenderer::StudioDrawTransformFinish(int iMode, int flags, entity_state_t* pplayer, float flYaw)
{
	alight_t lighting;
	Vector dir;

	if ((flags & STUDIO_RENDER) != 0)
	{
		if (0 == IEngineStudio.StudioCheckBBox())
			return false;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
		{
			m_pPlayerInfo = NULL;
			return true;
		}
	}

	StudioSetupTransformBones(iMode, pplayer, flYaw);
	StudioSaveBones();
	m_pPlayerInfo->renderframe = m_nFrameCount;
	m_pPlayerInfo = NULL;

	StudioCalcAttachments();

	if ((flags & STUDIO_EVENTS) != 0)
	{
		IEngineStudio.StudioClientEvents();
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);

			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(Vector) * 4);
		}
	}

	if ((flags & STUDIO_RENDER) != 0)
	{
		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);
		IEngineStudio.StudioEntityLight(&lighting);
		IEngineStudio.StudioSetupLighting(&lighting);

		StudioRenderModel(false);

		CLTE_SetOrigin(m_pCurrentEntity->index, m_vecModelPos.x, m_vecModelPos.y, m_vecModelPos.z);
	}

	return true;
}

// The yaw half of StudioProcessGait every transform's draw repeats (name ours, not a function
// of the original): turns the model to the gait yaw and returns the spine's turn
static float StudioTransformGaitYaw(cl_entity_t* pEnt, player_info_t* pPlayerInfo, float& flGaitMovement)
{
	float flYaw = pEnt->angles[YAW] - pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		pPlayerInfo->gaityaw = pPlayerInfo->gaityaw - 180;
		flGaitMovement = -flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		pPlayerInfo->gaityaw = pPlayerInfo->gaityaw + 180;
		flGaitMovement = -flGaitMovement;
		flYaw = flYaw + 180;
	}

	pEnt->angles[YAW] = pPlayerInfo->gaityaw;
	if (pEnt->angles[YAW] < 0)
		pEnt->angles[YAW] += 360;
	pEnt->latched.prevangles[YAW] = pEnt->angles[YAW];

	return flYaw;
}

// The frame half (name ours, not a function of the original): the model's frame follows the
// gait frame. bSwimFast: in water the frame runs at three times the movement, as the houndeye's
// and headcrab's swim do (VERIFY: no linearmovement check there, as in the original).
static void StudioTransformGaitFrame(cl_entity_t* pEnt, player_info_t* pPlayerInfo, mstudioseqdesc_t* pseqdesc, float flGaitMovement, float dt, bool bAdvance, bool bSwimFast, double clTime)
{
	if (bAdvance)
	{
		if (bSwimFast)
			pPlayerInfo->gaitframe = ((flGaitMovement * 3.0) / pseqdesc->linearmovement[0]) * pseqdesc->numframes + pPlayerInfo->gaitframe;
		else if (pseqdesc->linearmovement[0] > 0)
			pPlayerInfo->gaitframe += (flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
		else
			pPlayerInfo->gaitframe += pseqdesc->fps * dt;

		pPlayerInfo->gaitframe = pPlayerInfo->gaitframe - (int)(pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
		if (pPlayerInfo->gaitframe < 0)
			pPlayerInfo->gaitframe += pseqdesc->numframes;
	}

	pEnt->latched.prevframe = (255.0 / pseqdesc->numframes) * pPlayerInfo->gaitframe;
	pEnt->curstate.frame = pEnt->latched.prevframe;
	pEnt->latched.prevanimtime = clTime;
	pEnt->curstate.animtime = pEnt->latched.prevanimtime;
}

// true when the point or 8 units below it is in water (name ours, not a function of the original)
static bool StudioTransformInWater(const Vector& vecOrigin)
{
	Vector vecPoint = vecOrigin;
	if (gEngfuncs.PM_PointContents(vecPoint, NULL) == CONTENTS_WATER)
		return true;
	vecPoint[2] = vecPoint[2] - 8.0;
	return gEngfuncs.PM_PointContents(vecPoint, NULL) == CONTENTS_WATER;
}

static float StudioTransformDt(double clTime, double clOldTime)
{
	float dt = clTime - clOldTime;
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;
	return dt;
}

/*
====================
StudioDrawAssassin

====================
*/
// orig client 0xc7600, name ours. RC2: playerclass 1, models/hassassin.mdl. The legs turn to
// the gait yaw and the spine back (StudioSetupTransformBones); sequence 5's blend follows the
// pitch; swimming (sequence 4) the body lies pitched in water
bool CStudioModelRenderer::StudioDrawAssassin(int flags, entity_state_t* pplayer)
{
	(int)gEngfuncs.pfnGetCvarFloat("developer"); // read and unused in the original

	m_pRenderModel = m_pAssassinModel;
	if (!m_pRenderModel)
		return false;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	m_fDoInterp = 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
	StudioEstimateGait(pplayer);

	float flYaw = 0;
	Vector orig_angles = m_pCurrentEntity->angles;
	Vector orig_curangles = m_pCurrentEntity->curstate.angles;

	const int seq = m_pCurrentEntity->curstate.sequence;
	if (seq == 0 || seq == 1 || seq == 3 || seq == 4)
	{
		const float dt = StudioTransformDt(m_clTime, m_clOldTime);
		flYaw = StudioTransformGaitYaw(m_pCurrentEntity, m_pPlayerInfo, m_flGaitMovement);

		if (seq == 3 || seq == 4)
		{
			mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + seq;
			StudioTransformGaitFrame(m_pCurrentEntity, m_pPlayerInfo, pseqdesc, m_flGaitMovement, dt, m_fDoInterp, false, m_clTime);
		}
	}

	float flPitch = m_pCurrentEntity->angles[PITCH];
	if (m_pCurrentEntity->curstate.sequence == 5)
	{
		// the blend follows the pitch: -22 down to 45 up
		float flAbs;
		if (flPitch < -22)
		{
			flPitch = -22;
			flAbs = -flPitch;
		}
		else if (flPitch > 45)
		{
			flPitch = 45;
			flAbs = flPitch;
		}
		else if (flPitch <= 0)
			flAbs = -flPitch;
		else
			flAbs = flPitch;

		if (flAbs < 0.1)
			m_pCurrentEntity->curstate.blending[0] = 127;
		else if (flPitch <= 0)
			m_pCurrentEntity->curstate.blending[0] = (int)((1.0 - flPitch * (-1.0 / 22.0)) * 127.0);
		else
			m_pCurrentEntity->curstate.blending[0] = (int)((flPitch * (1.0 / 45.0) + 1.0) * 127.0);
	}

	m_pCurrentEntity->angles[PITCH] = 0;
	m_pCurrentEntity->latched.prevangles[PITCH] = 0;

	if (m_pCurrentEntity->curstate.sequence == 4 && StudioTransformInWater(m_pCurrentEntity->curstate.origin))
	{
		if (flPitch < -10)
			flPitch = -10;
		else if (flPitch > 10)
			flPitch = 10;
		m_pCurrentEntity->angles[PITCH] = flPitch - 90;
		m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	}

	VectorCopy(m_pCurrentEntity->angles, m_pCurrentEntity->curstate.angles);
	StudioSetUpTransform(false);
	VectorCopy(orig_angles, m_pCurrentEntity->angles);
	VectorCopy(orig_curangles, m_pCurrentEntity->curstate.angles);

	return StudioDrawTransformFinish(1, flags, pplayer, flYaw);
}

/*
====================
StudioDrawHoundeye

====================
*/
// orig client 0xc7e90, name ours. RC2: playerclass 2, models/houndeye.mdl. Sequences 0-5 and
// 15 turn to the gait yaw (and keep it); from 3 on the frame follows the gait, sequence 3 at
// three times the pace in water; the model never pitches
bool CStudioModelRenderer::StudioDrawHoundeye(int flags, entity_state_t* pplayer)
{
	m_pRenderModel = m_pHoundeyeModel;
	if (!m_pRenderModel)
		return false;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	m_fDoInterp = 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
	StudioEstimateGait(pplayer);

	float flYaw = 0;
	const int seq = m_pCurrentEntity->curstate.sequence;
	if ((seq >= 0 && seq <= 5) || seq == 15)
	{
		const float dt = StudioTransformDt(m_clTime, m_clOldTime);
		flYaw = StudioTransformGaitYaw(m_pCurrentEntity, m_pPlayerInfo, m_flGaitMovement);

		if (m_pCurrentEntity->curstate.sequence >= 3)
		{
			mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
			const bool bSwimFast = m_fDoInterp && m_pCurrentEntity->curstate.sequence == 3 && StudioTransformInWater(m_pCurrentEntity->curstate.origin);
			StudioTransformGaitFrame(m_pCurrentEntity, m_pPlayerInfo, pseqdesc, m_flGaitMovement, dt, m_fDoInterp, bSwimFast, m_clTime);
		}
	}

	// the angles after the gait turn are the ones kept
	Vector orig_angles = m_pCurrentEntity->angles;
	Vector orig_curangles = m_pCurrentEntity->curstate.angles;

	m_pCurrentEntity->angles[PITCH] = 0;
	m_pCurrentEntity->latched.prevangles[PITCH] = 0;
	VectorCopy(m_pCurrentEntity->angles, m_pCurrentEntity->curstate.angles);
	StudioSetUpTransform(false);
	VectorCopy(orig_angles, m_pCurrentEntity->angles);
	VectorCopy(orig_curangles, m_pCurrentEntity->curstate.angles);

	return StudioDrawTransformFinish(2, flags, pplayer, flYaw);
}

/*
====================
StudioDrawController

====================
*/
// orig client 0xc8570, name ours. RC2: playerclass 3, models/controller.mdl: no gait at all,
// and the model never pitches
bool CStudioModelRenderer::StudioDrawController(int flags, entity_state_t* pplayer)
{
	m_pRenderModel = m_pControllerModel;
	if (!m_pRenderModel)
		return false;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	m_fDoInterp = 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);

	Vector orig_angles = m_pCurrentEntity->angles;
	Vector orig_curangles = m_pCurrentEntity->curstate.angles;

	m_pCurrentEntity->angles[PITCH] = 0;
	m_pCurrentEntity->latched.prevangles[PITCH] = 0;
	VectorCopy(m_pCurrentEntity->angles, m_pCurrentEntity->curstate.angles);
	StudioSetUpTransform(false);
	VectorCopy(orig_angles, m_pCurrentEntity->angles);
	VectorCopy(orig_curangles, m_pCurrentEntity->curstate.angles);

	return StudioDrawTransformFinish(3, flags, pplayer, 0);
}

/*
====================
StudioDrawStukabat

====================
*/
// orig client 0xc8890, name ours. RC2: playerclass 4, models/stukabat.mdl. Sequence 19 turns
// to the gait yaw and its frame follows the gait; the model never rolls, and pitches (at most
// 30 degrees) only in sequence 10
bool CStudioModelRenderer::StudioDrawStukabat(int flags, entity_state_t* pplayer)
{
	m_pRenderModel = m_pStukabatModel;
	if (!m_pRenderModel)
		return false;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	m_fDoInterp = 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
	StudioEstimateGait(pplayer);

	float flYaw = 0;
	if (m_pCurrentEntity->curstate.sequence == 19)
	{
		const float dt = StudioTransformDt(m_clTime, m_clOldTime);
		flYaw = StudioTransformGaitYaw(m_pCurrentEntity, m_pPlayerInfo, m_flGaitMovement);

		mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
		StudioTransformGaitFrame(m_pCurrentEntity, m_pPlayerInfo, pseqdesc, m_flGaitMovement, dt, m_fDoInterp, false, m_clTime);
	}

	Vector orig_angles = m_pCurrentEntity->angles;
	Vector orig_curangles = m_pCurrentEntity->curstate.angles;

	m_pCurrentEntity->angles[ROLL] = 0;
	m_pCurrentEntity->latched.prevangles[ROLL] = 0;
	if (m_pCurrentEntity->curstate.sequence == 10)
	{
		if (m_pCurrentEntity->angles[PITCH] < -30)
			m_pCurrentEntity->angles[PITCH] = -30;
		else if (m_pCurrentEntity->angles[PITCH] > 30)
			m_pCurrentEntity->angles[PITCH] = 30;
	}
	else
	{
		m_pCurrentEntity->angles[PITCH] = 0;
		m_pCurrentEntity->latched.prevangles[PITCH] = 0;
	}
	VectorCopy(m_pCurrentEntity->angles, m_pCurrentEntity->curstate.angles);
	StudioSetUpTransform(false);
	VectorCopy(orig_angles, m_pCurrentEntity->angles);
	VectorCopy(orig_curangles, m_pCurrentEntity->curstate.angles);

	return StudioDrawTransformFinish(4, flags, pplayer, flYaw);
}

/*
====================
StudioDrawHeadcrab

====================
*/
// orig client 0xc8f50, name ours. RC2: playerclass 5, models/headcrab.mdl. Sequences 3 and 4
// turn to the gait yaw and, while interpolating, their frame follows the gait (sequence 3 at
// three times the pace in water); the model never pitches
bool CStudioModelRenderer::StudioDrawHeadcrab(int flags, entity_state_t* pplayer)
{
	m_pRenderModel = m_pHeadcrabModel;
	if (!m_pRenderModel)
		return false;

	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	m_fDoInterp = 0 == g_bEntityFrozen[m_pCurrentEntity->index];

	m_pPlayerInfo = IEngineStudio.PlayerInfo(m_nPlayerIndex);
	StudioEstimateGait(pplayer);

	float flYaw = 0;
	const int seq = m_pCurrentEntity->curstate.sequence;
	if (seq == 3 || seq == 4)
	{
		const float dt = StudioTransformDt(m_clTime, m_clOldTime);
		flYaw = StudioTransformGaitYaw(m_pCurrentEntity, m_pPlayerInfo, m_flGaitMovement);

		// unlike the houndeye's, nothing of the frame without interpolation
		if (m_fDoInterp)
		{
			mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
			const bool bSwimFast = m_pCurrentEntity->curstate.sequence == 3 && StudioTransformInWater(m_pCurrentEntity->curstate.origin);
			StudioTransformGaitFrame(m_pCurrentEntity, m_pPlayerInfo, pseqdesc, m_flGaitMovement, dt, true, bSwimFast, m_clTime);
		}
	}

	Vector orig_angles = m_pCurrentEntity->angles;
	Vector orig_curangles = m_pCurrentEntity->curstate.angles;

	m_pCurrentEntity->angles[PITCH] = 0;
	m_pCurrentEntity->latched.prevangles[PITCH] = 0;
	VectorCopy(m_pCurrentEntity->angles, m_pCurrentEntity->curstate.angles);
	StudioSetUpTransform(false);
	VectorCopy(orig_angles, m_pCurrentEntity->angles);
	VectorCopy(orig_curangles, m_pCurrentEntity->curstate.angles);

	return StudioDrawTransformFinish(5, flags, pplayer, flYaw);
}
