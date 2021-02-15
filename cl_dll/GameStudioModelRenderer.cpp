//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <assert.h>
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
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "Exports.h"

#include "phy_corpse.h"
#include"physics.h"

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;
/*
====================
CGameStudioModelRenderer

====================
*/
CGameStudioModelRenderer::CGameStudioModelRenderer( void )
{
}
void CGameStudioModelRenderer::Init(void)
{
	CStudioModelRenderer::Init();
	InitPhysicsInterface(NULL);
	gPhysics.InitSystem(&m_clTime, m_plighttransform, &IEngineStudio);
}
int CGameStudioModelRenderer::StudioDrawRagdoll(int flags)
{
	alight_t lighting;
	vec3_t dir;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	IEngineStudio.GetTimes(&m_nFrameCount, &m_clTime, &m_clOldTime);
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);
	IEngineStudio.GetAliasScale(&m_fSoftwareXScale, &m_fSoftwareYScale);

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(m_pRenderModel);
	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	StudioSetUpTransform(0);

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!IEngineStudio.StudioCheckBBox())
			return 0;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW)
	{
		StudioMergeBones(m_pRenderModel);
	}
	else
	{
		StudioSetupBones();
		gPhysics.SetupBonesPhysically(m_pCurrentEntity->index);
		m_pCurrentEntity->origin.x = (*m_pbonetransform)[1][0][3];
		m_pCurrentEntity->origin.y = (*m_pbonetransform)[1][1][3];
		m_pCurrentEntity->origin.z = (*m_pbonetransform)[1][2][3];
	}
	StudioSaveBones();

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments();
		IEngineStudio.StudioClientEvents();
		// copy attachments into global entity array
		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);

			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(vec3_t) * 4);
		}
	}

	if (flags & STUDIO_RENDER)
	{
		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);

		IEngineStudio.StudioEntityLight(&lighting);

		// model and frame independant
		IEngineStudio.StudioSetupLighting(&lighting);

		// get remap colors

		m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;

		IEngineStudio.StudioSetRemapColors(m_nTopColor, m_nBottomColor);

		StudioRenderModel();
	}

	return 1;
}
cvar_t* _drawOriginalDead = NULL;
int CGameStudioModelRenderer::StudioDrawModel(int flags)
{
	// we need a better place to call the physics update function
#pragma region physics world update
	static int lastFrame = -1;
	IEngineStudio.GetTimes(&lastFrame, &m_clTime, &m_clOldTime);
	if (lastFrame != m_nFrameCount)
	{
		gPhysics.Update(m_clTime - m_clOldTime);
		lastFrame = m_nFrameCount;
	}
#pragma endregion

	// init
	if (!_drawOriginalDead) {
		gEngfuncs.Cvar_SetValue("r_corpse", 0);
		_drawOriginalDead = IEngineStudio.GetCvar("r_corpse");
	}
	
	/*if (!_studioOn)
	{
		gEngfuncs.Cvar_SetValue("studio_on", 1);
		gEngfuncs.Cvar_SetValue("_fade", 1);
		gEngfuncs.Cvar_SetValue("_die", 3000);
		gEngfuncs.Cvar_SetValue("play_explosion", 0);

		_studioOn = IEngineStudio.GetCvar("studio_on");
		_fade= IEngineStudio.GetCvar("_fade");
		_die = IEngineStudio.GetCvar("_die");
		play_explosion= IEngineStudio.GetCvar("play_explosion");
	}*/

#pragma region Explosion
	/*if (play_explosion->value != 0)
	{
		cl_entity_t* local = gEngfuncs.GetLocalPlayer();
		gPhysics.Explosion((Vector3*)(&local->origin), 90);
		play_explosion->value = 0;
		gEngfuncs.Con_DPrintf("play explosion at local player.\n");
	}*/
#pragma endregion
	
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	
	if (pgCorpseMgr->IsRagdollCorpse(m_pCurrentEntity))
		return StudioDrawRagdoll(flags);

	// hard coded here, we need a better way.
	if ((31 <= m_pCurrentEntity->curstate.sequence &&
		m_pCurrentEntity->curstate.sequence <= 43 &&
		strstr(m_pCurrentEntity->model->name, "scientist"))||
		(15 <= m_pCurrentEntity->curstate.sequence &&
			m_pCurrentEntity->curstate.sequence <= 19 &&
			strstr(m_pCurrentEntity->model->name, "zombie")))
	{
		// 如果实体原来没死
		if (!pgCorpseMgr->IsEntityDead(m_pCurrentEntity)) 
		{
			// init pose
			CStudioModelRenderer::StudioDrawModel(0);

			pgCorpseMgr->CreateRagdollCorpse(m_pCurrentEntity);
			pgCorpseMgr->EntityDie(m_pCurrentEntity);
		}
		else
		{
			if (_drawOriginalDead->value == 0)
				return 0;
		}
	}
	else
	{
		pgCorpseMgr->EntityRespawn(m_pCurrentEntity);
	}

#pragma region Note

	//if (IsRagdollEntity(m_pCurrentEntity))
	//	return StudioDrawRagdoll(flags);

	//bool IsRagdollModelAndInDeathSequence;//TODO
	//// 如果当前模型支持布娃娃 且 处于死亡动画
	//if (IsRagdollModelAndInDeathSequence)
	//{
	//	// 如果实体原来处于非死亡状态
	//	if (!IsEntityDying(m_pCurrentEntity->index)) 
	//	{
	//		// 到此说明正在播放首帧死亡动画
	//		// TODO: 创建临时实体，赋给它一个布娃娃控制器，激活布娃娃控制器
	//		SetEntityDying(m_pCurrentEntity->index, true);
	//	}
	//	else
	//	{
	//		// 到此说明实体早已死了，已经给它创建过布娃娃尸体实体了，就不渲染这个实体了。
	//		return 0;
	//	}
	//}
	//else 
	//{
	//	// 对于不支持的动画来说，这个步骤不影响任何东西；
	//	// 对于支持布娃娃的模型 但 正在播放非死亡动画，说明实体活着。
	//	SetEntityDying(m_pCurrentEntity->index, false);
	//}

#pragma endregion

	// 正常渲染
	return CStudioModelRenderer::StudioDrawModel(flags);
}

int CGameStudioModelRenderer::StudioDrawPlayer(int flags, entity_state_s* pplayer)
{
	return CStudioModelRenderer::StudioDrawPlayer(flags, pplayer);
}

////////////////////////////////////
// Hooks to class implementation
////////////////////////////////////

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	return g_StudioRenderer.StudioDrawPlayer( flags, pplayer );
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	return g_StudioRenderer.StudioDrawModel( flags );
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	g_StudioRenderer.Init();
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
int DLLEXPORT HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio )
{
//	RecClStudioInterface(version, ppinterface, pstudio);

	if ( version != STUDIO_INTERFACE_VERSION )
		return 0;

	// Point the engine to our callbacks
	*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy( &IEngineStudio, pstudio, sizeof( IEngineStudio ) );

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}

#pragma region phy_corpse.cpp
CorpseManager* pgCorpseMgr = nullptr;

CorpseManager::CorpseManager(void)
{
	memset(_entityDead, 0, MAX_ENTITIES);
}

bool CorpseManager::IsEntityDead(cl_entity_t* ent)
{
	return _entityDead[ent->index];
}

void CorpseManager::EntityDie(cl_entity_t* ent)
{
	_entityDead[ent->index] = true;
}

void CorpseManager::EntityRespawn(cl_entity_t* ent)
{
	_entityDead[ent->index] = false;
}

TEMPENTITY* CorpseManager::CreateRagdollCorpse(cl_entity_t* ent)
{
	TEMPENTITY* tempent = gEngfuncs.pEfxAPI->CL_TempEntAlloc(ent->curstate.origin, ent->model);
	tempent->entity.curstate.iuser1 = ent->index;
	tempent->entity.curstate.iuser3 = PhyCorpseFlag1;
	tempent->entity.curstate.iuser4 = PhyCorpseFlag2;
	tempent->entity.curstate.body = ent->curstate.body;
	tempent->entity.curstate.skin = ent->curstate.skin;
	entity_state_t* entstate = &ent->curstate;
	entity_state_t* tempstate = &tempent->entity.curstate;
	tempent->entity.angles = ent->angles;
	tempent->entity.latched = ent->latched;
	tempstate->angles = entstate->angles;
	tempstate->animtime = entstate->animtime;
	tempstate->sequence = entstate->sequence;
	tempstate->aiment = entstate->aiment;
	tempstate->frame = entstate->frame;
	

	tempent->die = 3000;
	tempent->fadeSpeed = 1;
	tempent->entity.index = _corpseIndex++;
	gPhysics.CreateRagdollControllerHeader(tempent->entity.index, IEngineStudio.Mod_Extradata(ent->model));
	gPhysics.StartRagdoll(tempent->entity.index);
	gPhysics.SetVelocity(tempent->entity.index, (Vector3*)&ent->curstate.velocity);

	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	Vector v = (ent->origin - local->origin).Normalize();
	v = v * 5;
	gPhysics.SetVelocity(tempent->entity.index, (Vector3*)&v);
	
	gEngfuncs.Con_DPrintf("corpse [%d]'s velocity is %f\n", tempent->entity.index, ent->curstate.velocity.Length());
	gEngfuncs.Con_DPrintf("create corpse [%d] for entity [%d]\n", tempent->entity.index, ent->index);
	return tempent;
}

bool CorpseManager::IsRagdollCorpse(cl_entity_t* ent)
{
	return (ent->curstate.iuser3 == PhyCorpseFlag1 &&
		ent->curstate.iuser4 == PhyCorpseFlag2);
}
#pragma endregion

