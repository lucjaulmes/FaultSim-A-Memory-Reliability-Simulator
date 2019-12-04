#include <boost/test/unit_test.hpp>

#include "Settings.hh"
#include "GroupDomain_dimm.hh"
#include "ChipKillRepair.hh"

namespace chipkill
{

Settings settings()
{
	Settings settings {};

	settings.organization = Settings::DIMM;

	settings.chips_per_rank = 18;
	settings.chip_bus_bits = 4;
	settings.ranks = 1;
	settings.banks = 8;
	settings.rows = 16384;
	settings.cols = 2048;
	settings.data_block_bits = 512;

	settings.repairmode = Settings::DDC;  // Data Device Correct
	settings.correct = 1;
    settings.detect = 2;
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
GroupDomain_dimm &domain = *GroupDomain_dimm::genModule(conf, 0);


BOOST_AUTO_TEST_CASE( ChipKill_DRAM_chip_count )
{
	BOOST_CHECK( domain.getChildren().size() == 18 );
}

};
