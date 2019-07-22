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


Simulation::Simulation(uint64_t interval_t, uint64_t scrub_interval_t, double fit_factor_t, uint test_mode_t,
    bool debug_mode_t, bool cont_running_t, uint64_t output_bucket_t) :
	m_interval(interval_t)
	, m_scrub_interval(scrub_interval_t)
	, m_fit_factor(fit_factor_t)
	, test_mode(test_mode_t)
	, debug_mode(debug_mode_t)
	, cont_running(cont_running_t)
	, m_output_bucket(output_bucket_t)
{
	m_iteration = 0;    // start at time zero

	if ((m_scrub_interval % m_interval) != 0)
	{
		std::cout << "ERROR: Scrub interval must be a multiple of simulation time step interval\n";
		exit(0);
	}
}

void Simulation::addDomain(FaultDomain *domain)
{
	domain->setDebug(debug_mode);
	m_domains.push_back(domain);
}

void Simulation::init(uint64_t max_s)
{
	for (FaultDomain *fd: m_domains)
		fd->init(m_interval, max_s, m_fit_factor);
}

void Simulation::reset()
{
	for (FaultDomain *fd: m_domains)
		fd->reset();
}

void Simulation::finalize()
{
	for (FaultDomain *fd: m_domains)
		fd->finalize();
}

void Simulation::resetStats()
{
	stat_total_failures = 0;
	stat_total_sims = 0;
	stat_sim_seconds = 0;
}

void Simulation::simulate(uint64_t max_time, uint64_t n_sims, int verbose, std::string output_file)
{
	//Reset Stats before starting any simulation
	resetStats();

	uint64_t bin_length = m_output_bucket;

	// Max time of simulation in seconds
	stat_sim_seconds = max_time;

	// Number of bins that the output file will have
	fail_time_bins = new uint64_t[max_time / bin_length];
	fail_uncorrectable = new uint64_t[max_time / bin_length];
	fail_undetectable = new uint64_t[max_time / bin_length];

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

		uint64_t trans, perm;
		getFaultCounts(&trans, &perm);
		if (failures != 0)
		{
			stat_total_failures++;
			if (verbose) std::cout << "F";   // uncorrected
		}
		else if (trans + perm != 0)
		{
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
	// returns number of uncorrectable simulations

	// reset the domain states e.g. recorded errors for the simulated timeframe
	reset();

	// calculate number of iterations
	uint64_t max_iterations = max_s / m_interval;

	// compute the ratio at which scrubbing needs to be performed
	uint64_t scrub_ratio = m_scrub_interval / m_interval;
	uint64_t errors = 0;

	/*************************************************
	 * THIS IS THE LOOP FOR A SINGLE RUN FOR N YEARS *
	 *************************************************/
	for (uint64_t iter = 0; iter < max_iterations; iter++)
	{
		// loop through all fault domains and update
		for (FaultDomain *fd: m_domains)
		{
			// Insert Faults Hierarchially: GroupDomain -> Lower Domains -> ..
			// Since (time between updates) << (Total Running Time), faults can be assumed to be inserted instantaneously
			int newfault = fd->update(test_mode);
			uint64_t n_undetected = 0;
			uint64_t n_uncorrected = 0;

			// Run the Repair function: This will check the correctability/ detectability of the fault(s)
			// Repairing is also done instantaneously
			if (newfault)
			{
				if (verbose == 2)
				{
					// Dump all FaultRanges before
					std::cout << "FAULTS INSERTED: BEFORE REPAIR\n";
					fd->dumpState();
				}

				std::tie(n_undetected, n_uncorrected) = fd->repair();

				if (verbose == 2)
				{
					// Dump all FaultRanges after
					std::cout << "FAULTS INSERTED: AFTER REPAIR\n";
					fd->dumpState();
				}
			}

			if (n_undetected || n_uncorrected)
			{
				// Update the appropriate Bin to log into the output file
				uint64_t bin = (iter * m_interval) / bin_length;
				fail_time_bins[bin]++;

				if (n_uncorrected > 0)
					fail_uncorrectable[bin]++;
				if (n_undetected > 0)
					fail_undetectable[bin]++;

				errors++;

				if (!cont_running)
				{
					// if any iteration fails to repair, halt the simulation and report failure
					finalize();
					return 1;
				}
			}

		}

		// Check if the time to scrub the domain has arrived
		if ((iter % scrub_ratio) == 0)
			for (FaultDomain *fd: m_domains)
			{
				fd->scrub();

				// User Defined Special operation to be performed while Scrubbing
				// (??) -> causes failure if returns 1
				if (fd->fill_repl())
				{
					finalize();
					return 1;
				}
			}
	}

	/***********************************************/

	finalize();
	if (errors > 0)
		return 1;
	else
		return 0;
}

void Simulation::getFaultCounts(uint64_t *pTrans, uint64_t *pPerm)
{
	uint64_t sum_perm = 0;
	uint64_t sum_trans = 0;

	for (FaultDomain *fd: m_domains)
	{
		sum_perm += fd->getFaultCountPerm();
		sum_trans += fd->getFaultCountTrans();
	}

	*pTrans = sum_trans;
	*pPerm = sum_perm;
}

void Simulation::printStats()
{
	std::cout << "\n";
	// loop through all domains and report itemized failures, while aggregating them to calculate overall stats

	for (FaultDomain *fd: m_domains)
		fd->printStats();

	std::cout << "\n";
}
