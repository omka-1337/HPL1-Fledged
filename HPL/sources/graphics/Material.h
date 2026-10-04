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
#ifndef HPL_MATERIAL_H
#define HPL_MATERIAL_H

#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/GPUProgram.h"
#include "system/StringTypes.h"

#include <vector>

class TiXmlElement;

namespace hpl {

	enum eMaterialTexture
	{
		eMaterialTexture_Diffuse,
		eMaterialTexture_Normal,
		eMaterialTexture_Specular,
		eMaterialTexture_Refraction,	// used once (more often in black plague)
		eMaterialTexture_Illumination,	// glowing parts: lamps, screens, dials
		eMaterialTexture_None
	};

	enum eMaterialBlendMode
	{
		eMaterialBlendMode_None,
		eMaterialBlendMode_Add,
		eMaterialBlendMode_Mul,
		eMaterialBlendMode_MulX2,
		eMaterialBlendMode_Replace,
		eMaterialBlendMode_Alpha,
		eMaterialBlendMode_DestAlphaAdd,
		eMaterialBlendMode_LastEnum
	};

	enum eMaterialAlphaMode
	{
		eMaterialAlphaMode_Solid,
		eMaterialAlphaMode_Trans
	};

	//! Determines what color channels are going to be affected
	enum eMaterialChannelMode
	{
		eMaterialChannelMode_RGBA,
		eMaterialChannelMode_Z
	};

	/**
	 * Where a surface's specular strength comes from. The content asks for
	 * both kinds: BumpSpecular masks the highlight with the normal map's own
	 * alpha channel, BumpColorSpecular tints it with a separate map, which is
	 * what BumpSpec_Light_fp.cg and BumpColorSpec_Light_fp.cg did.
	 * The values are passed to the light shader as they are, so do not
	 * renumber them without changing Light.frag.
	 */
	enum eMaterialSpecularMode
	{
		eMaterialSpecularMode_None = 0,
		eMaterialSpecularMode_Gloss = 1,	// strength from the normal map alpha
		eMaterialSpecularMode_Color = 2		// strength and tint from a map
	};

	//---------------------------------------------------

	class cRenderSettings;
	class cTextureManager;
	class cGpuProgramManager;

	//---------------------------------------------------------------

	class iMaterialProgramSetup
	{
	public:
		virtual ~iMaterialProgramSetup() = default;
		virtual void Setup(iGpuProgram *apProgram,cRenderSettings* apRenderSettings)=0;
		virtual void SetupMatrix(cMatrixf *apModelMatrix, cRenderSettings* apRenderSettings){}
	};

	//---------------------------------------------------

	class iMaterial : public iResourceBase
	{
	public:
		iMaterial(const tString& asName,iLowLevelGraphics* apLowLevelGraphics,
				cTextureManager *apTextureManager, cGpuProgramManager* apProgramManager);
		virtual ~iMaterial();

		virtual void Update(float afTimeStep){}

		//The new render system stuff
		virtual iGpuProgram* GetProgramEx(){return NULL;}

		virtual iMaterialProgramSetup * GetProgramSetup(){return NULL;}

		virtual eMaterialAlphaMode GetAlphaMode(){return eMaterialAlphaMode_Solid;}
		virtual eMaterialBlendMode GetBlendMode(){return eMaterialBlendMode_Replace;}
		virtual eMaterialChannelMode GetChannelMode(){return eMaterialChannelMode_RGBA;}

		virtual iTexture* GetTexture(int alUnit){return NULL;}

		virtual eMaterialSpecularMode GetSpecularMode(){return eMaterialSpecularMode_None;}

		bool HasAlpha(){ return mbHasAlpha;}
		void SetHasAlpha(bool abX){ mbHasAlpha= abX; }

		virtual bool LoadData(TiXmlElement* apRootElem){ return true; }

		iTexture* GetTexture(eMaterialTexture aType) { return mvTexture[aType]; }
		void SetTexture(iTexture* apTex,eMaterialTexture aType){ mvTexture[aType] = apTex; }

		bool IsTransperant() {return mbIsTransperant;}

		/**
		 * A few blended materials - window halos, lamp glows - are meant to be
		 * seen through the geometry in front of them.
		 */
		bool GetDepthTest() const { return mbDepthTest; }
		void SetDepthTest(bool abX) { mbDepthTest = abX; }

		const tString& GetPhysicsMaterial(){ return msPhysicsMaterial;}
		void SetPhysicsMaterial(const tString& asName){ msPhysicsMaterial = asName;}

	protected:
		iLowLevelGraphics* mpLowLevelGraphics;
		cTextureManager* mpTextureManager;
		cGpuProgramManager* mpProgramManager;

		bool mbIsTransperant;
		bool mbDepthTest = true;
		bool mbHasAlpha;

		tString msPhysicsMaterial;

		std::vector<iTexture*> mvTexture;
	};

	typedef std::vector<iMaterial*> tMaterialVec;
	typedef tMaterialVec::iterator tMaterialVecIt;

	class iMaterialType
	{
	public:
		virtual ~iMaterialType() {}
		virtual bool IsCorrect(tString asName)=0;
		virtual iMaterial* Create(const tString& asName, const tString& asTypeName,
			iLowLevelGraphics* apLowLevelGraphics,
			cTextureManager *apTextureManager, cGpuProgramManager* apProgramManager)=0;
	};

};
#endif // HPL_MATERIAL_H
