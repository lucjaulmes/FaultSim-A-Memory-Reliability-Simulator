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

#include "BCHRepair.hh"
#include "DRAMDomain.hh"

BCHRepair::BCHRepair(std::string name, int n_correct, int n_detect, uint64_t deviceBitWidth) : RepairScheme(name)
	, m_n_correct(n_correct)
	, m_n_detect(n_detect)
	, m_bitwidth(deviceBitWidth)
{
}

std::pair<uint64_t, uint64_t> BCHRepair::repair(FaultDomain *fd)
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;

	// Repair up to N bit faults in a single row.
	// Similar to ChipKill except that only 1 bit can be bad across all devices, instead of 1 symbol being bad.
	std::list<FaultDomain *> &pChips = fd->getChildren();

	for (FaultDomain *fd: pChips)
		for (FaultRange *fr: dynamic_cast<DRAMDomain*>(fd)->getRanges())
			fr->touched = 0;

	// Take each chip in turn.  For every fault range, compare with all chips including itself, any intersection of
	// fault range is treated as a fault. if count exceeds correction ability, fail.
	for (FaultDomain *fd0: pChips)
	{
		// For each fault in first chip, query the second chip to see if it has
		// an intersecting fault range, touched variable tells us about the location being already addressed or not
		for (FaultRange *frOrg: dynamic_cast<DRAMDomain *>(fd0)->getRanges())
		{
			FaultRange frTemp = *frOrg; // This is a fault location of a chip

			uint32_t n_intersections = 0;

			if (frTemp.touched < frTemp.max_faults)
			{
				unsigned bit_shift = 0;
				if (m_n_correct == 1) // Depending on the scheme, we will need to group the bits
				{
					bit_shift = 2;  //SECDED will give ECC every 8 byte granularity, group by 4 locations in the fault range per chip
				}
				else if (m_n_correct == 3)
				{
					bit_shift = 4;  //3EC4ED will give ECC every 32 byte granularity, group by 16 locations in the fault range per chip
				}
				else if (m_n_correct == 6)
				{
					bit_shift = 5;  //6EC7ED will give ECC every 64 byte granularity, group by 32 locations in the fault range per chip
				}
				else
					assert(0);

				//Clear the last few bits to accomodate the address range
				frTemp.fAddr = frTemp.fAddr >> bit_shift;
				frTemp.fAddr = frTemp.fAddr << bit_shift;
				frTemp.fWildMask = frTemp.fWildMask >> bit_shift;
				frTemp.fWildMask = frTemp.fWildMask << bit_shift;
				// This gives me the number of loops for the addresses near the fault range to iterate
				unsigned loopcount_locations = 1 << bit_shift;

				for (unsigned ii = 0; ii < loopcount_locations; ii++)
				{
					// for each other chip including the current one, count number of intersecting faults
					for (FaultDomain *fd1: pChips)
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
							if (frTemp.intersects(fr1) && (fr1->touched < fr1->max_faults))
							{
								// immediately move on to the next chip, we don't care about other ranges
								n_intersections += 1;
								break;
							}

					frTemp.fAddr = frTemp.fAddr + 1;
				}

				if (n_intersections > m_n_detect)
				{
					n_undetectable += n_intersections - m_n_detect;
					frOrg->transient_remove = false;
					return std::make_pair(n_undetectable, n_uncorrectable);
				}
				else if (n_intersections > m_n_correct)
				{
					n_uncorrectable += n_intersections - m_n_correct;
					frOrg->transient_remove = false;
					return std::make_pair(n_undetectable, n_uncorrectable);
				}

			}
		}
	}
	return std::make_pair(n_undetectable, n_uncorrectable);
}

uint64_t BCHRepair::fill_repl(FaultDomain *fd)
{
	return 0;
}

void BCHRepair::printStats()
{
	RepairScheme::printStats();
}

void BCHRepair::resetStats()
{
	RepairScheme::resetStats();
}


