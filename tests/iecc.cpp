#include <boost/test/unit_test.hpp>

#include "dram_common.hh"
#include "Settings.hh"
#include "FaultDomain.hh"
#include "DRAMDomain.hh"
#include "GroupDomain_dimm.hh"
#include "BCHRepair_inDRAM.hh"

#include "utils.hh"

namespace indram
{

Settings settings()
{
	Settings settings {};

	settings.organization = Settings::DIMM;

	settings.chips_per_rank = 16;
	settings.chip_bus_bits = 4;
	settings.ranks = 1;
	settings.banks = 8;
	settings.rows = 16384;
	settings.cols = 2176;
	settings.data_block_bits = 512;

	settings.repairmode = Settings::IECC;
	settings.correct = 0;
    settings.detect = 0;
	settings.iecc_codeword = 136;
	settings.iecc_dataword = 128;

	settings.faultmode = Settings::JAGUAR;
	settings.fit_factor = 0.;
	settings.scf_factor = 0.;
	settings.tsv_fit = 0.;
	settings.enable_tsv = false;
	settings.enable_transient = false;
	settings.enable_permanent = false;
	settings.fit_transient = {14.2, 1.4, 1.4, 0.2, 0.8, 0.3, 0.9};
	settings.fit_permanent = {18.6, 0.3, 5.6, 8.2, 10.0, 1.4, 2.8};

	settings.sw_tol = {0., 0., 0., 0., 0., 0., 0.};

	return settings;
}

Settings conf = settings();
std::unique_ptr<GroupDomain_dimm> domain {GroupDomain_dimm::genModule(conf, 0)};
std::vector<DRAMDomain *> chips = get_chips(*domain);

const unsigned symbol_size = domain->burst_size() / domain->data_chips();



BOOST_AUTO_TEST_CASE( IECC_DRAM_1bit )
{
	FaultRange *fr = chips[0]->genRandomRange(DRAM_1BIT, false);
	chips[0]->insertFault(fr);

	BOOST_CHECK( domain->repair().any() == false );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( IECC_DRAM_2bit_separate )
{
	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1BIT, false);
	FaultRange *fr1 = new FaultRange(*fr0);

	// Inject in the next IECC codeword
	unsigned wordsize = (conf.iecc_codeword - conf.iecc_dataword) / chips[0]->getNum<Bits>();
	diff<Cols>(fr0, fr1, wordsize);

	chips[0]->insertFault(fr0);
	chips[0]->insertFault(fr1);

	BOOST_CHECK( domain->repair().any() == false );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( IECC_DRAM_2bit_codeword )
{
	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1BIT, false);
	FaultRange *fr1 = new FaultRange(*fr0);

	// Inject in the same IECC codeword
	unsigned wordsize = (conf.iecc_codeword - conf.iecc_dataword) / chips[0]->getNum<Bits>();
	unsigned col = chips[0]->get<Cols>(fr0->fAddr);

	unsigned word = col / wordsize, pos = col % wordsize;
	chips[0]->put<Cols>(fr1->fAddr, word * wordsize + (pos + 1) % wordsize);

	FaultIntersection fi0(fr0, 127);
	FaultIntersection fi1(fr1, 127);
	fi0.intersection(fi1);

	BOOST_CHECK( fi0.bit_count_aggregate(128) == 2 );

	chips[0]->insertFault(fr0);
	chips[0]->insertFault(fr1);

	BOOST_CHECK( domain->repair().any() == true );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( IECC_DRAM_2x_same_1bit )
{
	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1BIT, false);
	FaultRange *fr1 = new FaultRange(*fr0);

	FaultIntersection fi0(fr0, 127);
	FaultIntersection fi1(fr1, 127);
	fi0.intersection(fi1);

	BOOST_CHECK( fi0.bit_count_aggregate(128) == 1 );

	// twice the same 1BIT fault should be correctable
	chips[0]->insertFault(fr0);
	chips[0]->insertFault(fr1);

	BOOST_CHECK( domain->repair().any() == false );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( IECC_DRAM_1bit_1col )
{
	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1COL, false);
	FaultRange *fr1 = new FaultRange(*fr0);
	chips[0]->put<Rows>(fr1->fWildMask, 0);
	chips[0]->put<Bits>(fr1->fWildMask, 0);

	FaultIntersection fi0(fr0, 127);
	FaultIntersection fi1(fr1, 127);
	fi0.intersection(fi1);

	BOOST_CHECK( fi0.bit_count_aggregate(128) == chips[0]->getNum<Cols>() );

	// Inject in the same IECC codeword
	chips[0]->insertFault(fr0);
	chips[0]->insertFault(fr1);

	BOOST_CHECK( domain->repair().any() == true );

	domain->reset();
}

};
