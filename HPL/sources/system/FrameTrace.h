/*
 * Frame tracing for the Linux port.
 * Off unless HPL_FRAME_TRACE is set; see cGame::Run.
 */
#ifndef HPL_FRAMETRACE_H
#define HPL_FRAMETRACE_H

#include "system/StringTypes.h"

#include <cstdint>

namespace hpl {

	namespace FrameTrace {

		/** Costs one branch when tracing is off. */
		bool Enabled();
		void SetEnabled(bool abX);

		/** Call once per frame before anything contributes. */
		void Reset();

		/** Accumulate time against a label for this frame. */
		void Add(const tString& asName, uint64_t alMS);

		/** "scene 127, physics 118, entities 2" - heaviest first, empty if nothing was recorded. */
		tString Report();

	}
}

#endif // HPL_FRAMETRACE_H
