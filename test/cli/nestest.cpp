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

struct BitmapHeader {
    unsigned int size;
    unsigned int reserved;
    unsigned int offset;
    unsigned int biSize;
    int width;
    int height;
    unsigned short planes;
    unsigned short bitCount;
    unsigned int compression;
    unsigned int sizeImage;
    unsigned int dpmX;
    unsigned int dpmY;
    unsigned int numberOfPalettes;
    unsigned int cir;
};

static char* bitmap;
void displayToBitmap(unsigned short* display)
{
    struct BitmapHeader hed;
    memset(&hed, 0, sizeof(hed));
    hed.offset = sizeof(hed) + 2;
    hed.biSize = 40;
    hed.width = 256;
    hed.height = 240;
    hed.planes = 1;
    hed.bitCount = 32;
    hed.compression = 0;
    hed.sizeImage = 256 * 240 * 4;
    hed.numberOfPalettes = 0;
    hed.cir = 0;
    FILE* fp = fopen(bitmap, "wb");
    if (!fp) return;
    fwrite("BM", 1, 2, fp);
    fwrite(&hed, 1, 52, fp);
    for (int y = 0; y < 240; y++) {
        int posY = 240 - y - 1;
        unsigned int line[256];
        for (int x = 0; x < 256; x++) {
            unsigned short p = display[posY * 256 + x];
            unsigned char r = (p & 0b0111110000000000) >> 7;
            unsigned char g = (p & 0b0000001111100000) >> 2;
            unsigned char b = (p & 0b0000000000011111) << 3;
            line[x] = b;
            line[x] <<= 8;
            line[x] |= g;
            line[x] <<= 8;
            line[x] |= r;
        }
        fwrite(line, 1, sizeof(line), fp);
    }
    fclose(fp);
}

int main(int argc, char* argv[])
{
    if (argc < 5) {
        puts("usage: nestest rom-file frames break bitmap");
        return 1;
    }
    bitmap = argv[4];
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
        displayToBitmap(nes->display);
        exit(0);
    });
    for (int i = 0; i < atoi(argv[2]); i++) {
        nes.tick(0, 0);
    }
    displayToBitmap(nes.display);
    return 0;
}