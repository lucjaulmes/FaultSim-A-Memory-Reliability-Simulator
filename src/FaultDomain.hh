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
#include <iostream>
#include <memory>

#include "FaultRange.hh"
#include "RepairScheme.hh"
#include "dram_common.hh"


class FaultDomain
{
protected:
	std::string m_name;
	bool debug;

	std::list<std::shared_ptr<RepairScheme>> m_repairSchemes;

public:
	inline
	FaultDomain(const std::string &name)
		: m_name(name), debug(false), m_repairSchemes()
	{
	}

	inline
	virtual ~FaultDomain()
	{
		m_repairSchemes.clear();
	}

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
	void addRepair(std::shared_ptr<RepairScheme> repair)
	{
		m_repairSchemes.push_back(repair);
	}

	inline
	void addRepair(RepairScheme* repair)
	{
		m_repairSchemes.push_back(std::shared_ptr<RepairScheme>(repair));
	}

	virtual faults_t getFaultCount() = 0;
	virtual void prepare() = 0;

	inline
	virtual failures_t repair()
	{
		prepare();

		// In the absence of repair schemes, all faults are undetected and uncorrected
		faults_t n_faults = getFaultCount();
		failures_t errors = {n_faults.total(), n_faults.total()};

		for (std::shared_ptr<RepairScheme> rs: m_repairSchemes)
		{
			failures_t after_repair = rs->repair(this);

			errors.uncorrected = std::min(errors.uncorrected, after_repair.uncorrected);
			errors.undetected  = std::min(errors.undetected, after_repair.undetected);
		}

		return errors;
	}

	/** reset after each sim run */
	virtual void reset()
	{
		for (std::shared_ptr<RepairScheme> rs: m_repairSchemes)
			rs->reset();
	}

	virtual void printStats(uint64_t sim_seconds [[gnu::unused]])
	{
		for (std::shared_ptr<RepairScheme> rs: m_repairSchemes)
			rs->printStats();
	}

	virtual void scrub() = 0;
	virtual void dumpState() {}
};

#endif /* FAULTDOMAIN_HH_ */
