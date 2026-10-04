/*
 * Copyright (C) 2026 - Omka1337, written with the help of Claude Code
 *
 * This file is part of HPL1 Fledged, which continues HPL1 Rehatched by
 * zenmumbler and the HPL1 Engine by Frictional Games.
 *
 * HPL1 Fledged is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HPL1 Fledged is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HPL1 Fledged.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "graphics/ogl2/SkyBox.h"

#include "graphics/GPUProgram.h"
#include "graphics/Texture.h"
#include "math/MathTypes.h"

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl3.h>
#endif

namespace hpl {

	//-----------------------------------------------------------------------

	cSkyBoxDrawer::cSkyBoxDrawer() : mVAO(0)
	{
		glGenVertexArrays(1, &mVAO);
	}

	cSkyBoxDrawer::~cSkyBoxDrawer()
	{
		if(mVAO) glDeleteVertexArrays(1, &mVAO);
	}

	//-----------------------------------------------------------------------

	void cSkyBoxDrawer::Draw(iGpuProgram *apProgram, const cMatrixf &a_mtxInvViewProj,
							 const cColor &aColor, iTexture *apCubeMap)
	{
		// Sits at the far plane and only fills what nothing else covered, so it
		// tests against existing depth but never writes any of its own.
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);

		apProgram->Bind();
		apProgram->SetMatrixf("invViewProj", a_mtxInvViewProj);
		apProgram->SetVec3f("skyColor", aColor.r, aColor.g, aColor.b);
		apProgram->SetFloat("useTexture", apCubeMap ? 1.0f : 0.0f);

		if(apCubeMap)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)apCubeMap->GetCurrentLowlevelHandle());
		}

		glBindVertexArray(mVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);

		apProgram->UnBind();

		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
		glEnable(GL_CULL_FACE);
	}

	//-----------------------------------------------------------------------

}
