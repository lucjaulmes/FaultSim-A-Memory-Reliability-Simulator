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

#include <string>
#include <iostream>
#include <iomanip>
#include <list>
#include <stack>

#include "ChipKillRepair.hh"
#include "DRAMDomain.hh"
#include "FaultRange.hh"


ChipKillRepair::ChipKillRepair(std::string name, int n_sym_correct, int n_sym_detect, int log_symbol_size)
	: RepairScheme(name), m_n_correct(n_sym_correct), m_n_detect(n_sym_detect), m_symbol_mask((1ULL << log_symbol_size) - 1)
	, m_failure_sizes()
	, m_swtol(DRAM_MAX, 0.), gen(random64_engine_t(), random_uniform_t(0, 1))
{
}

void ChipKillRepair::allow_software_tolerance(std::vector<double> tolerating_probability)
{
	assert(tolerating_probability.size() == DRAM_MAX);
	m_swtol = tolerating_probability;
}

std::pair<uint64_t, uint64_t> ChipKillRepair::repair(FaultDomain *fd)
{
	std::list<FaultIntersection> sdc, due, failures = compute_failure_intersections(fd);

	for (auto fail: failures)
	{
		if (fail.chip_count() > m_n_detect)
			sdc.push_back(fail);
		else
			due.push_back(fail);
	}

	remove_duplicate_failures(sdc);
	remove_duplicate_failures(due);

	software_tolerate_failures(due);

	for (auto fail: due)
	{
		fault_class_t cls = fail.m_pDRAM->maskClass(fail.fWildMask);
		m_failure_sizes[std::make_tuple(cls, fail.chip_count(), UNCORRECTED)]++;

		for (FaultRange *fr: fail.intersecting)
			fr->transient_remove = false;
	}

	for (auto fail: sdc)
	{
		fault_class_t cls = fail.m_pDRAM->maskClass(fail.fWildMask);
		m_failure_sizes[std::make_tuple(cls, fail.chip_count(), UNDETECTED)]++;

		for (FaultRange *fr: fail.intersecting)
			fr->transient_remove = false;
	}

	return std::make_pair(sdc.size(), due.size());
}

std::list<FaultIntersection> ChipKillRepair::compute_failure_intersections(FaultDomain *fd)
{
	std::list<FaultDomain *> &pChips = fd->getChildren();
	DRAMDomain *dram0 = dynamic_cast<DRAMDomain*>(pChips.front());
	// Make sure number of children is appropriate for level of ChipKill, i.e. 18 chips per chipkill symbol correction.
	assert(pChips.size() == (m_n_correct * 18));

	// Clear out the touched values for all chips
	for (FaultDomain *chip: pChips)
		for (FaultRange *fr: dynamic_cast<DRAMDomain*>(chip)->getRanges())
			fr->touched = 0;

	// Found failures and a stack to building them through the fault range traversal
	std::list<FaultIntersection> failures;
	std::stack<FaultIntersection> error_intersection({FaultIntersection(dram0)});

	// Perform a DFS of intersecting fault ranges
	auto chip = pChips.cbegin();
	auto faultrange = dram0->getRanges().cbegin();
	std::stack<decltype(std::make_pair(chip, faultrange))> traversal({{chip, faultrange}});

	while (!traversal.empty())
	{
		std::tie(chip, faultrange) = traversal.top();
		traversal.pop();

		// Traverse all (chip, faultrange) pairs. No need to continue if we reach more intersections than we can detect
		// NB: if we reach more intersections than we can correct, a subset might still be undetectable.
		while (chip != pChips.cend() && !(error_intersection.top().chip_count() > m_n_detect))
		{
			const auto end = dynamic_cast<DRAMDomain*>(*chip)->getRanges().cend();
			for (; faultrange != end; ++faultrange)
			{
				FaultIntersection frInt(*faultrange, m_symbol_mask);

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
			if (++chip != pChips.cend())
				faultrange = dynamic_cast<DRAMDomain*>(*chip)->getRanges().cbegin();
		}

		FaultIntersection &intersection = error_intersection.top();

		// mark intersecting errors based on how many intersection symbols are affected
		// NB: for double chipkill we might mark a triple error and a double error containing this triple error
		if (intersection.chip_count() > m_n_correct)
			failures.push_back(intersection);
		else
		{
			fault_class_t cls = intersection.m_pDRAM->maskClass(intersection.fWildMask);
			m_failure_sizes[std::make_tuple(cls, intersection.chip_count(), CORRECTED)]++;
		}

		error_intersection.pop();
	}

	return failures;
}

void ChipKillRepair::software_tolerate_failures(std::list<FaultIntersection> &failures)
{
	for (auto it = failures.begin(); it != failures.end();)
	{
		fault_class_t cls = it->m_pDRAM->maskClass(it->fWildMask);
		// if (!it->transient) ++it; else
		if (gen() < m_swtol.at(cls))
		{
			m_failure_sizes[std::make_tuple(cls, it->chip_count(), SW_TOLERATED)]++;
			it = failures.erase(it);
		}
		else
			++it;
	}
}

void ChipKillRepair::remove_duplicate_failures(std::list<FaultIntersection> &failures)
{
	if (failures.empty())
		return;

	failures.sort([] (FaultRange &a, FaultRange &b)
		{
			return std::make_pair(a.fAddr, a.fAddr | a.fWildMask) < std::make_pair(b.fAddr, b.fAddr | b.fWildMask);
		});

	for (auto it = failures.begin(), nx = std::next(it); nx != failures.end(); )
	{
		uint64_t it_start = it->fAddr, it_end = (it_start | it->fWildMask) + 1; // it = {}
		uint64_t nx_start = nx->fAddr, nx_end = (nx_start | nx->fWildMask) + 1; // nx = ()

		// from sorting
		assert (it_start <= nx_start);

		if (it_end <= nx_start)
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

uint64_t ChipKillRepair::fill_repl(FaultDomain *fd)
{
	return 0;
}

void ChipKillRepair::printStats()
{
	std::cout << "Failure sizes\nsize:symbols:outcome:count\n";

	size_t fault_class, n_symbols, outcome;
	for (auto &stat: m_failure_sizes)
	{
		std::tie(fault_class, n_symbols, outcome) = stat.first;
		std::cout << DRAMDomain::faultClassString(fault_class_t(fault_class))
				  << ':' << n_symbols << ':' << outcome << ':' << stat.second << '\n';
	}
	std::cout << std::endl;

	RepairScheme::printStats();
}

void ChipKillRepair::resetStats()
{
	m_failure_sizes.clear();

	RepairScheme::resetStats();
}

void FaultIntersection::intersection(const FaultIntersection &fr)
{
	// NB: only works for errors that intersect, meaning that have equal address bits outside of their respective masks
	fAddr = (fAddr & ~fWildMask) | (fr.fAddr & ~fr.fWildMask);
	fWildMask &= fr.fWildMask;

	transient_remove = transient = transient || fr.transient;
	std::copy(fr.intersecting.begin(), fr.intersecting.end(), std::back_inserter(intersecting));
}

std::string FaultIntersection::toString()
{
	std::ostringstream build;
	build << FaultRange::toString();
	build << " intersection of " << intersecting.size() << " faults";
	return build.str();
}
