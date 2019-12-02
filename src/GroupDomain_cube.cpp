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

#include "GroupDomain_cube.hh"
#include <iostream>
#include <random>
#include <chrono>
#include "Settings.hh"

extern struct Settings settings;

GroupDomain_cube::GroupDomain_cube(const char *name, unsigned cube_model, uint64_t chips, uint64_t banks, uint64_t burst_size,
								   uint64_t cube_addr_dec_depth, uint64_t cube_ecc_tsv, uint64_t cube_redun_tsv, bool enable_tsv)
	: GroupDomain(name)
	, m_chips(chips), m_banks(banks), m_burst_size(burst_size)
	, cube_model(cube_model == 1 ? HORIZONTAL : VERTICAL), cube_data_tsv(burst_size / 2), enable_tsv(enable_tsv)
	, m_cube_addr_dec_depth(cube_addr_dec_depth), cube_ecc_tsv(cube_ecc_tsv), cube_redun_tsv(cube_redun_tsv)
	, tsv_transientFIT(0), tsv_permanentFIT(0)
	, gen(), tsv_dist()
{
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	gen.seed(seed);

	/* Total number of TSVs in each category.
	 * Horizontal channel config, assuming 32B (256b) of data: if DDR is used, then we have 512 data bits out
	 * Vertical channel config, assuming 16B (128b) of data: there are ~20 (16 data + maybe 4 ECC) TSVs per bank.
	 * If DDR is used and in 8 bursts, we will get 256 bits out for vertical channel config.
	 * */

	// DR DEBUG: this is wrong - also get chip size from the DRAMDomains
	total_addr_tsv = 0;

	if (horizontalTSV())
	{
		// Horizontal Channels
		// The total TSVs per bank will be equal to this number, divided by
		// (number of banks + ecc per bank + data burst TSVs + ecc for data + redundant tsv)
		total_tsv = (total_addr_tsv + cube_ecc_tsv + cube_redun_tsv + cube_data_tsv) * chips;
		tsv_shared_accross_chips = false;
	}
	else
	{
		// Vertical Channels
		total_tsv = (total_addr_tsv + cube_redun_tsv) * chips + (cube_ecc_tsv + cube_data_tsv) * banks;
		tsv_shared_accross_chips = true;
	}

	tsv_dist.param(std::uniform_int_distribution<uint64_t>(0, total_tsv - 1).param());
	tsv_dist = std::uniform_int_distribution<uint64_t>(0, total_tsv - 1);

	tsv_bitmap = new bool[total_tsv]();
	tsv_info = new uint64_t[total_tsv]();

	if (settings.verbose)
	{
		std::cout << "# -------------------------------------------------------------------\n";
		std::cout << "# GroupDomain_cube(" << m_name << ")\n";
		std::cout << "# cube_addr_dec_depth " << cube_addr_dec_depth << "\n";
		std::cout << "# enable_tsv " << enable_tsv << "\n";
		std::cout << "# chips " << chips << "\n";
		std::cout << "# banks " << banks << "\n";
		std::cout << "# burst_size " << burst_size << "\n";
		std::cout << "# cube_ecc_tsv " << cube_ecc_tsv << "\n";
		std::cout << "# cube_redun_tsv " << cube_redun_tsv << "\n";
		std::cout << "# cube_data_tsv " << cube_data_tsv << "\n";
		std::cout << "# total_addr_tsv " << total_addr_tsv << "\n";
		std::cout << "# total_tsv " << total_tsv << "\n";
		std::cout << "# cube_addr_dec_depth " << cube_addr_dec_depth << "\n";
		std::cout << "# -------------------------------------------------------------------\n";
	}
}

GroupDomain_cube::~GroupDomain_cube()
{
	delete[] tsv_bitmap;
	delete[] tsv_info;
}

void GroupDomain_cube::addDomain(FaultDomain *domain)
{
	/* TODO:
	 * hook up domain so that the TSV faults we inject in generateTSV() are affecting these chips.
	*/

	GroupDomain::addDomain(domain);
}

void GroupDomain_cube::generateTSV(bool transient)
{
	//Check if TSVs are enabled
	if (!enable_tsv)
		return;

	// Record the fault and update the info for TSV
	uint64_t location = tsv_dist(gen);

		// only record un-correctable faults for overall simulation success determination
	if (transient)
		tsv_n_faults_transientFIT_class++;
	else
		tsv_n_faults_permanentFIT_class++;

	if (tsv_bitmap[location] == false)
	{
		tsv_bitmap[location] = true;
		tsv_info[location] = transient ? 1 : 2;
	}
/*
 * from (legacy) DRAMDomain::update() -> once GroupDomain_cube::update() had generated TSV locations,
 * each chip went over all the positions and inserted into the TSV faults into its faults ranges:
 *
	for (unsigned ii = (chip_in_rank * cube_data_tsv); ii < ((chip_in_rank + 1) * cube_data_tsv); ii++)
	{
		if (tsv_bitmap[ii] == true && tsv_info[ii] <= 2)
		{
			bool transient;
			if (tsv_info[ii] == 1)
			{
				n_faults_permanent++;
				n_faults_permanent_tsv++;
				transient = false;
			}
			else // tsv_info[ii] == 2
			{
				n_faults_transient++;
				n_faults_transient_tsv++;
				transient = true;
			}

			for (uint jj = 0; jj < (m_cols * m_bitwidth / cube_data_tsv); jj++)
			{
				m_faultRanges.push_back(genRandomRange(0, 0, 0, 1, 1, transient, (ii % cube_data_tsv) + (jj * cube_data_tsv), true));
				//std::cout << "|" <<(ii%cube_data_tsv)+(jj*cube_data_tsv)<< "|";
			}

			tsv_info[ii] += 2;
		}
	}
*/
}
