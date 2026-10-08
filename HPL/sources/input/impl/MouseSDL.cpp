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
#include "input/impl/MouseSDL.h"

#include <SDL2/SDL.h>
#include <cstdlib>
#include <cstring>

#include "graphics/LowLevelGraphics.h"
#include "system/Log.h"
#include "input/impl/LowLevelInputSDL.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMouseSDL::cMouseSDL(cLowLevelInputSDL *apLowLevelInputSDL,iLowLevelGraphics *apLowLevelGraphics) : iMouse("SDL Portable Mouse")
	{
		mfMaxPercent = 0.7f;
		mfMinPercent = 0.1f;
		mlBufferSize = 6;

		mvMButtonArray.resize(eMButton_LastEnum);
		mvMButtonArray.assign(mvMButtonArray.size(),false);

		mpLowLevelInputSDL = apLowLevelInputSDL;
		mpLowLevelGraphics = apLowLevelGraphics;

		mvMouseRelPos = cVector2f(0,0);

		// HPL_MOUSE_ACCEL=<max>[:<rate>] - how far the gain may climb, and how
		// fast it gets there in gain per second. 3:4 reaches triple speed after
		// about half a second of held movement.
		mfAccelMax = 1.0f;
		mfAccelRate = 4.0f;
		mfAccelBase = 1.0f;
		mfAccelGain = 1.0f;
		mlLastMotionMS = 0;
		mbCursorPlaced = false;
		mlLastMoveMS = 0;
		if(const char *pEnv = getenv("HPL_MOUSE_ACCEL"))
		{
			mfAccelMax = (float)atof(pEnv);
			const char *pColon = strchr(pEnv, ':');
			if(pColon)
			{
				mfAccelRate = (float)atof(pColon + 1);
				const char *pSecond = strchr(pColon + 1, ':');
				if(pSecond) mfAccelBase = (float)atof(pSecond + 1);
			}
			if(mfAccelMax < 0.01f) mfAccelMax = 1.0f;
			if(mfAccelRate <= 0.0f) mfAccelRate = 4.0f;
			if(mfAccelBase <= 0.0f) mfAccelBase = 1.0f;
			if(mfAccelMax < mfAccelBase) mfAccelMax = mfAccelBase;
			mfAccelGain = mfAccelBase;
			Log(" Mouse acceleration: %.2fx rising to %.2fx at %.2f per second\n",
				mfAccelBase, mfAccelMax, mfAccelRate);
		}
		mvMouseAbsPos = cVector2f(0,0);

		mbWheelUpMoved = false;
		mbWheelDownMoved = false;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cMouseSDL::OwnsCursor() const
	{
		return mfAccelMax > 1.0f || mfAccelBase != 1.0f;
	}

	//-----------------------------------------------------------------------

	void cMouseSDL::Update()
	{
		cVector2f vScreenSize = mpLowLevelGraphics->GetScreenSize();
		cVector2f vVirtualSize = mpLowLevelGraphics->GetVirtualSize();

		//mvMouseRelPos = cVector2f(0,0);

		//Log("Input start\n");
		mbWheelUpMoved = false;
		mbWheelDownMoved = false;

		std::list<SDL_Event>::iterator it = mpLowLevelInputSDL->mlstEvents.begin();
		for(; it != mpLowLevelInputSDL->mlstEvents.end(); ++it)
		{
			SDL_Event *pEvent = &(*it);

			if(	pEvent->type != SDL_MOUSEMOTION &&
				pEvent->type != SDL_MOUSEBUTTONDOWN &&
				pEvent->type != SDL_MOUSEBUTTONUP &&
			    pEvent->type != SDL_MOUSEWHEEL)
			{
				continue;
			}

			if(pEvent->type == SDL_MOUSEMOTION)
			{
				// With acceleration on, the cursor is the engine's own: it is
				// moved by the accelerated delta below instead of following the
				// system pointer, so the same feel applies to the inventory and
				// the menus as to looking around. A stick cannot drive a
				// pointer that only has one speed.
				if(OwnsCursor() == false)
				{
					mvMouseAbsPos = cVector2f((float)pEvent->motion.x,(float)pEvent->motion.y);
					mvMouseAbsPos = (mvMouseAbsPos/vScreenSize)*vVirtualSize;
				}

				Uint8 buttonState = pEvent->motion.state;

				//Set button here as well just to be sure
				/*if(buttonState & SDL_BUTTON(1)) mvMButtonArray[eMButton_Left] = true;
				if(buttonState & SDL_BUTTON(2)) mvMButtonArray[eMButton_Middle] = true;
				if(buttonState & SDL_BUTTON(3)) mvMButtonArray[eMButton_Right] = true;*/
			}
			else if(pEvent->type == SDL_MOUSEWHEEL)
			{
				Sint32 mul = (pEvent->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1 : 0;
				Sint32 y = pEvent->wheel.y * mul;
				if (y > 0) {
					mbWheelUpMoved = true;
				}
				else {
					mbWheelDownMoved = true;
				}
			}
			else
			{
				bool bButtonIsDown = pEvent->type==SDL_MOUSEBUTTONDOWN;

				//if(pEvent->button.button == SDL_BUTTON_WHEELUP)Log(" Wheel %d!\n",bButtonIsDown);

				switch(pEvent->button.button)
				{
					case SDL_BUTTON_LEFT: mvMButtonArray[eMButton_Left] = bButtonIsDown;break;
					case SDL_BUTTON_MIDDLE: mvMButtonArray[eMButton_Middle] = bButtonIsDown;break;
					case SDL_BUTTON_RIGHT: mvMButtonArray[eMButton_Right] = bButtonIsDown;break;
				}
			}
		}

		if(mbWheelDownMoved)	mvMButtonArray[eMButton_WheelDown] = true;
		else					mvMButtonArray[eMButton_WheelDown] = false;
		if(mbWheelUpMoved)		mvMButtonArray[eMButton_WheelUp] = true;
		else					mvMButtonArray[eMButton_WheelUp] = false;

		int lX,lY;
		SDL_GetRelativeMouseState(&lX, &lY);

		// One line, once, saying whether relative motion is actually arriving.
		// SDL reporting relative mode on is not the same as the compositor
		// delivering it, and that difference is invisible from a log: the view
		// simply stops turning. Function-local so it costs nothing after it has
		// fired.
		{
			static int slFrames = 0;
			static int slFramesWithMotion = 0;
			static bool sbReported = false;

			if(sbReported == false)
			{
				++slFrames;
				if(lX != 0 || lY != 0) ++slFramesWithMotion;

				if(slFrames >= 600)
				{
					Log(" Mouse after %d frames: %d carried relative motion, mode is %s\n",
						slFrames, slFramesWithMotion,
						SDL_GetRelativeMouseMode() == SDL_TRUE ? "on" : "off");
					sbReported = true;
				}
			}
		}

		mvMouseRelPos = cVector2f((float)lX,(float)lY);
		mvMouseRelPos = (mvMouseRelPos/vScreenSize)*vVirtualSize;

		// That division makes the pointer resolution dependent, and the pad is
		// not: gptokeyb moves it a fixed number of pixels whatever the panel,
		// so the same push covers less than half as much of a 1080p screen as
		// of a 480p one and both the view and the cursor crawl. A reviewer on a
		// Retroid Pocket 5 reported exactly that. Scaling back out against the
		// height the mapping was tuned on undoes it, and leaves the devices it
		// was tuned on untouched.
		if(OwnsCursor() && vScreenSize.y > 0)
		{
			const float kTunedForHeight = 480.0f;
			mvMouseRelPos = mvMouseRelPos * (vScreenSize.y / kTunedForHeight);
		}

		if(OwnsCursor())
		{
			// Start in the middle rather than in a corner: nothing has told the
			// engine where the cursor is until the first movement arrives.
			if(mbCursorPlaced == false)
			{
				mvMouseAbsPos = vVirtualSize * 0.5f;
				mbCursorPlaced = true;
			}

			const unsigned int lNow = SDL_GetTicks();
			const float fDelta = (mlLastMotionMS == 0)
				? 0.0f : (float)(lNow - mlLastMotionMS) / 1000.0f;
			mlLastMotionMS = lNow;

			// Idle is measured from the last frame that actually moved, not
			// from this one. A single empty frame means nothing: the pad sends
			// movement on its own schedule and the game runs at twenty-odd
			// frames a second, so empty frames arrive constantly even while the
			// stick is held. Reacting to one of those would make the gain jump
			// about and the pointer move in steps.
			if(lX != 0 || lY != 0) mlLastMoveMS = lNow;

			const bool bIdle = (mlLastMoveMS == 0)
				|| (lNow - mlLastMoveMS) > 150;

			// The gain walks towards where it should be rather than being set
			// there. Snapping it back on every pause was the whole cause of the
			// stutter: the speed changed between one frame and the next, and at
			// twenty-odd frames a second that reads as the pointer jerking.
			// Climbing and falling at the same measured rate keeps the motion
			// continuous while still making acceleration something you earn by
			// holding on.
			const float fTarget = bIdle ? mfAccelBase : mfAccelMax;
			const float fStep = mfAccelRate * fDelta;

			if(mfAccelGain < fTarget)
			{
				mfAccelGain += fStep;
				if(mfAccelGain > fTarget) mfAccelGain = fTarget;
			}
			else if(mfAccelGain > fTarget)
			{
				mfAccelGain -= fStep;
				if(mfAccelGain < fTarget) mfAccelGain = fTarget;
			}

			mvMouseRelPos = mvMouseRelPos * mfAccelGain;

			mvMouseAbsPos += mvMouseRelPos;

			if(mvMouseAbsPos.x < 0) mvMouseAbsPos.x = 0;
			if(mvMouseAbsPos.y < 0) mvMouseAbsPos.y = 0;
			if(mvMouseAbsPos.x > vVirtualSize.x) mvMouseAbsPos.x = vVirtualSize.x;
			if(mvMouseAbsPos.y > vVirtualSize.y) mvMouseAbsPos.y = vVirtualSize.y;
		}
	}

	//-----------------------------------------------------------------------

	bool cMouseSDL::ButtonIsDown(eMButton mButton)
	{
		return mvMButtonArray[mButton];
	}

	//-----------------------------------------------------------------------

	cVector2f cMouseSDL::GetAbsPosition()
	{
		// Do a transform with the screen-size to the the float coordinates.
		cVector2f vPos = mvMouseAbsPos;

		return vPos;
	}

	//-----------------------------------------------------------------------

	cVector2f cMouseSDL::GetRelPosition()
	{
		// Do a transform with the screen-size to the the float coordinates.
		cVector2f vPos = mvMouseRelPos;
		//Ok this is?
		mvMouseRelPos = cVector2f(0,0);

		return vPos;
	}

	//-----------------------------------------------------------------------

	void cMouseSDL::Reset()
	{
		mvMouseRelPos = cVector2f(0,0);

		int lX,lY; //Just to clear the rel pos.

		SDL_PumpEvents();
		SDL_GetRelativeMouseState(&lX, &lY);
	}

	//-----------------------------------------------------------------------

	void cMouseSDL::SetSmoothProperties(float afMinPercent,
		float afMaxPercent,unsigned int alBufferSize)
	{
		mfMaxPercent = afMaxPercent;
		mfMinPercent = afMinPercent;
		mlBufferSize = alBufferSize;
	}

	//-----------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	/////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

}
