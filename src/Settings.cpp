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

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "dram_common.hh"
#include "Settings.hh"

// make a boost::property_tree::id_translator for std containers
template<typename T> struct container_translator
{
	typedef std::string internal_type;
	typedef T external_type;

	boost::optional<T> get_value(const std::string& str) const
	{
		if (str.empty())
			return boost::none;

		T values;
		std::stringstream ss(str);

		typename T::value_type temp_value;
		while (ss >> temp_value)
			values.insert(values.end(), temp_value);

		return boost::make_optional(values);
	}

	boost::optional<std::string> put_value(const T& b) {
		std::stringstream ss;
		size_t i = 0;
		for (auto v : b)
			ss << (i++?" ":"") << v;
		return ss.str();
	}
};


// put the translator in the namespace for std::vector<T>
namespace boost::property_tree {
    template<typename ch, typename traits, typename alloc, typename T>
	struct translator_between<std::basic_string<ch, traits, alloc>, std::vector<T> > {
		typedef container_translator<std::vector<T>> type;
	};
}


int parse_settings(const std::string &ininame, std::vector<std::string> &config_overrides)
{
	boost::property_tree::ptree pt;
	boost::property_tree::ini_parser::read_ini(ininame.c_str(), pt);

	std::cout << "The selected config file is: " << ininame << std::endl;
	for (const std::string &opt: config_overrides)
	{
		auto pos = opt.find_first_of("=");
		if (pos == std::string::npos)
		{
			std::cout << "ERROR: Invalid option on command line: " << opt << std::endl;
			return 1;
		}
		pt.put(opt.substr(0, pos), opt.substr(pos + 1));
		std::cout << "  + override " << opt.substr(0, pos) << '=' << opt.substr(pos + 1) << std::endl;
	}

	settings.sim_mode = pt.get<int>("Sim.sim_mode");
	settings.interval_s = pt.get<uint64_t>("Sim.interval_s");
	settings.scrub_s = pt.get<uint64_t>("Sim.scrub_s");
	settings.max_s = pt.get<uint64_t>("Sim.max_s");
	settings.n_sims = pt.get<uint64_t>("Sim.n_sims");
	settings.continue_running = pt.get<bool>("Sim.continue_running");
	settings.verbose = pt.get<int>("Sim.verbose");
	settings.debug = pt.get<int>("Sim.debug");
	settings.output_bucket_s = pt.get<uint64_t>("Sim.output_bucket_s");

	settings.stack_3D = pt.get<int>("Org.stack_3D");
	settings.chips_per_rank = pt.get<int>("Org.chips_per_rank");
	settings.chip_bus_bits = pt.get<int>("Org.chip_bus_bits");
	settings.ranks = pt.get<int>("Org.ranks");
	settings.banks = pt.get<int>("Org.banks");
	settings.rows = pt.get<int>("Org.rows");
	settings.cols = pt.get<int>("Org.cols");
	settings.cube_model = pt.get<int>("Org.cube_model");
	settings.cube_addr_dec_depth = pt.get<int>("Org.cube_addr_dec_depth");
	settings.cube_ecc_tsv = pt.get<int>("Org.cube_ecc_tsv");
	settings.cube_redun_tsv = pt.get<int>("Org.cube_redun_tsv");
	settings.data_block_bits = pt.get<int>("Org.data_block_bits");

	settings.faultmode = pt.get<int>("Fault.faultmode");
	settings.enable_permanent = pt.get<int>("Fault.enable_permanent");
	settings.enable_transient = pt.get<int>("Fault.enable_transient");
	settings.enable_tsv = pt.get<int>("Fault.enable_tsv");
	settings.fit_factor = pt.get<double>("Fault.fit_factor");
	settings.scf_factor = pt.get<double>("Fault.scf_factor", 1.0);
	settings.tsv_fit = pt.get<double>("Fault.tsv_fit");

	settings.repairmode = pt.get<int>("ECC.repairmode");

	// specify all the tolerance probabilities, in order, starting with 1WORD
	// e.g. ".9 0 .1" means 90% tolerance of 1WORD DUEs, 0% of 1COL DUEs, and 10% of 1ROW DUEs
	settings.due_tol = pt.get<std::vector<double>>("ECC.due_tol", {0.});
	settings.due_tol.resize(DRAM_MAX - DRAM_1WORD, 0.);
	// set 1BIT = 1WORD
	settings.due_tol.insert(settings.due_tol.begin(), settings.due_tol.front());

	return 0;
}
