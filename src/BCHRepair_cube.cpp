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

#include "BCHRepair_cube.hh"
#include "DRAMDomain.hh"
#include "Settings.hh"

extern struct Settings settings;

BCHRepair_cube::BCHRepair_cube(std::string name, int n_correct, int n_detect, uint64_t data_block_bits) : RepairScheme(name)
	, m_n_correct(n_correct)
	, m_n_detect(n_detect)
	, m_bitwidth(data_block_bits)
{
	m_log_block_bits = log2(data_block_bits);
}

std::pair<uint64_t, uint64_t> BCHRepair_cube::repair(GroupDomain *fd)
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;

	// Repair up to N bit faults in a single block
	std::list<FaultDomain *> &pChips = fd->getChildren();
	//assert( pChips->size() == (m_n_repair * 18) );

	for (FaultDomain *fd: pChips)
		for (FaultRange *fr: dynamic_cast<DRAMDomain *>(fd)->getRanges())
			fr->touched = 0;

	// Take each chip in turn.  For every fault range in a chip, see which neighbors intersect it's ECC block(s).
	// Count the failed bits in each ECC block.
	for (FaultDomain *fd: pChips)
	{
		for (FaultRange *frOrg: dynamic_cast<DRAMDomain *>(fd)->getRanges())
		{
			FaultRange frTemp = *frOrg;

			uint32_t n_intersections = 0;

			if (frTemp.touched < frTemp.max_faults)
			{
				if (settings.debug)
					std::cout << m_name << ": outer " << frTemp.toString() << "\n";

				unsigned bit_shift = m_log_block_bits; // ECC every 64 byte i.e 512 bit granularity
				frTemp.fAddr = frTemp.fAddr >> bit_shift;
				frTemp.fAddr = frTemp.fAddr << bit_shift;
				frTemp.fWildMask = frTemp.fWildMask >> bit_shift;
				frTemp.fWildMask = frTemp.fWildMask << bit_shift;
				// This gives me the number of loops for the addresses near the fault range to iterate
				unsigned loopcount_locations = 1 << bit_shift;

				for (unsigned ii = 0; ii < loopcount_locations; ii++)
				{
					for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd)->getRanges())
					{
						if (settings.debug)
							std::cout << m_name << ": inner " << fr1->toString() << " bit " << ii << "\n";

						if (fr1->touched < fr1->max_faults)
						{
							if (frTemp.intersects(fr1))
							{
								if (settings.debug)
									std::cout << m_name << ": INTERSECT " << n_intersections << "\n";

								n_intersections++;

								// There was a failed bit in at least one row of the FaultRange of interest.
								// We now only care about further intersections that are in the overlapping
								// rows of the two ranges.  Narrow down the search to only those rows in common
								// to both FaultRanges.  This is achieved by;
								// 1) Set upper mask bits to zero if they are not wild in range under test
								// 2) For those wild bits that we cleared, use the specific address bit value
								uint64_t fr1_fAddr_upper = (fr1->fAddr >> bit_shift) << bit_shift;
								uint64_t frTemp_fAddr_lower = (frTemp.fAddr & ((0x1 << bit_shift) - 1));

								uint64_t old_wild_mask = frTemp.fWildMask;
								frTemp.fWildMask &= fr1->fWildMask;
								uint64_t changed_wild_bits = old_wild_mask ^ frTemp.fWildMask;
								frTemp.fAddr = (fr1_fAddr_upper & changed_wild_bits) | (frTemp.fAddr & (~changed_wild_bits)) | frTemp_fAddr_lower;

								// immediately move on to the next location
								break;
							}
							else
							{
								if (settings.debug)
									std::cout << m_name << ": NONE " << n_intersections << "\n";
							}
						}
					}
					frTemp.fAddr = frTemp.fAddr + 1;
				}

				// For this algorithm, one intersection with the bit being tested actually means one
				// faulty bit in the
				if (n_intersections <= m_n_correct)
				{
					// correctable
				}
				if (n_intersections > m_n_correct)
				{
					n_uncorrectable += (n_intersections - m_n_correct);
					frOrg->transient_remove = false;
					if (!settings.continue_running)
						return std::make_pair(n_undetectable, n_uncorrectable);
				}
				if (n_intersections >= m_n_detect)
					n_undetectable += (n_intersections - m_n_detect);
			}
		}
	}
	return std::make_pair(n_undetectable, n_uncorrectable);
}
