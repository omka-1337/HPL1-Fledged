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
		eMaterialSpecularMode specular;
	};

	static MaterialTypeTraits TraitsForType(const tString& asTypeName)
	{
		const tString sType = cString::ToLowerCase(asTypeName);

		if(sType == "additive")
			return { eMaterialBlendMode_Add, true, eMaterialSpecularMode_None };
		if(sType == "alpha")
			return { eMaterialBlendMode_Alpha, true, eMaterialSpecularMode_None };
		if(sType == "modulative")
			return { eMaterialBlendMode_Mul, true, eMaterialSpecularMode_None };
		if(sType == "modulativex2")
			return { eMaterialBlendMode_MulX2, true, eMaterialSpecularMode_None };

		// The two specular families, 247 materials between them in the retail
		// data, which is most of what makes wet stone and metal read as wet
		// stone and metal rather than as flat paint.
		if(sType == "bumpspecular")
			return { eMaterialBlendMode_Replace, false, eMaterialSpecularMode_Gloss };
		if(sType == "bumpcolorspecular")
			return { eMaterialBlendMode_Replace, false, eMaterialSpecularMode_Color };
		// DiffuseSpec_Light_fp.cg masked its highlight with nothing at all and
		// dotted against a flat (0,0,1) normal. Gloss plus the flat stand-in
		// normal map, whose alpha is 1, comes out the same.
		if(sType == "diffusespecular")
			return { eMaterialBlendMode_Replace, false, eMaterialSpecularMode_Gloss };

		// Everything else is an opaque surface: diffuse, plain bump, flat, 2D,
		// and the handful of water/cube/envmap materials that still need their
		// own shaders.
		return { eMaterialBlendMode_Replace, false, eMaterialSpecularMode_None };
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
		_specularMode = traits.specular;

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
		if (alUnit == 5)
		{
			// Unit 5 is the light pass's specular map, and only the Color mode
			// reads it. Gloss mode takes its strength from the normal map's
			// alpha instead, so there is nothing to bind here.
			return (_specularMode == eMaterialSpecularMode_Color)
				? mvTexture[eMaterialTexture_Specular] : NULL;
		}
		return NULL;
	}

	//-----------------------------------------------------------------------

	eMaterialSpecularMode Material_Universal::GetSpecularMode()
	{
		// Every BumpColorSpecular material in the retail data declares a
		// specular map, so this only catches a missing or unloadable file.
		// Dropping the highlight is the quiet failure; falling through to the
		// normal map's alpha would paint the surface white instead.
		if(_specularMode == eMaterialSpecularMode_Color
			&& mvTexture[eMaterialTexture_Specular] == NULL)
		{
			return eMaterialSpecularMode_None;
		}
		return _specularMode;
	}


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
