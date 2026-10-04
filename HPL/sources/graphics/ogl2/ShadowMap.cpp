/*
 * Depth-only render target used for spot light shadows.
 * This file is part of Rehatched.
 */
#include "graphics/ogl2/ShadowMap.h"

#include "system/Log.h"

#include <cstddef>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl3.h>
#endif

namespace hpl {

	//-----------------------------------------------------------------------

	cShadowMap::cShadowMap(int alSize)
		: mFBO(0), mDepthTexture(0), mlSize(alSize), mlPreviousFBO(0)
	{
		glGenTextures(1, &mDepthTexture);
		glBindTexture(GL_TEXTURE_2D, mDepthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mlSize, mlSize, 0,
					 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		// A depth-compare sampler gets 2x2 percentage-closer filtering for free
		// from the hardware, which takes the worst of the stair-stepping off.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Clamp to edge rather than to a border colour: GLES has no border
		// clamp, and the shader checks the bounds itself anyway.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &mFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthTexture, 0);

		// Depth only - there is no colour attachment to draw to or read from.
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			Error("Shadow map framebuffer incomplete (0x%x), shadows disabled\n", status);
			glDeleteFramebuffers(1, &mFBO);
			mFBO = 0;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	//-----------------------------------------------------------------------

	cShadowMap::~cShadowMap()
	{
		if(mFBO) glDeleteFramebuffers(1, &mFBO);
		if(mDepthTexture) glDeleteTextures(1, &mDepthTexture);
	}

	//-----------------------------------------------------------------------

	void cShadowMap::BeginRender()
	{
		// The scene may be going into the post-process target, so the previous
		// binding has to be restored rather than assumed to be the back buffer.
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mlPreviousFBO);

		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glViewport(0, 0, mlSize, mlSize);

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Push the recorded depth away from the light so that a surface does
		// not shadow itself along its own slope. Kept small: too much and the
		// shadow separates from the foot of its caster, which reads as the
		// object hovering above the floor.
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 1.5f);
	}

	//-----------------------------------------------------------------------

	void cShadowMap::EndRender(int alScreenWidth, int alScreenHeight)
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.0f, 0.0f);

		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)mlPreviousFBO);
		glViewport(0, 0, alScreenWidth, alScreenHeight);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	//-----------------------------------------------------------------------

	void cShadowMap::BindAsTexture(int alUnit)
	{
		glActiveTexture(GL_TEXTURE0 + alUnit);
		glBindTexture(GL_TEXTURE_2D, mDepthTexture);
		glActiveTexture(GL_TEXTURE0);
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CUBE SHADOW MAP
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cShadowMapCube::cShadowMapCube(int alSize)
		: mFBO(0), mDepthCube(0), mlSize(alSize), mlPreviousFBO(0)
	{
		glGenTextures(1, &mDepthCube);
		glBindTexture(GL_TEXTURE_CUBE_MAP, mDepthCube);

		for(int i=0; i<6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT24,
						 mlSize, mlSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &mFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							   GL_TEXTURE_CUBE_MAP_POSITIVE_X, mDepthCube, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			Error("Cube shadow framebuffer incomplete (0x%x), point lights will not cast shadows\n", status);
			glDeleteFramebuffers(1, &mFBO);
			mFBO = 0;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}

	//-----------------------------------------------------------------------

	cShadowMapCube::~cShadowMapCube()
	{
		if(mFBO) glDeleteFramebuffers(1, &mFBO);
		if(mDepthCube) glDeleteTextures(1, &mDepthCube);
	}

	//-----------------------------------------------------------------------

	void cShadowMapCube::BeginRender()
	{
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mlPreviousFBO);

		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glViewport(0, 0, mlSize, mlSize);

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 1.5f);
	}

	//-----------------------------------------------------------------------

	void cShadowMapCube::BeginFace(int alFace)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							   GL_TEXTURE_CUBE_MAP_POSITIVE_X + alFace, mDepthCube, 0);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	//-----------------------------------------------------------------------

	void cShadowMapCube::EndRender(int alScreenWidth, int alScreenHeight)
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.0f, 0.0f);

		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)mlPreviousFBO);
		glViewport(0, 0, alScreenWidth, alScreenHeight);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	//-----------------------------------------------------------------------

	void cShadowMapCube::BindAsTexture(int alUnit)
	{
		glActiveTexture(GL_TEXTURE0 + alUnit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, mDepthCube);
		glActiveTexture(GL_TEXTURE0);
	}

	//-----------------------------------------------------------------------

}
