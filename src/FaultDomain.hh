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

	inline
	void countFault(bool transient)
	{
		if (transient)
			n_faults_transient++;
		else
			n_faults_permanent++;
	}

public:
	inline
	FaultDomain(const char *name)
		: m_name(name), debug(false)
	{
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
		// In the absence of repair schemes, all faults are undetected and uncorrected
		uint64_t all_faults = n_faults_transient + n_faults_permanent;
		return std::make_pair(all_faults, all_faults);
	}

	virtual uint64_t fill_repl() { return 0; }
	virtual void scrub() = 0;
	// reset after each sim run
	virtual void reset() {}
	virtual void dumpState() {}

	virtual void resetStats() {}
	virtual void printStats(uint64_t max_time) = 0;

protected:
	// 3D memory variables
	uint64_t cube_model_enable;
	uint64_t cube_data_tsv;
	uint64_t enable_tsv;

	bool *tsv_bitmap;
	uint64_t *tsv_info;
};

#endif /* FAULTDOMAIN_HH_ */
