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
