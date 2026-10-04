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
 * Draws the sky behind everything else, as a full-screen pass rather than a
 * box mesh - the view ray is rebuilt from the inverse view-projection.
 */
#ifndef HPL_SKYBOX_H
#define HPL_SKYBOX_H

#include "math/MathTypes.h"

namespace hpl {

	class cColor;
	class iGpuProgram;
	class iTexture;

	class cSkyBoxDrawer
	{
	public:
		cSkyBoxDrawer();
		~cSkyBoxDrawer();

		void Draw(iGpuProgram *apProgram, const cMatrixf &a_mtxInvViewProj,
				  const cColor &aColor, iTexture *apCubeMap);

	private:
		unsigned int mVAO;
	};

}

#endif // HPL_SKYBOX_H
