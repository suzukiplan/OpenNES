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
        unsigned char pattern[2][0x1000];
        unsigned char name[4]; // 0: LeftTop, 1: RightTop, 2: LeftBottom, 3: RightBottom
        unsigned char nameBuffer[4][0x400];
        unsigned char palette[0x20];
        unsigned char oam[0x100];
        unsigned char vramAddrTmp[2];
        int vramAddrUpdateClock;
    } M;

    struct Register {
        unsigned char ctrl;
        unsigned char mask;
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
            case 0x2000: // Controller
                /**
                 * VPHB SINN
                 * |||| ||||
                 * |||| ||++- Base nametable address (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
                 * |||| |+--- VRAM address increment per CPU read/write of PPUDATA (0: add 1, going across; 1: add 32, going down)
                 * |||| +---- Sprite pattern table address for 8x8 sprites (0: $0000; 1: $1000; ignored in 8x16 mode)
                 * |||+------ Background pattern table address (0: $0000; 1: $1000)
                 * ||+------- Sprite size (0: 8x8 pixels; 1: 8x16 pixels)
                 * |+-------- PPU master/slave select (0: read backdrop from EXT pins; 1: output color on EXT pins)
                 * +--------- Generate an NMI at the start of the vertical blanking interval (0: off; 1: on)
                 */
                R.ctrl = value;
                break;
            case 0x2001: // Mask
                /**
                 * BGRs bMmG
                 * |||| ||||
                 * |||| |||+- Greyscale (0: normal color, 1: produce a greyscale display)
                 * |||| ||+-- 1: Show background in leftmost 8 pixels of screen, 0: Hide
                 * |||| |+--- 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
                 * |||| +---- 1: Show background
                 * |||+------ 1: Show sprites
                 * ||+------- Emphasize red
                 * |+-------- Emphasize green
                 * +--------- Emphasize blue
                 */
                R.mask = value;
                break;
            case 0x2005: // Scroll
                R.scroll[R.internalFlag & 1] = value;
                R.internalFlag ^= 0b00000001;
                break;
            case 0x2006: // VRAM address
                M.vramAddrTmp[R.internalFlag & 1] = value;
                if (R.internalFlag & 1) {
                    // update after 3 PPU clocks
                    M.vramAddrUpdateClock = (R.clock + 3) % frameCycleClock;
                    R.internalFlag |= 0b10000000;
                }
                R.internalFlag ^= 0b00000001;
                break;
            case 0x2007: { // write VRAM
                if (addr < 0x2000) {
                    M.pattern[addr / 0x1000][addr & 0xFFF] = value;
                } else if (addr < 0x3000) {
                    M.nameBuffer[addr / 0x400][addr & 0x3FF] = value;
                    if (isprint(value)) fprintf(stderr, "%c", value); // 暫定処理
                } else if (addr < 0x3F00) {
                    addr -= 0x1000;
                    M.nameBuffer[addr / 0x400][addr & 0x3FF] = value;
                    if (isprint(value)) fprintf(stderr, "%c", value); // 暫定処理
                } else if (addr < 0x4000) {
                    M.palette[addr & 0x1F] = value;
                }
                R.vramAddr += R.ctrl & 0b00000100 ? 32 : 1;
                break;
            }
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
                    if (R.ctrl & 0x80) cpu->NMI();
                    CB.endOfFrame(CB.arg);
                    return;
                case 261: // pre-render scanline
                    R.status &= 0b01111111;
                    return;
            }
        }
        // draw pixel
        if (R.line < 240) {
            int pixel = R.clock % 341;
            if (pixel < 256) {
                display[R.line * 256 + pixel] = _bgPixelOf(pixel, R.line);
            }
        }
    }

  private:
    inline unsigned char _bgPixelOf(int x, int y)
    {
        return 0;
    }
};

#endif // INCLUDE_PPU_HPP
