#define BOOST_TEST_MODULE header-only multiunit test
#include <boost/test/included/unit_test.hpp>

#include <memory>

#include "dram_common.hh"
#include "Settings.hh"
#include "FaultDomain.hh"
#include "DRAMDomain.hh"
#include "GroupDomain_dimm.hh"

#include "utils.hh"


namespace noecc
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
	settings.cols = 2048;
	settings.data_block_bits = 512;

	settings.repairmode = Settings::NONE;
	settings.correct = 0;
	settings.detect = 0;
	settings.iecc_codeword = 0;
	settings.iecc_symbols = 0;

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

const unsigned symbol_size = 1;


BOOST_AUTO_TEST_CASE( noECC_DRAM_chip_count )
{
	BOOST_CHECK( domain->getChildren().size() == 16 );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( noECC_DRAM_1fault )
{
	domain->reset();

	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1BIT, true);
	chips[0]->insertFault(fr0);

	BOOST_CHECK( chips[0]->getRanges().size() == 1 );

	auto &err = domain->intersecting_ranges(symbol_size);

	BOOST_CHECK( err.size() == 1 );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( noECC_DRAM_2faults_intersecting )
{
	domain->reset();

	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1BIT, true);
	FaultRange *fr1 = new FaultRange(*fr0);

	// Same 1 bit fault at the same position in 2 chips
	chips[0]->insertFault(fr0);
	chips[1]->insertFault(fr1);

	auto &err = domain->intersecting_ranges(1, [] (auto &f) { return f.chip_count() >= 2; });

	BOOST_CHECK( err.size() == 1 );

	domain->reset();
}

BOOST_AUTO_TEST_CASE( noECC_DRAM_2faults_different )
{
	domain->reset();

	FaultRange *fr0 = chips[0]->genRandomRange(DRAM_1BIT, true);
	FaultRange *fr1 = new FaultRange(*fr0);

	// Inject in different positions
	diff<Banks>(fr0, fr1);

	chips[0]->insertFault(fr0);
	chips[1]->insertFault(fr1);

	auto &err = domain->intersecting_ranges(1, [] (auto &f) { return f.chip_count() >= 2; });

	BOOST_CHECK( err.size() == 0 );

	domain->reset();
}

};
