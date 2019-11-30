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

GroupDomain::GroupDomain(const char *name) : FaultDomain(name)
{
	// RAW faults in this domain before detection/correction
	n_faults = {0, 0};
	// Errors after detection/correction
	n_errors = {0, 0};
}

GroupDomain::~GroupDomain()
{
	for (RepairScheme *rs: m_repairSchemes)
		delete rs;

	for (FaultDomain *fd: m_children)
		delete fd;

	m_repairSchemes.clear();
	m_children.clear();
}

void GroupDomain::reset()
{
	// reset per-simulation statistics used internally
	n_faults = {0, 0};
	// used to indicate whether the domain failed during a single simulation
	n_errors = {0, 0};

	stat_n_simulations++;

	for (RepairScheme *rs: m_repairSchemes)
		rs->reset();

	for (FaultDomain *fd: m_children)
		fd->reset();
}

faults_t GroupDomain::getFaultCount()
{
	n_faults = {0, 0};

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
	for (RepairScheme *rs: m_repairSchemes)
	{
		// TODO: would be nice to share information between repair schemes,
		// so that a scheme can act on the outputs/results of the previous one(s)
		failures_t after_repair = rs->repair(this);

		fail.uncorrected = std::min(fail.uncorrected, after_repair.uncorrected);
		fail.undetected  = std::min(fail.undetected, after_repair.undetected);

		// if any repair happened, dump
		if (debug)
		{
			if (faults_before_repair != 0)
			{
				std::cout << ">>> REPAIR " << m_name << " USING " << rs->getName() << " (state dump)\n";
				dumpState();
				std::cout << "FAULTS_BEFORE: " << faults_before_repair << " FAULTS_AFTER: " << fail.uncorrected << "\n";
				std::cout << "<<< END\n";
			}
		}
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
	for (RepairScheme *rs: m_repairSchemes)
		rs->printStats();

	for (FaultDomain *fd: m_children)
		fd->printStats(sim_seconds);

	// TODO: some slightly more advanced stats. At least some variability.
	double sim_seconds_to_FIT = 3600e9 / sim_seconds;

	double device_fail_rate = ((double)stat_total_failures) / ((double)stat_n_simulations);
	double FIT_raw = device_fail_rate * sim_seconds_to_FIT;

	double uncorrected_fail_rate = ((double)stat_n_failures.uncorrected) / ((double)stat_n_simulations);
	double FIT_uncorr = uncorrected_fail_rate * sim_seconds_to_FIT;

	double undetected_fail_rate = ((double)stat_n_failures.undetected) / ((double)stat_n_simulations);
	double FIT_undet = undetected_fail_rate * sim_seconds_to_FIT;

	std::cout << "[" << m_name << "] sims " << stat_n_simulations << " failed_sims " << stat_total_failures
	    << " rate_raw " << device_fail_rate << " FIT_raw " << FIT_raw
	    << " rate_uncorr " << uncorrected_fail_rate << " FIT_uncorr " << FIT_uncorr
	    << " rate_undet " << undetected_fail_rate << " FIT_undet " << FIT_undet << "\n";
}
