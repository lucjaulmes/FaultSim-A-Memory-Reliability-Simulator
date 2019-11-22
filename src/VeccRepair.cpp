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

#include "VeccRepair.hh"
#include "DRAMDomain.hh"

VeccRepair::VeccRepair(std::string name, int n_sym_correct, int n_sym_detect, int log_symbol_size, int n_sym_added, double protected_fraction)
	: ChipKillRepair(name, n_sym_correct, n_sym_detect, log_symbol_size)
	, m_n_additional(n_sym_added), m_protected_fraction(protected_fraction), total_failures(0), tolerated_failures(0)
{
	assert(protected_fraction >= 0. && protected_fraction <= 1.);
}

void VeccRepair::allow_software_tolerance(std::vector<double> tolerating_probability, std::vector<double> unprotected_tolerating_probability)
{
	assert(tolerating_probability.size() == DRAM_MAX && unprotected_tolerating_probability.size() == DRAM_MAX);
	m_swtol = tolerating_probability;
	m_unprotected_swtol = unprotected_tolerating_probability;
}

std::pair<uint64_t, uint64_t> VeccRepair::repair(FaultDomain *fd)
{
	std::list<FaultIntersection> sdc, due, failures = compute_failure_intersections(fd);
	vecc_tolerate(failures, fd);

	for (auto fail: failures)
	{
		if (fail.chip_count() - fail.offset > m_n_detect)
			sdc.push_back(fail);
		else if (fail.chip_count() - fail.offset > m_n_correct)
			due.push_back(fail);
		else
			m_failure_sizes[std::make_tuple(fail.m_pDRAM->maskClass(fail.fWildMask), fail.chip_count(), VECC_PROTECTED)]++;
	}

	total_failures += failures.size();
	tolerated_failures += (failures.size() - sdc.size() - due.size());

	remove_duplicate_failures(sdc);
	remove_duplicate_failures(due);

	//software_tolerate_failures(sdc);
	software_tolerate_failures(due);

	for (auto fail: due)
	{
		fault_class_t cls = fail.m_pDRAM->maskClass(fail.fWildMask);
		m_failure_sizes[std::make_tuple(cls, fail.chip_count(), UNCORRECTED)]++;

		for (FaultRange *fr: fail.intersecting)
			fr->transient_remove = false;
	}

	for (auto fail: sdc)
	{
		fault_class_t cls = fail.m_pDRAM->maskClass(fail.fWildMask);
		m_failure_sizes[std::make_tuple(cls, fail.chip_count(), UNDETECTED)]++;

		for (FaultRange *fr: fail.intersecting)
			fr->transient_remove = false;
	}

	return std::make_pair(sdc.size(), due.size());
}

void VeccRepair::software_tolerate_failures(std::list<FaultIntersection> &failures)
{
	for (auto it = failures.begin(); it != failures.end();)
	{
		fault_class_t cls = it->m_pDRAM->maskClass(it->fWildMask);
		// if (!it->transient); else
		if (it->chip_count() <= m_n_correct + m_n_additional
				// memory region intentionally not protected by VECC, different software tolerance
				&& gen() < m_unprotected_swtol.at(cls))
		{
			m_failure_sizes[std::make_tuple(cls, it->chip_count(), SW_UNPROT_TOLERATED)]++;
			it = failures.erase(it);
		}
		else if (gen() < m_swtol.at(it->m_pDRAM->maskClass(it->fWildMask)))
		{
			m_failure_sizes[std::make_tuple(cls, it->chip_count(), SW_TOLERATED)]++;
			it = failures.erase(it);
		}
		else
			++it;
	}
}

void VeccRepair::vecc_tolerate(std::list<FaultIntersection> &failures, FaultDomain *fd)
{
	std::list<FaultDomain *> &pChips = fd->getChildren();
	DRAMDomain *dram0 = dynamic_cast<DRAMDomain*>(pChips.front());

	size_t n_ranks = 1ULL << dram0->getLogRanks();
	assert(n_ranks > 1);

	// Check which DRAM rows are protected
	std::map<uint64_t, bool> protected_pages_map;
	for (auto fail = failures.begin(); fail != failures.end(); ++fail)
	{
		if (dram0->maskClass(fail->fWildMask) >= DRAM_NBANK || fail->chip_count() > m_n_correct + m_n_additional)
		{
			// do not even attempt on full rank fail or more
			// also do not attempt if there are more failed symbols than we could correct with additional protection
			continue;
		}

		uint64_t first_row = fail->fAddr, last_row = (fail->fAddr | fail->fWildMask) + 1, row_size = 0;
		dram0->setBits(first_row, 0); dram0->setCols(first_row, 0);
		dram0->setBits(last_row, 0);  dram0->setCols(last_row, 0);
		dram0->setRows(row_size, 1);

		bool all_rows_protected = true;
		if (last_row == first_row)
			last_row += row_size;

		for (uint64_t address = first_row; address != last_row; address += row_size)
		{
			auto it = protected_pages_map.find(address);
			if (it != protected_pages_map.end())
				;
			else if (gen() < m_protected_fraction)
			{
				// Get a random location in the (an?) other rank, where the supplementary symbols for this DRAM row is stored
				FaultRange *redundant_location = dram0->genRandomRange(DRAM_1BIT, true);
				dram0->setRanks(redundant_location->fAddr, (dram0->getRanks(fail->fAddr) + 1) % n_ranks);

				// TODO What is the size? symbol size × redundant symbols × ECC words per DRAM row
				// -> transform to chip address bit address = ...address bits[DRAM chip][chip width]
				// e.g. 2 symbols / ECC word => 2B / 64B.
				// NB 1 tolerated symbol == 2 stored symbols.
				size_t protecting_chunk_mask = (row_size * 2 * m_n_additional / 64) - 1;
				redundant_location->fAddr	  &= ~protecting_chunk_mask;
				redundant_location->fWildMask |= protecting_chunk_mask;

				// TODO bool too simple if m_n_additional > 1
				bool redundant_location_ok = true;
				for (auto chip: pChips)
				{
					std::list<FaultRange *> list = dynamic_cast<DRAMDomain*>(chip)->getRanges();
					auto intersection = std::find_if(list.begin(), list.end(), [&] (FaultRange *fault)
						{
							return fault->intersects(redundant_location);
						});

					if (intersection != list.end())
					{
						redundant_location_ok = false;
						break;
					}
				}
				std::tie(it, std::ignore) = protected_pages_map.insert({address, redundant_location_ok});
			}
			else
				std::tie(it, std::ignore) = protected_pages_map.insert({address, false});

			all_rows_protected = all_rows_protected && it->second;
		}

		// TODO bool too simple if m_n_additional > 1, because we might offset by N < m_n_additional symbols
		if (all_rows_protected)
			fail->offset += m_n_additional;
	}
}
