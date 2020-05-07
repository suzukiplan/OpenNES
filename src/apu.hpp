// SUZUKI PLAN - OpenNES (GPLv3)
#ifndef INCLUDE_APU_HPP
#define INCLUDE_APU_HPP
#include "OpenNES.h"

class APU
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

#endif // INCLUDE_APU_HPP
