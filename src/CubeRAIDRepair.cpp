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

#include "CubeRAIDRepair.hh"
#include "DRAMDomain.hh"
#include "GroupDomain_cube.hh"
#include "Settings.hh"

extern struct Settings settings;

CubeRAIDRepair::CubeRAIDRepair(std::string name, unsigned n_sym_correct, unsigned n_sym_detect, unsigned data_block_bits)
	: RepairScheme(name)
	, m_n_correct(n_sym_correct)
	, m_n_detect(n_sym_detect)
	, m_data_block_bits(data_block_bits)
	, m_log_block_bits(log2(data_block_bits))
{
}

failures_t CubeRAIDRepair::repair(FaultDomain *fd)
{
	GroupDomain_cube *cd = dynamic_cast<GroupDomain_cube *>(fd);
	failures_t fail = {0, 0};
	// Repair this module.  Assume 8-bit symbols.

	std::list<FaultDomain *> &pChips = cd->getChildren();

	//Clear out the touched values for all chips
	for (FaultDomain *cd: pChips)
		for (FaultRange *fr: dynamic_cast<DRAMDomain *>(cd)->getRanges())
			fr->touched = 0;

	// Take each chip in turn.  For every fault range,
	// count the number of intersecting faults.
	// if count exceeds correction ability, fail.
	for (FaultDomain *fd0: pChips)
	{
		// For each fault in first chip, query the other chips to see if they have
		// an intersecting fault range.
		for (FaultRange *frOrg0: dynamic_cast<DRAMDomain *>(fd0)->getRanges())
		{
			// Round the FR size to that of a detection block (e.g. cache line)
			// on a copy, otherwise fault is modified as a side-effect
			FaultRange frTemp0 = *frOrg0;
			frTemp0.fWildMask |= ((1 << m_log_block_bits) - 1);

			uint32_t n_intersections = 0;
			if (frTemp0.touched < frTemp0.max_faults)
			{
				// for each other chip, count number of intersecting faults
				for (FaultDomain *fd1: pChips)
				{
					if (fd0 == fd1) continue;    // skip if we're looking at the first chip

					for (FaultRange *frOrg1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
					{
						// round the FR size to that of a detection block (e.g. cache line)
						FaultRange frTemp1 = *frOrg1;
						frTemp1.fWildMask |= ((1 << m_log_block_bits) - 1);

						if (frTemp1.touched < frTemp1.max_faults)
						{
							if (frTemp0.intersects(&frTemp1))
							{
								// count the intersection
								n_intersections++;
								break;
							}
						}
					}
				}
			}

			// 1 intersection implies 2 overlapping faults
			if (n_intersections < m_n_correct)
			{
				// correctable
			}
			if (n_intersections >= m_n_correct)
			{
				// uncorrectable fault discovered
				fail.uncorrected += (n_intersections + 1 - m_n_correct);
				frOrg0->transient_remove = false;

				if (!settings.continue_running)
					return fail;
			}
			if (n_intersections >= m_n_detect)
				fail.undetected += (n_intersections + 1 - m_n_detect);
		}
	}

	return fail;
}
