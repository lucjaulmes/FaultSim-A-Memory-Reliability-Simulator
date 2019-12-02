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

#ifndef DRAM_COMMON_HH_
#define DRAM_COMMON_HH_

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/variate_generator.hpp>
#include <ctime>
#include <sys/time.h>


typedef enum : int { DRAM_1BIT = 0, DRAM_1WORD, DRAM_1COL, DRAM_1ROW, DRAM_1BANK, DRAM_NBANK, DRAM_NRANK, DRAM_MAX } fault_class_t;

// 64-bit random doubles for determining if failure happened
typedef boost::mt19937_64                     random64_engine_t;    // Mersenne Twister
typedef boost::random::uniform_real_distribution<double> random_uniform_t;
typedef boost::random::variate_generator<random64_engine_t, random_uniform_t> random_generator_t;   // Variate generator

// 32-bit random integers for determining fault locations
typedef boost::mt19937                      random32_engine_t;


// Just some counts
typedef struct faults_t
{
	uint64_t transient, permanent;

	inline
	uint64_t total()
	{
		return transient + permanent;
	}

	inline
	struct faults_t& operator+=(const struct faults_t &other)
	{
		transient += other.transient, permanent += other.permanent;
		return *this;
	}

	inline friend std::ostream& operator<< (std::ostream& stream, const struct faults_t& f) {
		stream << f.transient << " transient, " << f.permanent << " permanent";
		return stream;
	}
} faults_t;


typedef struct failures_t
{
	uint64_t undetected, uncorrected;

	inline
	struct failures_t& operator+=(const struct failures_t &other)
	{
		undetected += other.undetected, uncorrected += other.uncorrected;
		return *this;
	}

	inline friend std::ostream& operator<< (std::ostream& stream, const struct failures_t& e) {
		stream << e.undetected << " undetected, " << e.uncorrected << " uncorrected";
		return stream;
	}
} failures_t;



#endif /* DRAM_COMMON_HH_ */
