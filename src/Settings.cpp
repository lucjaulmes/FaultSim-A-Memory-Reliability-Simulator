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

#include <unordered_map>
#include <iostream>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "dram_common.hh"
#include "Settings.hh"

/** A boost::property_tree::id_translator for std containers */
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

	boost::optional<std::string> put_value(const T& values) {
		std::stringstream ss;
		size_t i = 0;
		for (auto v : values)
			ss << (i++?" ":"") << v;
		return ss.str();
	}
};


/** A boost::property_tree::id_translator to extract enum values */
template <typename T>
struct enum_translator
{
	typedef std::string internal_type;
	typedef T external_type;

	const std::unordered_map<std::string, T> allowed_values;

	enum_translator(const std::unordered_map<std::string, T> &mapping)
		: allowed_values(std::move(mapping))
	{
	}

	boost::optional<T> get_value(const std::string& str) const
	{
		if (str.empty())
			return boost::none;

		std::string key;
		for (auto c = str.begin(); c != str.end(); ++c)
			// only keep lower-case alphanumerical characters.
			if (std::isalnum(static_cast<unsigned char>(*c)))
				key.push_back(std::tolower(static_cast<unsigned char>(*c)));

		auto pos = allowed_values.find(key);
		if (pos != allowed_values.end())
			return boost::make_optional(pos->second);

		std::cerr << "ERROR: value must be one of";
		char c = ':';
		for (auto &pair: allowed_values)
		{
			std::cerr << c << ' ' << pair.first;
			c = ',';
		}
		std::cerr << " ; got " << key << std::endl;
		return boost::none;
	}
};


int Settings::parse_settings(const std::string &ininame, std::vector<std::string> &config_overrides)
{
	boost::property_tree::iptree pt;
	boost::property_tree::ini_parser::read_ini(ininame.c_str(), pt);

	container_translator<std::vector<double>> vec_tr;
	enum_translator<decltype(organization)> org_tr({{"dimm", DIMM}, {"stack", STACK_3D}});
	enum_translator<decltype(cube_model)> cube_tr({{"vertical", VERTICAL}, {"horizontal", HORIZONTAL}});
	enum_translator<decltype(faultmode)> fm_tr({{"jaguar", JAGUAR}, {"uniformbit", UNIFORM_BIT}, {"manual", MANUAL}});
	enum_translator<decltype(repairmode)> repair_tr({  // e.g. "iecc + DDC" is a valid key
		{"none", NONE}, {"bch", BCH}, {"ddc", DDC}, {"raid", RAID}, {"vecc", VECC}, {"iecc", IECC},
		{"ieccbch", IECC | BCH}, {"ieccddc", IECC | DDC}, {"ieccraid", IECC | RAID}, {"ieccvecc", IECC | VECC},
		{"bchiecc", IECC | BCH}, {"ddciecc", IECC | DDC}, {"raidiecc", IECC | RAID}, {"vecciecc", IECC | VECC}
	});

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

	try
	{
		scrub_s = pt.get<uint64_t>("sim.scrub_s");
		max_s = pt.get<uint64_t>("sim.max_s");
		n_sims = pt.get<uint64_t>("sim.n_sims");
		output_bucket_s = pt.get<uint64_t>("sim.output_bucket_s");

		continue_running = pt.get<bool>("sim.continue_running");
		verbose = pt.get<int>("sim.verbose");
		debug = pt.get<int>("sim.debug");

		organization = pt.get<decltype(organization)>("org.organization", org_tr);
		chips_per_rank = pt.get<int>("org.chips_per_rank");
		chip_bus_bits = pt.get<int>("org.chip_bus_bits");
		ranks = pt.get<int>("org.ranks");
		banks = pt.get<int>("org.banks");
		rows = pt.get<int>("org.rows");
		cols = pt.get<int>("org.cols");
		data_block_bits = pt.get<int>("org.data_block_bits");

		if (organization == STACK_3D)
		{
			cube_model = pt.get<decltype(cube_model)>("org.cube.model", cube_tr);
			cube_addr_dec_depth = pt.get<int>("org.cube.addr_dec_depth");
			cube_ecc_tsv = pt.get<int>("org.cube.ecc_tsv");
			cube_redun_tsv = pt.get<int>("org.cube.redun_tsv");
			tsv_fit = pt.get<double>("fault.tsv_fit");
		}

		enable_permanent = pt.get<bool>("fault.enable_permanent");
		enable_transient = pt.get<bool>("fault.enable_transient");
		enable_tsv = pt.get<bool>("fault.enable_tsv");
		fit_factor = pt.get<double>("fault.fit_factor");
		scf_factor = pt.get<double>("fault.scf_factor", 1.0);

		faultmode = pt.get<decltype(faultmode)>("fault.faultmode", fm_tr);

		if (faultmode == UNIFORM_BIT)
		{
			fit_transient.clear();
			fit_permanent.clear();

			fit_transient.push_back(33.05);
			fit_permanent.push_back(33.05);

			fit_transient.resize(DRAM_MAX, 0.);
			fit_permanent.resize(DRAM_MAX, 0.);
		}
		else if (faultmode == MANUAL)
		{
			fit_transient = pt.get<std::vector<double>>("fault.fit_transient", vec_tr);
			fit_permanent = pt.get<std::vector<double>>("fault.fit_permanent", vec_tr);

			if (fit_transient.size() != DRAM_MAX || fit_permanent.size() != DRAM_MAX)
			{
				std::cerr << "ERROR: Wrong number of FIT rates\n";
				std::abort();
			}
		}

		repairmode = pt.get<decltype(repairmode)>("ECC.repairmode", repair_tr);

		// specify all the tolerance probabilities, in order, starting with 1WORD
		// e.g. ".9 0 .1" means 90% tolerance of 1WORD DUEs, 0% of 1COL DUEs, and 10% of 1ROW DUEs
		sw_tol = pt.get<std::vector<double>>("ECC.sw_tol", vec_tr);
		sw_tol.resize(DRAM_MAX - DRAM_1WORD, 0.);
		sw_tol.insert(sw_tol.begin(), sw_tol.front());  // set 1BIT = 1WORD

		if ((repairmode & ~IECC) != NONE)
		{
			correct = pt.get<unsigned>("ECC.correct");
			detect  = pt.get<unsigned>("ECC.detect");
		}

		if (repairmode & IECC)
		{
			iecc_codeword = pt.get<unsigned>("ECC.iecc.correct");
			iecc_symbols  = pt.get<unsigned>("ECC.iecc.detect");
		}

		if ((repairmode & ~IECC) == VECC)
		{
			vecc_protection = pt.get<double>("ECC.vecc.protection");
			vecc_correct = pt.get<unsigned>("ECC.vecc.correct");

			// Same as above, but for the VECC–unprotected regions
			vecc_sw_tol = pt.get<std::vector<double>>("ECC.vecc.sw_tol", sw_tol, vec_tr);
			vecc_sw_tol.resize(DRAM_MAX - DRAM_1WORD, 0.);
			vecc_sw_tol.insert(vecc_sw_tol.begin(), vecc_sw_tol.front());
		}
	}
	catch (boost::wrapexcept<boost::property_tree::ptree_bad_path> &e)
	{
		std::cerr << "Exception while loading config file: " << e.what() << std::endl;
		return 1;
	}

	pt.put<std::vector<double>>("fault.fit_transient", fit_transient, vec_tr);
	pt.put<std::vector<double>>("fault.fit_permanent", fit_permanent, vec_tr);
	pt.put<std::vector<double>>("ECC.sw_tol", sw_tol, vec_tr);
	pt.put<std::vector<double>>("ECC.vecc.sw_tol", vecc_sw_tol, vec_tr);
	pt.put<double>("ECC.vecc.protection", vecc_protection);

	// Updated tree, can be written out (NB 1BIT should be stripped from both sw_tol values)

	return 0;
}
