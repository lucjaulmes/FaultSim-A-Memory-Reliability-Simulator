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

#include "EventSimulation.hh"
#include "FaultDomain.hh"
#include "DRAMDomain.hh"
#include "FaultRange.hh"
#include <list>
#include <iostream>
#include <fstream>
#include <queue>
#include <iomanip>
#include <stdio.h>
#include <math.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>



EventSimulation::EventSimulation(uint64_t interval_t, uint64_t scrub_interval_t, uint test_mode_t,
    bool debug_mode_t, bool cont_running_t, uint64_t output_bucket_t)
	: Simulation(interval_t, scrub_interval_t, test_mode_t, debug_mode_t, cont_running_t, output_bucket_t)
{
}

// Event-driven simulation takes over the task of injecting errors into the chips
// from the DRAMDomains. It also advances time in variable increments according to event times

uint64_t EventSimulation::runOne(uint64_t max_s, int verbose, uint64_t bin_length)
{
	// reset the domain states e.g. recorded errors for the simulated timeframe
	reset();

	// Generate all the fault events that will happen
	std::vector<std::pair<double, FaultRange *>> q1;
	for (GroupDomain *fd: m_domains)
		for (FaultDomain *fd: fd->getChildren())
		{
			DRAMDomain *pD = dynamic_cast<DRAMDomain *>(fd);
			for (int errtype = 0; errtype < DRAM_MAX * 2; errtype++)
			{
				bool transient = errtype % 2;
				fault_class_t fault = fault_class_t(errtype / 2);

				for (double currtime = pD->next_fault_event(fault, transient); currtime <= (double)max_s;
							currtime += pD->next_fault_event(fault, transient))
				{
					FaultRange *fr = pD->genRandomRange(fault, transient);
					fr->m_pDRAM->countFault(fr->transient);

					q1.push_back(std::make_pair(currtime, fr));
				}
			}
		}

	// Sort the fault events in arrival order
	std::sort(q1.begin(), q1.end(), [] (auto &a, auto &b) { return (a.first < b.first); });


	uint64_t errors = 0;

	// Step through the event list, injecting a fault into corresponding chip at each event, and invoking ECC
	for (auto time_fault_pair = q1.begin(); time_fault_pair != q1.end(); )
	{
		double timestamp = time_fault_pair->first;
		FaultRange *fr = time_fault_pair->second;

		// Peek at the future
		double next_timestamp = ++time_fault_pair != q1.end() ? time_fault_pair->first : (timestamp + m_interval);

		DRAMDomain *pDRAM = fr->m_pDRAM;
		pDRAM->getRanges().push_back(fr);

		if (verbose == 2)
		{
			std::cout << "FAULTS INSERTED: BEFORE REPAIR\n";
			pDRAM->dumpState();
		}


		// Somewhat artificial notion of “repair interval” to simulate faults that appear simultaneously
		if (floor(timestamp / m_interval) == floor(next_timestamp / m_interval))
			continue;


		// Run the repair function: This will check the correctability / detectability of the fault(s)
		uint64_t n_undetected = 0;
		uint64_t n_uncorrected = 0;
		// TODO should be called on the GroupDomain in m_domains that contains pDRAM
		std::tie(n_undetected, n_uncorrected) = m_domains.front()->repair();

		if (verbose == 2)
		{
			std::cout << "FAULTS INSERTED: AFTER REPAIR\n";
			pDRAM->dumpState();
		}


		if (n_undetected || n_uncorrected)
		{
			uint64_t bin = timestamp / bin_length;
			fail_time_bins[bin]++;

			if (n_uncorrected > 0)
				fail_uncorrectable[bin]++;
			if (n_undetected > 0)
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
		if (floor(timestamp / m_scrub_interval) == floor(next_timestamp / m_scrub_interval))
			continue;

		for (FaultDomain *fd: m_domains)
		{
			fd->scrub();
			if (fd->fill_repl())
			{
				finalize();
				return 1;
			}
		}
	}

	finalize();
	if (errors > 0)
		return 1;
	else
		return 0;
}
