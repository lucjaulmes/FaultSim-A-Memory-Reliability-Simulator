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

#include <iostream>
#include <string>

#include "BCHRepair.hh"
#include "GroupDomain.hh"
#include "DRAMDomain.hh"

BCHRepair::BCHRepair(std::string name, int n_correct, int n_detect, uint64_t deviceBitWidth) : RepairScheme(name)
	, m_n_correct(n_correct)
	, m_n_detect(n_detect)
	, m_bitwidth(deviceBitWidth)
{
	// word mask should be 2, 4 and 5 bits respectively
	assert(m_n_correct + 1 == m_n_detect);

	if (m_n_correct == 1)
		// SECDED => ECC computed at 8B granularity => group by 4 locations per chip
		m_word_mask = 1ULL << 2;
	else if (m_n_correct == 3)
		// 3EC4ED => ECC computed at 32B granularity => group by 16 locations per chip
		m_word_mask = 1ULL << 4;
	else if (m_n_correct == 6)
		// 6EC7ED => ECC computed at 64B granularity => group by 32 locations per chip
		m_word_mask = 1ULL << 5;
	else
	{
		std::cerr << "BCH " << n_correct << "EC" << n_detect << "ED" << " not implemented!" << std::endl;
		std::abort();
	}
}

std::pair<uint64_t, uint64_t> BCHRepair::repair(GroupDomain *fd)
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;

	// Repair up to N bit faults in a single row.
	// Similar to ChipKill except that only 1 bit can be bad across all devices, instead of 1 symbol being bad.
	std::list<FaultDomain *> &pChips = fd->getChildren();

	// Take each chip in turn.  For every fault range, compare with all chips including itself, any intersection of
	// fault range is treated as a fault. if count exceeds correction ability, fail.
	for (FaultDomain *fd0: pChips)
	{
		// For each fault in first chip, query the second chip to see if it has an intersecting fault range
		for (FaultRange *frOrg: dynamic_cast<DRAMDomain *>(fd0)->getRanges())
		{
			FaultRange frTemp = *frOrg; // This is a fault location of a chip

			// Clear the last few bits to match errors that affect the same ECC word distributed across chips
			frTemp.fAddr = frTemp.fAddr & ~m_word_mask;
			frTemp.fWildMask = frTemp.fWildMask | m_word_mask;

			// Count the number of bits that are affected
			uint32_t n_errors = __builtin_popcount(frOrg->fWildMask | m_word_mask);

			// for each other chip including the current one, count number of intersecting faults
			for (FaultDomain *fd1: pChips)
			{
				// Aggregate erroneous bits for this word in chip fd1
				uint32_t chip_error_mask = 0;
				for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
					if (frTemp.intersects(fr1))
						chip_error_mask |= (fr1->fWildMask & m_word_mask);

				n_errors += __builtin_popcount(chip_error_mask);
			}


			if (n_errors > m_n_detect)
			{
				n_undetectable += n_errors - m_n_detect;
				frOrg->transient_remove = false;
				return std::make_pair(n_undetectable, n_uncorrectable);
			}
			else if (n_errors > m_n_correct)
			{
				n_uncorrectable += n_errors - m_n_correct;
				frOrg->transient_remove = false;
				return std::make_pair(n_undetectable, n_uncorrectable);
			}
		}
	}
	return std::make_pair(n_undetectable, n_uncorrectable);
}
