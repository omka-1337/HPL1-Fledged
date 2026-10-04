/*
 * 2023 by zenmumbler
 * This file is part of Rehatched
 */

#include "graphics/Material_Universal.h"
#include "resources/GpuProgramManager.h"
#include "resources/TextureManager.h"
#include "graphics/GPUProgram.h"
#include "system/String.h"
#include "graphics/Renderer3D.h"
#include "scene/PortalContainer.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PROGRAM SETUP
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	class cAmbProgramSetup : public iMaterialProgramSetup
	{
	public:
		void Setup(iGpuProgram *apProgram, cRenderSettings* apRenderSettings) {
			if(apRenderSettings->mpSector)
				apProgram->SetColor3f("ambientColor", apRenderSettings->mAmbientColor * apRenderSettings->mpSector->GetAmbientColor());
			else
				apProgram->SetColor3f("ambientColor", apRenderSettings->mAmbientColor);

			// Opaque geometry is batched under one program bind, and it all
			// shares the same cutoff. The transparent pass binds per object
			// and overrides this itself.
			apProgram->SetFloat("alphaCutoff", 0.6f);
		}
	};

	static cAmbProgramSetup gAmbProgramSetup;

	iTexture *Material_Universal::_flatNormalMap = NULL;


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	/**
	 * How a .mat Type maps onto the pipeline. Every type still draws through
	 * the one Universal program - lighting is not back yet - but the blend
	 * mode and the transparency flag are what the content actually asks for.
	 * Without this every effect material drew as an opaque quad.
	 */
	struct MaterialTypeTraits
	{
		eMaterialBlendMode blendMode;
		bool transparent;
	};

	static MaterialTypeTraits TraitsForType(const tString& asTypeName)
	{
		const tString sType = cString::ToLowerCase(asTypeName);

		if(sType == "additive")
			return { eMaterialBlendMode_Add, true };
		if(sType == "alpha")
			return { eMaterialBlendMode_Alpha, true };
		if(sType == "modulative")
			return { eMaterialBlendMode_Mul, true };
		if(sType == "modulativex2")
			return { eMaterialBlendMode_MulX2, true };

		// Everything else is an opaque surface: diffuse, the bump family,
		// flat, 2D, and the handful of water/cube/envmap materials that still
		// need their own shaders.
		return { eMaterialBlendMode_Replace, false };
	}

	//-----------------------------------------------------------------------

	Material_Universal::Material_Universal(const tString& asName, const tString& asTypeName,
		iLowLevelGraphics* apLowLevelGraphics,
		cTextureManager *apTextureManager, cGpuProgramManager* apProgramManager)
		: iMaterial(asName,apLowLevelGraphics,apTextureManager,apProgramManager)
	{
		const MaterialTypeTraits traits = TraitsForType(asTypeName);

		mbIsTransperant = traits.transparent;
		_blendMode = traits.blendMode;

		// A blended surface fades out instead of ending on a hard edge, so the
		// cutoff that alpha-tested geometry relies on would eat its gradient.
		_alphaCutoff = traits.transparent ? 0.0f : 0.6f;

		// Deferred to GetProgramEx(): the textures are attached after the
		// material is constructed, and the shader choice depends on them.
		_program = NULL;
	}

	//-----------------------------------------------------------------------

	Material_Universal::~Material_Universal()
	{
		if (_program)
			mpProgramManager->Destroy(_program);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iGpuProgram* Material_Universal::GetProgramEx() {
		if(_program == NULL)
		{
			const bool bHasIllumination = mvTexture[eMaterialTexture_Illumination] != NULL;

			// A blended surface is its own light source - halos, light shafts,
			// smoke. The original drew these with Diffuse_Color_fp.cg, which
			// does not touch the ambient term; modulating them by it made the
			// window halos four times too dim to see.
			const char *pFragment = mbIsTransperant
				? "Unlit.frag"
				: (bHasIllumination ? "UniversalIllum.frag" : "Universal.frag");

			_program = mpProgramManager->CreateProgram("Universal.vert", pFragment);

			// Programs are cached by name, so this costs one extra program
			// overall, and it keeps the glow out of the common shader.
			if(_program && bHasIllumination)
			{
				_program->Bind();
				_program->SetTextureBindingIndex("illuminationMap", 1);
				_program->UnBind();
			}
		}
		return _program;
	}

	//------------------------------------------------------------------------------------

	iMaterialProgramSetup * Material_Universal::GetProgramSetup()
	{
		return &gAmbProgramSetup;
	}

	//------------------------------------------------------------------------------------

	eMaterialAlphaMode Material_Universal::GetAlphaMode()
	{
		return mbHasAlpha ? eMaterialAlphaMode_Trans : eMaterialAlphaMode_Solid;
	}

	//------------------------------------------------------------------------------------

	eMaterialBlendMode Material_Universal::GetBlendMode()
	{
		return _blendMode;
	}

	//------------------------------------------------------------------------------------

	eMaterialChannelMode Material_Universal::GetChannelMode()
	{
		return eMaterialChannelMode_RGBA;
	}

	//-----------------------------------------------------------------------

	iTexture* Material_Universal::GetTexture(int alUnit)
	{
		if (alUnit == 0)
			return mvTexture[eMaterialTexture_Diffuse];
		if (alUnit == 1)
			return mvTexture[eMaterialTexture_Illumination];
		if (alUnit == 4)
		{
			// Unit 4 is the light pass's normal map. Materials without one get
			// a flat stand-in so the shader needs no per-material switch - the
			// state tree binds one program for the whole pass.
			iTexture *pNormal = mvTexture[eMaterialTexture_Normal];
			return pNormal ? pNormal : _flatNormalMap;
		}
		return NULL;
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	iMaterial* MaterialType_Universal::Create(const tString& asName, const tString& asTypeName,
										iLowLevelGraphics* apLowLevelGraphics,
										cTextureManager *apTextureManager, cGpuProgramManager* apProgramManager)
	{
		return new Material_Universal(asName, asTypeName, apLowLevelGraphics, apTextureManager, apProgramManager);
	}

	//-----------------------------------------------------------------------

}
