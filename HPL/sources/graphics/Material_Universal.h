/*
 * 2023 by zenmumbler
 * This file is part of Rehatched
 */
#ifndef HPL_MATERIAL_UNIVERSAL_H
#define HPL_MATERIAL_UNIVERSAL_H

#include "graphics/Material.h"

namespace hpl {

	class Material_Universal : public iMaterial
	{
	public:
		Material_Universal(const tString& asName,
						   const tString& asTypeName,
						   iLowLevelGraphics* apLowLevelGraphics,
						   cTextureManager *apTextureManager,
						   cGpuProgramManager* apProgramManager);

		virtual ~Material_Universal();

		iGpuProgram* GetProgramEx() override;

		iMaterialProgramSetup* GetProgramSetup() override;

		eMaterialAlphaMode GetAlphaMode() override;
		eMaterialBlendMode GetBlendMode() override;
		eMaterialChannelMode GetChannelMode() override;

		iTexture* GetTexture(int alUnit) override;

		eMaterialSpecularMode GetSpecularMode() override;

		/**
		 * Alpha below this is discarded in the fragment shader. Blended
		 * materials disable the test (0) so their soft edges survive.
		 */
		float GetAlphaCutoff() const { return _alphaCutoff; }

		/**
		 * Stands in for materials with no normal map so the light shader can
		 * sample unconditionally. Set once by the renderer.
		 */
		static void SetFlatNormalMap(iTexture *apTexture) { _flatNormalMap = apTexture; }

	protected:
		iGpuProgram* _program;
		eMaterialBlendMode _blendMode;
		float _alphaCutoff;
		eMaterialSpecularMode _specularMode;

		static iTexture *_flatNormalMap;
	};

	class MaterialType_Universal : public iMaterialType
	{
	public:
		bool IsCorrect(tString asName){
			return true;
		}

		iMaterial* Create(const tString& asName, const tString& asTypeName,
			iLowLevelGraphics* apLowLevelGraphics,
			cTextureManager *apTextureManager, cGpuProgramManager* apProgramManager);

	private:
	};


};

#endif // HPL_MATERIAL_UNIVERSAL_H
