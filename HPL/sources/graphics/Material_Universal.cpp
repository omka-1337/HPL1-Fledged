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
#include "scene/Camera.h"
#include "math/Math.h"

#include <cstdlib>

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

	/**
	 * The same as the ambient setup, plus what a reflection needs: where the
	 * surface is in the world and where it is being looked at from. Only the
	 * reflective material asks for this, so nothing else in the pass pays the
	 * two extra matrices.
	 */
	class cEnvProgramSetup : public cAmbProgramSetup
	{
	public:
		/**
		 * The program these uniforms belong to. Programs are cached by name, so
		 * every reflective material shares one pointer and one static setup.
		 */
		iGpuProgram *mpFor = NULL;

		void SetupMatrix(cMatrixf *apModelMatrix, cRenderSettings* apRenderSettings) {
			if(apModelMatrix == NULL || apRenderSettings == NULL) return;
			if(apRenderSettings->mpProgram == NULL) return;

			// The same surface is also drawn by the depth pass and by one pass
			// per light, each with its own program, and those have no eyePos and
			// not always a model matrix. Setting a uniform that is not there
			// writes a warning, which would be once per object per frame for the
			// whole time the ice is on screen.
			if(apRenderSettings->mpProgram != mpFor) return;

			apRenderSettings->mpProgram->SetMatrixf("model", *apModelMatrix);
			apRenderSettings->mpProgram->SetMatrixf("normalMatrix",
				cMath::MatrixTranspose(cMath::MatrixInverse(*apModelMatrix)));

			if(apRenderSettings->mpCamera)
			{
				const cVector3f vEye = apRenderSettings->mpCamera->GetPosition();
				apRenderSettings->mpProgram->SetVec3f("eyePos", vEye.x, vEye.y, vEye.z);
			}
		}
	};

	static cEnvProgramSetup gEnvProgramSetup;

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

		// The acid pools are opaque where they are drawn - they replace what is
		// under them rather than blending with it - but they have to be drawn
		// after everything else, because what they show is the scene behind
		// them, pulled sideways. The transparent pass is where that happens.
		if(sType == "water")
			return { eMaterialBlendMode_Replace, true, eMaterialSpecularMode_None };

		// Everything else is an opaque surface: diffuse, plain bump, flat, 2D.
		return { eMaterialBlendMode_Replace, false, eMaterialSpecularMode_None };
	}

	//-----------------------------------------------------------------------

	Material_Universal::Material_Universal(const tString& asName, const tString& asTypeName,
		iLowLevelGraphics* apLowLevelGraphics,
		cTextureManager *apTextureManager, cGpuProgramManager* apProgramManager)
		: iMaterial(asName,apLowLevelGraphics,apTextureManager,apProgramManager)
	{
		const MaterialTypeTraits traits = TraitsForType(asTypeName);

		// Kept out of MaterialTypeTraits deliberately: one material in the game
		// is reflective, and widening that struct would mean touching every
		// branch of it for a single case.
		_reflects = cString::ToLowerCase(asTypeName) == "environmentmapreflect";
		// A refractive pool costs a full-screen copy of the scene every frame it
		// is on screen, which on a tile based GPU means resolving the tile buffer
		// out to memory. It is worth it on anything that can afford it and it is
		// the first thing to give up on anything that cannot, so the port can
		// turn it off and the pools then draw as flat acid.
		static const bool sbRefractionAllowed = []() {
			const char *pEnv = getenv("HPL_NO_REFRACTION");
			return !(pEnv != NULL && pEnv[0] == '1');
		}();

		_refracts = sbRefractionAllowed && cString::ToLowerCase(asTypeName) == "water";

		mbIsTransperant = traits.transparent;
		_blendMode = traits.blendMode;
		_specularMode = traits.specular;

		// A blended surface fades out instead of ending on a hard edge, so the
		// cutoff that alpha-tested geometry relies on would eat its gradient.
		// Water rides in the same pass without being blended, and its own shader
		// never tests alpha at all.
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

			if(_refracts)
			{
				_program = mpProgramManager->CreateProgram("UniversalWater.vert",
														   "UniversalWater.frag");
				if(_program)
				{
					_program->Bind();
					_program->SetTextureBindingIndex("refractMap", 1);
					_program->SetTextureBindingIndex("screenMap", 7);
					_program->UnBind();
				}
				return _program;
			}

			if(_reflects)
			{
				// Its own pair rather than more branches in the common shader:
				// the reflection needs a normal and a world position, which the
				// ambient vertex shader has no reason to carry for everything
				// else in the scene.
				_program = mpProgramManager->CreateProgram("UniversalEnv.vert",
														   "UniversalEnv.frag");
				if(_program)
				{
					_program->Bind();
					_program->SetTextureBindingIndex("envMap", 6);
					_program->UnBind();
					gEnvProgramSetup.mpFor = _program;
				}
				return _program;
			}

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
		return _reflects ? (iMaterialProgramSetup*)&gEnvProgramSetup
						 : (iMaterialProgramSetup*)&gAmbProgramSetup;
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
		{
			// The wave map rides on the illumination unit. No water material
			// declares an illumination texture, and the two units that would
			// read better here - 2 and 3 - are bound by the shadow code behind
			// the back of the texture cache, so a material must not use them.
			if(_refracts) return mvTexture[eMaterialTexture_Refraction];
			return mvTexture[eMaterialTexture_Illumination];
		}
		if (alUnit == 4)
		{
			// Unit 4 is the light pass's normal map. Materials without one get
			// a flat stand-in so the shader needs no per-material switch - the
			// state tree binds one program for the whole pass.
			iTexture *pNormal = mvTexture[eMaterialTexture_Normal];
			return pNormal ? pNormal : _flatNormalMap;
		}
		if (alUnit == 6)
		{
			return _reflects ? mvTexture[eMaterialTexture_CubeMap] : NULL;
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
