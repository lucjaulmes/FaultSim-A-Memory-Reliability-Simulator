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


typedef struct faults_t
{
	uint64_t transient, permanent;

	inline
	uint64_t total()
	{
		return transient + permanent;
	}

	inline
	struct faults_t& operator+=(const struct faults_t &other)
	{
		transient += other.transient, permanent += other.permanent;
		return *this;
	}
} faults_t;


typedef struct failures_t
{
	uint64_t undetected, uncorrected;

	inline
	struct failures_t& operator+=(const struct failures_t &other)
	{
		undetected += other.undetected, uncorrected += other.uncorrected;
		return *this;
	}
} failures_t;



class FaultDomain
{
protected:
	std::string m_name;
	bool debug;

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

	virtual faults_t getFaultCount() = 0;

	inline
	virtual failures_t repair()
	{
		// In the absence of repair schemes, all faults are undetected and uncorrected
		faults_t n_faults = getFaultCount();
		return {n_faults.total(), n_faults.total()};
	}

	virtual void scrub() = 0;
	/** reset after each sim run */
	virtual void reset() = 0;
	virtual void dumpState() {}

	virtual void printStats(uint64_t max_time) = 0;
};

#endif /* FAULTDOMAIN_HH_ */
