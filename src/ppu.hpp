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
    bool dipswIgnoreMirroring;
    bool dipswMirroring;

  public:
    unsigned char display[256 * 240]; // NES pallete display
    struct VideoMemory {
        unsigned char pattern[2][0x1000];
        unsigned char name[4]; // 0: LeftTop, 1: RightTop, 2: LeftBottom, 3: RightBottom
        unsigned char nameBuffer[4][0x400];
        unsigned char palette[8][4];
        unsigned char oam[0x100];
        unsigned char vramAddrTmp[2];
        int vramAddrUpdateClock;
    } M;

    // Temporarily stores work information obtained from ctrl and mask.
    // Store to $ 2000, $ 2001, or recalculate on state load.
    // Note: This data does not require state saving.
    struct WorkAreaCtrl {
        int baseNameTableIndex;   // Base nametable index (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
        int vramIncrement;        // VRAM address increment per CPU read/write of PPUDATA (1 or 32)
        int spritePatternIndex;   // Sprite pattern table index for 8x8 sprites (0 or 1 / ignored in 8x16 mode)
        int bgPatternIndex;       // BG pattern table index
        bool isSprite8x16;        // Sprite size (false: 8x8 pixels; true: 8x16 pixels)
        int ppuMasterSlaveSelect; // PPU master/slave select (0: read backdrop from EXT pins; 1: output color on EXT pins)
        bool generateNMI;         // Generate an NMI at the start of the vertical blanking interval (false: off; true: on)
    };
    struct WorkArea {
        struct WorkAreaCtrl ctrl;
    } W;

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
        this->dipswIgnoreMirroring = rom->ignoreMirroring;
        this->dipswMirroring = rom->mirroring;
        _updateWorkAreaCtrl();
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
                R.ctrl = value;
                _updateWorkAreaCtrl();
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
                if (R.vramAddr < 0x2000) {
                    M.pattern[R.vramAddr / 0x1000][R.vramAddr & 0xFFF] = value;
                } else if (R.vramAddr < 0x3F00) {
                    M.nameBuffer[(R.vramAddr & 0xFFF) / 0x400][R.vramAddr & 0x3FF] = value;
                } else if (R.vramAddr < 0x4000) {
                    M.palette[(R.vramAddr & 0x1F) / 4][R.vramAddr & 0x3] = value;
                }
                R.vramAddr += W.ctrl.vramIncrement;
                R.vramAddr &= 0x3FFF;
                break;
            }
        }
    }

    inline void tick(M6502* cpu)
    {
        R.clock++;
        R.clock %= frameCycleClock;
        const int pixel = R.clock % 341;
        if (R.internalFlag & 0b10000000 && R.clock == M.vramAddrUpdateClock) {
            R.internalFlag &= 0b01111111;
            R.vramAddr = M.vramAddrTmp[0] * 256 + M.vramAddrTmp[1];
            R.vramAddr &= 0x3FFF;
        }
        // update current line
        if (R.line != R.clock / 341) {
            R.line = R.clock / 341;
            switch (R.line) {
                case 0:
                    // clear display to the sprite palette #0 color 0
                    memset(display, M.palette[4][0], sizeof(display));
                    break;
                case 241: // start vertical blanking line
                    R.status |= 0b10000000;
                    if (W.ctrl.generateNMI) cpu->NMI();
                    CB.endOfFrame(CB.arg);
                    return;
                case 261: // pre-render scanline
                    R.status &= 0b01111111;
                    return;
            }
        }
        // draw BG pixel if current position is in (0, 0) until (255, 239)
        if (R.line < 240) {
            if (pixel < 256) {
                unsigned char c = _bgPixelOf(pixel, R.line);
                if (0 == (c & 0x80)) {
                    display[R.line * 256 + pixel] = c;
                }
            }
        }
    }

  private:
    inline unsigned char _bgPixelOf(int x, int y)
    {
        x += R.scroll[0];
        y += R.scroll[1];
        int nameIndex = 0;
        if (255 < x) nameIndex += 1;
        if (255 < y) nameIndex += 2;
        x &= 0xFF;
        y &= 0xFF;
        int namePtr = x / 8;
        namePtr += y / 8 * 32;
        int pattern = M.nameBuffer[M.name[nameIndex]][namePtr] * 16;
        x &= 0b0111;
        int bit = 0b10000000 >> x;
        y &= 0b0111;
        unsigned char colorU = M.pattern[W.ctrl.bgPatternIndex][pattern + y] & bit ? 0x02 : 0x00;
        unsigned char colorL = M.pattern[W.ctrl.bgPatternIndex][pattern + 8 + y] & bit ? 0x01 : 0x00;
        unsigned char color = colorU | colorL;
        // TODO: acquire 2bit palette index from attribute area (temporarily always use BG palette 0)
        return color ? M.palette[0][color] : 0x80;
    }

    inline void _updateWorkAreaCtrl()
    {
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
        W.ctrl.baseNameTableIndex = R.ctrl & 0b00000011;
        W.ctrl.vramIncrement = R.ctrl & 0b00000100 ? 32 : 1;
        W.ctrl.spritePatternIndex = R.ctrl & 0b00001000 ? 1 : 0;
        W.ctrl.bgPatternIndex = R.ctrl & 0b00010000 ? 1 : 0;
        W.ctrl.isSprite8x16 = R.ctrl & 0b00100000 ? true : false;
        W.ctrl.ppuMasterSlaveSelect = R.ctrl & 0b01000000 ? 1 : 0;
        W.ctrl.generateNMI = R.ctrl & 0b10000000 ? true : false;

        // update name table index
        if (dipswIgnoreMirroring) {
            // 4 screen
            M.name[0] = W.ctrl.baseNameTableIndex;
            M.name[1] = (W.ctrl.baseNameTableIndex + 1) & 0b00000011;
            M.name[2] = (W.ctrl.baseNameTableIndex + 2) & 0b00000011;
            M.name[3] = (W.ctrl.baseNameTableIndex + 3) & 0b00000011;
        } else if (dipswMirroring) {
            // vertical mirroring
            M.name[0] = W.ctrl.baseNameTableIndex;
            M.name[1] = (W.ctrl.baseNameTableIndex + 1) & 0b00000011;
            M.name[2] = W.ctrl.baseNameTableIndex;
            M.name[3] = (W.ctrl.baseNameTableIndex + 1) & 0b00000011;
        } else {
            // horizontal mirroring
            M.name[0] = W.ctrl.baseNameTableIndex;
            M.name[1] = W.ctrl.baseNameTableIndex;
            M.name[2] = (W.ctrl.baseNameTableIndex + 1) & 0b00000011;
            M.name[3] = (W.ctrl.baseNameTableIndex + 1) & 0b00000011;
        }
    }
};

#endif // INCLUDE_PPU_HPP
