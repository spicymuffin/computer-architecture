#include <benchmark/benchmark.h>
#include <vector>
#include <iostream>

constexpr int ARRAY_SIZE = 1000000;
std::vector<int> A(ARRAY_SIZE, 1);

static void BM_MemoryAccess(benchmark::State& state)
{
    for (auto _ : state)
    {
        int sum = 0;
        for (int i = 0; i < ARRAY_SIZE; i++)
        {
            sum += A[i];
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_MemoryAccess);

static void BM_OptimizedMemoryAccess(benchmark::State& state)
{
    for (auto _ : state)
    {
        int sum = 0;
        for (int i = 0; i < ARRAY_SIZE; i++)
        {
            // Fill here!!
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_OptimizedMemoryAccess);
BENCHMARK_MAIN();