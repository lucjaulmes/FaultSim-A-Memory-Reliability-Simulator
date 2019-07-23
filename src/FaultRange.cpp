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
#include <sstream>

#include "DRAMDomain.hh"
#include "FaultRange.hh"
#include "dram_common.hh"

FaultRange::FaultRange(DRAMDomain *pDRAM) :
	m_pDRAM(pDRAM)
{
	transient = 0;
	fAddr = fWildMask = 0;
	touched = 0;
	fault_mode = 0;
	transient_remove = true;
	recent_touched = false;
	max_faults = 0;
	TSV = false;
}

bool FaultRange::intersects(FaultRange *fr) const
{
	uint64_t fAddr0 = fAddr;
	uint64_t fMask0 = fWildMask;
	uint64_t fAddr1 = fr->fAddr;
	uint64_t fMask1 = fr->fWildMask;

	uint64_t combined_mask = fMask0 | fMask1;
	uint64_t equal_addr = ~(fAddr0 ^ fAddr1);
	uint64_t finalterm = ~(combined_mask | equal_addr);

	return finalterm == 0;
}

std::string FaultRange::toString() const
{
	std::ostringstream build;

	build << (transient ? "transient" : "permanent") << " TSV " << TSV;
	build << ' ' << m_pDRAM->faultClassString(m_pDRAM->maskClass(fWildMask));

	build << " fAddr("  << m_pDRAM->getRanks(fAddr)
				 << ',' << m_pDRAM->getBanks(fAddr)
				 << ',' << m_pDRAM->getRows(fAddr)
				 << ',' << m_pDRAM->getCols(fAddr)
				 << ',' << m_pDRAM->getBits(fAddr) << ')';

	build << std::hex << " fMask 0x(" << m_pDRAM->getRanks(fWildMask)
							   << ',' << m_pDRAM->getBanks(fWildMask)
							   << ',' << m_pDRAM->getRows(fWildMask)
							   << ',' << m_pDRAM->getCols(fWildMask)
							   << ',' << m_pDRAM->getBits(fWildMask) << ')';
	return build.str();
}

void FaultRange::clear()
{
}

