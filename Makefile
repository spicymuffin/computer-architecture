main:
	g++ -S -o benchmark_memory.s benchmark_memory.cpp
clean:
	rm -f benchmark_memory.s
	rm -f benchmark
benchmark:
	g++ -o benchmark benchmark_memory.cpp -lbenchmark -lpthread -O3
	./benchmark