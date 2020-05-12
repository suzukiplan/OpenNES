// SUZUKI PLAN - OpenNES (GPLv3)
#include "OpenNES.h"

static const unsigned short _colorTableRGB555[256] = {
    0x3DEF, 0x001F, 0x0017, 0x20B7, 0x4810, 0x5404, 0x5440, 0x4440, 0x28C0, 0x01E0, 0x01A0,
    0x0160, 0x010B, 0x0000, 0x0000, 0x0000, 0x5EF7, 0x01FF, 0x017F, 0x351F, 0x6C19, 0x700B,
    0x7CE0, 0x7162, 0x55E0, 0x02E0, 0x02A0, 0x02A8, 0x0231, 0x0000, 0x0000, 0x0000, 0x7FFF,
    0x1EFF, 0x363F, 0x4DFF, 0x7DFF, 0x7D73, 0x7DEB, 0x7E88, 0x7EE0, 0x5FE3, 0x2F6A, 0x2FF3,
    0x03BB, 0x3DEF, 0x0000, 0x0000, 0x7FFF, 0x539F, 0x5EFF, 0x6EFF, 0x7EFF, 0x7E98, 0x7B56,
    0x7F95, 0x7F6F, 0x6FEF, 0x5FF7, 0x5FFB, 0x03FF, 0x7F7F, 0x0000, 0x0000};

static const unsigned short _colorTableRGB565[256] = {
    0x7BCF, 0x2016, 0x2817, 0x6094, 0x990F, 0xB086, 0xA180, 0x7A00, 0x4AC0, 0x3B40, 0x3B60,
    0x3308, 0x3290, 0x0000, 0x0000, 0x0000, 0xB596, 0x431F, 0x421F, 0x921E, 0xDA18, 0xDA0C,
    0xE280, 0xC380, 0x8C40, 0x5500, 0x4D42, 0x4D0D, 0x4498, 0x0000, 0x0000, 0x0000, 0xFFFF,
    0x651F, 0x541F, 0xA39F, 0xF31F, 0xFB16, 0xFBC6, 0xFD00, 0xEE84, 0x9F40, 0x7788, 0x7712,
    0x669C, 0x7BCF, 0x0000, 0x0000, 0xFFFF, 0x969F, 0xA5DF, 0xC59F, 0xE59F, 0xFDDD, 0xFE57,
    0xFED4, 0xFF92, 0xCF90, 0xA794, 0xA7F9, 0xA7FE, 0xA514, 0x0000, 0x0000};

static const unsigned short* _colorTable;

static unsigned char ppuRead(void* arg, unsigned short addr) { return ((OpenNES*)arg)->ppu->inPort(addr); }
static void ppuWrite(void* arg, unsigned short addr, unsigned char value) { ((OpenNES*)arg)->ppu->outPort(addr, value); }
static unsigned char apuRead(void* arg, unsigned short addr) { return ((OpenNES*)arg)->apu->inPort(addr); }
static void apuWrite(void* arg, unsigned short addr, unsigned char value) { ((OpenNES*)arg)->apu->outPort(addr, value); }
static unsigned char readMemory(void* arg, unsigned short addr) { return ((OpenNES*)arg)->mmu->readMemory(addr); }
static void writeMemory(void* arg, unsigned short addr, unsigned char value) { ((OpenNES*)arg)->mmu->writeMemory(addr, value); }

static void oamdma(void* arg, unsigned char page)
{
    fprintf(stderr, "EXECUTE OAM DMA\n");
    OpenNES* nes = (OpenNES*)arg;
    nes->cpu->consumeClock(nes->cpu->R.tickCount & 1);
    unsigned short addr = page;
    addr <<= 8;
    do {
        unsigned char data = readMemory(nes, addr);
        nes->cpu->consumeClock(1);
        nes->ppu->M.oam[addr & 0xFF] = data;
        nes->cpu->consumeClock(1);
        addr++;
    } while (addr & 0xFF);
    nes->cpu->consumeClock(1);
}

OpenNES::OpenNES(bool isNTSC, ColorMode colorMode)
{
    this->isNTSC = isNTSC;
    this->cpuClockHz = isNTSC ? 1789773 : 1773447;
    switch (colorMode) {
        case ColorMode::RGB555: _colorTable = _colorTableRGB555; break;
        case ColorMode::RGB565: _colorTable = _colorTableRGB565; break;
    }
    this->apu = new APU();
    this->ppu = new PPU();
    ppu->setEndOfFrame(this, [](void* arg) {
        OpenNES* nes = (OpenNES*)arg;
        for (int i = 0; i < 256 * 240; i++) {
            nes->display[i] = _colorTable[nes->ppu->display[i]];
        }
    });
    this->mmu = new MMU(ppuRead, ppuWrite, apuRead, apuWrite, oamdma, this);
    this->cpu = new M6502(M6502_MODE_RP2A03, readMemory, writeMemory, this);
    this->cpu->setConsumeClock([](void* arg) {
        OpenNES* nes = (OpenNES*)arg;
        // execute 3 PPU ticks
        // https://wiki.nesdev.com/w/index.php/PPU_frame_timing
        nes->ppu->tick(nes->cpu);
        nes->ppu->tick(nes->cpu);
        nes->ppu->tick(nes->cpu);
    });
}

OpenNES::~OpenNES()
{
    if (this->cpu) delete this->cpu;
    if (this->mmu) delete this->mmu;
    if (this->ppu) delete this->ppu;
    if (this->apu) delete this->apu;
}

bool OpenNES::loadRom(void* data, size_t size)
{
    bool result = this->mmu ? mmu->loadRom((unsigned char*)data, size) : false;
    if (result) {
        ppu->setup(&mmu->romData, isNTSC);
        this->reset();
    }
    return result;
}

bool OpenNES::loadRomFile(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size < 1) {
        fclose(fp);
        return false;
    }
    unsigned char* data = (unsigned char*)malloc(size);
    if (!data) {
        fclose(fp);
        return false;
    }
    fseek(fp, 0, SEEK_SET);
    if (size != fread(data, 1, size, fp)) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    bool result = loadRom(data, size);
    free(data);
    return result;
}

void OpenNES::reset()
{
    memset(display, 0, sizeof(display));
    if (cpu) cpu->reset();
}

void OpenNES::tick(unsigned char pad1, unsigned char pad2)
{
    if (!cpu || !mmu) return;
    mmu->R.pad[0] = pad1;
    mmu->R.pad[1] = pad2;
    cpu->execute(cpuClockHz / 60);
}