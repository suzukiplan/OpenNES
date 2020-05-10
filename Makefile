all: nestest
	./nestest test/rom/branch_timing_tests/1.Branch_Basics.nes 3 >result_1.Branch_Basics.log

OpenNES.o: src/OpenNES.cpp src/OpenNES.h src/mmu.hpp src/ppu.hpp src/apu.hpp src/M6502/m6502.hpp
	clang++ -std=c++14 -c src/OpenNES.cpp

nestest: OpenNES.o test/cli/nestest.cpp
	clang++ -std=c++14 -o nestest test/cli/nestest.cpp OpenNES.o
