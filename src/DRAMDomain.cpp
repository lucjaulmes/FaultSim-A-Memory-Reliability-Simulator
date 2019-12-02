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

#include <cmath>
#include <chrono>
#include <iostream>
#include <cassert>

#include "GroupDomain.hh"
#include "Settings.hh"
#include "faultsim.hh"

#include "DRAMDomain.hh"


extern struct Settings settings;

DRAMDomain::DRAMDomain(char *name, unsigned id, uint32_t bitwidth, uint32_t ranks, uint32_t banks, uint32_t rows,
					   uint32_t cols, double weibull_shape_parameter)
	: FaultDomain(name)
    , n_faults({0}), n_class_faults({{0}}), n_tsv_faults({0})
	, FIT_rate({{0.}})
	, chip_in_rank(id)
    , weibull_shape(1. / weibull_shape_parameter)
{
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	gen.seed(seed);
	gen32.seed(seed);

	m_size[Bits]  = bitwidth;
	m_size[Cols]  = cols;
	m_size[Rows]  = rows;
	m_size[Banks] = banks;
	m_size[Ranks] = ranks;

	m_logsize[Bits]  = ceil(log2(bitwidth));
	m_logsize[Cols]  = ceil(log2(cols));
	m_logsize[Rows]  = ceil(log2(rows));
	m_logsize[Banks] = ceil(log2(banks));
	m_logsize[Ranks] = ceil(log2(ranks));

	m_shift[Bits]  = 0;
	m_shift[Cols]  = m_logsize[Bits];
	m_shift[Rows]  = m_logsize[Bits] + m_logsize[Cols];
	m_shift[Banks] = m_logsize[Bits] + m_logsize[Cols] + m_logsize[Rows];
	m_shift[Ranks] = m_logsize[Bits] + m_logsize[Cols] + m_logsize[Rows] + m_logsize[Banks];

	m_mask[Bits]  = (bitwidth - 1) << m_shift[Bits];
	m_mask[Cols]  = (cols     - 1) << m_shift[Cols];
	m_mask[Rows]  = (rows     - 1) << m_shift[Rows];
	m_mask[Banks] = (banks    - 1) << m_shift[Banks];
	m_mask[Ranks] = (ranks    - 1) << m_shift[Ranks];

	if (settings.verbose)
	{
		double gbits = ((double)(ranks * banks * rows * cols * bitwidth)) / ((double)1024 * 1024 * 1024);

		std::cout << "# -------------------------------------------------------------------\n";
		std::cout << "# DRAMDomain(" << m_name << ")\n";
		std::cout << "# ranks " << ranks << "\n";
		std::cout << "# banks " << banks << "\n";
		std::cout << "# rows " << rows << "\n";
		std::cout << "# cols " << cols << "\n";
		std::cout << "# bitwidth " << bitwidth << "\n";
		std::cout << "# gbits " << gbits << "\n";
		std::cout << "# -------------------------------------------------------------------\n";
	}
}

fault_class_t DRAMDomain::maskClass(uint64_t mask)
{
	// “any rank” set in mask => several ranks affected.
	// repeat in decreasing hierarchical order.
	if (m_mask[Ranks] && (mask & m_mask[Ranks]) == m_mask[Ranks])
		return DRAM_NRANK;

	else if (m_mask[Banks] && (mask & m_mask[Banks]) == m_mask[Banks])
		return DRAM_NBANK;

	// a bank needs both row and col wildcards to be failed, otherwise it is a row or column failure
	else if (m_mask[Rows] && (mask & m_mask[Rows]) == m_mask[Rows] && m_mask[Cols] && (mask & m_mask[Cols]) == m_mask[Cols])
		return DRAM_1BANK;

	else if (m_mask[Rows] && (mask & m_mask[Rows]) == m_mask[Rows])
		return DRAM_1COL;

	else if (m_mask[Cols] && (mask & m_mask[Cols]) == m_mask[Cols])
		return DRAM_1ROW;

	else if (m_mask[Bits] && (mask & m_mask[Bits]) == m_mask[Bits])
		return DRAM_1WORD;

	else
		return DRAM_1BIT;
}

const char *DRAMDomain::faultClassString(fault_class_t i)
{
	switch (i)
	{
		case DRAM_1BIT:
			return "1BIT";

		case DRAM_1WORD:
			return "1WORD";

		case DRAM_1COL:
			return "1COL";

		case DRAM_1ROW:
			return "1ROW";

		case DRAM_1BANK:
			return "1BANK";

		case DRAM_NBANK:
			return "NBANK";

		case DRAM_NRANK:
			return "NRANK";

		default:
			assert(0);
	}

	return "";
}

void DRAMDomain::dumpState()
{
	if (m_faultRanges.size() != 0)
	{
		std::cout << m_name << " ";

		for (FaultRange *fr: m_faultRanges)
			std::cout << fr->toString() << "\n";
	}
}

void DRAMDomain::scrub()
{
	// delete all transient faults
	for (auto it = m_faultRanges.begin(); it != m_faultRanges.end(); )
	{
		FaultRange *fr = *it;
		if (fr->transient && fr->transient_remove)
		{
			it = m_faultRanges.erase(it);
			delete fr;
		}
		else
			it++;
	}
}

FaultRange *DRAMDomain::genRandomRange(fault_class_t faultClass, bool transient)
{
	switch (faultClass)
	{
		case DRAM_1BIT:
			return genRandomRange(1, 1, 1, 1, 1, transient, -1, false);
			break;

		case DRAM_1WORD:
			return genRandomRange(1, 1, 1, 1, 0, transient, -1, false);
			break;

		case DRAM_1COL:
			return genRandomRange(1, 1, 0, 1, 0, transient, -1, false);
			break;

		case DRAM_1ROW:
			return genRandomRange(1, 1, 1, 0, 0, transient, -1, false);
			break;

		case DRAM_1BANK:
			return genRandomRange(1, 1, 0, 0, 0, transient, -1, false);
			break;

		case DRAM_NBANK:
			return genRandomRange(1, 0, 0, 0, 0, transient, -1, false);
			break;

		case DRAM_NRANK:
			return genRandomRange(0, 0, 0, 0, 0, transient, -1, false);
			break;

		default:
			assert(0);
	}
}

FaultRange *DRAMDomain::genRandomRange(bool rank, bool bank, bool row, bool col, bool bit, bool transient,
									   int64_t rowbit_num, bool isTSV)
{
	uint64_t address = 0, wildcard_mask = 0;
	 // maximum number of bits covered by FaultRange
	uint64_t max_faults = 1;


	// parameter 1 = fixed, 0 = wild
	if (rank)
		put<Ranks>(address, random<Ranks>());
	else
	{
		put<Ranks>(wildcard_mask, ~0U);
		max_faults *= m_size[Ranks];
	}

	if (bank)
		put<Banks>(address, random<Banks>());
	else
	{
		put<Banks>(wildcard_mask, ~0U);
		max_faults *= m_size[Banks];
	}

	if (row)
		put<Rows>(address, random<Rows>());
	else
	{
		put<Rows>(wildcard_mask, ~0U);
		max_faults *= m_size[Rows];
	}

	// We're not specifying a specific single bit in a row (TSV fault)
	// so generate column and bit values as normal
	if (rowbit_num == -1)
	{
		if (col)
			put<Cols>(address, random<Cols>());
		else
		{
			put<Cols>(wildcard_mask, ~0U);
			max_faults *= m_size[Cols];
		}

		if (bit)
			put<Bits>(address, random<Bits>());
		else
		{
			put<Bits>(wildcard_mask, ~0U);
			max_faults *= m_size[Bits];
		}
	}
	else
	{
		// For TSV faults we're specifying a single bit position in the row
		// so we treat the column and bit fields as a single field
		address |= (uint64_t)(rowbit_num);
	}


	return new FaultRange(this, address, wildcard_mask, isTSV, transient, max_faults);
}

void DRAMDomain::printStats(uint64_t max_time)
{
	std::cout << " Transient: ";

	for (int i = 0; i < DRAM_MAX; i++)
		std::cout << n_class_faults[i].transient << ' ';

	std::cout << "TSV " << n_tsv_faults.transient << " Permanent: ";

	for (int i = 0; i < DRAM_MAX; i++)
		std::cout << n_class_faults[i].permanent << ' ';

	std::cout << "TSV " << n_tsv_faults.permanent << '\n';

	// For extra verbose mode, output list of all fault ranges
	if (settings.verbose == 2)
	{
		for (FaultRange *fr: m_faultRanges)
			std::cout << fr->toString() << '\n';
	}
}
