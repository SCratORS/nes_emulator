#include "CARTRIDGE.h"

CARTRIDGE::CARTRIDGE(const std::string& sFileName) {
	struct sHeader
	{
		char name[4];
		uint8_t prg_rom_chunks;
		uint8_t chr_rom_chunks;
		uint8_t control_byte;
		uint8_t mapper_num;
		uint8_t prg_ram_size;
		uint8_t tv_system1;
		uint8_t tv_system2;
		char unused[5];
	} header;

	bImageValid = false;

	std::ifstream ifs;
	ifs.open(sFileName, std::ifstream::binary);
	if (ifs.is_open()) {
		ifs.read((char*)&header, sizeof(sHeader));
		if (header.name[0] == 'N' && header.name[1] == 'E' &&
			header.name[2] == 'S' && header.name[3] == '\32') { 
			if (header.control_byte & 0x04) ifs.seekg(512, std::ios_base::cur);
			nMapperID = (header.mapper_num & 0xF0) | (header.control_byte >> 4);
			hw_mirror = (header.control_byte & 0x01) ? 0x0A : 0x0C;
			uint8_t nFileType = 1; //default as iNes v1 format
			if ((header.mapper_num & 0x0C) == 0x08) nFileType = 2; //it's iNes v2 format

			if (nFileType == 1)	{ //iNes v1 format serialization
				nPRGBanks = header.prg_rom_chunks;
				nCHRBanks = header.chr_rom_chunks?header.chr_rom_chunks:1;
			}
			if (nFileType == 2)	{ //iNes v2 format serialization
				nPRGBanks = ((header.prg_ram_size & 0x07) << 8) | header.prg_rom_chunks;
				nCHRBanks = ((header.prg_ram_size & 0x38) << 8) | header.chr_rom_chunks;
			}
			vPRGMemory.resize(nPRGBanks * 0x4000);
			vCHRMemory.resize(nCHRBanks * 0x2000);
			ifs.read((char*)vPRGMemory.data(), vPRGMemory.size());
			ifs.read((char*)vCHRMemory.data(), vCHRMemory.size());
			switch (nMapperID) {
				case   0: pMapper = new Mapper_000(nPRGBanks, nCHRBanks); break;
				case   1: pMapper = new Mapper_001(nPRGBanks, nCHRBanks); break;
				case   2: pMapper = new Mapper_002(nPRGBanks, nCHRBanks); break;
				case   3: pMapper = new Mapper_003(nPRGBanks, nCHRBanks); break;
				case   4: pMapper = new Mapper_004(nPRGBanks, nCHRBanks); break;
				case   7: pMapper = new Mapper_007(nPRGBanks, nCHRBanks); break;
				case   23: pMapper = new Mapper_023(nPRGBanks, nCHRBanks); break;
				case   66: pMapper = new Mapper_066(nPRGBanks, nCHRBanks); break;
				case   71: pMapper = new Mapper_071(nPRGBanks, nCHRBanks); break;
				case   212: pMapper = new Mapper_212(nPRGBanks, nCHRBanks); break;
			}
			bImageValid = pMapper != NULL;
		}
		ifs.close();
	}
}

CARTRIDGE::~CARTRIDGE() { }

bool CARTRIDGE::ImageValid() {return bImageValid;}
void CARTRIDGE::reset() {pMapper->reset();}
uint8_t CARTRIDGE::Mirror() { uint8_t m = pMapper->mirror();return m==0x01?hw_mirror:m; }
void CARTRIDGE::IRQScanline(int16_t x, int16_t y, uint8_t mask, uint8_t control) {pMapper->scanline(x, y, mask, control);}
bool CARTRIDGE::IRQState() {return pMapper->irqState();}
void CARTRIDGE::IRQClear() {pMapper->irqClear();}

bool CARTRIDGE::MemAccess(uint16_t addr, uint8_t &data, bool write) {
	uint32_t mapped_addr = 0;
	if (pMapper->CPUMapAddress(addr, mapped_addr, data, write)) {
		if (mapped_addr != 0xFFFFFFFF) {// Mapper has actually set the data value, for example cartridge based RAM 
			uint8_t &M = vPRGMemory[mapped_addr];
			if (write) M = data; else data = M;
		}
		return true;
	}
	return false;
}

bool CARTRIDGE::PPUMemAccess(uint16_t addr, uint8_t &data, bool write) {
	uint32_t mapped_addr = 0;
	if (pMapper->PPUMapAddress(addr, mapped_addr, write)) {
		uint8_t &M = vCHRMemory[mapped_addr];
		if (write) M = data; else data = M;
		return true;
	}
	return false;
}