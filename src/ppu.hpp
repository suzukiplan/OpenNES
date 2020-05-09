// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_PPU_HPP
#define INCLUDE_PPU_HPP
#include "OpenNES.h"

class PPU
{
  public:
    struct Register {
        unsigned char status;
        int clock;
        int line;
    } R;

    PPU()
    {
        memset(&R, 0, sizeof(R));
    }

    inline unsigned char inPort(unsigned short addr)
    {
        switch (addr) {
            case 0x2002: return R.status;
        }
        return 0;
    }

    inline void outPort(unsigned short addr, unsigned char value)
    {
    }

    inline void tick(M6502* cpu)
    {
        R.clock++;
        R.clock %= 89342;
        if (R.line != R.clock / 341) {
            R.line = R.clock / 341;
            switch (R.line) {
                case 241: // start vertical blanking line
                    R.status |= 0b10000000;
                    cpu->NMI();
                    return;
                case 261: // pre-render scanline
                    R.status &= 0b01111111;
                    return;
            }
        }
    }
};

#endif // INCLUDE_PPU_HPP
