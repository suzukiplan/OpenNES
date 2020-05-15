all: build 
	make exec-test RP=test/rom/branch_timing_tests RF=1.Branch_Basics FR=60 BR=E4F0
	make exec-test RP=test/rom/branch_timing_tests RF=2.Backward_Branch FR=60 BR=E4F0
	make exec-test RP=test/rom/branch_timing_tests RF=3.Forward_Branch FR=60 BR=E4F0
	make exec-test RP=test/rom/cpu_dummy_reads RF=cpu_dummy_reads FR=60 BR=E372
	make exec-test RP=test/rom/cpu_dummy_writes RF=cpu_dummy_writes_oam FR=60 BR=0

build: src/M6502/m6502.hpp nestest test/results

exec-test:
	./nestest $(RP)/$(RF).nes $(FR) $(BR) test/results/$(RF).bmp > result_$(RF).log
	sips -s format png test/results/$(RF).bmp -o test/results/$(RF).png
	rm test/results/$(RF).bmp
	rm result_$(RF).log

src/M6502/m6502.hpp:
	git submodule init
	git submodule update

OpenNES.o: Makefile src/OpenNES.cpp src/OpenNES.h src/mmu.hpp src/ppu.hpp src/apu.hpp src/M6502/m6502.hpp
	clang++ -std=c++14 -O -c src/OpenNES.cpp

nestest: Makefile OpenNES.o test/cli/nestest.cpp
	clang++ -std=c++14 -O -o nestest test/cli/nestest.cpp OpenNES.o

test/results:
	mkdir test/results
