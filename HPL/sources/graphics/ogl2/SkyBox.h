/*
 * Draws the sky behind everything else, as a full-screen pass rather than a
 * box mesh - the view ray is rebuilt from the inverse view-projection.
 * This file is part of Rehatched.
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
