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

    void _freeData()
    {
        if (romData.prgData) free(romData.prgData);
        if (romData.chrData) free(romData.chrData);
        memset(&romData, 0, sizeof(romData));
    }

    void _clearRAM()
    {
        memset(&M, 0, sizeof(M));
        memset(&R, 0, sizeof(R));
    }

    size_t _getSizeValue(unsigned int exponent, unsigned int multiplier)
    {
        if (exponent > 60) exponent = 60;
        multiplier = multiplier * 2 + 1;
        return multiplier * (size_t)1 << exponent;
    }

  public:
    struct RomData {
        unsigned char* prgData;       // raw data of ROM
        unsigned char* chrData;       // raw data of CHR
        unsigned char trainer[0x200]; // 512-byte trainer at $7000-$71FF (stored before PRG data)
        size_t prgSize;               // size of program data (unit size: byte)
        size_t chrSize;               // size of character data (unit size: byte)
        int mapper;                   // number of mapper
        bool ignoreMirroring;         // Ignore mirroring control or above mirroring bit; instead provide four-screen VRAM
        bool hasTrainer;              // exist flag of 512-byte trainer at $7000-$71FF (stored before PRG data)
        bool hasButtryBackup;         // has buttery backup (sram)
        bool mirroring;               // TRUE: vertical (horizontal arrangement) (CIRAM A10 = PPU A10), FALSE: horizontal (vertical arrangement) (CIRAM A10 = PPU A11)
        bool isNes20;                 // flags 8-15 are in NES 2.0 format
        bool isPlayChoice10;          // 8KB of Hint Screen data stored after CHR data (not part of the official specification)
        bool isVS;                    // VS Unisystem
    } romData;

    struct MainMemory {
        unsigned char ram[0x800];    // WRAM
        unsigned char exRam[0x2000]; // Extra RAM
        unsigned char sram[0x2000];  // Battery backup
    } M;

    struct Register {
        unsigned char bank[4];
        unsigned char pad[2];
        unsigned char reserved[10];
    } R;

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
        if (page < 0x20) return M.ram[addr & 0x7FF];                                                      // WRAM
        if (page < 0x40) return ppuRead(arg, 0x2000 + (addr & 0b111));                                    // PPU I/O
        if (addr < 0x4020) return apuRead(arg, addr);                                                     // APU I/O
        if (page < 0x60) return M.exRam[addr - 0x4000];                                                   // ExRAM
        if (romData.hasTrainer && 0x7000 <= addr && addr < 0x7200) return romData.trainer[addr - 0x7000]; // Trainer
        if (page < 0x80) return romData.hasButtryBackup ? M.sram[addr - 0x6000] : 0;                      // SRAM (Battery backup)
        // Read from ROM
        unsigned int ptr = R.bank[(addr & 0b0110000000000000) >> 13];
        ptr *= 0x2000;
        ptr |= addr & 0x1FFF;
        return ptr < romData.prgSize ? romData.prgData[ptr] : 0x00;
    }

    void writeMemory(unsigned short addr, unsigned char value)
    {
        unsigned char page = (addr & 0xFF00) >> 8;
        if (page < 0x20) {
            M.ram[addr & 0x7FF] = value;
        } else if (page < 0x40) {
            ppuWrite(arg, 0x2000 + (addr & 0b111), value);
        } else if (addr < 0x4020) {
            apuWrite(arg, addr, value);
        } else if (page < 0x60) {
            M.exRam[addr - 0x4000] = value;
        } else if (romData.hasButtryBackup && page < 0x80) {
            M.sram[addr - 0x6000] = value;
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
        romData.isNes20 = (data[7] & 0b00001100) == 0b00001000 ? true : false;
        if (romData.isNes20) {
            if ((data[9] & 0x0F) == 0x0F) {
                romData.prgSize = _getSizeValue(data[4] >> 2, data[4] & 0b11);
                romData.chrSize = _getSizeValue(data[5] >> 2, data[5] & 0b11);
            } else {
                romData.prgSize = (((data[9] & 0x0F) << 8) | data[4]) * 0x4000;
                romData.prgSize = (((data[9] & 0x0F) << 8) | data[5]) * 0x2000;
            }
        } else {
            romData.prgSize = (data[4] ? data[4] : 256) * 0x4000;
            romData.chrSize = data[5] * 0x2000;
        }
        romData.mapper = (data[6] & 0b11110000) >> 4;
        romData.ignoreMirroring = data[6] & 0b00001000 ? true : false;
        romData.hasTrainer = data[6] & 0b00000100 ? true : false;
        romData.hasButtryBackup = data[6] & 0b00000010 ? true : false;
        romData.mirroring = data[6] & 0b000000001 ? true : false;
        romData.prgData = (unsigned char*)malloc(romData.prgSize);
        romData.chrData = (unsigned char*)malloc(romData.chrSize);
        romData.mapper += data[7] & 0b11110000;
        romData.isPlayChoice10 = data[7] & 0b00000010 ? true : false;
        romData.isVS = data[7] & 0b00000001 ? true : false;
        int ptr = 16;
        if (romData.hasTrainer) {
            if (size < ptr + 512) return false;
            memcpy(romData.trainer, &data[ptr], 512);
            ptr += 512;
        }
        if (size < ptr + romData.prgSize + romData.chrSize) return false;
        if (NULL == (romData.prgData = (unsigned char*)malloc(romData.prgSize))) return false;
        memcpy(romData.prgData, &data[ptr], romData.prgSize);
        if (0x8000 <= romData.prgSize) {
            R.bank[0] = 0;
            R.bank[1] = 1;
            R.bank[2] = 2;
            R.bank[3] = 3;
        } else {
            R.bank[0] = 0;
            R.bank[1] = 1;
            R.bank[2] = 0;
            R.bank[3] = 1;
        }
        ptr += romData.prgSize;
        if (0 < romData.chrSize) {
            if (NULL == (romData.chrData = (unsigned char*)malloc(romData.chrSize))) return false;
            memcpy(romData.chrData, &data[ptr], romData.chrSize);
        } else {
            romData.chrData = NULL;
        }
        // do not support play choise 10
        return true;
    }

    bool isUsingExRam()
    {
        char buf[0x2000];
        memset(buf, 0, sizeof(buf));
        return 0 != memcmp(M.exRam, buf, 0x2000);
    }

    size_t getStateSize()
    {
        size_t size = 4;
        size += 3;
        size += sizeof(M.ram);
        size += 3;
        size += sizeof(R);
        if (isUsingExRam()) {
            size += 3;
            size += sizeof(M.exRam);
        }
        if (romData.hasButtryBackup) {
            size += 3;
            size += sizeof(M.sram);
        }
        return size;
    }

    void saveState(void* data)
    {
        char* cp = (char*)data;
        memcpy(cp, "MM", 2);
        int ptr = 2;
        unsigned short ds;
        ds = (unsigned short)getStateSize();
        cp[ptr++] = (ds & 0xFF00) >> 8;
        cp[ptr++] = ds & 0xFF;

        ds = (unsigned short)sizeof(M.ram);
        cp[ptr++] = 'W';
        cp[ptr++] = (ds & 0xFF00) >> 8;
        cp[ptr++] = ds & 0xFF;
        memcpy(&cp[ptr], M.ram, ds);
        ptr += ds;

        ds = (unsigned short)sizeof(R);
        cp[ptr++] = 'R';
        cp[ptr++] = (ds & 0xFF00) >> 8;
        cp[ptr++] = ds & 0xFF;
        memcpy(&cp[ptr], &R, ds);
        ptr += ds;

        if (isUsingExRam()) {
            ds = (unsigned short)sizeof(M.exRam);
            cp[ptr++] = 'E';
            cp[ptr++] = (ds & 0xFF00) >> 8;
            cp[ptr++] = ds & 0xFF;
            memcpy(&cp[ptr], M.exRam, ds);
            ptr += ds;
        }

        if (romData.hasButtryBackup) {
            ds = (unsigned short)sizeof(M.sram);
            cp[ptr++] = 'S';
            cp[ptr++] = (ds & 0xFF00) >> 8;
            cp[ptr++] = ds & 0xFF;
            memcpy(&cp[ptr], M.sram, ds);
            ptr += ds;
        }
    }

    size_t loadState(void* data)
    {
        _clearRAM();
        char* cp = (char*)data;
        if (memcmp(cp, "MM", 2)) return 0;
        size_t size = cp[2];
        size *= 256;
        size += cp[3];
        int ptr = 4;
        while (ptr < size) {
            char t = cp[ptr++];
            unsigned short ds = cp[ptr++] * 256;
            ds += cp[ptr++];
            switch (t) {
                case 'W': memcpy(M.ram, &cp[ptr], ds); break;
                case 'R': memcpy(&R, &cp[ptr], ds); break;
                case 'E': memcpy(M.exRam, &cp[ptr], ds); break;
                case 'S': memcpy(M.sram, &cp[ptr], ds); break;
            }
            ptr += ds;
        }
        return size;
    }
};

#endif // INCLUDE_MMU_HPP
