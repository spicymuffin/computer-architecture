datapath: datapath.cpp
	g++ -std=c++17 -o datapath datapath.cpp

datapath_debug: datapath.cpp
	g++ -std=c++17 -o datapath_debug datapath.cpp -DFILE_REDIRECT -DPRINT_DISASM_INSTR_STREAM -DPRINT_BRANCH_STATUS -DEXTENDED_INSTRUCTION_SET -DRUNTIME_DEBUG

run: datapath
	./datapath > run_output.txt

debug: datapath_debug
	./datapath_debug > debug_output.txt

clean:
	rm -f datapath datapath_debug
