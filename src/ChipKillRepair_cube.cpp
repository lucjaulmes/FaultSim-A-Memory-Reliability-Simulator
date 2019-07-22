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

#include <string>
#include <time.h>

#include "ChipKillRepair_cube.hh"
#include "DRAMDomain.hh"

ChipKillRepair_cube::ChipKillRepair_cube(std::string name, int n_sym_correct, int n_sym_detect,
    FaultDomain *fd) : RepairScheme(name)
	, m_n_correct(n_sym_correct)
	, m_n_detect(n_sym_detect)
{
	DRAMDomain *DRAMchip = dynamic_cast<DRAMDomain *>(fd->getChildren().front());
	logBits = DRAMchip->getLogBits();
	logCols = DRAMchip->getLogCols();
	logRows = DRAMchip->getLogRows();
	banks = DRAMchip->getBanks();
}

std::pair<uint64_t, uint64_t> ChipKillRepair_cube::repair(FaultDomain *fd)
{
	//Choose the algorithm based on the whether its modelled as vertical channels or horizontal channels
	if (fd->cube_model_enable == 1)
		return repair_hc(fd);
	else
		return repair_vc(fd);
}


std::pair<uint64_t, uint64_t> ChipKillRepair_cube::repair_hc(FaultDomain *fd)
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;
	std::list<FaultDomain *> &pChips = fd->getChildren();
	//Initialize the counters to count chips
	uint64_t counter1 = 0;
	uint64_t counter2 = 0;

	int64_t bank_number1 = 0;
	int64_t bank_number2 = 0;

	//Clear out the touched values for all chips
	for (FaultDomain *fd: pChips)
		for (FaultRange *fr: dynamic_cast<DRAMDomain *>(fd)->getRanges())
			fr->touched = 0;


	//Take the 1st Chip and check if other chips also fail. We use only upto 8 chips
	for (FaultDomain *fd0: pChips)
	{
		// For each fault in first chip, query the second chip to see if it has an intersecting fault range.
		for (FaultRange *fr0: dynamic_cast<DRAMDomain *>(fd0)->getRanges())
		{
			// Make a copy, otherwise fault is modified as a side-effect
			FaultRange frTemp = *fr0;
			//8 Bytes are protected per chip
			frTemp.fWildMask = ((0x1 << 6) - 1);
			uint32_t n_intersections = 0;
			counter2 = 0;
			// for each other chip, count number of intersecting faults
			for (uint64_t ii = 0; ii < banks; ii++)
			{
				//Adjusting for number of banks
				uint64_t bit_shift = logBits + logRows + logCols;
				uint64_t and_value = 1 << (logBits + logRows + logCols);
				and_value = and_value - 1;
				uint64_t lower_addr = frTemp.fAddr & and_value;
				frTemp.fAddr = frTemp.fAddr >> (3 + bit_shift);     //8 Banks
				frTemp.fAddr = frTemp.fAddr << 3;
				frTemp.fAddr = frTemp.fAddr + ii;
				frTemp.fAddr = frTemp.fAddr << bit_shift;
				frTemp.fAddr = frTemp.fAddr | lower_addr;

				//Start looping accross chips
				for (FaultDomain *fd1: pChips)
				{
					if (counter1 < 2 && counter2 < 2)
					{
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
							if (frTemp.intersects(fr1))
							{
								// count the intersection
								n_intersections++;
								fr1->touched++;
								break;
							}
					}
					if ((counter1 < 2 || counter2 < 2) && (counter1 == 4 || counter2 == 4))
					{
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
						{
							bank_number2 = getbank_number(*fr1);
							if (bank_number1 != -1 && bank_number2 != -1)
							{
								if (bank_number2 == (bank_number1 >> 1))
								{
									if (frTemp.intersects(fr1))
									{
										// count the intersection
										n_intersections++;
										fr1->touched++;
										break;
									}
								}
							}
							else if ((bank_number1 == -1) && (bank_number2 < 4) && (bank_number2 > -1))
							{
								if (frTemp.intersects(fr1))
								{
									// count the intersection
									n_intersections++;
									fr1->touched++;
									break;
								}

							}
							else if ((bank_number2 == -1))
							{
								if (frTemp.intersects(fr1))
								{
									// count the intersection
									n_intersections++;
									fr1->touched++;
									break;
								}

							}
						}
					}
					if (counter1 > 1 && counter1 < 4 && counter2 > 1 && counter2 < 4)
					{
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
							if (frTemp.intersects(fr1))
							{
								// count the intersection
								n_intersections++;
								fr1->touched++;
								break;
							}
					}
					if (((counter1 > 1 && counter1 < 4) || (counter2 > 1 && counter2 < 4)) && (counter1 == 4 || counter2 == 4))
					{
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
						{
							bank_number2 = getbank_number(*fr1);
							if (bank_number2 == ((bank_number1 >> 1) | 0x4))
							{
								if (frTemp.intersects(fr1))
								{
									// count the intersection
									n_intersections++;
									fr1->touched++;
									break;
								}
							}
						}
					}
					if (counter1 > 4 && counter1 < 7 && counter2 > 4 && counter2 < 7)
					{
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
							if (frTemp.intersects(fr1))
							{
								// count the intersection
								n_intersections++;
								fr1->touched++;
								break;
							}
					}
					if (((counter1 > 4 && counter1 < 7) || (counter2 > 4 && counter2 < 7)) && (counter1 == 7 || counter2 == 7))
					{
						for (FaultRange *fr1: dynamic_cast<DRAMDomain *>(fd1)->getRanges())
						{
							bank_number2 = getbank_number(*fr1);
							if (bank_number2 == (bank_number1 >> 1))
							{
								if (frTemp.intersects(fr1))
								{
									// count the intersection
									n_intersections++;
									fr1->touched++;
									break;
								}
							}
						}
					}
					counter2++;
				}
			}
			if (n_intersections > m_n_correct)
				n_uncorrectable += n_intersections - m_n_correct;
			if (n_intersections > m_n_detect)
				n_undetectable += n_intersections - m_n_detect;
		}
	}
	return std::make_pair(n_undetectable, n_uncorrectable);
}

std::pair<uint64_t, uint64_t> ChipKillRepair_cube::repair_vc(FaultDomain *fd)
{
	uint64_t n_undetectable = 0, n_uncorrectable = 0;
	return std::make_pair(n_undetectable, n_uncorrectable);
}

int64_t ChipKillRepair_cube::getbank_number(FaultRange frTemp)
{
	int64_t n_bank_t = 0;
	int64_t n_bankMask_t = 0;
	int64_t fAddr = frTemp.fAddr >> (logRows + logCols + logBits);
	int64_t fWildMask = frTemp.fWildMask >> (logRows + logCols + logBits);
	n_bankMask_t = fWildMask & (banks - 1);
	n_bank_t = fAddr & (banks - 1);
	if (n_bankMask_t == (banks - 1))
		return -1;
	return n_bank_t;
}

uint64_t ChipKillRepair_cube::fill_repl(FaultDomain *fd)
{
	return 0;
}

void ChipKillRepair_cube::printStats()
{
	RepairScheme::printStats();
}

void ChipKillRepair_cube::resetStats()
{
	RepairScheme::resetStats();
}
