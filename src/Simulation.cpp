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
#include <list>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "Simulation.hh"
#include "FaultDomain.hh"
#include "DRAMDomain.hh"


Simulation::Simulation(uint64_t interval_t, uint64_t scrub_interval_t, uint test_mode_t,
					   bool debug_mode_t, bool cont_running_t, uint64_t output_bucket_t)
	: m_interval(interval_t)
	, m_scrub_interval(scrub_interval_t)
	, test_mode(test_mode_t)
	, debug_mode(debug_mode_t)
	, cont_running(cont_running_t)
	, m_output_bucket(output_bucket_t)
	, stat_total_failures(0)
	, stat_total_corrected(0)
	, stat_total_sims(0)
	, stat_sim_seconds(0)
{
	if ((m_scrub_interval % m_interval) != 0)
	{
		std::cout << "ERROR: Scrub interval must be a multiple of simulation time step interval\n";
		exit(0);
	}
}

Simulation::~Simulation()
{
	for (GroupDomain *fd: m_domains)
		delete fd;
	m_domains.clear();
}

void Simulation::addDomain(GroupDomain *domain)
{
	domain->setDebug(debug_mode);
	m_domains.push_back(domain);
}

void Simulation::reset()
{
	for (GroupDomain *fd: m_domains)
		fd->reset();
}

void Simulation::finalize()
{
	for (GroupDomain *fd: m_domains)
		fd->finalize();
}

void Simulation::simulate(uint64_t max_time, uint64_t n_sims, int verbose, std::string output_file)
{
	uint64_t bin_length = m_output_bucket;

	// Max time of simulation in seconds
	stat_sim_seconds = max_time;

	// Number of bins that the output file will have
	fail_time_bins.resize(max_time / bin_length);
	fail_uncorrectable.resize(max_time / bin_length);
	fail_undetectable.resize(max_time / bin_length);

	for (uint i = 0; i < max_time / bin_length; i++)
	{
		fail_time_bins[i] = 0;
		fail_uncorrectable[i] = 0;
		fail_undetectable[i] = 0;
	}

	if (verbose)
	{
		std::cout << "# ===================================================================\n";
		std::cout << "# SIMULATION STARTS\n";
		std::cout << "# ===================================================================\n\n";
	}

	/**************************************************************
	 * MONTE CARLO SIMULATION LOOP : THIS IS THE HEART OF FAULTSIM *
	 **************************************************************/
	for (uint64_t i = 0; i < n_sims; i++)
	{

		uint64_t failures = runOne(max_time, verbose, bin_length);
		stat_total_sims++;

		faults_t fault_count = {0, 0};
		for (GroupDomain *fd: m_domains)
			fault_count += fd->getFaultCount();

		if (failures != 0)
		{
			stat_total_failures++;
			if (verbose) std::cout << "F";   // uncorrected
		}
		else if (fault_count.total() != 0)
		{
			stat_total_corrected++;
			if (verbose) std::cout << "C";    // corrected
		}
		else
		{
			if (verbose) std::cout << ".";   // no failures
		}

		if (verbose) fflush(stdout);
	}
	/**************************************************************/

	if (verbose)
	{
		std::cout << "\n\n# ===================================================================\n";
		std::cout << "# SIMULATION ENDS\n";
		std::cout << "# ===================================================================\n";
	}

	std::cout << "Out of " << stat_total_sims << " simulations, "
			<< stat_total_failures << " failed and "
			<< stat_total_corrected << " encountered correctable errors\n";

	std::ofstream opfile(output_file);
	if (!opfile.is_open())
	{
		std::cout << "ERROR: output file " << output_file << ": opening failed\n" << std::endl;
		return;
	}

	opfile << "WEEKS,FAULT,FAULT-CUMU,P(FAULT),P(FAULT-CUMU)"
			<< ",UNCORRECTABLE,UNCORRECTABLE-CUMU,P(UNCORRECTABLE),P(UNCORRECTABLE-CUMU)"
			<< ",UNDETECTABLE,UNDETECTABLE-CUMU,P(UNDETECTABLE),P(UNDETECTABLE-CUMU)"
	    << std::endl;

	int64_t fail_cumulative = 0;
	int64_t uncorrectable_cumulative = 0;
	int64_t undetectable_cumulative = 0;

	const double per_sim = 1. / n_sims;
	for (uint64_t jj = 0; jj < max_time / bin_length; jj++)
	{
		fail_cumulative += fail_time_bins[jj];
		uncorrectable_cumulative += fail_uncorrectable[jj];
		undetectable_cumulative += fail_undetectable[jj];

		double p_fail = fail_time_bins[jj] * per_sim;
		double p_uncorrectable = fail_uncorrectable[jj] * per_sim;
		double p_undetectable = fail_undetectable[jj] * per_sim;

		double p_fail_cumulative = fail_cumulative * per_sim;
		double p_uncorrectable_cumulative = uncorrectable_cumulative * per_sim;
		double p_undetectable_cumulative = undetectable_cumulative * per_sim;

		opfile << jj * 12 // why 12 ?
			<< ',' << fail_time_bins[jj]
			<< ',' << fail_cumulative
			<< ',' << std::fixed << std::setprecision(6) << p_fail
			<< ',' << std::fixed << std::setprecision(6) << p_fail_cumulative
			<< ',' << fail_uncorrectable[jj]
			<< ',' << uncorrectable_cumulative
			<< ',' << std::fixed << std::setprecision(6) << p_uncorrectable
			<< ',' << p_uncorrectable_cumulative
			<< ',' << fail_undetectable[jj]
			<< ',' << undetectable_cumulative
			<< ',' << std::fixed << std::setprecision(6) << p_undetectable
			<< ',' << p_undetectable_cumulative
			<< '\n';
	}
}


uint64_t Simulation::runOne(uint64_t max_s, int verbose, uint64_t bin_length)
{
	// reset the domain states e.g. recorded errors for the simulated timeframe
	reset();

	// Generate all the fault events that will happen
	std::vector<std::pair<double, FaultRange *>> q1;
	for (GroupDomain *fd: m_domains)
		for (FaultDomain *fr: fd->getChildren())
		{
			DRAMDomain *chip = dynamic_cast<DRAMDomain *>(fr);
			for (int errtype = 0; errtype < DRAM_MAX * 2; errtype++)
			{
				bool transient = errtype % 2;
				fault_class_t fault = fault_class_t(errtype / 2);

				double event_time = 0, max_time = max_s;
				while ((event_time += chip->next_fault_event(fault, transient)) <= max_time)
					q1.push_back(std::make_pair(event_time, chip->genRandomRange(fault, transient)));
			}
		}


	/* TODO:
	 * Allow GroupDomain-level error injections, probably using a GroupDomain-level function
	 * that does nothing from GroupDomain_dimm and inserts TSV faults for GroupDomain_cube.
	 * */

	// Sort the fault events in arrival order
	std::sort(q1.begin(), q1.end(), [] (auto &a, auto &b) { return (a.first < b.first); });


	uint64_t errors = 0;

	// Step through the event list, injecting a fault into corresponding chip at each event, and invoking ECC
	for (auto time_fault_pair = q1.begin(); time_fault_pair != q1.end(); ++time_fault_pair)
	{
		double timestamp = time_fault_pair->first;
		FaultRange *fr = time_fault_pair->second;

		DRAMDomain *pDRAM = fr->m_pDRAM;
		pDRAM->insertFault(fr);

		if (verbose == 2)
		{
			std::cout << "FAULTS INSERTED: BEFORE REPAIR\n";
			pDRAM->dumpState();
		}

		// Peek at the future
		auto next_pair = std::next(time_fault_pair);
		bool repair_before_next = next_pair == q1.end() || floor(timestamp / m_interval) != floor(next_pair->first / m_interval);
		bool scrub_before_next  = next_pair == q1.end() || floor(timestamp / m_scrub_interval) != floor(next_pair->first / m_scrub_interval);


		// Somewhat artificial notion of “repair interval” to simulate faults that appear simultaneously
		if (!repair_before_next)
			continue;


		// Run the repair function: This will check the correctability / detectability of the fault(s)
		failures_t failure_count = m_domains.front()->repair();

		if (verbose == 2)
		{
			std::cout << "FAULTS INSERTED: AFTER REPAIR\n";
			pDRAM->dumpState();
		}


		if (failure_count.undetected || failure_count.uncorrected)
		{
			uint64_t bin = timestamp / bin_length;
			fail_time_bins[bin]++;

			if (failure_count.uncorrected > 0)
				fail_uncorrectable[bin]++;
			if (failure_count.undetected > 0)
				fail_undetectable[bin]++;

			errors++;

			if (!cont_running)
			{
				// if any repair fails, halt the simulation and report failure
				finalize();
				return 1;
			}
		}

		// Scrubbing is performed after a fault has occured and if the next fault is in a different scrub interval
		if (!scrub_before_next)
			continue;

		for (FaultDomain *fd: m_domains)
			fd->scrub();
	}

	/***********************************************/

	// returns number of uncorrectable simulations

	finalize();
	if (errors > 0)
		return 1;
	else
		return 0;
}

void Simulation::printStats(uint64_t max_time)
{
	std::cout << "\n";
	// loop through all domains and report itemized failures, while aggregating them to calculate overall stats

	for (GroupDomain *fd: m_domains)
		fd->printStats(max_time);

	std::cout << "\n";
}
