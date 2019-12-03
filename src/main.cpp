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

#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <cstring>

#include "faultsim.hh"
#include "GroupDomain.hh"
#include "GroupDomain_dimm.hh"
#include "GroupDomain_cube.hh"
#include "DRAMDomain.hh"
#include "ChipKillRepair.hh"
#include "VeccRepair.hh"
#include "ChipKillRepair_cube.hh"
#include "BCHRepair_cube.hh"
#include "CubeRAIDRepair.hh"
#include "BCHRepair.hh"
#include "BCHRepair_inDRAM.hh"
#include "Simulation.hh"
#include "Settings.hh"

void printBanner();
GroupDomain *genModuleDIMM();
GroupDomain *genModule3D();

enum return_value { SUCCESS = 0, ERROR_IN_COMMAND_LINE = 1, ERROR_UNHANDLED_EXCEPTION = 2 };

void printBanner()
{
	std::cout << "# --------------------------------------------------------------------------------\n";
	std::cout << "# FAULTSIM (v0.1 alpha) - A Fast, Configurable Memory Resilience Simulator\n";
	std::cout << "# (c) 2013-2015 Advanced Micro Devices Inc.\n";
	std::cout << "# --------------------------------------------------------------------------------\n\n";
}

struct Settings settings = {};

int main(int argc, char **argv)
{
	printBanner();

	/** Define and parse the program options */
	namespace po = boost::program_options;
	po::options_description desc("Options");
	std::string configfile;
	std::vector<std::string> config_overrides;

	desc.add_options()
		("help", "Print help messages")
		("config", po::value<std::vector<std::string>>(&config_overrides), "Manually specify configuration file items as section.key=value")
		("outfile", po::value<std::string>(&settings.output_file)->required(), "Output file name")
		("configfile", po::value<std::string>(&configfile), "Indicate .ini configuration file to use");

	po::positional_options_description pd;
	pd.add("configfile", 1).add("outfile", 1);

	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);

		/** --help option */
		if ((argc == 1) || vm.count("help"))
		{
			std::cout << "FaultSim\n" << desc << std::endl;
			return SUCCESS;
		}

		po::notify(vm);
	}
	catch (po::error &e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		std::cerr << desc << std::endl;
		return ERROR_IN_COMMAND_LINE;
	}
	catch (std::exception &e)
	{
		std::cerr << "Unhandled Exception reached the top of main: " << e.what()
		    << ", application will now exit" << std::endl;
		return ERROR_UNHANDLED_EXCEPTION;

	}

	if (parse_settings(configfile, config_overrides))
		return ERROR_IN_COMMAND_LINE;

	// Build the physical memory organization and attach ECC scheme /////
	GroupDomain *module = NULL;

	if (!settings.stack_3D)
		module = genModuleDIMM();
	else
		module = genModule3D();

	// Configure simulator ///////////////////////////////////////////////
	// Simulator settings are as follows:
	// a. The setting.scrub_s (in seconds) indicates the granularity of scrubbing transient faults.
	// b. The setting.debug will enable debug messages
	// c. The setting.continue_running will enable users to continue running even if an uncorrectable error occurs
	//    (until an undetectable error occurs).
	// d. The settings.output_bucket_s will bucket system failure times

	Simulation sim(settings.scrub_s, settings.debug, settings.continue_running, settings.output_bucket_s);

	sim.addDomain(module);       // register the top-level memory object with the simulation engine

	// Run simulator //////////////////////////////////////////////////
	sim.simulate(settings.max_s, settings.n_sims, settings.verbose, settings.output_file);
	sim.printStats(settings.max_s);

	return SUCCESS;
}

/*
 * Simulate a DIMM module
 */

GroupDomain *genModuleDIMM()
{
	// Create a DIMM or a CUBE
	// settings.data_block_bits is the number of bits per transaction when you create a DIMM
	GroupDomain *dimm0 = new GroupDomain_dimm("MODULE0", settings.chips_per_rank, settings.banks, settings.data_block_bits);

	for (uint32_t i = 0; i < settings.chips_per_rank; i++)
	{
		char buf[20];
		sprintf(buf, "MODULE0.DRAM%d", i);
		DRAMDomain *dram0 = new DRAMDomain(dimm0, buf, i, settings.chip_bus_bits, settings.ranks, settings.banks, settings.rows,
										   settings.cols);

		double scf_factor = settings.scf_factor;
		for (int cls = DRAM_1BIT; cls != DRAM_MAX; cls++)
		{
			dram0->setFIT(DRAM_1BIT, true, settings.fit_transient[cls] * settings.fit_factor * scf_factor);
			dram0->setFIT(DRAM_1BIT, true, settings.fit_permanent[cls] * settings.fit_factor * scf_factor);

			scf_factor = 1.;
		}

		dimm0->addDomain(dram0);
	}

	if (settings.repairmode & 8)
	{
		// ECC 8 + N = in-DRAM ECC + ECC(N)
		BCHRepair_inDRAM *iecc = new BCHRepair_inDRAM("inDRAM SEC", 128, 8);
		dimm0->addChildRepair(iecc);
		settings.repairmode &= ~8;
	}

	//Add the 2D Repair Schemes
	if (settings.repairmode == 0)
	{
		// do nothing (no ECC)
	}
	else if (settings.repairmode == 1)
	{
		ChipKillRepair *ck0 = new ChipKillRepair(std::string("CK1"), 1, 2);
		dimm0->addRepair(ck0);
	}
	else if (settings.repairmode == 2)
	{
		ChipKillRepair *ck0 = new ChipKillRepair(std::string("CK2"), 2, 4);
		dimm0->addRepair(ck0);
	}
	else if (settings.repairmode == 3)
	{
		BCHRepair *bch0 = new BCHRepair(std::string("SECDED"), 1, 2, 4);
		dimm0->addRepair(bch0);
	}
	else if (settings.repairmode == 4)
	{
		BCHRepair *bch1 = new BCHRepair(std::string("3EC4ED"), 3, 4, 4);
		dimm0->addRepair(bch1);   //Repair from Fault Domain
	}
	else if (settings.repairmode == 5)
	{
		BCHRepair *bch2 = new BCHRepair(std::string("6EC7ED"), 6, 7, 4);
		dimm0->addRepair(bch2);
	}
	else if (settings.repairmode == 6)
	{
		VeccRepair *vecc1 = new VeccRepair(std::string("VECC1"), 1, 2, 1, settings.vecc_protection);
		vecc1->allow_software_tolerance(settings.sw_tol, settings.vecc_sw_tol);
		dimm0->addRepair(vecc1);
	}
	else
		assert(0);

	if (settings.repairmode != 6)
	{
		// VECC has software-level tolerance already built-in. For other ECCs add it afterwards.
		SoftwareTolerance *swtol = new SoftwareTolerance(std::string("SWTOL"), settings.sw_tol);
		dimm0->addRepair(swtol);
	}

	return dimm0;
}

GroupDomain *genModule3D()
{
	// Create a stack or a CUBE
	// settings.data_block_bits is the number of bits per transaction when you create a Cube

	GroupDomain_cube *stack0 = new GroupDomain_cube("MODULE0", 1, settings.chips_per_rank, settings.banks, settings.data_block_bits,
	    settings.cube_addr_dec_depth, settings.cube_ecc_tsv, settings.cube_redun_tsv, settings.enable_tsv);

	// Set FIT rates for TSVs, these are set at the GroupDomain level as these are common to the entire cube
	stack0->setFIT_TSV(true, settings.tsv_fit);
	stack0->setFIT_TSV(false, settings.tsv_fit);

	for (uint32_t i = 0; i < settings.chips_per_rank; i++)
	{
		char buf[20];
		sprintf(buf, "MODULE0.DRAM%d", i);
		DRAMDomain *dram0 = new DRAMDomain(stack0, buf, i, settings.chip_bus_bits, settings.ranks, settings.banks,
										   settings.rows, settings.cols);

		double scf_factor = settings.scf_factor;
		for (int cls = DRAM_1BIT; cls != DRAM_NRANK; cls++)
		{
			dram0->setFIT(DRAM_1BIT, true, settings.fit_transient[cls] * settings.fit_factor * scf_factor);
			dram0->setFIT(DRAM_1BIT, true, settings.fit_permanent[cls] * settings.fit_factor * scf_factor);

			scf_factor = 1.;
		}

		// Rank FIT rates cannot be directly translated to 3D stack
		dram0->setFIT(DRAM_NRANK, true,  0.);
		dram0->setFIT(DRAM_NRANK, false, 0.);

		stack0->addDomain(dram0);
	}

	if (settings.repairmode == 1)
	{
		ChipKillRepair_cube *ck0 = new ChipKillRepair_cube(std::string("CK1"), 1, 2, stack0);
		stack0->addRepair(ck0);
	}
	else if (settings.repairmode == 2)
	{
		// settings.data_block_bits used as RAID is computed over 512 bits (in our design)
		CubeRAIDRepair *ck1 = new CubeRAIDRepair(std::string("RAID"), 1, 2, settings.data_block_bits);
		stack0->addRepair(ck1);
	}
	else if (settings.repairmode == 3)
	{
		// settings.data_block_bits used as SECDED/3EC4ED/6EC7ED is computed over 512 bits (in our design)
		BCHRepair_cube *bch0 = new BCHRepair_cube(std::string("SECDED"), 1, 2, settings.data_block_bits);
		stack0->addRepair(bch0);
	}
	else if (settings.repairmode == 4)
	{
		BCHRepair_cube *bch1 = new BCHRepair_cube(std::string("3EC4ED"), 3, 4, settings.data_block_bits);
		stack0->addRepair(bch1);   //Repair from Fault Domain
	}
	else if (settings.repairmode == 5)
	{
		BCHRepair_cube *bch2 = new BCHRepair_cube(std::string("6EC7ED"), 6, 7, settings.data_block_bits);
		stack0->addRepair(bch2);
	}
	else if (settings.repairmode == 6)
		assert(0);

	return stack0;
}
