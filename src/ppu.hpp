// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_PPU_HPP
#define INCLUDE_PPU_HPP
#include "OpenNES.h"

class PPU
{
  private:
    struct Callback {
        void* arg;
        void (*endOfFrame)(void* arg);
    } CB;
    int frameCycleClock;

  public:
    unsigned char display[256 * 240]; // NES pallete display
    struct VideoMemory {
        unsigned char ptn1;
        unsigned char ptn2;
        unsigned char name[4]; // 0: LeftTop, 1: RightTop, 2: LeftBottom, 3: RightBottom
        unsigned char nameBuffer[4][0x400];
        unsigned char palette[0x20];
        unsigned char oam[0x100];
        unsigned char vramAddrTmp[2];
        int vramAddrUpdateClock;
    } M;

    struct Register {
        unsigned char status;
        unsigned char internalFlag; // bit7: need update vram, bit0: toggle
        unsigned char scroll[2];
        unsigned short vramAddr;
        int clock;
        int line;
    } R;

    void setEndOfFrame(void* arg, void (*endOfFrame)(void* arg))
    {
        CB.arg = arg;
        CB.endOfFrame = endOfFrame;
    }

    void setup(MMU::RomData* rom, bool isNTSC)
    {
        memset(&R, 0, sizeof(R));
        memset(&M, 0, sizeof(M));
        if (rom->ignoreMirroring) {
            // 4 screen
            M.name[0] = 0;
            M.name[1] = 1;
            M.name[2] = 2;
            M.name[3] = 3;
        } else if (rom->mirroring) {
            // vertical mirroring
            M.name[0] = 0;
            M.name[1] = 1;
            M.name[2] = 0;
            M.name[3] = 1;
        } else {
            // horizontal mirroring
            M.name[0] = 0;
            M.name[1] = 0;
            M.name[2] = 1;
            M.name[3] = 1;
        }
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
                M.vramAddrTmp[R.internalFlag & 1] = value;
                if (R.internalFlag & 1) {
                    // update after 3 PPU clocks
                    M.vramAddrUpdateClock = (R.clock + 3) % frameCycleClock;
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
        if (R.internalFlag & 0b10000000 && R.clock == M.vramAddrUpdateClock) {
            R.internalFlag &= 0b01111111;
            R.vramAddr = M.vramAddrTmp[0] * 256 + M.vramAddrTmp[1];
        }
        if (R.line != R.clock / 341) {
            R.line = R.clock / 341;
            switch (R.line) {
                case 241: // start vertical blanking line
                    R.status |= 0b10000000;
                    cpu->NMI();
                    CB.endOfFrame(CB.arg);
                    return;
                case 261: // pre-render scanline
                    R.status &= 0b01111111;
                    return;
            }
        }
    }
};

#endif // INCLUDE_PPU_HPP
