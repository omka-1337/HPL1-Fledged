/*
 * Depth-only render target used for spot light shadows.
 * This file is part of Rehatched.
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
	};

}

#endif // HPL_SHADOWMAP_H
