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

#include "ChipKillRepair.hh"
#include "DRAMDomain.hh"

ChipKillRepair::ChipKillRepair(std::string name, int n_sym_correct, int n_sym_detect)
	: RepairScheme(name), m_n_correct(n_sym_correct), m_n_detect(n_sym_detect)
{
}

std::pair<uint64_t, uint64_t> ChipKillRepair::repair(FaultDomain *fd)
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;
	// Repair this module, assuming 8-bit symbols.

	std::list<FaultDomain *> &pChips = fd->getChildren();
	// Make sure number of children is appropriate for level of ChipKill, i.e. 18 chips per chipkill symbol correction.
	assert(pChips.size() == (m_n_correct * 18));

	// Clear out the touched values for all chips
	for (FaultDomain *chip: pChips)
		for (FaultRange *fr: dynamic_cast<DRAMDomain*>(chip)->getRanges())
			fr->touched = 0;

	// Take each chip in turn.  For every fault range, count the number of intersecting faults (rounded to an 8-bit range).
	// If count exceeds correction ability, fail.
	for (FaultDomain *chip0: pChips)
	{
		// For each fault in a chip, query the following chips to see if they have intersecting fault ranges.
		for (FaultRange *frOrg: dynamic_cast<DRAMDomain*>(chip0)->getRanges())
		{
			// tweak the query range to cover 8-bit block on a copy, otherwise fault is modified as a side-effect
			FaultRange frTemp = *frOrg;
			frTemp.fWildMask |= ((0x1 << 3) - 1);
			uint32_t n_intersections = 0;
			if (frTemp.touched < frTemp.max_faults)
			{
				// for each other chip, count number of intersecting faults
				for (FaultDomain *chip1: pChips)
					for (FaultRange *fr1: dynamic_cast<DRAMDomain*>(chip1)->getRanges())
						if (frTemp.intersects(fr1))
						// && (fr1->touched < fr1->max_faults)
						{
							// count the intersection
							n_intersections++;
							break;
						}
			}
			if (n_intersections <= m_n_correct)
			{
				if (frOrg->fWildMask > m_n_correct)
					frOrg->transient_remove = false;
			}
			else if (n_intersections > m_n_detect)
			{
				n_undetectable += n_intersections - m_n_detect;
				frOrg->transient_remove = false;
			}
			else if (n_intersections > m_n_correct)
			{
				n_uncorrectable += n_intersections - m_n_correct;
				frOrg->transient_remove = false;
			}
		}
	}

	return std::make_pair(n_undetectable, n_uncorrectable);
}

uint64_t ChipKillRepair::fill_repl(FaultDomain *fd)
{
	return 0;
}

void ChipKillRepair::printStats()
{
	RepairScheme::printStats();
}

void ChipKillRepair::resetStats()
{
	RepairScheme::resetStats();
}
