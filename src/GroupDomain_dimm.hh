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

#ifndef GROUPDOMAIN_DIMM_HH_
#define GROUPDOMAIN_DIMM_HH_

#include <iostream>
#include <functional>

#include "dram_common.hh"
#include "GroupDomain.hh"

class GroupDomain_dimm : public GroupDomain
{
	/** Total Chips in a DIMM */
	const uint64_t m_chips;
	/** Total Banks per Chip */
	const uint64_t m_banks;
	/** The burst length per access, this determines the number of pins coming out of a Chip */
	const uint64_t m_burst_size;

public:
	virtual void setFIT_TSV(bool transient, double FIT)
	{
		std::cerr << "Error: attemting to set TSV FIT on a DIMM" << std::endl;
		std::abort();
	};

	GroupDomain_dimm(const char *name, uint64_t chips, uint64_t banks, uint64_t burst_length);

	std::list<FaultIntersection>
		intersecting_ranges(unsigned symbol_size,
							std::function<bool(FaultIntersection&)> predicate = [] (FaultIntersection &f) { return true; });
};


#endif /* GROUPDOMAIN_DIMM_HH_ */
