// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_PPU_HPP
#define INCLUDE_PPU_HPP
#include "OpenNES.h"

class PPU
{
  public:
    int frameCycleClock;
    struct Register {
        unsigned char status;
        unsigned char internalFlag; // bit7: need update vram, bit0: toggle
        unsigned char scroll[2];
        unsigned char vramAddrTmp[2];
        unsigned short vramAddr;
        int clock;
        int vramAddrUpdateClock;
        int line;
    } R;

    PPU(bool isNTSC)
    {
        memset(&R, 0, sizeof(R));
        frameCycleClock = isNTSC ? 89342 : 105710;
    }

    inline unsigned char inPort(unsigned short addr)
    {
        switch (addr) {
            case 0x2002:
                R.internalFlag = 0;
                return R.status;
        }
        return 0;
    }

    inline void outPort(unsigned short addr, unsigned char value)
    {
        switch (addr) {
            case 0x2005:
                R.scroll[R.internalFlag & 1] = value;
                R.internalFlag ^= 0b00000001;
                break;
            case 0x2006:
                R.vramAddrTmp[R.internalFlag & 1] = value;
                if (R.internalFlag & 1) {
                    // update after 3 PPU clocks
                    R.vramAddrUpdateClock = (R.clock + 3) % frameCycleClock;
                    R.internalFlag |= 0b10000000;
                }
                R.internalFlag ^= 0b00000001;
                break;
            case 0x2007:
                // 暫定処理: PPUに対するwriteが印字可能なASCIIコードだったらstderrに印字
                if (isprint(value)) {
                    fprintf(stderr, "%c", (char)value);
                }
                break;
        }
    }

    inline void tick(M6502* cpu)
    {
        R.clock++;
        R.clock %= frameCycleClock;
        if (R.internalFlag & 0b10000000 && R.clock == R.vramAddrUpdateClock) {
            R.internalFlag &= 0b01111111;
            R.vramAddr = R.vramAddrTmp[0] * 256 + R.vramAddrTmp[1];
        }
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
