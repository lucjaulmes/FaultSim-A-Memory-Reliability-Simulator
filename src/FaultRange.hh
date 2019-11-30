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

#ifndef FAULTRANGE_HH_
#define FAULTRANGE_HH_

#include <string>
#include <vector>
#include <list>
#include <tuple>


class DRAMDomain;


class FaultRange
{
public:
	DRAMDomain *m_pDRAM;

	uint64_t fAddr, fWildMask; // address of faulty range, and bit positions that are wildcards (all values)
	bool transient;
	bool TSV;

	uint64_t max_faults;

	uint64_t touched;
	uint64_t fault_mode;

	bool transient_remove;
	bool recent_touched;

	FaultRange(DRAMDomain *pDRAM);

	inline
	FaultRange(DRAMDomain *pDRAM, uint64_t addr, uint64_t mask, bool is_tsv, bool is_transient, uint64_t nbits)
		: m_pDRAM(pDRAM)
		, fAddr(addr), fWildMask(mask), transient(is_transient), TSV(is_tsv), max_faults(nbits)
	    , touched(0), fault_mode(0), transient_remove(true), recent_touched(false)
	{
	}

	virtual ~FaultRange() {}

	// does this FR intersect with the supplied FR?
	bool intersects(FaultRange *fr) const;
	// How many bits in any sym_bits-wide symbol could be faulty?
	//uint64_t maxFaultyBits( uint64_t sym_bits );
	bool isTSV();
	virtual std::string toString() const;   // for debugging

	inline friend std::ostream& operator<< (std::ostream& stream, const FaultRange& fr) {
		stream << fr.toString();
		return stream;
	}

	inline virtual
	void mark_uncorrectable() {
		transient_remove = false;
	}

	inline virtual
	bool scrub_candidate() {
		return transient && transient_remove;
	}

private:
	// Chip location of this fault range.
	uint32_t Chip;

	std::list<FaultRange> m_children;    // 'smaller' ranges contained within this one
};


class FaultIntersection: public FaultRange
{
private:
	std::vector<FaultRange*> intersecting;

	enum { CORRECTED = 0, UNCORRECTED, UNDETECTED } outcome;

public:
	// The intersection of 0 faults
	FaultIntersection() :
		FaultRange(nullptr, 0ULL, ~0ULL, false, false, 0), intersecting(), outcome(CORRECTED)
	{
	}

	// Use FaultRange copy constructor to create the intersection of 1 fault
	FaultIntersection(FaultRange *fault, uint64_t min_mask):
		FaultRange(*fault), intersecting(), outcome(UNDETECTED)
	{
		fAddr &= ~min_mask;
		fWildMask |= min_mask;
		intersecting.push_back(fault);
	}

	void intersection(const FaultIntersection &fr);

	// Each FaultRange represents chip with intersecting errors
	inline
	size_t chip_count()
	{
		return intersecting.size();
	}

	inline
	size_t bit_count(size_t word_mask)
	{
		// for BCH codes, return the number of faults per word
		size_t wrong_bits = 0;
		for (auto &fr: intersecting)
			wrong_bits += __builtin_popcount(fr->fWildMask & word_mask);

		return wrong_bits;
	}

	inline
	void mark_corrected()
	{
		outcome = CORRECTED;
	}

	inline
	void mark_uncorrectable()
	{
		outcome = UNCORRECTED;

		transient_remove = false;
		for (auto &fr: intersecting)
			fr->mark_uncorrectable();
	}

	inline
	void mark_undetectable()
	{
		outcome = UNDETECTED;

		transient_remove = false;
		for (auto &fr: intersecting)
			fr->mark_uncorrectable();
	}

	inline
	bool corrected()
	{
		return outcome == CORRECTED;
	}

	inline
	bool detected()
	{
		return outcome != UNDETECTED;
	}

	std::string toString();
};


#endif /* FAULTRANGE_HH_ */
