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

#ifndef GROUPDOMAIN_CUBE_HH_
#define GROUPDOMAIN_CUBE_HH_

#include <random>

#include "dram_common.hh"
#include "GroupDomain.hh"

class GroupDomain_cube : public GroupDomain
{
	/** Total Chips in a DIMM */
	const uint64_t m_chips;
	/** Total Banks per Chip */
	const uint64_t m_banks;
	/** The burst length per access, this determines the number of TSVs */
	const uint64_t m_burst_size;

	// 3D memory variables
	enum { HORIZONTAL, VERTICAL } cube_model;
	uint64_t cube_data_tsv;
	uint64_t enable_tsv;

	bool *tsv_bitmap;
	uint64_t *tsv_info;

	/** Address Decoding Depth */
	uint64_t m_cube_addr_dec_depth;

	uint64_t cube_ecc_tsv;
	uint64_t cube_redun_tsv;
	uint64_t total_addr_tsv;
	uint64_t total_tsv;
	bool tsv_shared_accross_chips;
	//Static Arrays for ISCA2014
	uint64_t tsv_swapped_hc[9];
	uint64_t tsv_swapped_vc[8];
	uint64_t tsv_swapped_mc[9][8];


	double tsv_transientFIT;
	double tsv_permanentFIT;
	uint64_t tsv_n_faults_transientFIT_class;
	uint64_t tsv_n_faults_permanentFIT_class;

	std::mt19937_64 gen;
	std::uniform_int_distribution<uint64_t> tsv_dist;

	void generateTSV(bool transient);

	void prepare()
	{
		// Good place to propagate TSV faults to DRAM chips?
	}

	GroupDomain_cube(const std::string& name, unsigned cube_model, uint64_t chips, uint64_t banks, uint64_t burst_length,
					 uint64_t cube_addr_dec_depth, uint64_t cube_ecc_tsv, uint64_t cube_redun_tsv, bool enable_tsv);

public:
	static GroupDomain_cube* genModule(Settings &settings, int module_id);
	~GroupDomain_cube();

	inline
	void setFIT_TSV(bool isTransient_TSV, double FIT_TSV)
	{
		if (isTransient_TSV)
			tsv_transientFIT = FIT_TSV;
		else
			tsv_permanentFIT = FIT_TSV;
	}

	inline
	bool horizontalTSV()
	{
		return cube_model == HORIZONTAL;
	}

	void addDomain(FaultDomain *domain);
};


#endif /* GROUPDOMAIN_CUBE_HH_ */
