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

#ifndef SIMULATION_HH_
#define SIMULATION_HH_

#include <string>
#include <vector>
#include <fstream>

#include "FaultDomain.hh"
#include "GroupDomain.hh"

class Simulation
{
public:
	Simulation(uint64_t scrub_interval, bool debug_mode, bool cont_running, uint64_t output_bucket);
	~Simulation();
	void reset();
	void finalize();
	void simulate(uint64_t max_time, uint64_t n_sims, int verbose, std::ofstream& output_file);
	void addDomain(GroupDomain *domain);
	void printStats(uint64_t max_time);

protected:
	const uint64_t m_scrub_interval;
	const bool m_debug_mode;
	const bool m_cont_running;
	const uint64_t m_output_bucket;


	uint64_t stat_total_failures, stat_total_corrected, stat_total_sims;

	std::vector<uint64_t> fail_time_bins;
	std::vector<uint64_t> fail_uncorrectable;
	std::vector<uint64_t> fail_undetectable;

	std::list<GroupDomain *> m_domains;

	virtual uint64_t runOne(uint64_t max_time, int verbose, uint64_t bin_length);
};


#endif /* SIMULATION_HH_ */
