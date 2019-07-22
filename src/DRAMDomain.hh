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
public:
	DRAMDomain(char *name, uint32_t n_bitwidth, uint32_t n_ranks, uint32_t n_banks, uint32_t n_rows, uint32_t n_cols);

	void setFIT(fault_class_t faultClass, bool isTransient, double FIT);
	void init(uint64_t interval, uint64_t sim_seconds, double fit_factor);
	int update(uint test_mode_t);   // perform one iteration
	std::pair<uint64_t, uint64_t> repair();
	void scrub();
	virtual void reset();

	std::list<FaultRange *> &getRanges();

	void dumpState();
	void printStats();
	void resetStats();
	uint32_t getLogBits();
	uint32_t getLogRanks();
	uint32_t getLogBanks();
	uint32_t getLogRows();
	uint32_t getLogCols();

	uint32_t getBits();
	uint32_t getRanks();
	uint32_t getBanks();
	uint32_t getRows();
	uint32_t getCols();


	// based on a fault, create the range of all faulty addresses
	void generateRanges(fault_class_t faultClass, bool transient);
	FaultRange *genRandomRange(fault_class_t faultClass, bool transient);
	FaultRange *genRandomRange(bool rank, bool bank, bool row, bool col, bool bit, bool transient, int64_t rowbit_num,
	    bool isTSV_t);
	double next_fault_event(fault_class_t faultClass, bool transient);
	bool fault_in_interval(fault_class_t faultClass, bool transient);

	const char *faultClassString(int i);

protected:
	struct fault_param { double transient, permanent ;};
	struct fault_param FIT_rate[DRAM_MAX];

	// Expected time between faults (for event-driven simulation)
	struct fault_param secs_per_fault[DRAM_MAX];

	// Probability of fault in an interval (for normal simulation)
	struct fault_param error_probability[DRAM_MAX];

	std::list<FaultRange *> m_faultRanges;

	random_generator_t gen;
	random32_engine_t eng32;

	uint64_t curr_interval;

	uint64_t n_faults_transient_class[DRAM_MAX];
	uint64_t n_faults_permanent_class[DRAM_MAX];

	uint64_t n_faults_transient_tsv, n_faults_permanent_tsv;

	uint32_t m_bitwidth, m_ranks, m_banks, m_rows, m_cols;
	uint32_t m_logBits, m_logRanks, m_logBanks, m_logRows, m_logCols;
};


#endif /* DRAMDOMAIN_HH_ */
