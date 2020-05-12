all: src/M6502/m6502.hpp nestest test/results
	make exec-test RP=test/rom/branch_timing_tests RF=1.Branch_Basics FR=60 BR=E4F0
	make exec-test RP=test/rom/branch_timing_tests RF=2.Backward_Branch FR=60 BR=E4F0
	make exec-test RP=test/rom/branch_timing_tests RF=3.Forward_Branch FR=60 BR=E4F0

exec-test:
	./nestest $(RP)/$(RF).nes $(FR) $(BR) test/results/$(RF).bmp > result_$(RF).log

src/M6502/m6502.hpp:
	git submodule init
	git submodule update

OpenNES.o: src/OpenNES.cpp src/OpenNES.h src/mmu.hpp src/ppu.hpp src/apu.hpp src/M6502/m6502.hpp
	clang++ -std=c++14 -c src/OpenNES.cpp

nestest: OpenNES.o test/cli/nestest.cpp
	clang++ -std=c++14 -o nestest test/cli/nestest.cpp OpenNES.o

test/results:
	mkdir test/results
