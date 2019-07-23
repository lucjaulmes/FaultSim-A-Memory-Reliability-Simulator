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

bool FaultRange::intersects(FaultRange *fr)
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

std::string FaultRange::toString()
{
	char buf[100];

	uint64_t fA = fAddr;
	uint64_t fM = fWildMask;

	// decode address
	uint fbit = fA & ((0x1 << m_pDRAM->getLogBits()) - 1);
	fA >>= m_pDRAM->getLogBits();
	uint fcol = fA & ((0x1 << m_pDRAM->getLogCols()) - 1);
	fA >>= m_pDRAM->getLogCols();
	uint frow = fA & ((0x1 << m_pDRAM->getLogRows()) - 1);
	fA >>= m_pDRAM->getLogRows();
	uint fbank = fA & ((0x1 << m_pDRAM->getLogBanks()) - 1);
	fA >>= m_pDRAM->getLogBanks();
	uint frank = fA & ((0x1 << m_pDRAM->getLogRanks()) - 1);
	fA >>= m_pDRAM->getLogRanks();

	// decode mask
	uint mbit = fM & ((0x1 << m_pDRAM->getLogBits()) - 1);
	fM >>= m_pDRAM->getLogBits();
	uint mcol = fM & ((0x1 << m_pDRAM->getLogCols()) - 1);
	fM >>= m_pDRAM->getLogCols();
	uint mrow = fM & ((0x1 << m_pDRAM->getLogRows()) - 1);
	fM >>= m_pDRAM->getLogRows();
	uint mbank = fM & ((0x1 << m_pDRAM->getLogBanks()) - 1);
	fM >>= m_pDRAM->getLogBanks();
	uint mrank = fM & ((0x1 << m_pDRAM->getLogRanks()) - 1);
	fM >>= m_pDRAM->getLogRanks();

	//sprintf( buf, "trans %d fAddr 0x%lX fMask 0x%lX", transient, fAddr, fWildMask );

	sprintf(buf, "TSV %d trans %d fAddr (%d,%d,%d,%d,%d) fMask 0x(%X,%X,%X,%X,%X)", TSV, transient,
	    frank, fbank, frow, fcol, fbit,
	    mrank, mbank, mrow, mcol, mbit);

	return std::string(buf);
}

void FaultRange::clear()
{
}
