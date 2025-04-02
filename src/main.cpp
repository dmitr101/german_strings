#include <iostream>
#include <string>

#include <benchmark/benchmark.h>

#include "german_string.h"

void TestBench(benchmark::State &state)
{
    for (auto _ : state)
    {
        // This function is not optimized, so it will be slow
        std::string str = "Hallo, wie geht's?";
    }
}

BENCHMARK_MAIN();