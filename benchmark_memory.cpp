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

#pragma GCC optimize("Ofast")
static void BM_OptimizedMemoryAccess(benchmark::State& state)
{
    for (auto _ : state)
    {
        int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
        int sum4 = 0, sum5 = 0, sum6 = 0, sum7 = 0;

        // main loop with unrolling factor of 8
        int limit = ARRAY_SIZE / 8 * 8;
        for (int i = 0; i < limit; i += 8)
        {
            sum0 += A[i];
            sum1 += A[i + 1];
            sum2 += A[i + 2];
            sum3 += A[i + 3];
            sum4 += A[i + 4];
            sum5 += A[i + 5];
            sum6 += A[i + 6];
            sum7 += A[i + 7];
        }

        // summing up all the accumulators
        int sum = sum0 + sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7;

        // handle any remaining elements after the last full unroll iteration
        for (int i = limit; i < ARRAY_SIZE; i++)
        {
            sum += A[i];
        }

        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_OptimizedMemoryAccess);
BENCHMARK_MAIN();