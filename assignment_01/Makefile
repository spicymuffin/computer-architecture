main:
	g++ -g -O0 -fverbose-asm -S -o benchmark_memory.s benchmark_memory.cpp
clean:
	rm -f benchmark_memory.s
	rm -f benchmark
benchmark: benchmark_memory.cpp
	g++ -o benchmark benchmark_memory.cpp -lbenchmark -lpthread
run:
	./benchmark