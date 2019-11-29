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

#ifndef VECCREPAIR_HH_
#define VECCREPAIR_HH_

#include <string>
#include <iostream>
#include <map>

#include "dram_common.hh"
#include "RepairScheme.hh"
#include "FaultRange.hh"
#include "ChipKillRepair.hh"

class VeccRepair : public ChipKillRepair
{
public:
	VeccRepair(std::string name, int n_sym_correct, int n_sym_detect, int log_symbol_size, int n_sym_extra, double protected_fraction);

	std::pair<uint64_t, uint64_t> repair(GroupDomain *fd);

	void printStats() {
		std::cout << tolerated_failures << " / " << total_failures << " = " << ((100. * tolerated_failures) / total_failures)
			<< "% of failures were tolerated, targeting " << (100. * m_protected_fraction) << "%\n";
		ChipKillRepair::printStats();
	}

	void allow_software_tolerance(std::vector<double> tolerating_probability, std::vector<double> unprotected_tolerating_probability);

private:
	static const int VECC_PROTECTED      = 5;
	static const int SW_UNPROT_TOLERATED = 6;

	const uint64_t m_n_additional;
	const double m_protected_fraction;
	std::vector<double> m_unprotected_swtol;

	size_t total_failures, tolerated_failures;

	void vecc_tolerate(std::list<FaultIntersection> &failures, GroupDomain *fd);
	void software_tolerate_failures(std::list<FaultIntersection> &failures);
};


#endif /* VECCREPAIR_HH_ */

