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
#include "graphics/ogl2/PostProcess.h"

#include "graphics/GPUProgram.h"
#include "system/Log.h"

#include <cstddef>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl3.h>
#endif

namespace hpl {

	//-----------------------------------------------------------------------

	cPostProcess::cPostProcess()
		: mFBO(0), mColorTexture(0), mDepthBuffer(0), mVAO(0), mlWidth(0), mlHeight(0)
	{
		// Core profile refuses to draw without a bound vertex array, even when
		// the vertices come from gl_VertexID alone.
		glGenVertexArrays(1, &mVAO);
	}

	//-----------------------------------------------------------------------

	cPostProcess::~cPostProcess()
	{
		Destroy();
		if(mVAO) glDeleteVertexArrays(1, &mVAO);
	}

	//-----------------------------------------------------------------------

	void cPostProcess::Destroy()
	{
		if(mFBO) { glDeleteFramebuffers(1, &mFBO); mFBO = 0; }
		if(mColorTexture) { glDeleteTextures(1, &mColorTexture); mColorTexture = 0; }
		if(mDepthBuffer) { glDeleteRenderbuffers(1, &mDepthBuffer); mDepthBuffer = 0; }
	}

	//-----------------------------------------------------------------------

	bool cPostProcess::Resize(int alWidth, int alHeight)
	{
		if(alWidth == mlWidth && alHeight == mlHeight && mFBO != 0) return true;
		if(alWidth <= 0 || alHeight <= 0) return false;

		Destroy();

		mlWidth = alWidth;
		mlHeight = alHeight;

		// Half float so bright areas keep their headroom for the bloom pass
		// instead of clipping at 1.0 the moment they are written.
		glGenTextures(1, &mColorTexture);
		glBindTexture(GL_TEXTURE_2D, mColorTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mlWidth, mlHeight, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenRenderbuffers(1, &mDepthBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mlWidth, mlHeight);

		glGenFramebuffers(1, &mFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTexture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer);

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			Error("Post-process framebuffer incomplete (0x%x); drawing straight to the back buffer\n", status);
			Destroy();
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------

	void cPostProcess::Begin()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glViewport(0, 0, mlWidth, mlHeight);

		// The frame's clear happened on the back buffer before this target was
		// bound, so it has to be repeated here - otherwise the depth buffer
		// holds last frame's leftovers and rejects the whole scene.
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glStencilMask(0xFF);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	//-----------------------------------------------------------------------

	void cPostProcess::Resolve(iGpuProgram *apProgram, float afGamma, float afBloomAmount)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, mlWidth, mlHeight);

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mColorTexture);

		apProgram->Bind();
		apProgram->SetFloat("invGamma", 1.0f / (afGamma > 0.01f ? afGamma : 1.0f));
		apProgram->SetFloat("bloomAmount", afBloomAmount);
		apProgram->SetVec2f("texelSize", 1.0f / (float)mlWidth, 1.0f / (float)mlHeight);

		glBindVertexArray(mVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);

		apProgram->UnBind();

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
	}

	//-----------------------------------------------------------------------

}
