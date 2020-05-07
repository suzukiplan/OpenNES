// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_MMU_HPP
#define INCLUDE_MMU_HPP
#include "OpenNES.h"
#include <stdlib.h>
#include <string.h>

class MMU
{
  private:
    void* arg;

    struct RomData {
        unsigned char* prgData;       // raw data of ROM
        unsigned char* chrData;       // raw data of CHR
        unsigned char trainer[0x200]; // 512-byte trainer at $7000-$71FF (stored before PRG data)
        int prg;                      // number of program data (unit size: 8KB)
        int chr;                      // number of character data (unit size: 8KB)
        int mapper;                   // number of mapper
        bool ignoreMirroring;         // Ignore mirroring control or above mirroring bit; instead provide four-screen VRAM
        bool hasTrainer;              // exist flag of 512-byte trainer at $7000-$71FF (stored before PRG data)
        bool hasButtryBackup;         // has buttery backup (sram)
        bool mirroring;               // TRUE: vertical (horizontal arrangement) (CIRAM A10 = PPU A10), FALSE: horizontal (vertical arrangement) (CIRAM A10 = PPU A11)
        bool isNes20;                 // flags 8-15 are in NES 2.0 format
        bool isPlayChoice10;          // 8KB of Hint Screen data stored after CHR data (not part of the official specification)
        bool isVS;                    // VS Unisystem
    } romData;

    struct Register {
        unsigned char bank[4];
        unsigned char pad[2];
    };

    void _freeData()
    {
        if (romData.prgData) free(romData.prgData);
        if (romData.chrData) free(romData.chrData);
        memset(&romData, 0, sizeof(romData));
    }

    void _clearRAM()
    {
        memset(ram, 0, sizeof(ram));
        memset(exRam, 0, sizeof(exRam));
        memset(sram, 0, sizeof(sram));
    }

  public:
    unsigned char ram[0x800];    // WRAM
    unsigned char exRam[0x2000]; // Extra RAM
    unsigned char sram[0x2000];  // Battery backup
    struct Register reg;         // MMU register
    unsigned char (*ppuRead)(void* arg, unsigned short addr);
    void (*ppuWrite)(void* arg, unsigned short addr, unsigned char value);
    unsigned char (*apuRead)(void* arg, unsigned short addr);
    void (*apuWrite)(void* arg, unsigned short addr, unsigned char value);

    MMU(unsigned char (*ppuRead)(void* arg, unsigned short addr),
        void (*ppuWrite)(void* arg, unsigned short addr, unsigned char value),
        unsigned char (*apuRead)(void* arg, unsigned short addr),
        void (*apuWrite)(void* arg, unsigned short addr, unsigned char value),
        void* arg)
    {
        this->arg = arg;
        _clearRAM();
        memset(&reg, 0, sizeof(reg));
        memset(&romData, 0, sizeof(romData));
        this->ppuRead = ppuRead;
        this->ppuWrite = ppuWrite;
        this->apuRead = apuRead;
        this->apuWrite = apuWrite;
    }

    ~MMU()
    {
        _freeData();
    }

    unsigned char readMemory(unsigned short addr)
    {
        unsigned char page = (addr & 0xFF00) >> 8;
        if (page < 0x08) return ram[addr];                                                                // WRAM (2KB)
        if (page < 0x20) return ram[addr & 0x7FF];                                                        // WRAM mirror
        if (page < 0x40) return ppuRead(arg, 0x2000 + (addr & 0b111));                                    // PPU I/O
        if (addr < 0x4020) return apuRead(arg, addr);                                                     // APU I/O
        if (page < 0x60) return exRam[addr - 0x4000];                                                     // ExRAM
        if (romData.hasTrainer && 0x7000 <= addr && addr < 0x7200) return romData.trainer[addr - 0x7000]; // Trainer
        if (romData.hasButtryBackup && page < 0x80) return sram[addr - 0x6000];                           // SRAM (Battery backup)
        // Read from ROM
        int p = (addr & 0b11000000) >> 6;
        int r = reg.bank[r];
        unsigned char* g = r < romData.prg ? &romData.prgData[r * 0x2000] : NULL;
        return g ? g[addr & 0x1FFF] : 0x00;
    }

    void writeMemory(unsigned short addr, unsigned char value)
    {
        unsigned char page = (addr & 0xFF00) >> 8;
        if (page < 0x08) {
            ram[addr] = value;
        } else if (page < 0x20) {
            ram[addr % 0x800] = value;
        } else if (page < 0x40) {
            ppuWrite(arg, 0x2000 + (addr & 0b111), value);
        } else if (addr < 0x4020) {
            apuWrite(arg, addr, value);
        } else if (page < 0x60) {
            exRam[addr - 0x4000] = value;
        } else if (romData.hasButtryBackup && page < 0x80) {
            sram[addr - 0x6000] = value;
        } else {
            // Write to ROM area
        }
    }

    bool loadRom(unsigned char* data, size_t size)
    {
        _freeData();
        _clearRAM();
        if (size < 16) return false;
        if (0 != strncmp((char*)data, "NES", 3)) return false;
        if (data[3] != 0x1A) return false;
        romData.prg = data[4] * 2;
        romData.chr = data[5];
        romData.mapper = (data[6] & 0b11110000) >> 4;
        romData.ignoreMirroring = data[6] & 0b00001000 ? true : false;
        romData.hasTrainer = data[6] & 0b00000100 ? true : false;
        romData.hasButtryBackup = data[6] & 0b00000010 ? true : false;
        romData.mirroring = data[6] & 0b000000001 ? true : false;
        romData.prgData = (unsigned char*)malloc(romData.prg * 0x2000);
        romData.chrData = (unsigned char*)malloc(romData.chr * 0x2000);
        romData.mapper += data[7] & 0b11110000;
        romData.isNes20 = (data[7] & 0b00001100) == 0b00001000 ? true : false;
        romData.isPlayChoice10 = data[7] & 0b00000010 ? true : false;
        romData.isVS = data[7] & 0b00000001 ? true : false;
        int ptr = 16;
        if (romData.hasTrainer) {
            if (size < ptr + 512) return false;
            memcpy(romData.trainer, &data[ptr], 512);
            ptr += 512;
        }
        if (size < ptr + romData.prg * 0x2000 + romData.chr * 0x2000) return false;
        if (NULL == (romData.prgData = (unsigned char*)malloc(romData.prg * 0x2000))) return false;
        memcpy(romData.prgData, &data[ptr], romData.prg * 0x2000);
        reg.bank[0] = 0;
        reg.bank[1] = 1;
        reg.bank[2] = 2;
        reg.bank[3] = 3;
        ptr += romData.prg * 0x2000;
        if (NULL == (romData.chrData = (unsigned char*)malloc(romData.chr * 0x2000))) return false;
        memcpy(romData.chrData, &data[ptr], romData.chr * 0x2000);
        // do not support play choise 10
        return true;
    }
};

#endif // INCLUDE_MMU_HPP
