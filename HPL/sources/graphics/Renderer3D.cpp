/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of HPL1 Engine.
 *
 * HPL1 Engine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HPL1 Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HPL1 Engine.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "graphics/Renderer3D.h"

#include "math/Math.h"
#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "resources/TextureManager.h"
#include "resources/GpuProgramManager.h"
#include "graphics/VertexBuffer.h"
#include "graphics/MeshCreator.h"
#include "scene/Camera.h"
#include "scene/Entity3D.h"
#include "graphics/RenderList.h"
#include "graphics/Renderable.h"
#include "scene/World3D.h"
#include "scene/RenderableContainer.h"
#include "scene/Light3D.h"
#include "scene/Light3DSpot.h"
#include "graphics/Bitmap.h"
#include "graphics/Material_Universal.h"
#include "graphics/ogl2/PostProcess.h"
#include "graphics/ogl2/SkyBox.h"
#include "graphics/ogl2/ShadowMap.h"
#include "system/FrameTrace.h"

#include <cstdlib>

#include <algorithm>
#include <set>
#include <vector>
#include "graphics/Material_Universal.h"
#include "graphics/RenderState.h"
#include "math/BoundingVolume.h"
#include "graphics/GPUProgram.h"
#include "system/Log.h"

namespace hpl {

	// Point lights need six depth passes each, so only the few nearest the
	// camera get them - which is always the one the player is carrying.
	static const int kDefaultShadowedPointLights = 2;

	/** HPL_SHADOW_BUDGET overrides how many point lights get a cube map. */
	static int MaxShadowedPointLights()
	{
		static const int lBudget = []() {
			const char *pEnv = getenv("HPL_SHADOW_BUDGET");
			if(pEnv == NULL) return kDefaultShadowedPointLights;
			const long lValue = strtol(pEnv, NULL, 10);
			return (lValue >= 0) ? (int)lValue : kDefaultShadowedPointLights;
		}();
		return lBudget;
	}

	/** HPL_POINT_SHADOWS=0 turns point light shadows off for comparison. */
	static bool PointShadowsEnabled()
	{
		static const bool bEnabled = []() {
			const char *pEnv = getenv("HPL_POINT_SHADOWS");
			return pEnv == NULL || pEnv[0] != '0';
		}();
		return bEnabled;
	}
	static const float kShadowCubeNear = 0.2f;


	//////////////////////////////////////////////////////////////////////////
	// RENDER SETTINGS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRenderSettings::cRenderSettings()
	{
		mpProgramOverride = NULL;
		mbNeedsLightingMatrices = false;

		mbFogActive = false;
		mfFogStart = 5.0f;
		mfFogEnd = 5.0f;
		mFogColor = cColor(1,1);
		mbFogCulling = false;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRenderer3D::cRenderer3D(iLowLevelGraphics *apLowLevelGraphics, cTextureManager* textureManager, cGpuProgramManager* programManager)
	{
		Log("  Creating Renderer3D\n");

		_llGfx = apLowLevelGraphics;
		_programManager = programManager;
		_textureManager = textureManager;

		mpSkyBoxTexture = NULL;
		mbAutoDestroySkybox = false;
		mbSkyBoxActive = false;
		mSkyBoxColor = cColor(1,1);

		mRenderSettings.mAmbientColor = cColor(0,1);

		mpRenderList = new cRenderList{};

		mDebugFlags = 0;

		mbLog = false;

		mfRenderTime =0;

		//Set up render settings.
		mRenderSettings.mpLowLevel = _llGfx;
		mRenderSettings.mbLog = false;
		mRenderSettings.mShowShadows = eRendererShowShadows_All;

		Log("   Load Renderer3D gpu programs:\n");

		///////////////////////////////////
		//Load diffuse program, for stuff like query rendering
		Log("    Diffuse\n");
		mpDiffuseProgram = _programManager->CreateProgram("Universal.vert", "Universal.frag");
		if(mpDiffuseProgram==NULL)
		{
			Error("Couldn't load Diffuse shader\n");
		}

		mpLightProgram = _programManager->CreateProgram("Light.vert", "Light.frag");
		if(mpLightProgram==NULL)
		{
			Error("Could not load light program - the scene will stay unlit!\n");
		}
		else
		{
			mpLightProgram->Bind();
			mpLightProgram->SetTextureBindingIndex("shadowMap", 2);
			mpLightProgram->SetTextureBindingIndex("shadowCube", 3);
			mpLightProgram->SetTextureBindingIndex("normalMap", 4);
			mpLightProgram->UnBind();
		}

		mpDepthProgram = _programManager->CreateProgram("PreZ.vert", "Depth.frag");
		// A single flat-normal texel, handed to every material that has no
		// normal map of its own (see Material_Universal::GetTexture).
		{
			const uint32_t lFlat = 0xFFFF8080u;	// RGBA 128,128,255,255
			Bitmap flatBmp;
			flatBmp.CreateFromRGBAPixels((void*)&lFlat, 1, 1);

			mpFlatNormalMap = _llGfx->CreateTexture("FlatNormal", eTextureTarget_2D);
			if(mpFlatNormalMap && mpFlatNormalMap->CreateFromBitmap(flatBmp))
				Material_Universal::SetFlatNormalMap(mpFlatNormalMap);
			else
				Error("Could not create the flat normal map - bump lighting will be wrong\n");
		}

		mfGamma = 1.0f;
		mfBloomAmount = 0.0f;
		mpPostProcess = new cPostProcess();
		mpPostProgram = _programManager->CreateProgram("Post.vert", "Post.frag");
		if(mpPostProgram == NULL)
			Error("Could not load the post-process program; gamma and bloom are off\n");

		mpSkyBoxDrawer = new cSkyBoxDrawer();
		mpSkyProgram = _programManager->CreateProgram("Sky.vert", "Sky.frag");
		if(mpSkyProgram == NULL)
			Error("Could not load the sky program; windows will show black\n");

		mpShadowMap = new cShadowMap(1024);
		mpShadowCube = new cShadowMapCube(512);
		if(mpDepthProgram == NULL || mpShadowMap->IsValid() == false)
		{
			Error("Could not set up shadow mapping - lights will not cast shadows\n");
		}

		/////////////////////////////////////////////
		//Create sky box graphics.

		Log("   init sky box\n");
		InitSkyBox();

		Log("  Renderer3D created\n");
	}

	//-----------------------------------------------------------------------

	cRenderer3D::~cRenderer3D()
	{
		delete mpRenderList;

		if(mpDiffuseProgram) _programManager->Destroy(mpDiffuseProgram);
		if(mpLightProgram) _programManager->Destroy(mpLightProgram);
		if(mpDepthProgram) _programManager->Destroy(mpDepthProgram);
		delete mpShadowMap;
		delete mpShadowCube;
		delete mpFlatNormalMap;
		delete mpPostProcess;
		delete mpSkyBoxDrawer;
		if(mpSkyProgram) _programManager->Destroy(mpSkyProgram);
		if(mpPostProgram) _programManager->Destroy(mpPostProgram);

		if(mpSkyBox) delete mpSkyBox;
		if(mpSkyBoxTexture && mbAutoDestroySkybox)
		{
			_textureManager->Destroy(mpSkyBoxTexture);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cRenderSettings::Clear()
	{
		mbDepthTest = true;

		mAlphaMode = eMaterialAlphaMode_Solid;
		mBlendMode = eMaterialBlendMode_None;
		mChannelMode = eMaterialChannelMode_RGBA;

		mpProgram = NULL;
		mpProgramOverride = NULL;
		mbNeedsLightingMatrices = false;

		mpSector = NULL;

		for(int i=0;i<MAX_TEXTUREUNITS;i++)
		{
			mpTexture[i] = NULL;
		}

		mpVtxBuffer = NULL;
	}

	void cRenderSettings::Reset(iLowLevelGraphics *apLowLevel)
	{
		if(mpProgram) mpProgram->UnBind();
		if(mpVtxBuffer) mpVtxBuffer->UnBind();

		for(int i=0;i<MAX_TEXTUREUNITS;i++)
		{
			if(mpTexture[i])
			{
				apLowLevel->SetTexture(i,NULL);
			}
		}

		Clear();
	}

	//-----------------------------------------------------------------------

	eRendererShowShadows cRenderer3D::GetShowShadows()
	{
		return mRenderSettings.mShowShadows;
	}
	void cRenderer3D::SetShowShadows(eRendererShowShadows aState)
	{
		mRenderSettings.mShowShadows = aState;
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::UpdateRenderList(cWorld3D* apWorld, cCamera *apCamera, float afFrameTime)
	{
		//Clear all objects to be rendereded
		mpRenderList->Clear();

		//Set some variables
		mpRenderList->SetFrameTime(afFrameTime);
		mpRenderList->SetCamera(apCamera);

		//Set the frustum
		mRenderSettings.mpFrustum = apCamera->GetFrustum();

		//Setup fog BV
		if(mRenderSettings.mbFogActive && mRenderSettings.mbFogCulling)
		{
			//This is becuase the fog line is a stright line infront of the camera.
			float fCornerDist = (mRenderSettings.mfFogEnd *2.0f) /
								cos(apCamera->GetFOV()*apCamera->GetAspect()*0.5f);

			mFogBV.SetSize(fCornerDist);
			mFogBV.SetPosition(apCamera->GetPosition());
		}

		//Add all objects to be rendered
		apWorld->GetRenderContainer()->GetVisible(mRenderSettings.mpFrustum, mpRenderList);

		//Compile an optimized render list.
		mpRenderList->Compile();
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderWorld(cWorld3D* apWorld, cCamera *apCamera, float afFrameTime)
	{
		mfRenderTime += afFrameTime;

		//////////////////////////////
		//Setup render settings and logging
		if(mDebugFlags & eRendererDebugFlag_LogRendering) {
			mbLog = true;
			mRenderSettings.mbLog = true;
		}
		else if(mbLog)
		{
			mbLog = false;
			mRenderSettings.mbLog = false;
		}
		mRenderSettings.mDebugFlags = mDebugFlags;

		/////////////////////////////////
		//Set up rendering
		_llGfx->SetCullActive(true);
		_llGfx->SetDepthTestActive(true);
		_llGfx->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);

		mRenderSettings.mpCamera = apCamera;

		for (int i=0; i < MAX_TEXTUREUNITS; ++i)
			_llGfx->SetTexture(i, NULL);

		mRenderSettings.Clear();

		////////////////////////////
		// Render Z
//		RenderZ(apCamera);

		////////////////////////////
		//Render Diffuse
		RenderDiffuse(apCamera);

		////////////////////////////
		//Render Occlusion Queries
		RenderOcclusionQueries(apCamera);

		////////////////////////////
		//Render lighting
		RenderLight(apCamera);


		////////////////////////////
		//Render sky box
		RenderSkyBox(apCamera);

		//Render transparent
		RenderTrans(apCamera);

		mRenderSettings.Reset(_llGfx);

		////////////////////////////
		//Render debug
//		RenderDebug(apCamera);
//		RenderPhysicsDebug(apWorld, apCamera);

		_llGfx->SetColorWriteActive(true, true, true, true);
		_llGfx->SetDepthWriteActive(true);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::SetSkyBox(iTexture *apTexture, bool abAutoDestroy)
	{
		if(mpSkyBoxTexture && mbAutoDestroySkybox)
		{
			_textureManager->Destroy(mpSkyBoxTexture);
		}

		mbAutoDestroySkybox = abAutoDestroy;
		mpSkyBoxTexture = apTexture;
		if(mpSkyBoxTexture)
		{
			mpSkyBoxTexture->SetWrapS(eTextureWrap_ClampToEdge);
			mpSkyBoxTexture->SetWrapT(eTextureWrap_ClampToEdge);
		}
	}

	void cRenderer3D::SetSkyBoxActive(bool abX)
	{
		mbSkyBoxActive = abX;
	}

	void cRenderer3D::SetSkyBoxColor(const cColor& aColor)
	{
		if(mSkyBoxColor == aColor) return;
		mSkyBoxColor = aColor;

		float *pColors = mpSkyBox->GetArray(VertexAttr_Color0);
		int colorStride = mpSkyBox->GetArrayStride(VertexAttr_Color0);

		int lNum = mpSkyBox->GetVertexCount();
		for(int i=0; i<lNum;++i)
		{
			pColors[0] = mSkyBoxColor.r;
			pColors[1] = mSkyBoxColor.g;
			pColors[2] = mSkyBoxColor.b;
			pColors[3] = mSkyBoxColor.a;
			pColors += colorStride;
		}

		mpSkyBox->UpdateData(VertexMask_Color0, false);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::SetFogActive(bool abX)
	{
		mRenderSettings.mbFogActive = abX;
	}
	void cRenderer3D::SetFogStart(float afX)
	{
		mRenderSettings.mfFogStart = afX;
	}
	void cRenderer3D::SetFogEnd(float afX)
	{
		mRenderSettings.mfFogEnd = afX;
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::FetchOcclusionQueries()
	{
		if(mbLog) Log("Fetching Occlusion Queries Result:\n");

		//With depth test
		cOcclusionQueryObjectIterator it = mpRenderList->GetQueryIterator();
		while(it.HasNext())
		{
			cOcclusionQueryObject *pObject = it.Next();
			//LogUpdate("Query: %d!\n",pObject->mpQuery);

			while(pObject->mpQuery->FetchResults()==false);

			if(mbLog) Log(" Query: %d SampleCount: %d\n",	pObject->mpQuery,
															pObject->mpQuery->GetSampleCount());
		}

		if(mbLog) Log("Done fetching queries\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cRenderer3D::InitSkyBox()
	{
		mpSkyBox = CreateSkyBoxVertexBuffer(_llGfx, 1);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderSkyBox(cCamera *apCamera)
	{
		if(mbSkyBoxActive == false || mpSkyProgram == NULL) return;
		if(mbLog) Log("Rendering Skybox:\n");

		// Only the rotation matters - the sky is infinitely far away.
		cMatrixf mtxViewRot = apCamera->GetViewMatrix();
		mtxViewRot.SetTranslation(cVector3f(0,0,0));

		const cMatrixf mtxInvViewProj = cMath::MatrixInverse(
			cMath::MatrixMul(apCamera->GetProjectionMatrix(), mtxViewRot));

		mpSkyBoxDrawer->Draw(mpSkyProgram, mtxInvViewProj, mSkyBoxColor, mpSkyBoxTexture);

		// Drawn outside the state tree, so its cache no longer reflects reality.
		mRenderSettings.mpProgram = NULL;
		mRenderSettings.mpVtxBuffer = NULL;
		mRenderSettings.mpTexture[0] = NULL;
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderZ(cCamera *apCamera)
	{
		if(mbLog) Log("Rendering ZBuffer:\n");

		//_llGfx->SetDepthTestFunc(eDepthTestFunc_Equal);
		_llGfx->SetColorWriteActive(false, false, false,false);
		mRenderSettings.mChannelMode = eMaterialChannelMode_Z;

		cRenderNode* pNode = mpRenderList->GetRootNode();
		pNode->Render(&mRenderSettings);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderOcclusionQueries(cCamera *apCamera)
	{
		if(mbLog) Log("Rendering Occlusion Queries:\n");
		_llGfx->SetColorWriteActive(false, false, false, false);
		mRenderSettings.mChannelMode = eMaterialChannelMode_Z;
		_llGfx->SetDepthWriteActive(false);

		////////////////////////////
		// Program
		if(mRenderSettings.mpProgram != mpDiffuseProgram)
		{
			if(mRenderSettings.mpProgram) mRenderSettings.mpProgram->UnBind();
			mRenderSettings.mpProgram = mpDiffuseProgram;

			mpDiffuseProgram->Bind();
			if(mbLog) Log(" Binding program %d\n",mpDiffuseProgram);
		}

		////////////////////////
		// Reset texture unit 0
		_llGfx->SetTexture(0,NULL);
		mRenderSettings.mpTexture[0] = NULL;

		////////////////////////
		// Keep track of what has been set
		iVertexBuffer *pPrevBuffer = mRenderSettings.mpVtxBuffer;
		cMatrixf viewProjMatrix = cMath::MatrixMul(apCamera->GetProjectionMatrix(), apCamera->GetViewMatrix());
		bool bPrevDepthTest = true;

		//////////////////////////////////
		//Iterate the query objects
		cOcclusionQueryObjectIterator it = mpRenderList->GetQueryIterator();
		while(it.HasNext())
		{
			cOcclusionQueryObject *pObject = it.Next();

			/*if(pObject->mbDepthTest) {
				mpLowLevelGraphics->SetColorWriteActive(true, true,true,true);
				mRenderSettings.mChannelMode = eMaterialChannelMode_RGBA;
			}
			else {
				mpLowLevelGraphics->SetColorWriteActive(false, false, false, false);
				mRenderSettings.mChannelMode = eMaterialChannelMode_Z;
			}*/


			/////////////////////
			//Set depth test
			if(bPrevDepthTest != pObject->mbDepthTest)
			{
				//mpLowLevelGraphics->SetDepthTestActive(pObject->mbDepthTest);
				if(pObject->mbDepthTest)
					_llGfx->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
				else
					_llGfx->SetDepthTestFunc(eDepthTestFunc_Always);

				bPrevDepthTest = pObject->mbDepthTest;
				if(mbLog) Log(" Setting depth test %d\n",pObject->mbDepthTest?1:0);
			}

			/////////////////////
			//Set matrix
			cMatrixf mvpMat = cMath::MatrixMul(viewProjMatrix, pObject->_matrix);
			mpDiffuseProgram->SetMatrixf("worldViewProj", mvpMat);
			//if(mbLog) Log(" Setting matrix %d\n",pObject->_matrix);

			/////////////////////
			//Set Vertex buffer and draw
			if(pPrevBuffer != pObject->mpVtxBuffer)
			{
				if(pPrevBuffer) pPrevBuffer->UnBind();
				pObject->mpVtxBuffer->Bind();
				pPrevBuffer = pObject->mpVtxBuffer;

				if(mbLog) Log(" Setting vtx buffer %d\n",pObject->mpVtxBuffer);
			}

			pObject->mpQuery->Begin();
			pObject->mpVtxBuffer->Draw();
			pObject->mpQuery->End();

			if(mbLog) Log(" Render with query: %d\n",pObject->mpQuery);
		}

		mRenderSettings.mpVtxBuffer = pPrevBuffer;

		//if(bPrevDepthTest==false)
		//	mpLowLevelGraphics->SetDepthTestActive(true);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderShadowMap(cLight3DSpot *apLight)
	{
		mpShadowMap->BeginRender();
		mpDepthProgram->Bind();

		const cMatrixf mtxLightViewProj = apLight->GetViewProjMatrix();

		// The casters were gathered from the light's own point of view by
		// cPortalContainer::AddLightShadowCasters, so this covers geometry the
		// camera cannot see - which is exactly what casts the shadow in.
		const bool bWithDynamic = (mRenderSettings.mShowShadows == eRendererShowShadows_All);

		for(int lPass=0; lPass<2; ++lPass)
		{
			if(lPass == 1 && bWithDynamic == false) break;

			const tCasterCacheSet &setCasters = (lPass == 0)
				? apLight->GetStaticCasters()
				: apLight->GetDynamicCasters();

			for(iRenderable *pObject : setCasters)
			{
				iVertexBuffer *pVtxBuffer = pObject->GetVertexBuffer();
				if(pVtxBuffer == NULL) continue;

				const cMatrixf mtxModel = pObject->GetModelMatrix(NULL);
				mpDepthProgram->SetMatrixf("worldViewProj",
					cMath::MatrixMul(mtxLightViewProj, mtxModel));

				pVtxBuffer->Bind();
				pVtxBuffer->Draw();
				pVtxBuffer->UnBind();
			}
		}

		mpDepthProgram->UnBind();

		const cVector2f vScreenSize = _llGfx->GetScreenSize();
		mpShadowMap->EndRender((int)vScreenSize.x, (int)vScreenSize.y);

		// The tree caches what it last bound; the pass above went around it.
		mRenderSettings.mpVtxBuffer = NULL;
		mRenderSettings.mpProgram = NULL;
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::BeginPostProcess()
	{
		if(mpPostProgram == NULL) return;

		const cVector2f vSize = _llGfx->GetScreenSize();
		if(mpPostProcess->Resize((int)vSize.x, (int)vSize.y) == false) return;

		mpPostProcess->Begin();
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::ResolvePostProcess()
	{
		if(mpPostProgram == NULL || mpPostProcess->IsValid() == false) return;

		// HPL_GAMMA and HPL_BLOOM override the config so the two can be tuned
		// without a rebuild.
		static const float sfGammaOverride = []() {
			const char *p = getenv("HPL_GAMMA");
			return p ? (float)atof(p) : -1.0f;
		}();
		static const float sfBloomOverride = []() {
			const char *p = getenv("HPL_BLOOM");
			return p ? (float)atof(p) : -1.0f;
		}();

		mpPostProcess->Resolve(mpPostProgram,
			sfGammaOverride > 0.0f ? sfGammaOverride : mfGamma,
			sfBloomOverride >= 0.0f ? sfBloomOverride : mfBloomAmount);

		// The resolve drew with its own program and no vertex buffer.
		mRenderSettings.mpProgram = NULL;
		mRenderSettings.mpVtxBuffer = NULL;
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderShadowCube(iLight3D *apLight)
	{
		const cVector3f vLightPos = apLight->GetWorldPosition();
		const float fFar = apLight->GetFarAttenuation();
		const float fNear = kShadowCubeNear;

		// 90 degree frustum, square aspect: the generic form collapses to this.
		// The face vectors below are the standard GL cube orientations, which
		// already assume an ordinary right-handed look-at - no handedness flip.
		const float Z = -(fFar + fNear) / (fFar - fNear);
		const float C = -(2.0f * fFar * fNear) / (fFar - fNear);
		const cMatrixf mtxProj(1,0,0,0,
							   0,1,0,0,
							   0,0,Z,C,
							   0,0,-1,0);

		// Face order must match GL_TEXTURE_CUBE_MAP_POSITIVE_X and onwards, or
		// the lookup direction in the shader lands on the wrong face.
		static const cVector3f vFaceDir[6] = {
			cVector3f( 1, 0, 0), cVector3f(-1, 0, 0),
			cVector3f( 0, 1, 0), cVector3f( 0,-1, 0),
			cVector3f( 0, 0, 1), cVector3f( 0, 0,-1)
		};
		static const cVector3f vFaceUp[6] = {
			cVector3f( 0,-1, 0), cVector3f( 0,-1, 0),
			cVector3f( 0, 0, 1), cVector3f( 0, 0,-1),
			cVector3f( 0,-1, 0), cVector3f( 0,-1, 0)
		};

		mpShadowCube->BeginRender();
		mpDepthProgram->Bind();

		const bool bWithDynamic = (mRenderSettings.mShowShadows == eRendererShowShadows_All);

		for(int lFace=0; lFace<6; ++lFace)
		{
			mpShadowCube->BeginFace(lFace);

			const cVector3f vForward = vFaceDir[lFace];
			const cVector3f vRight = cMath::Vector3Normalize(cMath::Vector3Cross(vForward, vFaceUp[lFace]));
			const cVector3f vUp = cMath::Vector3Cross(vRight, vForward);

			const cMatrixf mtxView(
				vRight.x,   vRight.y,   vRight.z,   -cMath::Vector3Dot(vRight, vLightPos),
				vUp.x,      vUp.y,      vUp.z,      -cMath::Vector3Dot(vUp, vLightPos),
				-vForward.x,-vForward.y,-vForward.z, cMath::Vector3Dot(vForward, vLightPos),
				0,0,0,1);

			const cMatrixf mtxViewProj = cMath::MatrixMul(mtxProj, mtxView);

			for(int lPass=0; lPass<2; ++lPass)
			{
				if(lPass == 1 && bWithDynamic == false) break;

				const tCasterCacheSet &setCasters = (lPass == 0)
					? apLight->GetStaticCasters()
					: apLight->GetDynamicCasters();

				for(iRenderable *pObject : setCasters)
				{
					iVertexBuffer *pVtxBuffer = pObject->GetVertexBuffer();
					if(pVtxBuffer == NULL) continue;

					mpDepthProgram->SetMatrixf("worldViewProj",
						cMath::MatrixMul(mtxViewProj, pObject->GetModelMatrix(NULL)));

					pVtxBuffer->Bind();
					pVtxBuffer->Draw();
					pVtxBuffer->UnBind();
				}
			}
		}

		mpDepthProgram->UnBind();

		const cVector2f vScreenSize = _llGfx->GetScreenSize();
		mpShadowCube->EndRender((int)vScreenSize.x, (int)vScreenSize.y);

		mRenderSettings.mpVtxBuffer = NULL;
		mRenderSettings.mpProgram = NULL;
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderLight(cCamera *apCamera)
	{
		if(mDebugFlags & eRendererDebugFlag_DisableLighting) return;
		if(mpLightProgram == NULL) return;
		if(mbLog) Log("Rendering Lighting:\n");

		mRenderSettings.mChannelMode = eMaterialChannelMode_RGBA;
		_llGfx->SetColorWriteActive(true, true, true, true);

		// The ambient pass already laid down exact depth for this geometry, so
		// match it rather than write it again, and sum each light on top.
		_llGfx->SetDepthTestFunc(eDepthTestFunc_Equal);
		_llGfx->SetDepthWriteActive(false);
		ApplyBlendMode(_llGfx, eMaterialBlendMode_Add);
		mRenderSettings.mBlendMode = eMaterialBlendMode_Add;

		// Drive the whole tree with the light shader instead of each material's
		// own program, and bind it once for every light.
		mpLightProgram->Bind();
		mpLightProgram->SetFloat("alphaCutoff", 0.6f);

		static const float sfUseBump = []() {
			const char *p = getenv("HPL_BUMP");
			return (p && p[0] == '0') ? 0.0f : 1.0f;
		}();
		mpLightProgram->SetFloat("useBump", sfUseBump);

		mRenderSettings.mpProgram = mpLightProgram;
		mRenderSettings.mpProgramSetup = NULL;
		mRenderSettings.mpProgramOverride = mpLightProgram;
		mRenderSettings.mbNeedsLightingMatrices = true;

		//////////////////////////////////////////////
		// Pick the point lights that get a cube map: the nearest few, which is
		// how the light in the player's hand always ends up in the set.
		std::set<iLight3D*> setCubeShadowed;
		if(PointShadowsEnabled() && mpShadowCube && mpShadowCube->IsValid() && mpDepthProgram
			&& mRenderSettings.mShowShadows != eRendererShowShadows_None)
		{
			std::vector<std::pair<float, iLight3D*>> vCandidates;

			cLight3DIterator candIt = mpRenderList->GetLightIt();
			while(candIt.HasNext())
			{
				iLight3D *pCandidate = candIt.Next();
				if(pCandidate->GetLightType() != eLight3DType_Point) continue;
				if(pCandidate->GetCastShadows() == false) continue;

				vCandidates.push_back({
					cMath::Vector3DistSqr(pCandidate->GetWorldPosition(), apCamera->GetPosition()),
					pCandidate });
			}

			if(FrameTrace::Enabled())
				FrameTrace::Add("pointlight_candidates", (uint64_t)vCandidates.size());

			std::sort(vCandidates.begin(), vCandidates.end(),
				[](const auto &a, const auto &b) { return a.first < b.first; });

			const int lTake = std::min((int)vCandidates.size(), MaxShadowedPointLights());
			for(int i=0; i<lTake; ++i) setCubeShadowed.insert(vCandidates[i].second);
		}

		cLight3DIterator lightIt = mpRenderList->GetLightIt();

		int lLightCount=0;
		while(lightIt.HasNext())
		{
			if(lLightCount >= MAX_NUM_OF_LIGHTS) break;

			iLight3D* pLight = lightIt.Next();

			// cRenderList::Compile() never fills mvObjectsPerLight - the loop
			// that counted objects per light is commented out - so the old
			// "skip lights that reach nothing" test rejected every light.

			if(mbLog) Log("-----Light %s ------\n", pLight->GetName().c_str());

			//////////////////////////////////////////////
			// Shadow map: a single projection for a spot, six faces for a point.
			float fShadowKind = 0.0f;

			const bool bSpotShadow = pLight->GetLightType() == eLight3DType_Spot
				&& pLight->GetCastShadows()
				&& mRenderSettings.mShowShadows != eRendererShowShadows_None
				&& mpDepthProgram && mpShadowMap && mpShadowMap->IsValid();

			const bool bCubeShadow = setCubeShadowed.count(pLight) > 0;

			if(bSpotShadow || bCubeShadow)
			{
				if(bSpotShadow)
				{
					RenderShadowMap(static_cast<cLight3DSpot*>(pLight));
					fShadowKind = 1.0f;
				}
				else
				{
					RenderShadowCube(pLight);
					fShadowKind = 2.0f;
				}

				// The depth pass left its own target and program state behind.
				_llGfx->SetDepthTestFunc(eDepthTestFunc_Equal);
				_llGfx->SetDepthWriteActive(false);
				ApplyBlendMode(_llGfx, eMaterialBlendMode_Add);

				mpLightProgram->Bind();
				mpLightProgram->SetFloat("alphaCutoff", 0.6f);
				mRenderSettings.mpProgram = mpLightProgram;

				if(bSpotShadow) mpShadowMap->BindAsTexture(2);
				else            mpShadowCube->BindAsTexture(3);
			}

			// Scissors the pass down to the light's screen footprint.
			if(pLight->BeginDraw(&mRenderSettings, _llGfx))
			{
				mpLightProgram->SetFloat("shadowKind", fShadowKind);
				if(fShadowKind > 1.5f)
				{
					mpLightProgram->SetFloat("shadowNear", kShadowCubeNear);
					mpLightProgram->SetFloat("shadowFar", pLight->GetFarAttenuation());
				}
				else if(fShadowKind > 0.5f)
				{
					mpLightProgram->SetMatrixf("lightViewProj",
						static_cast<cLight3DSpot*>(pLight)->GetViewProjMatrix());
				}

				const cVector3f vPos = pLight->GetWorldPosition();
				const cColor col = pLight->GetDiffuseColor();
				const float fRadius = pLight->GetFarAttenuation();

				mpLightProgram->SetVec3f("lightPos", vPos.x, vPos.y, vPos.z);
				mpLightProgram->SetVec3f("lightColor", col.r, col.g, col.b);
				mpLightProgram->SetFloat("lightRadius", fRadius > 0.0f ? fRadius : 1.0f);

				if(pLight->GetLightType() == eLight3DType_Spot)
				{
					cLight3DSpot *pSpot = static_cast<cLight3DSpot*>(pLight);
					const cVector3f vDir = pSpot->GetViewMatrix().GetForward() * -1.0f;

					mpLightProgram->SetFloat("lightIsSpot", 1.0f);
					mpLightProgram->SetVec3f("lightDir", vDir.x, vDir.y, vDir.z);
					mpLightProgram->SetFloat("lightCosFov", cos(pSpot->GetFOV() * 0.5f));
				}
				else
				{
					mpLightProgram->SetFloat("lightIsSpot", 0.0f);
				}

				cRenderNode* pNode = mpRenderList->GetRootNode();
				pNode->Render(&mRenderSettings);
			}
			pLight->EndDraw(&mRenderSettings, _llGfx);

			lLightCount++;
		}


		mRenderSettings.mpProgramOverride = NULL;
		mRenderSettings.mbNeedsLightingMatrices = false;
		mpLightProgram->UnBind();
		mRenderSettings.mpProgram = NULL;

		_llGfx->SetBlendActive(false);
		_llGfx->SetDepthWriteActive(true);
		_llGfx->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderDiffuse(cCamera *apCamera)
	{
		if(mbLog) Log("Rendering Diffuse:\n");

		_llGfx->SetColorWriteActive(true, true, true, true);
		mRenderSettings.mChannelMode = eMaterialChannelMode_RGBA;

		cRenderNode* pNode = mpRenderList->GetRootNode();
		pNode->Render(&mRenderSettings);
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderTrans(cCamera *apCamera)
	{
		if(mbLog) Log("Rendering Transparent:\n");

		cTransperantObjectIterator it = mpRenderList->GetTransperantIterator();
		if(it.HasNext() == false) return;

		// Blended surfaces read what is already in the framebuffer, so they
		// come after the opaque pass, must not write depth, and are drawn back
		// to front - which is the order the set already holds them in.
		_llGfx->SetDepthWriteActive(false);

		iGpuProgram *pBoundProgram = NULL;

		while(it.HasNext())
		{
			iRenderable *pObject = it.Next();

			iMaterial *pMaterial = pObject->GetMaterial();
			iVertexBuffer *pVtxBuffer = pObject->GetVertexBuffer();
			if(pMaterial == NULL || pVtxBuffer == NULL) continue;

			iGpuProgram *pProgram = pMaterial->GetProgramEx();
			if(pProgram == NULL) continue;

			if(mbLog) Log("Trans object '%s'\n", pObject->GetName().c_str());

			ApplyBlendMode(_llGfx, pMaterial->GetBlendMode());

			// Halos and glows are authored to show through whatever stands in
			// front of them; the flag was being ignored entirely.
			_llGfx->SetDepthTestActive(pMaterial->GetDepthTest());


			for(int i=0; i<MAX_TEXTUREUNITS; ++i)
				_llGfx->SetTexture(i, pMaterial->GetTexture(i));

			if(pProgram != pBoundProgram)
			{
				if(pBoundProgram) pBoundProgram->UnBind();
				pProgram->Bind();
				pBoundProgram = pProgram;
			}

			if(iMaterialProgramSetup *pSetup = pMaterial->GetProgramSetup())
				pSetup->Setup(pProgram, &mRenderSettings);

			// Overrides the cutoff the shared setup just wrote: these are the
			// materials whose gradients must survive.
			if(Material_Universal *pUniversal = dynamic_cast<Material_Universal*>(pMaterial))
				pProgram->SetFloat("alphaCutoff", pUniversal->GetAlphaCutoff());

			auto modelMatrix = pObject->GetModelMatrix(apCamera);
			auto mvMatrix = cMath::MatrixMul(apCamera->GetViewMatrix(), modelMatrix);
			auto mvpMatrix = cMath::MatrixMul(apCamera->GetProjectionMatrix(), mvMatrix);
			pProgram->SetMatrixf("worldViewProj", mvpMatrix);

			pVtxBuffer->Bind();
			pVtxBuffer->Draw();
			pVtxBuffer->UnBind();
		}

		if(pBoundProgram) pBoundProgram->UnBind();

		_llGfx->SetDepthTestActive(true);
		_llGfx->SetBlendActive(false);
		_llGfx->SetDepthWriteActive(true);

		// The settings cache was bypassed above; Render() resets it right
		// after this call, so the state tree starts the next frame clean.
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderDebug(cCamera *apCamera)
	{
	/*
		if(mDebugFlags)
		{
			mpLowLevelGraphics->SetDepthWriteActive(false);

			//Render Debug for objects
			cRenderableIterator objectIt = mpRenderList->GetObjectIt();
			while(objectIt.HasNext())
			{
				iRenderable* pObject = objectIt.Next();
				RenderDebugObject(apCamera, pObject);
			}

			//Render debug for lights.
			if(mDebugFlags & eRendererDebugFlag_DrawLightBoundingBox)
			{
				mpLowLevelGraphics->SetDepthTestActive(false);
				mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,apCamera->GetViewMatrix());

				cLight3DIterator lightIt = mpRenderList->GetLightIt();
				while(lightIt.HasNext())
				{
					iLight3D* pLight = lightIt.Next();
					cBoundingVolume *pBV = pLight->GetBoundingVolume();

					cColor Col = pLight->GetDiffuseColor();

					mpLowLevelGraphics->DrawSphere(pLight->GetWorldPosition(),0.1f,Col);
					mpLowLevelGraphics->DrawBoxMaxMin(pBV->GetMax(), pBV->GetMin(),Col);
				}

				mpLowLevelGraphics->SetDepthTestActive(true);
			}

			mpLowLevelGraphics->SetDepthWriteActive(true);
		}
	*/
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderDebugObject(cCamera *apCamera,iRenderable* &apObject)
	{
	/*
		iVertexBuffer* pVtxBuffer = apObject->GetVertexBuffer();

		if(mDebugFlags & eRendererDebugFlag_DrawBoundingBox)
		{
			mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,apCamera->GetViewMatrix());

			cBoundingVolume *pBV = apObject->GetBoundingVolume();

			mpLowLevelGraphics->DrawBoxMaxMin(pBV->GetMax(), pBV->GetMin(),cColor(1,1.0f,1.0f,1));
		}

		if(mDebugFlags & eRendererDebugFlag_DrawBoundingSphere)
		{
			mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,apCamera->GetViewMatrix());

			cBoundingVolume *pBV = apObject->GetBoundingVolume();

			mpLowLevelGraphics->DrawSphere(pBV->GetWorldCenter(), pBV->GetRadius(),cColor(1,1.0f,1.0f,1));
		}

		cMatrixf mtxModel;
		cMatrixf *pModelMtx = apObject->GetModelMatrix(apCamera);

		if(pModelMtx)
			mtxModel = cMath::MatrixMul(apCamera->GetViewMatrix(),*pModelMtx);
		else
			mtxModel = cMath::MatrixMul(apCamera->GetViewMatrix(),cMatrixf::Identity);

		mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,mtxModel);

		//Draw the debug graphics for the object.
		for(int i=0; i< pVtxBuffer->GetVertexCount(); i++)
		{
			cVector3f vPos = pVtxBuffer->GetVector3(VertexAttr_Position, i);

			if(mDebugFlags & eRendererDebugFlag_DrawNormals)
			{
				cVector3f vNormal = pVtxBuffer->GetVector3(VertexAttr_Normal, i);

				mpLowLevelGraphics->DrawLine(vPos,vPos+(vNormal*0.1f),cColor(0.5f,0.5f,1,1));
			}
			if(mDebugFlags & eRendererDebugFlag_DrawTangents)
			{
				cVector3f vTan = pVtxBuffer->GetVector3(VertexAttr_Tangent, i);

				mpLowLevelGraphics->DrawLine(vPos,vPos+(vTan*0.2f),cColor(1,0.0f,0.0f,1));
			}
		}
	*/
	}

	//-----------------------------------------------------------------------

	void cRenderer3D::RenderPhysicsDebug(cWorld3D *apWorld, cCamera *apCamera)
	{
	/*
		if ((mDebugFlags & eRendererDebugFlag_DrawPhysicsBox) == 0)
		{
			return;
		}

		mpLowLevelGraphics->SetDepthWriteActive(false);
		mpLowLevelGraphics->SetDepthTestActive(false);

		mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,apCamera->GetViewMatrix());

		apWorld->GetPhysicsWorld()->RenderDebugGeometry(mpLowLevelGraphics, cColor(1, 1, 0, 1));

		mpLowLevelGraphics->SetDepthTestActive(true);
		mpLowLevelGraphics->SetDepthWriteActive(true);
	*/
	}

	//-----------------------------------------------------------------------
}
