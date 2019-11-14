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

#include "DRAMDomain.hh"
#include "Settings.hh"
#include "faultsim.hh"
#include <cmath>
#include <ctime>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>

extern struct Settings settings;

DRAMDomain::DRAMDomain(char *name, uint32_t n_bitwidth, uint32_t n_ranks, uint32_t n_banks, uint32_t n_rows,
    uint32_t n_cols, double weibull_shape_parameter)
	: FaultDomain(name)
	, gen(random64_engine_t(), random_uniform_t(0, 1))
    , inv_weibull_shape(1. / weibull_shape_parameter)
	, m_bitwidth(n_bitwidth)
	, m_ranks(n_ranks)
	, m_banks(n_banks)
	, m_rows(n_rows)
	, m_cols(n_cols)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	gen.engine().seed(tv.tv_sec * 1000000 + (tv.tv_usec));

	//gettimeofday (&tv, NULL);
	eng32.seed(tv.tv_sec * 1000000 + (tv.tv_usec));

	for (int i = 0; i < DRAM_MAX; i++)
	{
		n_faults_transient_class[i] = 0;
		n_faults_permanent_class[i] = 0;

		FIT_rate[i] = {0., 0.};
	}

	n_faults_transient_tsv = n_faults_permanent_tsv = 0;

	m_logsize[BITS]  = log2(m_bitwidth);
	m_logsize[COLS]  = log2(m_cols);
	m_logsize[ROWS]  = log2(m_rows);
	m_logsize[BANKS] = log2(m_banks);
	m_logsize[RANKS] = log2(m_ranks);

	m_shift[BITS]  = 0;
	m_shift[COLS]  = m_logsize[BITS];
	m_shift[ROWS]  = m_logsize[BITS] + m_logsize[COLS];
	m_shift[BANKS] = m_logsize[BITS] + m_logsize[COLS] + m_logsize[ROWS];
	m_shift[RANKS] = m_logsize[BITS] + m_logsize[COLS] + m_logsize[ROWS] + m_logsize[BANKS];

	m_mask[BITS]  = (m_bitwidth - 1) << m_shift[BITS];
	m_mask[COLS]  = (m_cols     - 1) << m_shift[COLS];
	m_mask[ROWS]  = (m_rows     - 1) << m_shift[ROWS];
	m_mask[BANKS] = (m_banks    - 1) << m_shift[BANKS];
	m_mask[RANKS] = (m_ranks    - 1) << m_shift[RANKS];

	curr_interval = 0;

	if (settings.verbose)
	{
		double gbits = ((double)(m_ranks * m_banks * m_rows * m_cols * m_bitwidth)) / ((double)1024 * 1024 * 1024);

		std::cout << "# -------------------------------------------------------------------\n";
		std::cout << "# DRAMDomain(" << m_name << ")\n";
		std::cout << "# ranks " << m_ranks << "\n";
		std::cout << "# banks " << m_banks << "\n";
		std::cout << "# rows " << m_rows << "\n";
		std::cout << "# cols " << m_cols << "\n";
		std::cout << "# bitwidth " << m_bitwidth << "\n";
		std::cout << "# gbits " << gbits << "\n";
		std::cout << "# -------------------------------------------------------------------\n";
	}
}

std::list<FaultRange *> &DRAMDomain::getRanges()
{
	return m_faultRanges;
}

const std::list<FaultRange *> &DRAMDomain::getRanges() const
{
	return m_faultRanges;
}

fault_class_t DRAMDomain::maskClass(uint64_t mask)
{
	// rank bits set in mask => several ranks affected.
	// repeat in decreasing hierarchical order.
	if (m_mask[RANKS] && (mask & m_mask[RANKS]) == m_mask[RANKS])
		return DRAM_NRANK;

	else if (m_mask[BANKS] && (mask & m_mask[BANKS]) == m_mask[BANKS])
		return DRAM_NBANK;

	// a bank needs both row and col wildcards to be failed, otherwise it is a row or column failure
	else if (m_mask[ROWS] && (mask & m_mask[ROWS]) == m_mask[ROWS] && m_mask[COLS] && (mask & m_mask[COLS]) == m_mask[COLS])
		return DRAM_1BANK;

	else if (m_mask[ROWS] && (mask & m_mask[ROWS]) == m_mask[ROWS])
		return DRAM_1COL;

	else if (m_mask[COLS] && (mask & m_mask[COLS]) == m_mask[COLS])
		return DRAM_1ROW;

	else if (m_mask[BITS] && (mask & m_mask[BITS]) == m_mask[BITS])
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

int DRAMDomain::update(uint test_mode_t)
{
	int newfault = FaultDomain::update(test_mode_t);

	// Insert DRAM die faults
	for (uint i = 0; i < DRAM_MAX; i++)
	{
		fault_class_t f = fault_class_t(i);
		if (test_mode_t == 0)
		{
			bool transient = true;
			if (fault_in_interval(f, transient))
			{
				n_faults_transient++;
				n_faults_transient_class[i]++;
				generateRanges(f, transient);
				newfault = 1;
			}

			if (fault_in_interval(f, not transient))
			{
				n_faults_permanent++;
				n_faults_permanent_class[i]++;
				generateRanges(f, not transient);
				newfault = 1;
			}

		}
		else
		{
			if (i == (test_mode_t - 1))
			{
				n_faults_transient++;
				n_faults_transient_class[i]++;
				generateRanges(f, true);
				newfault = 1;
			}

			if (i == (test_mode_t - 1))
			{
				n_faults_permanent++;
				n_faults_permanent_class[i]++;
				generateRanges(f, false);
				newfault = 1;
			}
		}
	}

	// Insert TSV faults
	if ((cube_model_enable > 0) && enable_tsv)
	{
		for (uint ii = (children_counter * cube_data_tsv); ii < ((children_counter + 1) * cube_data_tsv); ii++)
		{
			if (tsv_bitmap[ii] == true)
			{
				if (tsv_info[ii] == 1)
				{
					n_faults_permanent++;
					n_faults_permanent_tsv++;
					newfault = 1;

					for (uint jj = 0; jj < (m_cols * m_bitwidth / cube_data_tsv); jj++)
					{
						m_faultRanges.push_back(genRandomRange(0, 0, 0, 1, 1, false, (ii % cube_data_tsv) + (jj * cube_data_tsv), true));
						//std::cout << "|" <<(ii%cube_data_tsv)+(jj*cube_data_tsv)<< "|";
					}
					tsv_info[ii] = 3;
				}
				else if (tsv_info[ii] == 2)
				{
					n_faults_transient++;
					n_faults_transient_tsv++;
					newfault = 1;

					for (uint jj = 0; jj < (m_cols * m_bitwidth / cube_data_tsv); jj++)
					{
						m_faultRanges.push_back(genRandomRange(0, 0, 0, 1, 1, true, (ii % cube_data_tsv) + (jj * cube_data_tsv), true));
						//std::cout << "|" <<(ii%cube_data_tsv)+(jj*cube_data_tsv)<< "|";
					}
					tsv_info[ii] = 4;
				}
			}
		}
	}

	curr_interval++;

	return newfault;
}


std::pair<uint64_t, uint64_t> DRAMDomain::repair()
{
	// override the remaining number of uncorrectable faults seen based on repair results
	return FaultDomain::repair();
}

bool first_time = 1;

void DRAMDomain::reset()
{
	FaultDomain::reset();

	for (FaultRange *fr: m_faultRanges)
		delete fr;

	m_faultRanges.clear();
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
	FaultDomain::scrub();

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

void DRAMDomain::setFIT(fault_class_t faultClass, bool isTransient, double FIT)
{
	if (isTransient)
		FIT_rate[faultClass].transient = FIT;
	else
		FIT_rate[faultClass].permanent = FIT;
}

double DRAMDomain::next_fault_event(fault_class_t faultClass, bool transient)
{
	// with default parameter weibull shape (= 1.) this is an exponential distribution
	double weibull_random = pow(-log(gen()), inv_weibull_shape);
	if (transient)
		return weibull_random * secs_per_fault[faultClass].transient;
	else
		return weibull_random * secs_per_fault[faultClass].permanent;
}

inline bool DRAMDomain::fault_in_interval(fault_class_t faultClass, bool transient)
{
	return gen() <= (transient ? error_probability[faultClass].transient : error_probability[faultClass].permanent);
}

void DRAMDomain::init(uint64_t interval, uint64_t sim_seconds)
{
	FaultDomain::init(interval, sim_seconds);
	// one-time initialization scales FIT rates to interval scale

	// For Event Driven sim, use expected values of MTBFs in seconds
	for (int i = 0; i < DRAM_MAX; i++)
	{
		secs_per_fault[i].transient = 3600e9 / (FIT_rate[i].transient);
		secs_per_fault[i].permanent = 3600e9 / (FIT_rate[i].permanent);
	}

	// For interval simulation, compute probability (using exponential model) of having a fault during any interval
	double interval_factor = interval / 3600e9;
	for (int i = 0; i < DRAM_MAX; i++)
	{
		error_probability[i].transient = 1.0 - exp(-FIT_rate[i].transient * interval_factor);
		error_probability[i].permanent = 1.0 - exp(-FIT_rate[i].permanent * interval_factor);
		assert(0 <= error_probability[i].transient && error_probability[i].transient <= 1);
		assert(0 <= error_probability[i].permanent && error_probability[i].permanent <= 1);
	}
}

void DRAMDomain::generateRanges(fault_class_t faultClass, bool transient)
{
	m_faultRanges.push_back(genRandomRange(faultClass, transient));
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
	FaultRange *fr = new FaultRange(this);
	fr->fAddr = 0;
	fr->fWildMask = 0;
	fr->Chip = 0;
	fr->transient = transient;
	fr->TSV = isTSV;
	fr->max_faults = 1; // maximum number of bits covered by FaultRange

	// parameter 1 = fixed, 0 = wild
	if (rank)
		setRanks(fr->fAddr, eng32() % m_ranks);
	else
	{
		setRanks(fr->fWildMask, m_ranks - 1);
		fr->max_faults *= m_ranks;
	}

	if (bank)
		setBanks(fr->fAddr, eng32() % m_banks);
	else
	{
		setBanks(fr->fWildMask, m_banks - 1);
		fr->max_faults *= m_banks;
	}

	if (row)
		setRows(fr->fAddr, eng32() % m_rows);
	else
	{
		setRows(fr->fWildMask, m_rows - 1);
		fr->max_faults *= m_rows;
	}

	// We're not specifying a specific single bit in a row (TSV fault)
	// so generate column and bit values as normal
	if (rowbit_num == -1)
	{
		if (col)
			setCols(fr->fAddr, eng32() % m_cols);
		else
		{
			setCols(fr->fWildMask, m_cols - 1);
			fr->max_faults *= m_cols;
		}

		if (bit)
			setBits(fr->fAddr, eng32() % m_bitwidth);
		else
		{
			setBits(fr->fWildMask, m_bitwidth - 1);
			fr->max_faults *= m_bitwidth;
		}
	}
	else
	{
		// For TSV faults we're specifying a single bit position in the row
		// so we treat the column and bit fields as a single field
		fr->fAddr |= (uint64_t)(rowbit_num);
	}

	return fr;
}

void DRAMDomain::printStats()
{
	FaultDomain::printStats();

	std::cout << " Transient: ";

	for (int i = 0; i < DRAM_MAX; i++)
		std::cout << n_faults_transient_class[i] << ' ';

	std::cout << "TSV " << n_faults_transient_tsv;

	std::cout << " Permanent: ";
	for (int i = 0; i < DRAM_MAX; i++)
		std::cout << n_faults_permanent_class[i] << ' ';

	std::cout << "TSV " << n_faults_permanent_tsv;

	std::cout << '\n';

	// For extra verbose mode, output list of all fault ranges
	if (settings.verbose == 2)
	{
		for (FaultRange *fr: m_faultRanges)
			std::cout << fr->toString() << '\n';
	}
}

void DRAMDomain::resetStats()
{
	FaultDomain::resetStats();
}
