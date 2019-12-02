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

#ifndef CHIPKILLREPAIR_CUBE_HH_
#define CHIPKILLREPAIR_CUBE_HH_

#include <string>

#include "RepairScheme.hh"
#include "GroupDomain.hh"
#include "GroupDomain_cube.hh"
#include "FaultRange.hh"
#include "DRAMDomain.hh"

class ChipKillRepair_cube : public RepairScheme
{
public:
	ChipKillRepair_cube(std::string name, int n_sym_correct, int n_sym_detect, GroupDomain_cube *fd);

	failures_t repair(FaultDomain *fd);
	void reset() {};

private:
	failures_t repair_horizontalTSV(GroupDomain_cube *fd);
	failures_t repair_verticalTSV(GroupDomain_cube *fd);

	inline int64_t getbank_number(FaultRange &fr)
	{
		if (fr.m_pDRAM->has<Banks>(fr.fWildMask))
			return fr.m_pDRAM->get<Banks>(fr.fAddr);
		else
			return -1;
	}

	const uint64_t m_n_correct, m_n_detect;
	uint32_t logBits, logCols, logRows, banks;
};


#endif /* CHIPKILLREPAIR_CUBE_HH_ */
