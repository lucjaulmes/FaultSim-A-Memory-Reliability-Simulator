#ifndef TEST_UTILS_HH_
#define TEST_UTILS_HH_

#include <algorithm>

#include "dram_common.hh"
#include "Settings.hh"
#include "FaultDomain.hh"
#include "DRAMDomain.hh"
#include "GroupDomain.hh"
#include "ChipKillRepair.hh"

inline
std::vector<DRAMDomain *> get_chips(GroupDomain &domain)
{
	std::vector<DRAMDomain *> chips;
	auto cast = [] (FaultDomain *fd) { return dynamic_cast<DRAMDomain *>(fd); };
	std::transform(domain.getChildren().begin(), domain.getChildren().end(),
				   std::back_inserter(chips), cast);
	return chips;
}



template <enum DramField F>
inline
void copy(const FaultRange *a, FaultRange *b)
{
	// if there’s just 1 possible value for this field, copying it is basically a NOP.
	assert( a->m_pDRAM->getNum<F>() == 1 || a->m_pDRAM->has<F>(a->fWildMask) );

	b->m_pDRAM->put<F>(b->fAddr, a->m_pDRAM->get<F>(a->fAddr));
	b->m_pDRAM->put<F>(b->fWildMask, a->m_pDRAM->get<F>(a->fWildMask));
}


template <enum DramField F>
inline
void diff(const FaultRange *a, FaultRange *b, unsigned shift = 1)
{
	assert( a->m_pDRAM->has<F>(a->fWildMask) );

	b->m_pDRAM->put<F>(b->fAddr, (a->m_pDRAM->get<F>(a->fAddr) + shift) % b->m_pDRAM->getNum<F>());
	b->m_pDRAM->put<F>(b->fWildMask, a->m_pDRAM->get<F>(a->fWildMask));

	assert( a->m_pDRAM->get<F>(a->fAddr) != b->m_pDRAM->get<F>(b->fAddr) );
}

#endif /* TEST_UTILS_HH_ */
