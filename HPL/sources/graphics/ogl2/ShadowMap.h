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
/*
 * Depth-only render target used for spot light shadows.
 */
#ifndef HPL_SHADOWMAP_H
#define HPL_SHADOWMAP_H

namespace hpl {

	class cShadowMap
	{
	public:
		explicit cShadowMap(int alSize);
		~cShadowMap();

		bool IsValid() const { return mFBO != 0; }

		/** Binds the target and clears it. Nothing but depth is written. */
		void BeginRender();

		/** Restores the default target and the given viewport. */
		void EndRender(int alScreenWidth, int alScreenHeight);

		void BindAsTexture(int alUnit);

		int GetSize() const { return mlSize; }

	private:
		unsigned int mFBO;
		unsigned int mDepthTexture;
		int mlSize;

		/** Whatever was bound before this pass - not necessarily the back buffer. */
		int mlPreviousFBO;
	};

	//---------------------------------------------------------------

	/**
	 * Six-faced depth target for point lights, which shine every way at once
	 * and so cannot be covered by a single projection.
	 */
	class cShadowMapCube
	{
	public:
		explicit cShadowMapCube(int alSize);
		~cShadowMapCube();

		bool IsValid() const { return mFBO != 0; }

		/** Shared state for all six faces. */
		void BeginRender();

		/** Attaches one face (0..5) and clears it. */
		void BeginFace(int alFace);

		void EndRender(int alScreenWidth, int alScreenHeight);

		void BindAsTexture(int alUnit);

		int GetSize() const { return mlSize; }

	private:
		unsigned int mFBO;
		unsigned int mDepthCube;
		int mlSize;

		int mlPreviousFBO;
	};

}

#endif // HPL_SHADOWMAP_H
