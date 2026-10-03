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
#include "graphics/RenderState.h"
#include "graphics/Renderer3D.h"

#include "graphics/Material.h"
#include "graphics/VertexBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/LowLevelGraphics.h"
#include "scene/Light3D.h"
#include "scene/Light3DSpot.h"
#include "scene/Camera.h"
#include "scene/PortalContainer.h"
#include "math/Math.h"
#include "system/Log.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iRenderState::SetMode(cRenderSettings* apSettings)
	{
		switch(mType)
		{
		case eRenderStateType_Sector:				SetSectorMode(apSettings); break;
		case eRenderStateType_AlphaMode:			SetAlphaMode(apSettings); break;
		case eRenderStateType_BlendMode:			SetBlendMode(apSettings); break;
		case eRenderStateType_GPUProgram:		SetGPUProgMode(apSettings); break;
		case eRenderStateType_Texture:			SetTextureMode(apSettings); break;
		case eRenderStateType_VertexBuffer:		SetVtxBuffMode(apSettings); break;
		case eRenderStateType_Matrix:			SetMatrixMode(apSettings); break;
		case eRenderStateType_Render:			SetRenderMode(apSettings); break;
		default: break;
		}
	}

	//-----------------------------------------------------------------------

	void iRenderState::Set(const iRenderState* apState)
	{
		mType = apState->mType;
		switch(mType)
		{
		case eRenderStateType_Sector:			mpSector = apState->mpSector;
												break;

		case eRenderStateType_AlphaMode:		mAlphaMode = apState->mAlphaMode;
												break;

		case eRenderStateType_BlendMode:		mBlendMode = apState->mBlendMode;
												mChannelMode = apState->mChannelMode;
												break;

		case eRenderStateType_GPUProgram:		mpProgram = apState->mpProgram;
												mpProgramSetup = apState->mpProgramSetup;
												break;

		case eRenderStateType_Texture:			for(int i=0;i<MAX_TEXTUREUNITS;i++)
													mpTexture[i] = apState->mpTexture[i];
												break;

		case eRenderStateType_VertexBuffer:		mpVtxBuffer = apState->mpVtxBuffer;
												break;

		case eRenderStateType_Matrix:			modelMatrix = apState->modelMatrix;
												break;

		case eRenderStateType_Render:			break;
		default: break;
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iRenderState::SetSectorMode(cRenderSettings* apSettings)
	{
		if(apSettings->mbLog) Log("Sector: %d\n",mpSector);
		apSettings->mpSector = mpSector;
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetAlphaMode(cRenderSettings* apSettings)
	{
		if(mAlphaMode != apSettings->mAlphaMode)
		{
			if(apSettings->mbLog) Log("Setting alpha: ");
			apSettings->mAlphaMode = mAlphaMode;

			if(apSettings->mbLog)Log((mAlphaMode == eMaterialAlphaMode_Solid) ? "Solid\n" : "Trans\n");
		}
	}

	//-----------------------------------------------------------------------

	void ApplyBlendMode(iLowLevelGraphics* apLowLevel, eMaterialBlendMode aMode)
	{
		if(aMode == eMaterialBlendMode_None)
		{
			apLowLevel->SetBlendActive(false);
			return;
		}

		apLowLevel->SetBlendActive(true);

		switch(aMode)
		{
		case eMaterialBlendMode_Add:
			apLowLevel->SetBlendFunc(eBlendFunc_One,eBlendFunc_One);
			break;
		case eMaterialBlendMode_Replace:
			apLowLevel->SetBlendFunc(eBlendFunc_One,eBlendFunc_Zero);
			break;
		case eMaterialBlendMode_Mul:
			apLowLevel->SetBlendFunc(eBlendFunc_Zero,eBlendFunc_SrcColor);
			break;
		case eMaterialBlendMode_MulX2:
			apLowLevel->SetBlendFunc(eBlendFunc_DestColor,eBlendFunc_SrcColor);
			break;
		case eMaterialBlendMode_Alpha:
			apLowLevel->SetBlendFunc(eBlendFunc_SrcAlpha,eBlendFunc_OneMinusSrcAlpha);
			break;
		case eMaterialBlendMode_DestAlphaAdd:
			apLowLevel->SetBlendFunc(eBlendFunc_DestAlpha,eBlendFunc_One);
			break;
		default:
			Warning("Invalid blend mode %d\n", aMode);
			break;
		}
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetBlendMode(cRenderSettings* apSettings)
	{
		if(mBlendMode != apSettings->mBlendMode)
		{
			if(apSettings->mbLog)Log("Setting blend mode: %d\n", mBlendMode);
			apSettings->mBlendMode = mBlendMode;

			ApplyBlendMode(apSettings->mpLowLevel, mBlendMode);
		}

		if(mChannelMode != apSettings->mChannelMode)
		{
			if(apSettings->mbLog)Log("Setting channel: ");
			apSettings->mChannelMode = mChannelMode;

			switch(mChannelMode)
			{
			case eMaterialChannelMode_RGBA:
				apSettings->mpLowLevel->SetColorWriteActive(true, true, true, true);
				if(apSettings->mbLog)Log("RGBA");
				break;
			case eMaterialChannelMode_Z:
				apSettings->mpLowLevel->SetColorWriteActive(false, false, false, false);
				if(apSettings->mbLog)Log("Z");
				break;
			default:
				if(apSettings->mbLog)Log("Invalid colour channel mode!");
				break;
			}

			if(apSettings->mbLog)Log("\n");
		}
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetGPUProgMode(cRenderSettings* apSettings)
	{
		// A pass may drive the whole tree with one program of its own; it has
		// already bound it and set its uniforms, so there is nothing to do.
		if(apSettings->mpProgramOverride)
			return;

		if(mpProgram != apSettings->mpProgram)
		{
			if(apSettings->mbLog){
				if(mpProgram)
					Log("Setting gpu program: '%s' ", mpProgram->GetName().c_str());
				else
					Log("Setting gpu program: NULL ");
			}

			if(mpProgram==NULL && apSettings->mpProgram)
			{
				if(apSettings->mbLog)Log("Unbinding old program");
				apSettings->mpProgram->UnBind();
			}
			apSettings->mpProgram = mpProgram;

			if(mpProgram)
			{
				if(apSettings->mbLog)Log("Binding new program");
				mpProgram->Bind();

				if(mpProgramSetup)
				{
					if(apSettings->mbLog)Log("Custom program setup %d ", mpProgram);
					mpProgramSetup->Setup(mpProgram, apSettings);
				}
				apSettings->mpProgramSetup = mpProgramSetup;
			}
			if(apSettings->mbLog)Log("\n");
		}
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetTextureMode(cRenderSettings* apSettings)
	{
		for(int i=0;i<MAX_TEXTUREUNITS;i++)
		{
			if(apSettings->mpTexture[i] != mpTexture[i])
			{
				if(apSettings->mbLog)
				{
					if(mpTexture[i]==NULL)
						Log("Setting texture: %d : NULL\n", i);
					else
						Log("Setting texture: %d : '%s'\n", i, mpTexture[i]->GetName().c_str());
				}

				apSettings->mpLowLevel->SetTexture(i,mpTexture[i]);
				apSettings->mpTexture[i] = mpTexture[i];
			}
		}
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetVtxBuffMode(cRenderSettings* apSettings)
	{
		if(mpVtxBuffer != apSettings->mpVtxBuffer)
		{
			if(apSettings->mbLog)Log("Setting vertex buffer: %d\n",(size_t)mpVtxBuffer);
			if(apSettings->mpVtxBuffer) apSettings->mpVtxBuffer->UnBind();
			apSettings->mpVtxBuffer = mpVtxBuffer;

			if(mpVtxBuffer)
			{
				mpVtxBuffer->Bind();
			}
		}
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetMatrixMode(cRenderSettings* apSettings)
	{
		if (apSettings->mbLog)Log("Setting model matrix: %s ", cMath::MatrixToChar(modelMatrix));

		if (apSettings->mpProgram)
		{
			auto mvMatrix = cMath::MatrixMul(apSettings->mpCamera->GetViewMatrix(), modelMatrix);
			auto mvpMatrix = cMath::MatrixMul(apSettings->mpCamera->GetProjectionMatrix(), mvMatrix);
			apSettings->mpProgram->SetMatrixf("worldViewProj", mvpMatrix);

			// Lighting is evaluated in world space, so that pass needs the model
			// matrix and a normal matrix as well.
			if(apSettings->mbNeedsLightingMatrices)
			{
				apSettings->mpProgram->SetMatrixf("model", modelMatrix);
				apSettings->mpProgram->SetMatrixf("normalMatrix",
					cMath::MatrixTranspose(cMath::MatrixInverse(modelMatrix)));
			}

			if (apSettings->mpProgramSetup)
			{
				// apSettings->mpProgramSetup->SetupMatrix(mpModelMatrix,apSettings);
			}
		}

		if (apSettings->mbLog)Log("\n");
	}

	//-----------------------------------------------------------------------

	void iRenderState::SetRenderMode(cRenderSettings* apSettings)
	{
		if(apSettings->mbLog)Log("Drawing\n-----------------\n");

		if(apSettings->mDebugFlags & eRendererDebugFlag_RenderLines)
		{
			apSettings->mpVtxBuffer->DrawWireframe();
		}
		else
		{
			apSettings->mpVtxBuffer->Draw();
		}
	}

	//-----------------------------------------------------------------------

}
