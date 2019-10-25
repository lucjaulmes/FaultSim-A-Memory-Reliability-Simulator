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

#include <string>
#include <iostream>
#include <list>
#include <vector>

#include "FaultDomain.hh"
#include "RepairScheme.hh"

FaultDomain::FaultDomain(const char *name_t)
{
	debug = 0;

	resetStats();
	m_name.append(name_t);

	// RAW faults in this domain before detection/correction
	n_faults_transient = n_faults_permanent = 0;
	// Errors after detection/correction
	n_errors_undetected = n_errors_uncorrected = 0;
	tsv_transientFIT = 0;
	tsv_permanentFIT = 0;
	cube_model_enable = 0;
	cube_addr_dec_depth = 0;
	children_counter = 0;
}

FaultDomain::~FaultDomain()
{
	for (FaultDomain *fd: m_children)
		delete fd;

	for (RepairScheme *rs: m_repairSchemes)
		delete rs;

	m_children.clear();
	m_repairSchemes.clear();
}

std::string FaultDomain::getName()
{
	return m_name;
}

std::list<FaultDomain *> &FaultDomain::getChildren()
{
	return m_children;
}

void FaultDomain::setDebug(bool dbg)
{
	debug = dbg;
}

void FaultDomain::reset()
{
	// reset per-simulation statistics used internally
	n_faults_transient = n_faults_permanent = 0;
	n_errors_undetected = n_errors_uncorrected = 0; // used to indicate whether the domain failed during a single simulation
	stat_n_simulations++;

	for (FaultDomain *fd: m_children)
		fd->reset();

	for (RepairScheme *rs: m_repairSchemes)
		rs->reset();
}

void FaultDomain::dumpState()
{
	for (FaultDomain *fd: m_children)
		fd->dumpState();
}

uint64_t FaultDomain::getFaultCountTrans()
{
	uint64_t sum = 0;

	for (FaultDomain *fd: m_children)
		sum += fd->getFaultCountTrans();

	sum += n_faults_transient;

	return sum;
}

uint64_t FaultDomain::getFaultCountPerm()
{
	uint64_t sum = 0;

	for (FaultDomain *fd: m_children)
		sum += fd->getFaultCountPerm();

	sum += n_faults_permanent;

	return sum;
}

uint64_t FaultDomain::getFaultCountUncorrected()
{
	// don't include children in uncorrected failure count
	// because if those faults get corrected here, they're invisible

	return n_errors_uncorrected;
}

uint64_t FaultDomain::getFaultCountUndetected()
{
	// don't include children in uncorrected failure count
	// because if those faults get corrected here, they're invisible

	return n_errors_undetected;
}

void FaultDomain::addDomain(FaultDomain *domain, uint32_t domaincounter)
{
	// DR ADDED
	// DR HACK - propagate 3D mode settings from parent to all children as they are added
	domain->cube_model_enable = cube_model_enable;
	domain->tsv_bitmap = tsv_bitmap;
	domain->cube_data_tsv = cube_data_tsv;
	domain->tsv_info = tsv_info;
	domain->children_counter = domaincounter;
	domain->enable_tsv = enable_tsv;
	m_children.push_back(domain);
}

void FaultDomain::init(uint64_t interval, uint64_t sim_seconds)
{
	m_interval = interval;
	m_sim_seconds = sim_seconds;

	for (FaultDomain *fd: m_children)
		fd->init(interval, sim_seconds);
}

int FaultDomain::update(uint test_mode_t)
{
	int return_val = 0;

	for (FaultDomain *fd: m_children)
		if (fd->update(test_mode_t) == 1)
			return_val = 1;

	return return_val;
}

void FaultDomain::addRepair(RepairScheme *repair)
{
	m_repairSchemes.push_back(repair);
}


std::pair<uint64_t, uint64_t> FaultDomain::repair()
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;

	// repair all children

	for (FaultDomain *fd: m_children)
	{
		uint64_t child_undet, child_uncorr;
		std::tie(child_undet, child_uncorr) = fd->repair();
		n_uncorrectable += child_uncorr;
		n_undetectable += child_undet;
	}

	// -- so here we reset n_undetectable and n_uncorrectable ?!

	// repair myself
	// default to the number of faults in myself and all children, in case there are no repair schemes
	uint64_t faults_before_repair = getFaultCountPerm() + getFaultCountTrans();
	n_undetectable = n_uncorrectable = faults_before_repair;

	// iteratively reduce number of faults with each successive repair scheme.
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

uint64_t FaultDomain::fill_repl()
{
	uint64_t n_uncorrectable = 0;

	// fill_repl for all children

	for (FaultDomain *fd: m_children)
		n_uncorrectable += fd->fill_repl();

	// fill_repl myself
	// default to the number of errors in myself and all children, in case there are no repair schemes
	n_uncorrectable = getFaultCountPerm() + getFaultCountTrans();

	for (RepairScheme *rs: m_repairSchemes)
	{
		uint64_t uncorrectable_after_repair = rs->fill_repl(this);
		if (n_uncorrectable > uncorrectable_after_repair)
			n_uncorrectable = uncorrectable_after_repair;
	}

	if (n_uncorrectable)
		n_errors_uncorrected++;

	return n_uncorrectable;
}

void FaultDomain::setFIT_TSV(bool isTransient_TSV, double FIT_TSV)
{
	if (isTransient_TSV)
		tsv_transientFIT = FIT_TSV;
	else
		tsv_permanentFIT = FIT_TSV;
}

void FaultDomain::scrub()
{
	// repair all children
	for (FaultDomain *fd: m_children)
		fd->scrub();
}

uint64_t FaultDomain::getFailedSimCount()
{
	return stat_n_failures;
}

void FaultDomain::finalize()
{
	// walk through all children and observe their error counts
	// If 1 or more children had a fault, record a failed simulation
	// at this level of the hierarchy.

	for (FaultDomain *fd: m_children)
		fd->finalize();

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

	// clear repair counters
	for (RepairScheme *rs: m_repairSchemes)
		rs->clear_counters();
}

void FaultDomain::printStats()
{
	for (RepairScheme *rs: m_repairSchemes)
		rs->printStats();

	for (FaultDomain *fd: m_children)
		fd->printStats();

	// TODO: some slightly more advanced stats. At least some variability.
	double sim_seconds_to_FIT = 3600e9 / m_sim_seconds;

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

void FaultDomain::resetStats()
{
	stat_n_simulations = stat_n_failures = 0;
	stat_n_failures_undetected = stat_n_failures_uncorrected = 0;

	for (FaultDomain *fd: m_children)
		fd->resetStats();

	for (RepairScheme *rs: m_repairSchemes)
		rs->resetStats();
}
