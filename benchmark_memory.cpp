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

#define UNROLL_DEPTH 3

static void BM_OptimizedMemoryAccess(benchmark::State& state)
{
    for (auto _ : state)
    {
        int acc[UNROLL_DEPTH];

        for (int i = 0; i < UNROLL_DEPTH; i++)
        {
            acc[i] = 0;
        }
        // sum unrolled
        for (int i = 0; i < ARRAY_SIZE; i += UNROLL_DEPTH)
        {
            for (int j = 0; j < UNROLL_DEPTH; j++)
            {
                acc[j] += A[i + j];
            }
        }

        // go through the remaining elements
        for (int i = ARRAY_SIZE - ARRAY_SIZE % UNROLL_DEPTH; i < ARRAY_SIZE; i++)
        {
            acc[0] += A[i];
        }

        int sum = 0;
        for (int i = 0; i < UNROLL_DEPTH; i++)
        {
            sum += acc[i];
        }

        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_OptimizedMemoryAccess);
BENCHMARK_MAIN();