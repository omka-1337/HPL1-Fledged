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
#include "system/FrameTrace.h"

#include <algorithm>
#include <map>
#include <vector>

namespace hpl {

	namespace FrameTrace {

		static bool gbEnabled = false;
		static std::map<tString, uint64_t> gmapTimes;

		bool Enabled() { return gbEnabled; }
		void SetEnabled(bool abX) { gbEnabled = abX; }

		void Reset() { gmapTimes.clear(); }

		void Add(const tString& asName, uint64_t alMS)
		{
			gmapTimes[asName] += alMS;
		}

		tString Report()
		{
			std::vector<std::pair<tString, uint64_t>> vEntries(gmapTimes.begin(), gmapTimes.end());

			std::sort(vEntries.begin(), vEntries.end(),
				[](const auto &a, const auto &b) { return a.second > b.second; });

			tString sReport;
			for(const auto &entry : vEntries)
			{
				// Only the parts that actually cost something are worth printing.
				if(entry.second == 0) continue;

				if(sReport.empty() == false) sReport += ", ";
				sReport += entry.first + " " + std::to_string(entry.second);
			}
			return sReport;
		}

	}
}
