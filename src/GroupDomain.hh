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

#ifndef GROUPDOMAIN_HH_
#define GROUPDOMAIN_HH_

#include <list>
#include <functional>

#include "FaultDomain.hh"
#include "RepairScheme.hh"

class GroupDomain : public FaultDomain
{
protected:
	// cross-simulation overall program run statistics
	uint64_t stat_n_simulations, stat_n_failures, stat_n_failures_undetected, stat_n_failures_uncorrected;

	// per-simulation run statistics
	uint64_t n_errors_uncorrected;
	uint64_t n_errors_undetected;

	std::list<FaultDomain *> m_children;
	std::list<RepairScheme *> m_repairSchemes;

public:
	GroupDomain(const char *name);
	virtual ~GroupDomain();

	virtual void setFIT_TSV(bool transient, double FIT) = 0;

    failures_t repair();
    void finalize();
	void reset();

	void dumpState();
    void printStats(uint64_t max_time);

	/** Return faults that intersect across children */
	std::list<FaultIntersection>
		intersecting_ranges(unsigned symbol_size, std::function<bool(FaultIntersection&)> predicate);

    inline void scrub()
	{
		// repair all children
		for (FaultDomain *fd: m_children)
			fd->scrub();
	}

	inline
	void addDomain(FaultDomain *domain)
	{
		m_children.push_back(domain);
	}

	inline
	void addRepair(RepairScheme *repair)
	{
		m_repairSchemes.push_back(repair);
	}

	inline
	std::list<FaultDomain *> &getChildren()
	{
		return m_children;
	}

	inline
	uint64_t getFailedSimCount()
	{
		return stat_n_failures;
	}

	faults_t getFaultCount();
	inline uint64_t getFaultCountUncorrected() { return n_errors_uncorrected; };
	inline uint64_t getFaultCountUndetected() { return n_errors_undetected; };
};


#endif /* GROUPDOMAIN_HH_ */
