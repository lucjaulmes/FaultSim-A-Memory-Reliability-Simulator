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

#ifndef SETTINGS_HH_
#define SETTINGS_HH_

#include <string>
#include <vector>
#include <fstream>
#include <type_traits>

struct Settings
{
	/** Scrubbing interval (seconds) */
	uint64_t scrub_s;
	/** Simulation total duration (seconds) */
	uint64_t max_s;
	/** Number of simulations to run total */
	uint64_t n_sims;
	/** Seconds per output histogram bucket */
	uint64_t output_bucket_s;

	/** Continue simulations after the first uncorrectable error */
	bool continue_running;
	/** Enable or disable runtime output */
	int verbose;
	/** Enable a lot of printing */
	bool debug;


	/** The topology to simulate */
	enum {DIMM, STACK_3D} organization;
	// Settings for all DRAMs
	unsigned chips_per_rank, chip_bus_bits, ranks, banks, rows, cols;
	/** Number of bits per transaction. Also symbol size (in bits) for RAID-like parity */
	uint64_t data_block_bits;


	// Settings for 3D stacks

	/** TODO document */
	enum {VERTICAL, HORIZONTAL} cube_model;
	/** TODO document */
	uint64_t cube_addr_dec_depth;
	/** TODO document */
	uint64_t cube_ecc_tsv;
	/** TODO document */
	uint64_t cube_redun_tsv;

	/** Fault injection model (uniform random bit errors, Jaguar FIT rates etc.) */
	enum {JAGUAR, UNIFORM_BIT, MANUAL} faultmode;
	/** Base FIT rate scaling factor for memory arrays */
	double fit_factor;
	/** Base SCF rate scaling factor for memory arrays */
	double scf_factor;
	/** FIT rate for TSVs */
	double tsv_fit;
	/** Enable TSV fault injection */
	bool enable_tsv;
	/** Enable transient fault injection */
	bool enable_transient;
	/** Enable permanent fault injection */
	bool enable_permanent;

	/** Transient fault rates, default to the values from Jaguar supercomputer */
	std::vector<double> fit_transient{14.2, 1.4, 1.4, 0.2, 0.8, 0.3, 0.9};

	/** Permanent fault rates, default to the values from Jaguar supercomputer */
	std::vector<double> fit_permanent{18.6, 0.3, 5.6, 8.2, 10.0, 1.4, 2.8};


	// ECC configuration

	/** Type of ECC to apply: DDC = Data Device Correct */
	enum {NONE = 0, BCH, DDC, RAID, VECC, IECC = 8} repairmode;

	unsigned correct, detect;
	unsigned iecc_codeword, iecc_dataword;


	/** Fraction of failures that the software can tolerate */
	std::vector<double> sw_tol;

	/** Fraction of memory protected by Virtualized ECC */
	double vecc_protection;
	/** Number of extra corrected symbols */
	unsigned vecc_correct;
	/** Fraction software-tolerated failures in VECC-unprotected memory */
	std::vector<double> vecc_sw_tol;

	/** Load values from the file at ininame */
	int parse_settings(const std::string &ininame, std::vector<std::string> &config_overrides);
};


extern struct Settings settings;


// define some enum operators
template<typename EnumType> using if_enum = typename std::enable_if<std::is_enum<EnumType>::value, EnumType>::type;
template<typename EnumType> using Int = typename std::underlying_type<EnumType>::type;

template<typename T, class = if_enum<T>> inline constexpr T operator~ (T a) { return (T)~(Int<T>)a; }

template<typename T, class = if_enum<T>> inline constexpr T operator| (T a, T b) { return (T)((Int<T>)a | (Int<T>)b); }
template<typename T, class = if_enum<T>> inline constexpr T operator& (T a, T b) { return (T)((Int<T>)a & (Int<T>)b); }
template<typename T, class = if_enum<T>> inline constexpr T operator^ (T a, T b) { return (T)((Int<T>)a ^ (Int<T>)b); }

template<typename T, class = if_enum<T>> inline T& operator|= (T& a, T b) { return (T&)((Int<T>&)a |= (Int<T>)b); }
template<typename T, class = if_enum<T>> inline T& operator&= (T& a, T b) { return (T&)((Int<T>&)a &= (Int<T>)b); }
template<typename T, class = if_enum<T>> inline T& operator^= (T& a, T b) { return (T&)((Int<T>&)a ^= (Int<T>)b); }

#endif // SETTINGS_HH_
