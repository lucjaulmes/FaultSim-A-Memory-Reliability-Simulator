/*
Copyright (c) 2015, Advanced Micro Devices, Inc. All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <functional>
#include <stack>

#include "FaultRange.hh"
#include "DRAMDomain.hh"

#include "GroupDomain_dimm.hh"

/** This functions returns the lost of fault intersections that intersect at a granularity given by symbol_size, subject to being
 * validated by the predicate.
 *
 * For example, to access all faults that would cause a DUE in ChipKill, and the predicate should test whether the FaultIntersection
 * contains at least 2 symbols (as they are always from different chips).
 *
 * To access all faults that would cause DUE in 3EC4ED, the predicate should test whether the number erroneous bits in the
 * FaultIntersection is at lest 3.
 */

std::list<FaultIntersection>& GroupDomain_dimm::intersecting_ranges(unsigned symbol_size,
																   std::function<bool(FaultIntersection&)> predicate)
{
	if (m_failures_computed)
	{
		for (auto it = m_failures.begin(); it != m_failures.end();)
			if (predicate(*it))
				++it;
			else
				it = m_failures.erase(it);

		return m_failures;
	}

	const uint64_t symbol_wild_mask = (1ULL << symbol_size) - 1;

	// Found failures and a stack to building them through the fault range traversal
	std::stack<FaultIntersection> error_intersection({FaultIntersection()});

	// Perform a DFS of intersecting fault ranges
	auto chip = m_children.cbegin();
	auto faultrange = dynamic_cast<DRAMDomain*>(*chip)->getRanges().cbegin();
	std::stack<decltype(std::make_pair(chip, faultrange))> traversal({{chip, faultrange}});

	while (!traversal.empty())
	{
		std::tie(chip, faultrange) = traversal.top();
		traversal.pop();

		// Traverse all (chip, faultrange) pairs.
		while (chip != m_children.cend())
		{
			const auto end = dynamic_cast<DRAMDomain*>(*chip)->getRanges().cend();
			for (; faultrange != end; ++faultrange)
			{
				FaultIntersection frInt(*faultrange, symbol_wild_mask);

				assert( (frInt.fAddr & frInt.fWildMask) == 0 );

				// check if the fault range *faultrange intersects the previous set of intersecting fault ranges
				if (frInt.intersects(&error_intersection.top()))
					frInt.intersection(error_intersection.top());
				else
					// no intersection, move on to the next fault range in this chip
					continue;

				// save the cumulated intersection for comparison with the next faults
				error_intersection.push(frInt);

				// we’ll come back here with the next fault range instead of this one
				traversal.push(std::make_pair(chip, std::next(faultrange)));

				// advance to next chip since only fautl ranges on different faults can intersect
				break;
			}

			// advance chip and set new FaultRange *faultrange here and not at the loop beginning,
			// to allow the pop() mechanism to work
			if (++chip != m_children.cend())
				faultrange = dynamic_cast<DRAMDomain*>(*chip)->getRanges().cbegin();
		}

		FaultIntersection &intersection = error_intersection.top();

		// mark intersecting errors based on how many intersection symbols are affected
		// NB: for double chipkill we might mark a triple error and a double error containing this triple error
		if (predicate(intersection))
			m_failures.push_back(intersection);

		error_intersection.pop();
	}

	return m_failures;
}

/* Removes duplicate failures
void GroupDomain_dimm::failures_stats()
{
	if (failures.empty())
		return;

	failures.sort([] (FaultRange &a, FaultRange &b)
		{
			return std::make_tuple(a.outcome, a.fAddr, a.fAddr | a.fWildMask) < std::make_tuple(b.outcome, b.fAddr, b.fAddr | b.fWildMask);
		});

	for (auto it = failures.begin(), nx = std::next(it); nx != failures.end(); )
	{
		uint64_t it_start = it->fAddr, it_end = (it_start | it->fWildMask) + 1; // it = {}
		uint64_t nx_start = nx->fAddr, nx_end = (nx_start | nx->fWildMask) + 1; // nx = ()

		// from sorting
		assert (it_start <= nx_start);

		if (it->outcome != nx->outcome || it_end <= nx_start)
			// disjoint case it_start < it_end ≤ nx_start < nx_end { } ( )  , move on.
			++nx, ++it;
		else if (it_end >= nx_end)
			// fully overlapping it_start ≤ nx_start ≤ nx_end ≤ it_end { ( ) }
			nx = failures.erase(nx);
		else if (it_start == nx_start)
		{
			// fully overlapping it_start = nx_start ≤ it_end < nx_end  [({]  } )
			it = failures.erase(it);
			nx = std::next(it);
		}
		else
		{
			// annoying overlap: it_start < nx_start < it_end < nx_end { ( } )
			// Typically 2 columns in the same bank. If not, print a warning.
			if (! (it->m_pDRAM->maskClass(it->fWildMask) == DRAM_1COL && nx->m_pDRAM->maskClass(nx->fWildMask) == DRAM_1COL
						&& it->m_pDRAM->getRanks(it->fWildMask) == nx->m_pDRAM->getRanks(nx->fWildMask)
						&& it->m_pDRAM->getBanks(it->fWildMask) == nx->m_pDRAM->getBanks(nx->fWildMask)
				  ))
				std::cout << "\nWARNING: unexpected partial overlap between ranges " << std::showbase << std::hex
					<< it_start << " | " << it->fWildMask << " -> " << it_end - 1 << " and "
					<< nx_start << " | " << nx->fWildMask << " -> " << nx_end - 1 << std::dec
					<< "\n  " << *it << "\n  " << *nx << std::endl;

			// Unsure masks can be combined, continue
			++nx, ++it;
		}
	}
}
*/
