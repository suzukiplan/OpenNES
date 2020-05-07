all: OpenNES.o

OpenNES.o: src/OpenNES.cpp src/OpenNES.h src/mmu.hpp src/ppu.hpp src/apu.hpp src/M6502/m6502.hpp
	clang++ -std=c++14 -c src/OpenNES.cpp

