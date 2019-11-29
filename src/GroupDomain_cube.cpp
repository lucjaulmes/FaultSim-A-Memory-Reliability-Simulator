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
#include <stdlib.h>
#include <ctime>
#include <sys/time.h>
#include "Settings.hh"

extern struct Settings settings;

GroupDomain_cube::GroupDomain_cube(const char *name, uint cube_model_t, uint64_t chips_t, uint64_t banks_t,
    uint64_t burst_size_t, uint64_t cube_addr_dec_depth_t, uint64_t cube_ecc_tsv_t, uint64_t cube_redun_tsv_t,
    bool enable_tsv_t) : GroupDomain(name)
	, eng()
	, gen(eng, random_uniform_t(0, 1))
{
	//Register Cube Model
	cube_model_enable = cube_model_t;
	cube_addr_dec_depth = cube_addr_dec_depth_t; //Address Decoding Depth

	//Check if TSVs need to be enabled for Fault Modelling
	enable_tsv = enable_tsv_t;

	//Params to figure out the number of TSVs in the chip
	chips = chips_t;
	banks = banks_t;
	burst_size = burst_size_t;

	/**************************************************/
	//Total number of TSVs in each category
	cube_ecc_tsv = cube_ecc_tsv_t;
	cube_redun_tsv = cube_redun_tsv_t;
	/*Assuming 32Bytes of DATA, we get 32*8 = 256 data bits out. If DDR is used, then we have 512 data bits out for horizontal channel config. Assuming 16Bytes of DATA, we get 16*8 = 128 data bits out. There are ~20 (16 Data Value + 4 ECC - may be) TSVs for Data per bank. If DDR is used and in 8 bursts, we will get 256 bits out for vertical channel config*/
	cube_data_tsv = burst_size / 2; // (DDR)

	/**************************************************/
	// Compute the number of Address TSVs required for each depth
	// DR DEBUG: this is wrong - also get chip size from the DRAMDomains
	total_addr_tsv = 0;

	if (cube_model_enable == 1)
	{
		// Horizontal Channels
		// The total TSVs per bank will be equal to this number, divided by
		// (number of banks + ecc per bank + data burst TSVs + ecc for data + redundant tsv)
		total_tsv = (total_addr_tsv + cube_ecc_tsv + cube_redun_tsv + cube_data_tsv) * chips;

		tsv_bitmap = new bool[(total_addr_tsv + cube_ecc_tsv + cube_redun_tsv + cube_data_tsv)*chips]();
		tsv_info = new uint64_t[(total_addr_tsv + cube_ecc_tsv + cube_redun_tsv + cube_data_tsv)*chips]();
		tsv_shared_accross_chips = false;
	}
	else    //Vertical Channels
	{
		total_tsv = (total_addr_tsv + cube_redun_tsv) * chips + (cube_ecc_tsv + cube_data_tsv) * banks;
		tsv_bitmap = new bool[(total_addr_tsv + cube_redun_tsv)*chips + (cube_ecc_tsv + cube_data_tsv)*banks]();
		tsv_info = new uint64_t[(total_addr_tsv + cube_redun_tsv)*chips + (cube_ecc_tsv + cube_data_tsv)*banks]();
		tsv_shared_accross_chips = true;
	}
	/**************************************************/
	struct timeval tv;
	gettimeofday(&tv, NULL);
	gen.engine().seed(tv.tv_sec * 1000000 + (tv.tv_usec));

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
	// propagate 3D mode settings from parent to all children as they are added
	domain->cube_model_enable = cube_model_enable;
	domain->tsv_bitmap = tsv_bitmap;
	domain->cube_data_tsv = cube_data_tsv;
	domain->tsv_info = tsv_info;
	domain->enable_tsv = enable_tsv;

	GroupDomain::addDomain(domain);
}

void GroupDomain_cube::generateTSV(bool transient)
{
	//Check if TSVs are enabled
	if (!enable_tsv)
		return;

	// Record the fault and update the info for TSV
	uint64_t location = eng() % total_tsv;

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

void GroupDomain_cube::setFIT(fault_class_t faultClass, bool isTransient, double FIT)
{
	assert(0);
}

void GroupDomain_cube::setFIT_TSV(bool tsv_isTransient, double FIT_TSV)
{
	FaultDomain::setFIT_TSV(tsv_isTransient, FIT_TSV);
}

void GroupDomain_cube::generateRanges(int faultClass)
{
}
