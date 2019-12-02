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
#include <list>
#include <map>
#include <cassert>

#include "FaultRange.hh"
#include "GroupDomain_dimm.hh"

#include "BCHRepair_inDRAM.hh"


void BCHRepair_inDRAM::insert(std::list<FaultRange*> &list, FaultIntersection &err)
{
	FaultIntersection *modified = new FaultIntersection(std::move(err));
	modified_ranges.insert(modified);
	list.push_back(dynamic_cast<FaultRange*>(modified));
}


std::map<uint64_t, std::list<FaultRange*>> BCHRepair_inDRAM::sort_per_bank(std::list<FaultRange*> &list)
{
	// sort per bank into buckets
	std::map<uint64_t, std::list<FaultRange*>> bank_ranges;
	for (auto it = list.begin(); it != list.end();)
	{
		FaultRange *fault = *it;
		DRAMDomain *dram = fault->m_pDRAM;

		fault_class_t cls = dram->maskClass(fault->fWildMask);
		// Leave big errors out of this
		if (cls > DRAM_1COL)
		{
			assert(not dram->has<Cols>(fault->fWildMask));
			++it;
			continue;
		}

		// DRAM_1COL or DRAM_1WORD or DRAM_1BIT
		uint64_t bank_address = fault->fAddr;
		dram->put<Cols>(bank_address, 0U);
		dram->put<Bits>(bank_address, 0U);

		bank_ranges[bank_address].push_back(fault);

		it = list.erase(it);
	}

	return bank_ranges;
}


failures_t BCHRepair_inDRAM::repair(FaultDomain *fd)
{
	DRAMDomain *dram = dynamic_cast<DRAMDomain*>(fd);
	if ((dram->getNum<Cols>() * dram->getNum<Bits>()) % (m_base_size + m_extra_size) != 0
			or (m_base_size + m_extra_size) % dram->getNum<Bits>() != 0)
	{
		std::cerr << "Wrong size of chip for in-DRAM BCH (" << m_base_size + m_extra_size << ", " << m_base_size << ") ECC\n";
		std::abort();
	}

	std::list<FaultRange*> &raw_faults = dram->getRanges();

	// NB: this is the number of columns in a codeword *before* correction
	const size_t codeword_cols_in = (m_base_size + m_extra_size) / dram->getNum<Bits>();
	const size_t codewords_per_row = dram->getNum<Cols>() / codeword_cols_in;
	// NB: this is the number of columns in a codeword *after* correction
	const size_t codeword_cols_out = m_base_size / dram->getNum<Bits>();

	for (auto &bank_ranges: sort_per_bank(raw_faults))
	{
		std::map<int32_t, FaultIntersection> columns;
		std::map<std::pair<int32_t, int32_t>, FaultIntersection> words;

		for (FaultRange *err: bank_ranges.second)
		{
			size_t codeword = dram->get<Cols>(err->fAddr) / codeword_cols_in;
			FaultIntersection add(err, m_base_size - 1);

			// Renumber the column post-ECC
			dram->put<Cols>(add.fAddr, codeword * codeword_cols_out);

			if (not dram->has<Rows>(err->fWildMask))
				columns[codeword].intersection(add);
			else
				words[std::make_pair(codeword, dram->get<Rows>(err->fAddr))].intersection(add);
		}

		std::vector<bool> failed_codeword_columns(codewords_per_row, false);
		for (auto column_error_it = columns.begin(); column_error_it != columns.end();)
		{
			FaultIntersection& col_err = column_error_it->second;

			if (col_err.bit_count_aggregate(m_base_size - 1) > m_n_correct)
			{
				failed_codeword_columns[column_error_it->first] = true;
				insert(raw_faults, col_err);
				column_error_it = columns.erase(column_error_it);
			}
			else
				++column_error_it;
		}

		int32_t row, codeword;
		auto column_error_it = columns.begin();
		for (auto &codeword_error_pair: words)
		{
			std::tie(codeword, row) = codeword_error_pair.first;
			FaultIntersection &word_err = codeword_error_pair.second;

			// skip a full column already failed
			if (failed_codeword_columns[codeword])
				continue;

			// find if there is a column error that matches this codeword
			while (column_error_it != columns.end() && column_error_it->first < codeword)
				++column_error_it;

			if (column_error_it != columns.end() && column_error_it->first == codeword)
				word_err.intersection(column_error_it->second);

			if (word_err.bit_count_aggregate(m_base_size - 1) > m_n_correct)
				insert(raw_faults, word_err);
		}
	}

	return {raw_faults.size(), raw_faults.size()};
}
