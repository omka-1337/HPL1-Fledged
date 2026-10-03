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

		/**
		 * Alpha below this is discarded in the fragment shader. Blended
		 * materials disable the test (0) so their soft edges survive.
		 */
		float GetAlphaCutoff() const { return _alphaCutoff; }

	protected:
		iGpuProgram* _program;
		eMaterialBlendMode _blendMode;
		float _alphaCutoff;
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
