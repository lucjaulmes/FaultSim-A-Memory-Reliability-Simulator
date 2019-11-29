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

#ifndef FAULTDOMAIN_HH_
#define FAULTDOMAIN_HH_

#include <list>
#include <tuple>
#include <vector>
#include <string>

#include "FaultRange.hh"
#include "dram_common.hh"


class FaultDomain
{
protected:
	std::string m_name;
	bool debug;

	// per-simulation run statistics
	uint64_t n_faults_transient;
	uint64_t n_faults_permanent;

public:
	inline
	FaultDomain(const char *name)
		: m_name(name), debug(false)
	{
		tsv_transientFIT = 0;
		tsv_permanentFIT = 0;
		cube_model_enable = 0;
		cube_addr_dec_depth = 0;
	}

	inline
	virtual ~FaultDomain() {}

	inline
	const std::string& getName() const
	{
		return m_name;
	}

	inline
	void setDebug(bool dbg)
	{
		debug = dbg;
	}


	inline
	virtual uint64_t getFaultCountTrans()
	{
		return n_faults_transient;
	};

	inline
	virtual uint64_t getFaultCountPerm()
	{
		return n_faults_permanent;
	};

	inline
	virtual std::pair<uint64_t, uint64_t> repair()
	{
		// In the absence of repair schemes, all faults are undetected uncorrected
		uint64_t all_faults = n_faults_transient + n_faults_permanent;
		return std::make_pair(all_faults, all_faults);
	}

	inline
	void countFault(bool transient)
	{
		if (transient)
			n_faults_transient++;
		else
			n_faults_permanent++;
	}

	// perform one iteration ; Prashant: Changed the update to return a non-void value
	virtual int update(uint test_mode_t) = 0;
	virtual uint64_t fill_repl() { return 0; }
	virtual void scrub() = 0;
	// set up before first simulation run
	virtual void init(uint64_t interval, uint64_t sim_seconds) = 0;
	// reset after each sim run
	virtual void reset() {}
	virtual void dumpState() {}

	virtual void resetStats() {}
	virtual void printStats() {} // output end-of-run stats

	inline
	void setFIT_TSV(bool isTransient_TSV, double FIT_TSV)
	{
		if (isTransient_TSV)
			tsv_transientFIT = FIT_TSV;
		else
			tsv_permanentFIT = FIT_TSV;
	}

	//3D memory variables
	uint64_t cube_model_enable;
	uint64_t cube_addr_dec_depth;
	double tsv_transientFIT;
	double tsv_permanentFIT;
	uint64_t tsv_n_faults_transientFIT_class;
	uint64_t tsv_n_faults_permanentFIT_class;
	uint64_t chips;
	uint64_t banks;
	uint64_t burst_size;
	uint64_t cube_ecc_tsv;
	uint64_t cube_redun_tsv;
	uint64_t cube_data_tsv;
	bool *tsv_bitmap;
	uint64_t *tsv_info;
	uint64_t total_addr_tsv;
	uint64_t total_tsv;
	bool tsv_shared_accross_chips;
	uint64_t enable_tsv;
	//Static Arrays for ISCA2014
	uint64_t tsv_swapped_hc[9];
	uint64_t tsv_swapped_vc[8];
	uint64_t tsv_swapped_mc[9][8];
	// End 3D memory variable declaration

	bool visited; // used during graph traversal algorithms
};

#endif /* FAULTDOMAIN_HH_ */
