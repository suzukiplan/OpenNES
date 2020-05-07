// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_PPU_HPP
#define INCLUDE_PPU_HPP
#include "OpenNES.h"

class PPU
{
  public:
    unsigned char inPort(unsigned short addr)
    {
        return 0;
    }
    void outPort(unsigned short addr, unsigned char value)
    {
    }
};

#endif // INCLUDE_PPU_HPP
