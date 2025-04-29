#include <iostream>
#include <string>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include "german_string.h"

constexpr auto SMALL_KNOWN_STRING = "Hello, World!";
constexpr auto LARGE_KNOWN_STRING = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

template <typename StringType>
std::vector<StringType> generate_random_strings(size_t count, size_t min_length, size_t max_length, uint32_t seed)
{
    std::vector<StringType> strings;
    strings.reserve(count);
    std::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+[]{}|;:,.<>?";
    std::mt19937 generator(seed);
    std::uniform_int_distribution<> length_distribution(min_length, max_length);
    std::uniform_int_distribution<> char_distribution(0, alphabet.size() - 1);

    constexpr float small_known_string_probability = 0.1f;  // 10% chance to generate the small known string
    constexpr float large_known_string_probability = 0.05f; // 5% chance to generate the large known string
    std::uniform_real_distribution<> known_string_distribution(0.0f, 1.0f);

    for (size_t i = 0; i < count; ++i)
    {
        // This is probably statistically wrong, but it works for the test
        if (known_string_distribution(generator) < small_known_string_probability)
        {
            strings.emplace_back(SMALL_KNOWN_STRING);
            continue;
        }
        else if (known_string_distribution(generator) < large_known_string_probability)
        {
            strings.emplace_back(LARGE_KNOWN_STRING);
            continue;
        }

        std::string new_built_string;
        size_t length = length_distribution(generator);
        for (size_t j = 0; j < length; ++j)
        {
            new_built_string += alphabet[char_distribution(generator)];
        }
        strings.emplace_back(new_built_string.c_str());
    }
    return strings;
}

void StdStringCountExisting(benchmark::State &state)
{
    size_t count = state.range(0);
    size_t min_length = state.range(1);
    size_t max_length = state.range(2);
    uint32_t seed = state.range(3);
    auto strings = generate_random_strings<std::string>(count, min_length, max_length, seed);
    for (auto _ : state)
    {
        size_t count_small = 0;
        size_t count_large = 0;
        for (const auto &str : strings)
        {
            if (str == SMALL_KNOWN_STRING)
            {
                ++count_small;
            }
            else if (str == LARGE_KNOWN_STRING)
            {
                ++count_large;
            }
        }
        benchmark::DoNotOptimize(count_small);
        benchmark::DoNotOptimize(count_large);
        benchmark::ClobberMemory();
    }
}

BENCHMARK(StdStringCountExisting)
    ->Args({1000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42});

void GermanStringCountExisting(benchmark::State &state)
{
    size_t count = state.range(0);
    size_t min_length = state.range(1);
    size_t max_length = state.range(2);
    uint32_t seed = state.range(3);
    auto strings = generate_random_strings<gs::german_string>(count, min_length, max_length, seed);

    const gs::german_string small_known_string(SMALL_KNOWN_STRING, std::strlen(SMALL_KNOWN_STRING), gs::persistent_t{});
    const gs::german_string large_known_string(LARGE_KNOWN_STRING, std::strlen(LARGE_KNOWN_STRING), gs::persistent_t{});

    for (auto _ : state)
    {
        size_t count_small = 0;
        size_t count_large = 0;
        for (const auto &str : strings)
        {
            if (str == small_known_string)
            {
                ++count_small;
            }
            else if (str == large_known_string)
            {
                ++count_large;
            }
        }
        benchmark::DoNotOptimize(count_small);
        benchmark::DoNotOptimize(count_large);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(GermanStringCountExisting)
    ->Args({1000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42});

template <typename TString>
void TemplateStringSort(benchmark::State &state)
{
    size_t count = state.range(0);
    size_t min_length = state.range(1);
    size_t max_length = state.range(2);
    uint32_t seed = state.range(3);
    auto strings = generate_random_strings<TString>(count, min_length, max_length, seed);

    for (auto _ : state)
    {
        std::sort(strings.begin(), strings.end());
        benchmark::DoNotOptimize(strings);
        std::sort(strings.rbegin(), strings.rend());
        benchmark::DoNotOptimize(strings);
    }
}

BENCHMARK_TEMPLATE(TemplateStringSort, std::string)
    ->Args({1000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42});

BENCHMARK_TEMPLATE(TemplateStringSort, gs::german_string)
    ->Args({1000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42});
// Benchmarks todo:
// Sorting and array of strings
// Serialization and deserialization, zero copy vs string_view

BENCHMARK_MAIN();