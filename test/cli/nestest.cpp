#include "../../src/OpenNES.h"
#include <ctype.h>

int hex2i(const char* hex)
{
    int result = 0;
    while (*hex) {
        result += isdigit(*hex) ? (*hex) - '0' : 10 + toupper(*hex) - 'A';
        hex++;
        if (*hex) result <<= 4;
    }
    return result;
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        puts("usage: nestest rom-file frames break");
        return 1;
    }
    OpenNES nes(true, OpenNES::ColorMode::RGB555);
    nes.enableDebug();
    if (!nes.loadRomFile(argv[1])) {
        puts("loadRom failed");
        return 2;
    }
    nes.reset();
    unsigned short breakAddr = hex2i(argv[3]);
    printf("break address: $%04X\n", breakAddr);
    nes.cpu->addBreakPoint(breakAddr, [](void* arg) {
        OpenNES* nes = (OpenNES*)arg;
        printf("DETECT BREAK: $%04X\n", nes->cpu->R.pc);
        exit(0);
    });
    for (int i = 0; i < atoi(argv[2]); i++) {
        nes.tick(0, 0);
    }
    return 0;
}