all: src/M6502/m6502.hpp nestest test/results
	./nestest test/rom/branch_timing_tests/1.Branch_Basics.nes 60 E4F0 test/results/1.Branch_Basics.bmp >result_1.Branch_Basics.log
	./nestest test/rom/branch_timing_tests/2.Backward_Branch.nes 60 E4F0 test/results/2.Backward_Branch.bmp >result_2.Backward_Branch.log
	./nestest test/rom/branch_timing_tests/3.Forward_Branch.nes 60 E4F0 test/results/3.Forward_Branch.bmp >result_3.Forward_Branch.log

src/M6502/m6502.hpp:
	git submodule init
	git submodule update

OpenNES.o: src/OpenNES.cpp src/OpenNES.h src/mmu.hpp src/ppu.hpp src/apu.hpp src/M6502/m6502.hpp
	clang++ -std=c++14 -c src/OpenNES.cpp

nestest: OpenNES.o test/cli/nestest.cpp
	clang++ -std=c++14 -o nestest test/cli/nestest.cpp OpenNES.o

test/results:
	mkdir test/results

