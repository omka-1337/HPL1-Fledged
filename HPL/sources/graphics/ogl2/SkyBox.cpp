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
