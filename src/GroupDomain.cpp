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

#include "GroupDomain.hh"
#include <iostream>
#include <stdlib.h>

GroupDomain::GroupDomain(const char *name)
	: FaultDomain(name)
	, stat_n_simulations(0), stat_total_failures(0)
	, stat_n_failures({0, 0}), n_errors({0, 0})
{
}

GroupDomain::~GroupDomain()
{
	for (FaultDomain *fd: m_children)
		delete fd;

	m_children.clear();
}

void GroupDomain::reset()
{
	// used to indicate whether the domain failed during a single simulation
	n_errors = {0, 0};

	stat_n_simulations++;

	for (FaultDomain *fd: m_children)
		fd->reset();

	FaultDomain::reset();
}

faults_t GroupDomain::getFaultCount()
{
	faults_t n_faults = {0, 0};

	for (FaultDomain *fd: m_children)
		n_faults += fd->getFaultCount();

	return n_faults;
}

void GroupDomain::dumpState()
{
	FaultDomain::dumpState();
	for (FaultDomain *fd: m_children)
		fd->dumpState();
}


failures_t GroupDomain::repair()
{
	prepare();

	uint64_t faults_before_repair = getFaultCount().total();
	failures_t fail = {0, 0};

	// Have each child domain repair itself (e.g. on-chip ECC).
	for (auto *fd: m_children)
	{
		uint64_t child_raw = fd->getFaultCount().total();
		failures_t child_fail = fd->repair();

		fail.undetected += std::min(child_fail.undetected, child_raw);
		fail.uncorrected += std::min(child_fail.uncorrected, child_raw);
	}

	// Apply group-level ECC, iteratively reduce number of faults with each successive repair scheme.
	for (std::shared_ptr<RepairScheme> rs: m_repairSchemes)
	{
		// TODO: would be nice to share information between repair schemes,
		// so that a scheme can act on the outputs/results of the previous one(s)
		failures_t after_repair = rs->repair(this);

		// if any repair happened, dump
		if (debug)
		{
			if (faults_before_repair != 0)
			{
				std::cout << ">>> REPAIR " << m_name << " USING " << rs->getName() << " (state dump)\n";
				dumpState();
				std::cout << "FAULTS_BEFORE: " << fail << " FAULTS_AFTER: " << after_repair << "\n";
				std::cout << "<<< END\n";
			}
		}

		fail.uncorrected = std::min(fail.uncorrected, after_repair.uncorrected);
		fail.undetected  = std::min(fail.undetected, after_repair.undetected);
	}

	if (fail.undetected > 0)
		n_errors.undetected++;

	if (fail.uncorrected > 0)
		n_errors.uncorrected++;

	return fail;
}


void GroupDomain::finalize()
{
	// walk through all children and observe their error counts
	// If 1 or more children had a fault, record a failed simulation

	// RAW error rates
	bool failure = getFaultCount().total() != 0;

	if (!failure)
		for (FaultDomain *fd: m_children)
			if (fd->getFaultCount().total() != 0)
			{
				failure = true;
				break;
			}

	if (failure)
		stat_total_failures++;

	// Determine per-simulation statistics
	if (n_errors.undetected != 0)
		stat_n_failures.undetected++;

	if (n_errors.uncorrected != 0)
		stat_n_failures.uncorrected++;
}


void GroupDomain::printStats(uint64_t sim_seconds)
{
	FaultDomain::printStats(sim_seconds);

	for (FaultDomain *fd: m_children)
		fd->printStats(sim_seconds);

	// TODO: some slightly more advanced stats? At least some variability.
	const double sim_seconds_to_FIT = 3600e9 / sim_seconds, nsim = stat_n_simulations;

	double device_fail_rate = stat_total_failures / nsim;
	double uncorrected_fail_rate = stat_n_failures.uncorrected / nsim;
	double undetected_fail_rate = stat_n_failures.undetected / nsim;

	std::cout << "[" << m_name << "] sims " << stat_n_simulations << " failed_sims " << stat_total_failures
		<< " rate_raw " << device_fail_rate << " FIT_raw " << device_fail_rate * sim_seconds_to_FIT
		<< " rate_uncorr " << uncorrected_fail_rate << " FIT_uncorr " << uncorrected_fail_rate * sim_seconds_to_FIT
		<< " rate_undet " << undetected_fail_rate << " FIT_undet " << undetected_fail_rate * sim_seconds_to_FIT << '\n';
}
