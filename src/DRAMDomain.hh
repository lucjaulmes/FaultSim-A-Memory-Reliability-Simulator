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

#ifndef DRAMDOMAIN_HH_
#define DRAMDOMAIN_HH_

#include "dram_common.hh"

#include <list>
#include <random>

#include "FaultDomain.hh"
class FaultRange;

enum DramField { Bits = 0, Cols, Rows, Banks, Ranks, FIELD_MAX };

class DRAMDomain : public FaultDomain
{
protected:
	uint32_t m_logsize[FIELD_MAX], m_shift[FIELD_MAX];
	uint64_t m_size[FIELD_MAX], m_mask[FIELD_MAX];

	// per-simulation run statistics
	faults_t n_faults;

	faults_t n_class_faults[DRAM_MAX], n_tsv_faults;

	struct fault_param { double transient, permanent; } FIT_rate[DRAM_MAX];

	std::list<FaultRange *> m_faultRanges;

	mutable std::mt19937_64 gen;
	mutable std::mt19937 gen32;
	std::weibull_distribution<double> time_dist;

	unsigned chip_in_rank;
	double weibull_shape;

public:
	DRAMDomain(char *name, unsigned id, uint32_t n_bitwidth, uint32_t n_ranks, uint32_t n_banks, uint32_t n_rows, uint32_t n_cols,
				double weibull_shape_parameter = 1.);

	inline
	void setFIT(fault_class_t faultClass, bool isTransient, double FIT)
	{
		if (isTransient)
			FIT_rate[faultClass].transient = FIT;
		else
			FIT_rate[faultClass].permanent = FIT;
	}

	inline
	void reset()
	{
		for (FaultRange *fr: m_faultRanges)
			delete fr;

		m_faultRanges.clear();
		n_faults = {0};
	}

	inline
	const std::list<FaultRange *> &getRanges() const
	{
		return m_faultRanges;
	}

	inline
	void insertFault(FaultRange *fr)
	{
		m_faultRanges.push_back(fr);
		fault_class_t cls = maskClass(fr->fWildMask);

		if (fr->transient)
		{
			n_faults.transient++;
			n_class_faults[cls].transient++;
		}
		else
		{
			n_faults.permanent++;
			n_class_faults[cls].permanent++;
		}
	}

	inline
	virtual faults_t getFaultCount()
	{
		return n_faults;
	};

	inline
	unsigned get_chip_num() const
	{
		return chip_in_rank;
	}

	void scrub();
	void dumpState();
	void printStats(uint64_t max_time);


	fault_class_t maskClass(uint64_t mask);
	static const char *faultClassString(fault_class_t i);

	FaultRange *genRandomRange(fault_class_t faultClass, bool transient);

	inline
	double next_fault_event(fault_class_t faultClass, bool transient) const
	{
		// with default parameter weibull shape (= 1.) this is an exponential distribution with expected value weibull_scale
		double weibull_scale = 3600e9 / (transient ? FIT_rate[faultClass].transient : FIT_rate[faultClass].permanent);
		return std::weibull_distribution<double>(weibull_shape, weibull_scale)(gen);
	}


	template <enum DramField F>
	inline uint32_t getNum() const { return m_size[F]; }

	template <enum DramField F>
	inline uint32_t getLog() const { return m_logsize[F]; }

	template <enum DramField F>
	inline uint32_t has(const uint64_t wildmask) const { return m_mask[F] != 0 && (wildmask & m_mask[F]) != m_mask[F]; }

	template <enum DramField F>
	inline uint32_t get(const uint64_t address) const { return (address & m_mask[F]) >> m_shift[F]; }

	template <enum DramField F>
	inline void put(uint64_t &address, const int32_t value) const
	{
		address = (address & ~m_mask[F]) | ((static_cast<uint64_t>(value) << m_shift[F]) & m_mask[F]);
	}

	template <enum DramField F>
	inline uint64_t set(const uint64_t address, const int32_t value) const
	{
		return (address & ~m_mask[F]) | ((static_cast<uint64_t>(value) << m_shift[F]) & m_mask[F]);
	}

	template <enum DramField F>
	inline uint32_t random() const
	{
		return std::uniform_int_distribution<uint32_t>(0, m_size[F] - 1)(gen);
	}

protected:
	FaultRange *genRandomRange(bool rank, bool bank, bool row, bool col, bool bit, bool transient, int64_t rowbit_num,
								bool isTSV_t);
};


#endif /* DRAMDOMAIN_HH_ */
