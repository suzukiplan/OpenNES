#include "../../src/OpenNES.h"
#include <ctype.h>

int main(int argc, char* argv[])
{
    if (argc < 3) {
        puts("usage: nestest rom-file frames");
        return 1;
    }
    OpenNES nes(true, OpenNES::ColorMode::RGB555);
    nes.enableDebug();
    if (!nes.loadRomFile(argv[1])) {
        puts("loadRom failed");
        return 2;
    }
    nes.reset();
    for (int i = 0; i < atoi(argv[2]); i++) {
        nes.tick(0, 0);
    }
    return 0;
}