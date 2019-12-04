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
GroupDomain *genModuleDIMM(int);
GroupDomain *genModule3D(int);

enum return_value { SUCCESS = 0, ERROR_IN_COMMAND_LINE = 1, ERROR_UNHANDLED_EXCEPTION = 2, ERROR_IN_CONFIGURATION = 3 };

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
	std::string config_file, output_file;
	std::vector<std::string> config_overrides;

	desc.add_options()
		("help,h", "Print help messages")
		("config,c", po::value<std::vector<std::string>>(&config_overrides), "Manually specify configuration file items as section.key=value")
		("outfile,o", po::value<std::string>(&output_file)->required(), "Output file name")
		("inifile,i", po::value<std::string>(&config_file), "Indicate .ini configuration file to use");

	po::positional_options_description pd;
	pd.add("inifile", 1).add("outfile", 1);

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
		std::cerr << "ERROR: " << e.what() << "\n\n" << desc << std::endl;
		return ERROR_IN_COMMAND_LINE;
	}
	catch (std::exception &e)
	{
		std::cerr << "Unhandled Exception reached the top of main: " << e.what() << std::endl;
		return ERROR_UNHANDLED_EXCEPTION;
	}

	if (settings.parse_settings(config_file, config_overrides))
		return ERROR_IN_CONFIGURATION;

	std::ofstream opfile(output_file);
	if (!opfile.is_open())
	{
		std::cerr << "ERROR: output file " << output_file << ": opening failed\n" << std::endl;
		return ERROR_IN_COMMAND_LINE;
	}

	// Build the physical memory organization and attach ECC scheme /////
	GroupDomain *module = NULL;

	if (!settings.organization)
		module = genModuleDIMM(0);
	else
		module = genModule3D(0);

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
	sim.simulate(settings.max_s, settings.n_sims, settings.verbose, opfile);
	sim.printStats(settings.max_s);

	return SUCCESS;
}

/*
 * Simulate a DIMM module
 */

GroupDomain *genModuleDIMM(int module_id)
{
	std::string mod = std::string("DIMM").append(std::to_string(module_id));

	GroupDomain *dimm0 = new GroupDomain_dimm(mod, settings.chips_per_rank, settings.banks, settings.data_block_bits);

	for (uint32_t i = 0; i < settings.chips_per_rank; i++)
	{
		std::string chip = mod.append(".DRAM").append(std::to_string(i));
		DRAMDomain *dram0 = new DRAMDomain(dimm0, chip, i, settings.chip_bus_bits, settings.ranks, settings.banks,
										   settings.rows, settings.cols);

		for (int cls = DRAM_1BIT; cls != DRAM_MAX; cls++)
		{
			double scf_factor = cls == DRAM_1BIT ? settings.scf_factor : 1.;
			dram0->setFIT(DRAM_1BIT, true, settings.fit_transient[cls] * settings.fit_factor * scf_factor);
			dram0->setFIT(DRAM_1BIT, true, settings.fit_permanent[cls] * settings.fit_factor * scf_factor);
		}

		dimm0->addDomain(dram0);
	}

	if (settings.repairmode & Settings::IECC)
	{
		// ECC 8 + N = in-DRAM ECC + ECC(N)
		BCHRepair_inDRAM *iecc = new BCHRepair_inDRAM("inDRAM SEC", 128, 16);
		dimm0->addChildRepair(iecc);
		settings.repairmode = settings.repairmode & ~Settings::IECC;
	}

	if (settings.repairmode == Settings::DDC)
	{
		std::string name = std::string("CK").append(std::to_string(settings.correct));
		ChipKillRepair *ck0 = new ChipKillRepair(name, settings.correct, settings.detect);
		dimm0->addRepair(ck0);
	}
	else if (settings.repairmode == Settings::BCH)
	{
		std::stringstream ss;
		ss << settings.correct << "EC" << settings.detect << "ED";
		BCHRepair *bch0 = new BCHRepair(ss.str(), settings.correct, settings.detect, settings.chip_bus_bits);
		dimm0->addRepair(bch0);
	}
	else if (settings.repairmode == Settings::VECC)
	{
		std::stringstream ss;
		ss << "VECC" << settings.correct << '+' << settings.vecc_correct;
		VeccRepair *vecc = new VeccRepair(ss.str(), settings.correct, settings.detect,
													settings.vecc_correct, settings.vecc_protection);
		vecc->allow_software_tolerance(settings.sw_tol, settings.vecc_sw_tol);
		dimm0->addRepair(vecc);
	}

	if (settings.repairmode != Settings::VECC)
	{
		// VECC has software-level tolerance already built-in. For other ECCs add it afterwards.
		SoftwareTolerance *swtol = new SoftwareTolerance(std::string("SWTOL"), settings.sw_tol);
		dimm0->addRepair(swtol);
	}

	return dimm0;
}

GroupDomain *genModule3D(int module_id)
{
	std::string mod = std::string("3DSTACK").append(std::to_string(module_id));

	GroupDomain_cube *stack0 = new GroupDomain_cube(mod, settings.cube_model, settings.chips_per_rank, settings.banks,
													settings.data_block_bits, settings.cube_addr_dec_depth, settings.cube_ecc_tsv,
													settings.cube_redun_tsv, settings.enable_tsv);

	// Set FIT rates for TSVs, these are set at the GroupDomain level as these are common to the entire cube
	stack0->setFIT_TSV(true, settings.tsv_fit);
	stack0->setFIT_TSV(false, settings.tsv_fit);

	for (uint32_t i = 0; i < settings.chips_per_rank; i++)
	{
		std::string chip = mod.append(".DRAM").append(std::to_string(i));
		DRAMDomain *dram0 = new DRAMDomain(stack0, chip, i, settings.chip_bus_bits, settings.ranks, settings.banks,
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

	if (settings.repairmode == Settings::DDC)
	{
		std::string name = std::string("CK").append(std::to_string(settings.correct));
		ChipKillRepair_cube *ck0 = new ChipKillRepair_cube(name, settings.correct, settings.detect, stack0);
		stack0->addRepair(ck0);
	}
	else if (settings.repairmode == Settings::RAID)
	{
		// settings.data_block_bits used as RAID is computed over 512 bits (in our design)
		CubeRAIDRepair *ck1 = new CubeRAIDRepair(std::string("RAID"), settings.correct, settings.detect, settings.data_block_bits);
		stack0->addRepair(ck1);
	}
	else if (settings.repairmode == Settings::BCH)
	{
		// settings.data_block_bits used as SECDED/3EC4ED/6EC7ED is computed over 512 bits (in our design)
		std::stringstream ss;
		ss << settings.correct << "EC" << settings.detect << "ED";
		BCHRepair_cube *bch0 = new BCHRepair_cube(ss.str(), settings.correct, settings.detect, settings.data_block_bits);
		stack0->addRepair(bch0);
	}

	return stack0;
}
