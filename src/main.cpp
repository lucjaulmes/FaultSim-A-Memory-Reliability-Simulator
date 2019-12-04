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

#include "GroupDomain.hh"
#include "GroupDomain_dimm.hh"
#include "GroupDomain_cube.hh"
#include "Simulation.hh"
#include "Settings.hh"


enum return_value { SUCCESS = 0, ERROR_IN_COMMAND_LINE = 1, ERROR_UNHANDLED_EXCEPTION = 2, ERROR_IN_CONFIGURATION = 3 };

void printBanner()
{
	std::cout << "# --------------------------------------------------------------------------------\n";
	std::cout << "# FAULTSIM (v0.1 alpha) - A Fast, Configurable Memory Resilience Simulator\n";
	std::cout << "# (c) 2013-2015 Advanced Micro Devices Inc.\n";
	std::cout << "# --------------------------------------------------------------------------------\n\n";
}


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

	if (settings.organization == Settings::DIMM)
		module = GroupDomain_dimm::genModule(settings, 0);
	else
		module = GroupDomain_cube::genModule(settings, 0);

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
