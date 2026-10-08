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
#ifdef WIN32
#pragma comment(lib, "OpenGL32.lib")
#endif

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl3.h>
#endif

#include <cstdlib>
#include <assert.h>
#include <stdlib.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "graphics/Bitmap.h"

#include "graphics/impl/LowLevelGraphicsSDL.h"

#include "graphics/ogl2/SDLTexture.h"
#include "graphics/ogl2/GLSLProgram.h"
#include "graphics/ogl2/VertexBufferVBO.h"
#include "graphics/ogl2/OcclusionQueryOGL.h"

#include "system/Log.h"

namespace hpl {

	static bool gbImGuiActive = true;

	//////////////////////////////////////////////////////////////////////////
	// GLOBAL FUNCTIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static GLenum GetGLBlendEnum(eBlendFunc aType)
	{
		switch(aType)
		{
			case eBlendFunc_Zero:					return GL_ZERO;
			case eBlendFunc_One:					return GL_ONE;
			case eBlendFunc_SrcColor:				return GL_SRC_COLOR;
			case eBlendFunc_OneMinusSrcColor:		return GL_ONE_MINUS_SRC_COLOR;
			case eBlendFunc_DestColor:				return GL_DST_COLOR;
			case eBlendFunc_OneMinusDestColor:		return GL_ONE_MINUS_DST_COLOR;
			case eBlendFunc_SrcAlpha:				return GL_SRC_ALPHA;
			case eBlendFunc_OneMinusSrcAlpha:		return GL_ONE_MINUS_SRC_ALPHA;
			case eBlendFunc_DestAlpha:				return GL_DST_ALPHA;
			case eBlendFunc_OneMinusDestAlpha:		return GL_ONE_MINUS_DST_ALPHA;
			case eBlendFunc_SrcAlphaSaturate:		return GL_SRC_ALPHA_SATURATE;
			default: return 0;
		}
	}

	//-----------------------------------------------------------------------

	static GLenum GetGLDepthTestFuncEnum(eDepthTestFunc aType)
	{
		switch(aType)
		{
			case eDepthTestFunc_Never:			return GL_NEVER;
			case eDepthTestFunc_Less:			return GL_LESS;
			case eDepthTestFunc_LessOrEqual:	return GL_LEQUAL;
			case eDepthTestFunc_Greater:		return GL_GREATER;
			case eDepthTestFunc_GreaterOrEqual:	return GL_GEQUAL;
			case eDepthTestFunc_Equal:			return GL_EQUAL;
			case eDepthTestFunc_NotEqual:		return GL_NOTEQUAL;
			case eDepthTestFunc_Always:			return GL_ALWAYS;
			default: return 0;
		}
	}

	//-----------------------------------------------------------------------

	static GLenum GetGLStencilFuncEnum(eStencilFunc aType)
	{
		switch(aType)
		{
			case eStencilFunc_Never:			return GL_NEVER;
			case eStencilFunc_Less:				return GL_LESS;
			case eStencilFunc_LessOrEqual:		return GL_LEQUAL;
			case eStencilFunc_Greater:			return GL_GREATER;
			case eStencilFunc_GreaterOrEqual:	return GL_GEQUAL;
			case eStencilFunc_Equal:			return GL_EQUAL;
			case eStencilFunc_NotEqual:			return GL_NOTEQUAL;
			case eStencilFunc_Always:			return GL_ALWAYS;
			default: return 0;
		}
	}

	//-----------------------------------------------------------------------

	static GLenum GetGLStencilOpEnum(eStencilOp aType)
	{
		switch(aType) {
			case eStencilOp_Keep:			return GL_KEEP;
			case eStencilOp_Zero:			return GL_ZERO;
			case eStencilOp_Replace:		return GL_REPLACE;
			case eStencilOp_Increment:		return GL_INCR;
			case eStencilOp_Decrement:		return GL_DECR;
			case eStencilOp_Invert:			return GL_INVERT;
			case eStencilOp_IncrementWrap:	return GL_INCR_WRAP;
			case eStencilOp_DecrementWrap:	return GL_DECR_WRAP;
			default: return 0;
		}
	}

	//-------------------------------------------------

	// used by SDLTexture.cpp
	bool cLowLevelGraphicsSDL::ImGuiAvailable() { return gbImGuiActive; }

	//-----------------------------------------------------------------------

	GLenum TextureTargetToGL(eTextureTarget aTarget)
	{
		switch (aTarget) {
			// GLES has no 1D target; such textures are stored as 2D, height 1.
			case eTextureTarget_1D:		return cGLSLProgram::TargetIsGLES() ? GL_TEXTURE_2D : GL_TEXTURE_1D;
			case eTextureTarget_2D:		return GL_TEXTURE_2D;
			case eTextureTarget_CubeMap:return GL_TEXTURE_CUBE_MAP;
			case eTextureTarget_3D:		return GL_TEXTURE_3D;
			default:					return 0;
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::cLowLevelGraphicsSDL() : iLowLevelGraphics()
	{
		mvVirtualSize.x = 800;
		mvVirtualSize.y = 600;

		for (int i = 0; i < MAX_TEXTUREUNITS; i++) {
			mpCurrentTexture[i] = NULL;
		}
	}

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::~cLowLevelGraphicsSDL()
	{

		if (mpImGuiContext) {
			if(gbImGuiActive) {
				ImGui_ImplSDL2_Shutdown();
				ImGui_ImplOpenGL3_Shutdown();
			}
			ImGui::DestroyContext(mpImGuiContext);
		}

		if (mpGLContext) {
			SDL_GL_DeleteContext(mpGLContext);
			mpGLContext = NULL;
		}

		if (mpWindow) {
			SDL_DestroyWindow(mpWindow);
			mpWindow = NULL;
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cLowLevelGraphicsSDL::Init(int alWidth, int alHeight, bool abFullscreen, const tString& asWindowCaption)
	{
		// The shipped config stores Width/Height as -1, meaning "use the desktop
		// resolution" - SDL 1.2 understood that, SDL2 does not and would make a
		// degenerate window. Resolve it before anything reads the screen size:
		// the mouse scaling, camera aspect and scissor rects all divide by it.
		if (alWidth <= 0 || alHeight <= 0)
		{
			SDL_DisplayMode mode;
			if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
				alWidth = mode.w;
				alHeight = mode.h;
			} else {
				Error("Could not query desktop display mode: %s\n", SDL_GetError());
				alWidth = 800;
				alHeight = 600;
			}
		}

		mvScreenSize.x = alWidth;
		mvScreenSize.y = alHeight;

		// GLES on handhelds, desktop core elsewhere. HPL_GLES=1 forces the ES
		// path on a desktop too, which is how it gets tested without hardware.
#if defined(__arm__) || defined(__aarch64__)
		const bool bUseGLES = getenv("HPL_GLES") == NULL || getenv("HPL_GLES")[0] != '0';
#else
		const bool bUseGLES = getenv("HPL_GLES") != NULL && getenv("HPL_GLES")[0] != '0';
#endif

		if(bUseGLES)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		}
		else
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		}
		cGLSLProgram::SetTargetIsGLES(bUseGLES);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		unsigned int mlFlags = SDL_WINDOW_OPENGL;

		// SDL_WINDOW_FULLSCREEN asks the display to change mode to whatever the
		// config says, and a handheld panel cannot do 800x600: the compositor
		// takes the 4:3 image and stretches it across a 16:9 screen, which is
		// what a Retroid Pocket 5 tester saw. FULLSCREEN_DESKTOP keeps the
		// panel's own resolution and shape, so the game renders at 16:9 on a
		// 16:9 screen and at 640x480 on a 640x480 one, whatever resolution the
		// config carried over from the player's desktop copy of the game.
		if (abFullscreen)
			mlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

		Log(" Creating display: %d x %d\n", alWidth, alHeight);
		mpWindow = SDL_CreateWindow(asWindowCaption.c_str(),
									SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									alWidth, alHeight, mlFlags);
		if (mpWindow == NULL) {
			FatalError("Unable to initialize display!\n");
			return false;
		}

		// Ask for the keyboard focus now that there is a window to give it to.
		// Under a wayland compositor the pointer lock belongs to the focused
		// surface, and a second run in the same session came up without focus:
		// SDL reported the lock granted while no relative motion ever arrived
		// and the view would not turn past the edge of the screen.
		SDL_RaiseWindow(mpWindow);

		// A window manager may hand back a different size than requested.
		int lRealWidth = alWidth, lRealHeight = alHeight;
		SDL_GetWindowSize(mpWindow, &lRealWidth, &lRealHeight);
		if (lRealWidth != alWidth || lRealHeight != alHeight) {
			Log("  Window manager gave %d x %d instead\n", lRealWidth, lRealHeight);
			mvScreenSize.x = lRealWidth;
			mvScreenSize.y = lRealHeight;
		}
		
		// GL context
		Log(" Setting up OpenGL\n");
		mpGLContext = SDL_GL_CreateContext(mpWindow);
		if (mpGLContext == NULL) {
			Error(SDL_GetError());
			FatalError("Unable to create GL context!\n");
			return false;
		}

		// Everything drawn is measured in the drawable, not in the window.
		//
		// A compositor that scales reports a window smaller than the buffer it
		// actually hands over: ask it for the window and the viewport comes out
		// a fraction of the frame, which puts the whole picture in the bottom
		// left corner on a black field, GL's origin being down there. A tester
		// on a Retroid Pocket 5 saw exactly that. Only the drawable size is
		// safe for the viewport, the off-screen targets and the camera's shape.
		// It needs the context, so it cannot be asked any earlier than this.
		int lDrawW = 0, lDrawH = 0;
		SDL_GL_GetDrawableSize(mpWindow, &lDrawW, &lDrawH);
		if (lDrawW > 0 && lDrawH > 0 &&
			(lDrawW != (int)mvScreenSize.x || lDrawH != (int)mvScreenSize.y))
		{
			Log("  Drawable is %d x %d, not %d x %d - using the drawable\n",
				lDrawW, lDrawH, (int)mvScreenSize.x, (int)mvScreenSize.y);
			mvScreenSize.x = lDrawW;
			mvScreenSize.y = lDrawH;
		}

		// GL defaults
		SetClearColor(cColor::Black);
		SetClearDepth(1.0f);
		SetClearStencil(0);
		SetCullMode(eCullMode_CounterClockwise);
		SetDepthTestActive(true);
		SetDepthTestFunc(eDepthTestFunc_Equal);

		// ImGui context
		mpImGuiContext = ImGui::CreateContext();
		// ImGui's GL3 backend resolves desktop entry points that an ES context
		// does not have, and crashes building its shaders. The debug menu is
		// not something a handheld needs, so it simply stays off there.
		gbImGuiActive = (bUseGLES == false);
		if(gbImGuiActive)
		{
			ImGui_ImplOpenGL3_Init();
			ImGui_ImplSDL2_InitForOpenGL(mpWindow, mpGLContext);
		}

		// Hide cursor by default
		ShowCursor(false);

		return true;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ShowCursor(bool abX)
	{
		if(abX)
			SDL_ShowCursor(SDL_ENABLE);
		else
			SDL_ShowCursor(SDL_DISABLE);
	}

	//-----------------------------------------------------------------------

	/**
	 * Asks for the pointer lock again, without the logging.
	 *
	 * A wayland compositor ties the lock to the focused surface, so one granted
	 * before the window had focus is not in force. Launching the game a second
	 * time without rebooting left it exactly there: SDL reported relative mode
	 * on, no relative motion arrived, the pointer walked to the edge of the
	 * screen and the view stopped turning. Re-asking on every focus gain costs
	 * nothing when the lock is already held.
	 */
	void cLowLevelGraphicsSDL::ReapplyInputGrab() {
		if(mpWindow == NULL) return;

		SDL_SetWindowGrab(mpWindow, mbWantInputGrab ? SDL_TRUE : SDL_FALSE);

		// Off and on again, not straight on.
		//
		// The compositor grants the pointer lock to whoever has the focus at
		// the moment it is asked, and the frontend holds the focus on every run
		// of the game but the first after a boot. Asking again once the focus
		// arrives does nothing, because SDL already believes the mode is on and
		// returns without telling the compositor anything: the motion starts
		// coming through while the pointer was never taken out of play, so it
		// walks to the side of the screen and the view stops turning there.
		// Clearing it first makes the second call a real request.
		if(mbWantInputGrab)
		{
			SDL_SetRelativeMouseMode(SDL_FALSE);
			SDL_SetRelativeMouseMode(SDL_TRUE);
		}
		else
		{
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetInputGrab(bool abX) {
		// Confine the pointer to the window as well as asking for relative
		// motion. The two go together: relative mode is meant to take the
		// pointer out of play, and where a compositor grants the motion but not
		// that, the pointer walks to the side of the screen and the view stops
		// turning. Confining it is the compositor's own job and needs no
		// warping, which is what made the camera shake when it was tried.
		if(mpWindow) SDL_SetWindowGrab(mpWindow, bWanted ? SDL_TRUE : SDL_FALSE);
		// Relative mode is what makes looking around work: without it the
		// pointer is an absolute position that stops at the edge of the screen,
		// and turning stops with it. It is not guaranteed to be available, so
		// say so loudly rather than leaving a player to wonder why the view
		// will not turn past a wall.
		// When the engine drives its own cursor (see cMouseSDL and
		// HPL_MOUSE_ACCEL) relative mode has to stay on even in the menus.
		// Letting it go would hand the system pointer back to the compositor,
		// and the moment that pointer reached a screen edge the deltas would
		// stop arriving and the cursor would freeze against the side.
		static const bool sbOwnCursor = getenv("HPL_MOUSE_ACCEL") != NULL;
		const bool bWanted = abX || sbOwnCursor;

		mbWantInputGrab = bWanted;

		const int lResult = SDL_SetRelativeMouseMode(bWanted ? SDL_TRUE : SDL_FALSE);
		if(lResult != 0)
		{
			Error("Could not %s relative mouse mode: %s - looking around will "
				"stop at the screen edge\n", bWanted ? "enter" : "leave", SDL_GetError());
		}
		else
		{
			Log(" Relative mouse mode %s (SDL reports %s)\n",
				bWanted ? "on" : "off",
				SDL_GetRelativeMouseMode() == SDL_TRUE ? "on" : "off");
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVsyncActive(bool abX)
	{
		SDL_GL_SetSwapInterval(abX ? 1 : 0);
	}

	//-----------------------------------------------------------------------

	Bitmap cLowLevelGraphicsSDL::GetScreenPixels()
	{
		glFinish();

		Bitmap bmp;
		bmp.Create(mvScreenSize.x, mvScreenSize.y);

		auto pixels = bmp.GetRawData<unsigned char>();

		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, mvScreenSize.x, mvScreenSize.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		return bmp;
	}

	//-----------------------------------------------------------------------

	iGpuProgram* cLowLevelGraphicsSDL::CreateGpuProgram(const tString& asName)
	{
		return new cGLSLProgram(asName);
	}

	//-----------------------------------------------------------------------

	iTexture* cLowLevelGraphicsSDL::CreateTexture(const tString &asName, eTextureTarget aTarget)
	{
		return new cSDLTexture(asName, aTarget);
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cLowLevelGraphicsSDL::CreateVertexBuffer(VertexAttributes aFlags,
														VertexBufferPrimitiveType aDrawType,
														VertexBufferUsageType aUsageType,
														int alReserveVtxSize,int alReserveIdxSize)
	{
		return new cVertexBufferVBO(aFlags, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize);
	}

	//-----------------------------------------------------------------------

	Sampler cLowLevelGraphicsSDL::CreateSampler(const SamplerDesc& descriptor)
	{
		return { 0 };
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetTexture(unsigned int alUnit, iTexture* apTex)
	{
		if(apTex == mpCurrentTexture[alUnit]) return;

		glActiveTexture(GL_TEXTURE0 + alUnit);

		if (apTex != NULL) {
			GLenum target = TextureTargetToGL(apTex->GetTarget());
			glBindTexture(target, apTex->GetCurrentLowlevelHandle());
		}
		else {
			// glBindTexture(GL_TEXTURE_2D, 0); // use arbitrary target
		}

		mpCurrentTexture[alUnit] = apTex;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::StartFrame() {
		ClearScreen();

		if(gbImGuiActive == false) return;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}

	void cLowLevelGraphicsSDL::SwapBuffers()
	{
		SDL_GL_SwapWindow(mpWindow);
	}
	
	void cLowLevelGraphicsSDL::EndFrame() {
		// render the debug views last on top of everything else.
		// Render() without a matching NewFrame() walks uninitialised state, so
		// both ends stay together when the debug UI is off.
		if(gbImGuiActive)
		{
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		SwapBuffers();
	}

	//-----------------------------------------------------------------------

	iOcclusionQuery* cLowLevelGraphicsSDL::CreateOcclusionQuery()
	{
		return new cOcclusionQueryOGL();
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::DestroyOcclusionQuery(iOcclusionQuery *apQuery)
	{
		if (apQuery) delete apQuery;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ClearScreen()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetClearColor(const cColor& aCol){
		glClearColor(aCol.r, aCol.g, aCol.b, aCol.a);
	}
	void cLowLevelGraphicsSDL::SetClearDepth(float afDepth){
		glClearDepthf(afDepth);
	}
	void cLowLevelGraphicsSDL::SetClearStencil(int alVal){
		glClearStencil(alVal);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColorWriteActive(bool abR,bool abG,bool abB,bool abA)
	{
		glColorMask(abR,abG,abB,abA);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthWriteActive(bool abX)
	{
		glDepthMask(abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthTestActive(bool abX)
	{
		if(abX) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthTestFunc(eDepthTestFunc aFunc)
	{
		glDepthFunc(GetGLDepthTestFuncEnum(aFunc));
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilActive(bool abX)
	{
		if(abX) glEnable(GL_STENCIL_TEST);
		else glDisable(GL_STENCIL_TEST);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencil(eStencilFunc aFunc,int alRef, unsigned int aMask,
					eStencilOp aFailOp,eStencilOp aZFailOp,eStencilOp aZPassOp)
	{
		glStencilFunc(GetGLStencilFuncEnum(aFunc), alRef, aMask);

		glStencilOp(GetGLStencilOpEnum(aFailOp), GetGLStencilOpEnum(aZFailOp),
					GetGLStencilOpEnum(aZPassOp));
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilTwoSide(eStencilFunc aFrontFunc,eStencilFunc aBackFunc,
					int alRef, unsigned int aMask,
					eStencilOp aFrontFailOp,eStencilOp aFrontZFailOp,eStencilOp aFrontZPassOp,
					eStencilOp aBackFailOp,eStencilOp aBackZFailOp,eStencilOp aBackZPassOp)
	{
		glStencilOpSeparate(GL_FRONT, GetGLStencilOpEnum(aFrontFailOp),
							GetGLStencilOpEnum(aFrontZFailOp),
							GetGLStencilOpEnum(aFrontZPassOp));
		glStencilOpSeparate(GL_BACK, GetGLStencilOpEnum(aBackFailOp),
							GetGLStencilOpEnum(aBackZFailOp),
							GetGLStencilOpEnum(aBackZPassOp));

		glStencilFuncSeparate(GL_FRONT,
							  GetGLStencilFuncEnum(aBackFunc),
							  alRef, aMask);
		glStencilFuncSeparate(GL_BACK,
							  GetGLStencilFuncEnum(aBackFunc),
							  alRef, aMask);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetCullActive(bool abX)
	{
		if(abX) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);
	}

	void cLowLevelGraphicsSDL::SetCullMode(eCullMode aMode)
	{
		if(aMode == eCullMode_Clockwise) glFrontFace(GL_CCW);
		else							glFrontFace(GL_CW);
		glCullFace(GL_BACK);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorActive(bool abX)
	{
		if(abX) glEnable(GL_SCISSOR_TEST);
		else glDisable(GL_SCISSOR_TEST);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorRect(const cRect2l &aRect)
	{
		glScissor(aRect.x, (mvScreenSize.y - aRect.y - 1)-aRect.h, aRect.w, aRect.h);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendActive(bool abX)
	{
		if(abX)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor)
	{
		glBlendFunc(GetGLBlendEnum(aSrcFactor),GetGLBlendEnum(aDestFactor));
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor,
		eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha)
	{
		glBlendFuncSeparate(GetGLBlendEnum(aSrcFactorColor),
							GetGLBlendEnum(aDestFactorColor),
							GetGLBlendEnum(aSrcFactorAlpha),
							GetGLBlendEnum(aDestFactorAlpha));
	}

	//-----------------------------------------------------------------------

	cVector2f cLowLevelGraphicsSDL::GetScreenSize()
	{
		return cVector2f((float)mvScreenSize.x, (float)mvScreenSize.y);
	}

	//-----------------------------------------------------------------------

	cVector2f cLowLevelGraphicsSDL::GetVirtualSize()
	{
		return mvVirtualSize;
	}

	//-----------------------------------------------------------------------

	cVector2f cLowLevelGraphicsSDL::GetVirtualMargin()
	{
		const cVector2f vScreen = GetScreenSize();
		if(vScreen.x <= 0 || vScreen.y <= 0) return cVector2f(0,0);
		if(mvVirtualSize.x <= 0 || mvVirtualSize.y <= 0) return cVector2f(0,0);

		const float fScreenAspect = vScreen.x / vScreen.y;
		const float fVirtualAspect = mvVirtualSize.x / mvVirtualSize.y;

		if(fScreenAspect > fVirtualAspect)
			return cVector2f((mvVirtualSize.y * fScreenAspect - mvVirtualSize.x) * 0.5f, 0);
		if(fScreenAspect < fVirtualAspect)
			return cVector2f(0, (mvVirtualSize.x / fScreenAspect - mvVirtualSize.y) * 0.5f);

		return cVector2f(0,0);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVirtualSize(cVector2f avSize)
	{
		mvVirtualSize = avSize;
	}

	//-----------------------------------------------------------------------

}
