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
 * Off-screen target for the 3D scene plus the full-screen pass that resolves
 * it to the back buffer. This is where gamma and bloom live.
 */
#ifndef HPL_POSTPROCESS_H
#define HPL_POSTPROCESS_H

namespace hpl {

	class iGpuProgram;

	class cPostProcess
	{
	public:
		cPostProcess();
		~cPostProcess();

		/** Recreates the targets when the viewport size changes. */
		bool Resize(int alWidth, int alHeight);

		bool IsValid() const { return mFBO != 0; }

		/** Directs subsequent drawing into the off-screen colour buffer. */
		void Begin();

		/** Resolves to the back buffer, applying gamma and bloom. */
		void Resolve(iGpuProgram *apProgram, float afGamma, float afBloomAmount);

	private:
		void Destroy();

		unsigned int mFBO;
		unsigned int mColorTexture;
		unsigned int mDepthBuffer;
		unsigned int mVAO;

		int mlWidth;
		int mlHeight;
	};

}

#endif // HPL_POSTPROCESS_H
