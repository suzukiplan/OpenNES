// SUZUKI PLAN - OpenNES (GPLv3)
#include "OpenNES.h"

static unsigned char ppuRead(void* arg, unsigned short addr) { return ((OpenNES*)arg)->ppu->inPort(addr); }
static void ppuWrite(void* arg, unsigned short addr, unsigned char value) { ((OpenNES*)arg)->ppu->outPort(addr, value); }
static unsigned char apuRead(void* arg, unsigned short addr) { return ((OpenNES*)arg)->apu->inPort(addr); }
static void apuWrite(void* arg, unsigned short addr, unsigned char value) { ((OpenNES*)arg)->apu->outPort(addr, value); }
static unsigned char readMemory(void* arg, unsigned short addr)
{
    unsigned char c = ((OpenNES*)arg)->mmu->readMemory(addr);
    printf("readMemory: $%04X: $%02X\n", addr, c);
    return c;
}
static void writeMemory(void* arg, unsigned short addr, unsigned char value)
{
    printf("writeMemory: $%04X: $%02X\n", addr, value);
    ((OpenNES*)arg)->mmu->writeMemory(addr, value);
}

OpenNES::OpenNES(bool isNTSC, ColorMode colorMode)
{
    this->isNTSC = isNTSC;
    this->colorMode = colorMode;
    this->apu = new APU();
    this->ppu = new PPU();
    this->mmu = new MMU(ppuRead, ppuWrite, apuRead, apuWrite, this);
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
    if (result) this->reset();
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
    if (cpu) cpu->reset();
}

void OpenNES::tick(unsigned char pad1, unsigned char pad2)
{
    if (!cpu || !mmu) return;
    mmu->reg.pad[0] = pad1;
    mmu->reg.pad[1] = pad2;
    cpu->execute(0, true);
}