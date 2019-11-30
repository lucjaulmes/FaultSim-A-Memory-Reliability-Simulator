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
#include "GroupDomain_dimm.hh"
#include "FaultRange.hh"


ChipKillRepair::ChipKillRepair(std::string name, int n_sym_correct, int n_sym_detect)
	: RepairScheme(name), m_n_correct(n_sym_correct), m_n_detect(n_sym_detect)
{
}

failures_t ChipKillRepair::repair(GroupDomain *fd)
{
	GroupDomain_dimm *dd = dynamic_cast<GroupDomain_dimm *>(fd);
	auto predicate = [this](FaultIntersection &error) { return error.chip_count() > m_n_correct; };

	const size_t log2_data_chips = floor(log2(dd->chips()));
	size_t symbol_bits = floor(log2(dd->burst_size() >> log2_data_chips));

	assert(dd->chips() == (1 << log2_data_chips) + 2 * m_n_correct);

	std::list<FaultIntersection>& failures = dd->intersecting_ranges(symbol_bits, predicate);

	failures_t count = {0, 0};
	for (auto fail: failures)
	{
		if (fail.chip_count() > m_n_detect)
		{
			fail.mark_undetectable();
			count.undetected++;
		}
		else
		{
			fail.mark_uncorrectable();
			count.uncorrected++;
		}
	}

	return count;
}
