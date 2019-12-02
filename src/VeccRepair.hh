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
#include "DRAMDomain.hh"
#include "GroupDomain_dimm.hh"
#include "ChipKillRepair.hh"


class SoftwareTolerance : public RepairScheme
{
protected:
	std::vector<double> m_swtol;
	random_generator_t gen;

	inline
	bool try_sw_tolerance(FaultIntersection &error, std::vector<double> swtol)
	{
		// TODO: memoize redundant locations
		fault_class_t cls = error.m_pDRAM->maskClass(error.fWildMask);
		return /*!it->transient && */ gen() < swtol.at(cls);
	}

	inline
	bool try_sw_tolerance(FaultIntersection &error)
	{
		return try_sw_tolerance(error, m_swtol);
	}

public:
	SoftwareTolerance(std::string name, std::vector<double> tolerating_probability)
		: RepairScheme(name)
		, m_swtol(tolerating_probability), gen(random64_engine_t(), random_uniform_t(0, 1))
	{
		assert(m_swtol.size() == DRAM_MAX);
	}

	failures_t repair(FaultDomain *fd)
	{
		GroupDomain_dimm *dd = dynamic_cast<GroupDomain_dimm *>(fd);
		std::list<FaultIntersection>& failures = dd->intersecting_ranges(log2(dd->burst_size()));
		failures_t remaining = {0, 0};

		for (auto it = failures.begin(); it != failures.end();)
			if (try_sw_tolerance(*it))
				it = failures.erase(it);
			else
			{
				if (it->detected())
					remaining.uncorrected++;
				else
					remaining.undetected++;
				++it;
			}

		return remaining;
	}

	void reset() {}
};

/** VeccRepair is a Software-aware but Hardware-level technique, which is why it reimplements SoftwareTolerance:
 *  the way in which the fraction of memory that has extended protection is chosen affects the software-level failure tolerance.
 *
 * That is, P(software tolerance anywhere in memory) != P(software tolerance in protected memory)
 *
 * By default, all the software tolerating probabilities are set to 0, but they can be specified with allow_software_tolerance()
 */
class VeccRepair : public SoftwareTolerance
{
public:

	VeccRepair(std::string name, int n_sym_correct, int n_sym_detect, int n_sym_extra, double protected_fraction);

	failures_t repair(FaultDomain *fd);

	void allow_software_tolerance(std::vector<double> tolerating_probability, std::vector<double> unprotected_tolerating_probability);

private:
	const uint64_t m_n_correct, m_n_detect, m_n_additional;
	const double m_protected_fraction;
	std::vector<double> m_unprotected_swtol, m_protected_swtol;

	inline
	uint64_t get_row_address(FaultRange *fr)
	{
		DRAMDomain *chip = fr->m_pDRAM;
		return chip->setCols(chip->setBits(fr->fAddr, 0), 0);
	}

	void vecc_tolerate(std::list<FaultIntersection> &failures, GroupDomain *fd);
	bool check_tier2(GroupDomain_dimm *dd, FaultIntersection& error);
};


#endif /* VECCREPAIR_HH_ */

