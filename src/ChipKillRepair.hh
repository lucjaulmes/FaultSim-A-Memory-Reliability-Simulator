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

#ifndef CHIPKILLREPAIR_HH_
#define CHIPKILLREPAIR_HH_

#include <string>
#include <tuple>
#include <map>

#include "dram_common.hh"
#include "RepairScheme.hh"
#include "FaultRange.hh"
#include "GroupDomain.hh"

class ChipKillRepair : public RepairScheme
{
public:
	ChipKillRepair(std::string name, int n_sym_correct, int n_sym_detect, int log_symbol_size = 3);

	std::pair<uint64_t, uint64_t> repair(GroupDomain *fd);
	virtual void printStats();

	virtual void allow_software_tolerance(std::vector<double> tolerating_probability);

protected:
	static const int CORRECTED    = 1;
	static const int UNCORRECTED  = 2;
	static const int UNDETECTED   = 3;
	static const int SW_TOLERATED = 4;

	const uint64_t m_n_correct, m_n_detect, m_symbol_mask;
	std::map<std::tuple<size_t, size_t, int>, size_t> m_failure_sizes;

	void remove_duplicate_failures(std::list<FaultIntersection> &failures);
	std::list<FaultIntersection> compute_failure_intersections(GroupDomain *fd);
	virtual void software_tolerate_failures(std::list<FaultIntersection> &failures);

	std::vector<double> m_swtol;
	random_generator_t gen;
};

#endif /* CHIPKILLREPAIR_HH_ */
