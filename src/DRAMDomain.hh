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

#include "FaultDomain.hh"
class FaultRange;

class DRAMDomain : public FaultDomain
{
	// per-simulation run statistics
	faults_t n_faults;

public:
	DRAMDomain(char *name, unsigned id, uint32_t n_bitwidth, uint32_t n_ranks, uint32_t n_banks, uint32_t n_rows, uint32_t n_cols,
				double weibull_shape_parameter = 1.);

	void setFIT(fault_class_t faultClass, bool isTransient, double FIT);
	void scrub();
	virtual void reset();

	const std::list<FaultRange *> &getRanges() const
	{
		return m_faultRanges;
	}

	void insertFault(FaultRange *fr)
	{
		m_faultRanges.push_back(fr);

		if (fr->transient)
			n_faults.transient++;
		else
			n_faults.permanent++;
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

	void dumpState();
	void printStats(uint64_t max_time);

	// based on a fault, create the range of all faulty addresses
	void generateRanges(fault_class_t faultClass, bool transient);
	FaultRange *genRandomRange(fault_class_t faultClass, bool transient);
	FaultRange *genRandomRange(bool rank, bool bank, bool row, bool col, bool bit, bool transient, int64_t rowbit_num,
	    bool isTSV_t);
	double next_fault_event(fault_class_t faultClass, bool transient);

	fault_class_t maskClass(uint64_t mask);
	static const char *faultClassString(fault_class_t i);

protected:
	struct fault_param { double transient, permanent ;};
	struct fault_param FIT_rate[DRAM_MAX];

	// Expected time between faults
	struct fault_param secs_per_fault[DRAM_MAX];

	std::list<FaultRange *> m_faultRanges;

	random_generator_t gen;
	random32_engine_t eng32;

	unsigned chip_in_rank;

	uint64_t n_faults_transient_class[DRAM_MAX];
	uint64_t n_faults_permanent_class[DRAM_MAX];

	uint64_t n_faults_transient_tsv, n_faults_permanent_tsv;


	double inv_weibull_shape;

	enum field { BITS = 0, COLS, ROWS, BANKS, RANKS, FIELD_MAX };
	uint32_t m_bitwidth, m_ranks, m_banks, m_rows, m_cols;
	uint32_t m_logsize[FIELD_MAX], m_shift[FIELD_MAX];
	uint64_t m_mask[FIELD_MAX];

	template <enum field F>
	inline uint32_t getFieldSize() { return m_logsize[F]; }

	template <enum field F>
	inline uint32_t hasField(uint64_t wildmask) { return m_mask[F] != 0 && (wildmask & m_mask[F]) != m_mask[F]; }

	template <enum field F>
	inline uint32_t getField(uint64_t address) { return (address & m_mask[F]) >> m_shift[F]; }

	template <enum field F>
	inline void putField(uint64_t &address, int32_t value)
	{
		address = (address & ~m_mask[F]) | ((static_cast<uint64_t>(value) << m_shift[F]) & m_mask[F]);
	}

	template <enum field F>
	inline uint64_t setField(uint64_t address, int32_t value)
	{
		return (address & ~m_mask[F]) | ((static_cast<uint64_t>(value) << m_shift[F]) & m_mask[F]);
	}

public:
	inline uint32_t getLogBits () { return getFieldSize<BITS>(); }
	inline uint32_t getLogCols () { return getFieldSize<COLS>(); }
	inline uint32_t getLogRows () { return getFieldSize<ROWS>(); }
	inline uint32_t getLogBanks() { return getFieldSize<BANKS>(); }
	inline uint32_t getLogRanks() { return getFieldSize<RANKS>(); }

	inline uint32_t hasBits (uint64_t mask) { return hasField<BITS>(mask); }
	inline uint32_t hasCols (uint64_t mask) { return hasField<COLS>(mask); }
	inline uint32_t hasRows (uint64_t mask) { return hasField<ROWS>(mask); }
	inline uint32_t hasBanks(uint64_t mask) { return hasField<BANKS>(mask); }
	inline uint32_t hasRanks(uint64_t mask) { return hasField<RANKS>(mask); }

	inline uint32_t getBits (uint64_t address) { return getField<BITS>(address); }
	inline uint32_t getCols (uint64_t address) { return getField<COLS>(address); }
	inline uint32_t getRows (uint64_t address) { return getField<ROWS>(address); }
	inline uint32_t getBanks(uint64_t address) { return getField<BANKS>(address); }
	inline uint32_t getRanks(uint64_t address) { return getField<RANKS>(address); }

	inline void putBits (uint64_t &address, uint32_t value) { putField<BITS>(address, value); }
	inline void putCols (uint64_t &address, uint32_t value) { putField<COLS>(address, value); }
	inline void putRows (uint64_t &address, uint32_t value) { putField<ROWS>(address, value); }
	inline void putBanks(uint64_t &address, uint32_t value) { putField<BANKS>(address, value); }
	inline void putRanks(uint64_t &address, uint32_t value) { putField<RANKS>(address, value); }

	inline uint64_t setBits (uint64_t address, uint32_t value) { return setField<BITS>(address, value); }
	inline uint64_t setCols (uint64_t address, uint32_t value) { return setField<COLS>(address, value); }
	inline uint64_t setRows (uint64_t address, uint32_t value) { return setField<ROWS>(address, value); }
	inline uint64_t setBanks(uint64_t address, uint32_t value) { return setField<BANKS>(address, value); }
	inline uint64_t setRanks(uint64_t address, uint32_t value) { return setField<RANKS>(address, value); }
};


#endif /* DRAMDOMAIN_HH_ */
