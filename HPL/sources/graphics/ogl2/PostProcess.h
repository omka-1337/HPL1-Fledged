/*
 * Off-screen target for the 3D scene plus the full-screen pass that resolves
 * it to the back buffer. This is where gamma and bloom live.
 * This file is part of Rehatched.
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
