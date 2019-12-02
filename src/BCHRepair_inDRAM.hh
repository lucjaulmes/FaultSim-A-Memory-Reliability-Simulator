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

#ifndef BCHREPAIR_INDRAM_HH_
#define BCHREPAIR_INDRAM_HH_

#include <set>
#include <list>
#include <tuple>
#include <string>
#include <algorithm>
#include <iostream>
#include <cassert>

#include "dram_common.hh"

#include "RepairScheme.hh"
#include "DRAMDomain.hh"


class BCHRepair_inDRAM : public RepairScheme
{
protected:
	size_t m_base_size, m_extra_size, m_n_correct;

	std::set<FaultIntersection *> modified_ranges;

	void insert(std::list<FaultRange*> &list, FaultIntersection &err);
	std::map<uint64_t, std::list<FaultRange*>> sort_per_bank(std::list<FaultRange*> &list);

public:
	BCHRepair_inDRAM(std::string name, size_t base = 128, size_t extra = 8)
		: RepairScheme(name)
		, m_base_size(base), m_extra_size(extra)
	{
		// bits must be a power of 2, and since galois fields have size 2^m - 1, the BCH code used will
		// be have m = ceil(log2(base)) size in bits of an element in the galois field
		size_t element = 1 + round(log2(base));
		size_t parity = extra % element;

		if (parity > 1 or base + extra >= (1ULL << element) - 1)
		{
			std::cerr << "Error, can not make a (" << base + extra << ", " << base << ") BCH code\n";
			std::abort();
		}
		else if (parity == 1)
		{
			std::cerr << "Error, DUE not yet implemented for in-DRAM BCH codes\n";
			std::abort();
		}

		m_n_correct = extra / element;
		// m_n_detect = m_n_correct + parity;
	}

	virtual ~BCHRepair_inDRAM() {}

	failures_t repair(FaultDomain *fd);

	virtual void reset()
	{
		for (auto &fr: modified_ranges)
			delete fr;

		modified_ranges.clear();
	}

	virtual void printStats() {}
};

#endif /* BCHREPAIR_INDRAM_HH_ */
