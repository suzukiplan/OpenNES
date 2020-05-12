// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_OPENNES_H
#define INCLUDE_OPENNES_H
#include "M6502/m6502.hpp"
#include "apu.hpp"
#include "mmu.hpp"
#include "ppu.hpp"
#include <stdio.h>

class OpenNES
{
  public:
    enum ColorMode {
        RGB555,
        RGB565,
    };

  private:
    bool isNTSC;
    int cpuClockHz;

  public:
    APU* apu;
    PPU* ppu;
    MMU* mmu;
    M6502* cpu;
    unsigned int tickCount;
    unsigned short display[256 * 240];
    OpenNES(bool isNTSC, ColorMode colorMode);
    ~OpenNES();
    bool loadRom(void* data, size_t size);
    bool loadRomFile(const char* filename);
    void reset();
    void tick(unsigned char pad1, unsigned char pad2);
    void enableDebug()
    {
        if (cpu) {
            cpu->setDebugMessage([](void* arg, const char* message) {
                M6502* cpu = ((OpenNES*)arg)->cpu;
                printf("%-40s PC:$%04X A:$%02X X:$%02X Y:$%02X S:$%02X P:$%02X\n", message, cpu->R.pc, cpu->R.a, cpu->R.x, cpu->R.y, cpu->R.s, cpu->R.p);
            });
        }
    }
};

#endif // INCLUDE_OPENNES_H
