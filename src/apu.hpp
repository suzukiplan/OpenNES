// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_APU_HPP
#define INCLUDE_APU_HPP
#include "OpenNES.h"

class APU
{
  public:
    unsigned char inPort(unsigned short addr)
    {
        fprintf(stdout, "APU read from $%04X\n", addr); // 暫定処理（後で消す）
        return 0;
    }
    void outPort(unsigned short addr, unsigned char value)
    {
        fprintf(stdout, "APU write $%02X to $%04X\n", value, addr); // 暫定処理（後で消す）
    }
};

#endif // INCLUDE_APU_HPP
