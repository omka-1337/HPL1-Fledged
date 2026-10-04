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
