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
#include <functional>
#include <algorithm>

#include "VeccRepair.hh"
#include "DRAMDomain.hh"
#include "GroupDomain_dimm.hh"

VeccRepair::VeccRepair(std::string name, int n_sym_correct, int n_sym_detect, int n_sym_added, double protected_fraction)
	: SoftwareTolerance(name, std::vector<double>(DRAM_MAX, 0.))
	, m_n_correct(n_sym_correct), m_n_detect(n_sym_detect)
	, m_n_additional(n_sym_added), m_protected_fraction(protected_fraction)
	, m_unprotected_swtol(DRAM_MAX, 0.), m_protected_swtol(DRAM_MAX, 0.)
{
	assert(protected_fraction >= 0. && protected_fraction <= 1.);
}

void VeccRepair::allow_software_tolerance(std::vector<double> tolerating_probability, std::vector<double> unprotected_tolerating_probability)
{
	assert(tolerating_probability.size() == DRAM_MAX && unprotected_tolerating_probability.size() == DRAM_MAX);

	m_swtol = tolerating_probability;
	m_unprotected_swtol = unprotected_tolerating_probability;

	if (m_protected_fraction == 0 || m_protected_fraction == 1)
		return;

	// swtol = unprot_swtol * (1 - prot) + prot_swtol * prot
	for (size_t c = 0; c < DRAM_MAX; c++)
		m_protected_swtol[c] = (m_swtol[c] - m_unprotected_swtol[c] * (1 - m_protected_fraction)) / m_protected_fraction;
}

failures_t VeccRepair::repair(FaultDomain *fd)
{
	GroupDomain_dimm *dd = dynamic_cast<GroupDomain_dimm *>(fd);
	assert(dynamic_cast<DRAMDomain*>(dd->getChildren().front())->getLog<Ranks>() > 0);

	const size_t log2_data_chips = floor(log2(dd->chips()));
	size_t symbol_bits = floor(log2(dd->burst_size() >> log2_data_chips));

	assert(dd->chips() == (1 << log2_data_chips) + 2 * m_n_correct);

	auto predicate = [this](FaultIntersection &error) { return error.chip_count() > m_n_correct; };

	std::list<FaultIntersection>& failures = dd->intersecting_ranges(symbol_bits, predicate);

	failures_t count = {0, 0};
	for (auto fail = failures.begin(); fail != failures.end(); )
	{
		if (check_tier2(dd, *fail))
		{
			fail = failures.erase(fail);
			continue;
		}
		else if (fail->chip_count() > m_n_detect)
		{
			fail->mark_undetectable();
			count.undetected++;
		}
		else
		{
			fail->mark_uncorrectable();
			count.uncorrected++;
		}
		++fail;
	}

	return count;
}

bool VeccRepair::check_tier2(GroupDomain_dimm *dd, FaultIntersection& error)
{
	if (error.chip_count() > m_n_correct + m_n_additional)
		return try_sw_tolerance(error, m_swtol);

	DRAMDomain *chip = error.m_pDRAM;
	fault_class_t cls = chip->maskClass(error.fWildMask);

	// Actually a bit of a shortcut.
	// The chances that an error can be tolerated for all the rows in a full bank is p^{#rows}
	// which is realistically always 0 (~16K rows?)

	if ((cls >= DRAM_1COL && cls != DRAM_1ROW) /*&& m_protected_fraction < 1.*/)
		return try_sw_tolerance(error, m_swtol);

	// TODO: implement a realistic evaluation for m_protection_fraction == 1.

	// TODO: remember pages that were protected
	if (distribution(gen) > m_protected_fraction)
		return try_sw_tolerance(error, m_unprotected_swtol);

	// Get a random location in a (the?) other rank, where the supplementary symbols (tier 2 ECC) for this DRAM row is stored
	FaultRange *tier2 = chip->genRandomRange(DRAM_1BIT, true);
	chip->put<Ranks>(tier2->fAddr, chip->get<Ranks>(error.fAddr) + 1);

	// 1 chip = 1 symbol (at least for amount of redundancy purposes) = dd->burst_size() / data_chips
	const size_t data_chips = 1ULL << static_cast<int>(floor(log2(dd->chips())));
	const size_t t2sym_size = dd->burst_size() / data_chips;
	const size_t error_size = std::max(dd->burst_size(), (error.fWildMask + 1) * data_chips);

	const size_t t2cl_size = 2 * m_n_additional * t2sym_size;
	const size_t t2err_size = t2cl_size * (error_size / dd->burst_size());

	// get the per-chip positions/masks right
	const size_t start = tier2->fAddr & ~(t2err_size / data_chips - 1), end = start + t2err_size / data_chips;
	tier2->fWildMask = (t2sym_size / data_chips - 1);
	auto find_intersecting_tier2 = [tier2](FaultRange *fr) { return tier2->intersects(fr); };

	// Iterate over all the cache lines in the fault range
	for (size_t addr = start; addr != end; addr += t2cl_size / data_chips)
	{
		// supposing all redundant symbols correct, we tolerate n_additional more symbols
		// decrement for every failed symbol: if < 0 we have an uncorrectable fault
		int allowance = m_n_correct + m_n_additional - error.chip_count();

		for (uint64_t symbol = 0; symbol < 2 * m_n_additional; tier2->fAddr += t2sym_size / data_chips, ++symbol)
		{
			for (auto chip: dd->getChildren())
			{
				DRAMDomain *dram = dynamic_cast<DRAMDomain*>(chip);

				// NB: only data chips used for Tier2 VECC to allow partial writes
				if (dram->getChipNum() >= data_chips)
					continue;

				std::list<FaultRange *> list = dram->getRanges();
				if (std::find_if(list.begin(), list.end(), find_intersecting_tier2) != list.end())
				{
					allowance--;
					break;
				}
			}

			// If any cache line encounters sufficient wong symbols, fail
			// (or tolerate at software-level with the probability for protected memory)
			if (allowance < 0)
				return try_sw_tolerance(error, m_protected_swtol);
		}

	}

	// Successfully protected!
	return true;
}
