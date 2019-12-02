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
#include <cassert>

#include "BCHRepair.hh"
#include "GroupDomain.hh"
#include "GroupDomain_dimm.hh"
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
		m_word_bits = 2;
	else if (m_n_correct == 3)
		// 3EC4ED => ECC computed at 32B granularity => group by 16 locations per chip
		m_word_bits = 4;
	else if (m_n_correct == 6)
		// 6EC7ED => ECC computed at 64B granularity => group by 32 locations per chip
		m_word_bits = 5;
	else
	{
		std::cerr << "BCH " << n_correct << "EC" << n_detect << "ED" << " not implemented!" << std::endl;
		std::abort();
	}
	m_word_mask = 1ULL << m_word_bits;
}

failures_t BCHRepair::repair(FaultDomain *fd)
{
	GroupDomain_dimm *dd = dynamic_cast<GroupDomain_dimm *>(fd);
	auto predicate = [this](FaultIntersection &error) { return error.bit_count(m_word_mask) > m_n_correct; };

	std::list<FaultIntersection>& failures = dd->intersecting_ranges(m_word_bits, predicate);

	failures_t count = {0, 0};
	for (auto fail: failures)
	{
		if (fail.bit_count(m_word_mask) > m_n_detect)
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
