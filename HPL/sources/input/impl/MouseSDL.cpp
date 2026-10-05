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
		mfAccelGain = 1.0f;
		mlLastMotionMS = 0;
		if(const char *pEnv = getenv("HPL_MOUSE_ACCEL"))
		{
			mfAccelMax = (float)atof(pEnv);
			const char *pColon = strchr(pEnv, ':');
			if(pColon) mfAccelRate = (float)atof(pColon + 1);
			if(mfAccelMax < 1.0f) mfAccelMax = 1.0f;
			if(mfAccelRate <= 0.0f) mfAccelRate = 4.0f;
			Log(" Mouse acceleration: up to %.2fx at %.2f per second\n",
				mfAccelMax, mfAccelRate);
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
				mvMouseAbsPos = cVector2f((float)pEvent->motion.x,(float)pEvent->motion.y);
				mvMouseAbsPos = (mvMouseAbsPos/vScreenSize)*vVirtualSize;

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

		mvMouseRelPos = cVector2f((float)lX,(float)lY);
		mvMouseRelPos = (mvMouseRelPos/vScreenSize)*vVirtualSize;

		if(mfAccelMax > 1.0f)
		{
			const unsigned int lNow = SDL_GetTicks();
			const float fDelta = (mlLastMotionMS == 0)
				? 0.0f : (float)(lNow - mlLastMotionMS) / 1000.0f;
			mlLastMotionMS = lNow;

			// Standing still, or a gap long enough to count as a new intention,
			// puts the gain back to one. That is what keeps a small deliberate
			// nudge precise: acceleration has to be earned by holding on.
			if((lX == 0 && lY == 0) || fDelta > 0.2f)
			{
				mfAccelGain = 1.0f;
			}
			else
			{
				mfAccelGain += mfAccelRate * fDelta;
				if(mfAccelGain > mfAccelMax) mfAccelGain = mfAccelMax;
			}

			mvMouseRelPos = mvMouseRelPos * mfAccelGain;
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
