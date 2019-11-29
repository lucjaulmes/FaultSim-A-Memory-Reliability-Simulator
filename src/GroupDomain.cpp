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
	n_faults_transient = n_faults_permanent = 0;
	// Errors after detection/correction
	n_errors_undetected = n_errors_uncorrected = 0;
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
	n_faults_transient = n_faults_permanent = 0;
	// used to indicate whether the domain failed during a single simulation
	n_errors_undetected = n_errors_uncorrected = 0;

	stat_n_simulations++;

	for (RepairScheme *rs: m_repairSchemes)
		rs->reset();

	for (FaultDomain *fd: m_children)
		fd->reset();
}

uint64_t GroupDomain::getFaultCountTrans()
{
	n_faults_transient = 0;

	for (FaultDomain *fd: m_children)
		n_faults_transient += fd->getFaultCountTrans();

	return n_faults_transient;
}

uint64_t GroupDomain::getFaultCountPerm()
{
	n_faults_permanent = 0;

	for (FaultDomain *fd: m_children)
		n_faults_permanent += fd->getFaultCountPerm();

	return n_faults_permanent;
}

void GroupDomain::dumpState()
{
	FaultDomain::dumpState();
	for (FaultDomain *fd: m_children)
		fd->dumpState();
}


std::pair<uint64_t, uint64_t> GroupDomain::repair()
{
	uint64_t faults_before_repair = getFaultCountPerm() + getFaultCountTrans();
	uint64_t n_undetectable = 0, n_uncorrectable = 0;

	// Have each child domain repair itself (e.g. on-chip ECC).
	for (auto *fd: m_children)
	{
		uint64_t child_raw = fd->getFaultCountTrans() + fd->getFaultCountPerm();

		uint64_t child_undetect, child_uncorrect;
		std::tie(child_undetect, child_uncorrect) = fd->repair();

		n_undetectable += std::min(child_undetect, child_raw);
		n_uncorrectable += std::min(child_uncorrect, child_raw);
	}

	// Apply group-level ECC, iteratively reduce number of faults with each successive repair scheme.
	for (RepairScheme *rs: m_repairSchemes)
	{
		// TODO: would be nice to share information between repair schemes,
		// so that a scheme can act on the outputs/results of the previous one(s)
		uint64_t uncorrectable_after_repair = 0;
		uint64_t undetectable_after_repair = 0;
		std::tie(undetectable_after_repair, uncorrectable_after_repair) = rs->repair(this);

		if (n_uncorrectable > uncorrectable_after_repair)
			n_uncorrectable = uncorrectable_after_repair;
		if (n_undetectable > undetectable_after_repair)
			n_undetectable = undetectable_after_repair;

		// if any repair happened, dump
		if (debug)
		{
			if (faults_before_repair != 0)
			{
				std::cout << ">>> REPAIR " << m_name << " USING " << rs->getName() << " (state dump)\n";
				dumpState();
				std::cout << "FAULTS_BEFORE: " << faults_before_repair << " FAULTS_AFTER: " << n_uncorrectable << "\n";
				std::cout << "<<< END\n";
			}
		}
	}

	if (n_undetectable > 0)
		n_errors_undetected++;

	if (n_uncorrectable > 0)
		n_errors_uncorrected++;

	return std::make_pair(n_undetectable, n_uncorrectable);
}

void GroupDomain::scrub()
{
	// repair all children
	for (FaultDomain *fd: m_children)
		fd->scrub();
}

uint64_t GroupDomain::getFailedSimCount()
{
	return stat_n_failures;
}


void GroupDomain::finalize()
{
	// walk through all children and observe their error counts
	// If 1 or more children had a fault, record a failed simulation
	// at this level of the hierarchy.

	// RAW error rates
	bool failure = (getFaultCountPerm() + getFaultCountTrans()) != 0;

	if (!failure)
		for (FaultDomain *fd: m_children)
			if (fd->getFaultCountPerm() + fd->getFaultCountTrans() != 0)
			{
				failure = true;
				break;
			}

	if (failure)
		stat_n_failures++;

	// Determine per-simulation statistics
	if (getFaultCountUndetected() != 0)
		stat_n_failures_undetected++;

	if (getFaultCountUncorrected() != 0)
		stat_n_failures_uncorrected++;
}

void GroupDomain::printStats(uint64_t sim_seconds)
{
	for (RepairScheme *rs: m_repairSchemes)
		rs->printStats();

	for (FaultDomain *fd: m_children)
		fd->printStats(sim_seconds);

	// TODO: some slightly more advanced stats. At least some variability.
	double sim_seconds_to_FIT = 3600e9 / sim_seconds;

	double device_fail_rate = ((double)stat_n_failures) / ((double)stat_n_simulations);
	double FIT_raw = device_fail_rate * sim_seconds_to_FIT;

	double uncorrected_fail_rate = ((double)stat_n_failures_uncorrected) / ((double)stat_n_simulations);
	double FIT_uncorr = uncorrected_fail_rate * sim_seconds_to_FIT;

	double undetected_fail_rate = ((double)stat_n_failures_undetected) / ((double)stat_n_simulations);
	double FIT_undet = undetected_fail_rate * sim_seconds_to_FIT;

	std::cout << "[" << m_name << "] sims " << stat_n_simulations << " failed_sims " << stat_n_failures
	    << " rate_raw " << device_fail_rate << " FIT_raw " << FIT_raw
	    << " rate_uncorr " << uncorrected_fail_rate << " FIT_uncorr " << FIT_uncorr
	    << " rate_undet " << undetected_fail_rate << " FIT_undet " << FIT_undet << "\n";
}
