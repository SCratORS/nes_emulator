#include "Mapper_003.h"

Mapper_003::Mapper_003(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks){}

Mapper_003::~Mapper_003(){}

bool Mapper_003::CPUMapAddress(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, bool write) {
	if (addr >= 0x8000 && addr <= 0xFFFF) {
		if (write) {
			nCHRBankSelect = data & 0x03;
			mapped_addr = addr;
		} else	{
			if (nPRGBanks == 1) mapped_addr = addr & 0x3FFF;
			if (nPRGBanks == 2) mapped_addr = addr & 0x7FFF;
			return true;
		}	
	}
	return false;
}

bool Mapper_003::PPUMapAddress(uint16_t addr, uint32_t &mapped_addr, bool write) {
	mapped_addr = nCHRBankSelect * 0x2000 + addr;
	return addr<0x2000;
}
