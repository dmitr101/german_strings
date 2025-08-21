#include <iostream>
#include <string>
#include <random>
#include <vector>
#include <algorithm>
#include <chrono>

#include <benchmark/benchmark.h>

#include "german_string.h"

constexpr auto SMALL_KNOWN_STRING = "Hello World";
constexpr auto MEDIUM_KNOWN_STRING = "The quick brown fox jumps over the lazy dog and then continues running through the forest.";
constexpr auto LARGE_KNOWN_STRING = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

// Common string patterns for realistic benchmarks
const std::vector<std::string> COMMON_PREFIXES = {
    "https://", "http://", "file://", "data:",
    "user_", "admin_", "guest_", "system_",
    "GET ", "POST ", "PUT ", "DELETE ",
    "ERROR:", "WARNING:", "INFO:", "DEBUG:",
    "/home/", "/usr/", "/var/", "/tmp/"};

const std::vector<std::string> COMMON_SUFFIXES = {
    ".txt", ".cpp", ".h", ".json", ".xml",
    "_backup", "_temp", "_old", "_new",
    "?query=1", "&param=value", "#section",
    ".log", ".dat", ".bin"};

template <typename StringType>
std::vector<StringType> generate_random_strings(size_t count, uint32_t min_length, uint32_t max_length, uint32_t seed)
{
    std::vector<StringType> strings;
    strings.reserve(count);
    std::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+[]{}|;:,.<>?/-_";
    std::mt19937 generator(seed);
    std::uniform_int_distribution<> length_distribution(min_length, max_length);
    std::uniform_int_distribution<> char_distribution(0, static_cast<int>(alphabet.size() - 1));

    constexpr float small_known_string_probability = 0.08f;
    constexpr float medium_known_string_probability = 0.05f;
    constexpr float large_known_string_probability = 0.03f;
    constexpr float common_pattern_probability = 0.15f;
    std::uniform_real_distribution<> known_string_distribution(0.0f, 1.0f);
    std::uniform_int_distribution<> prefix_distribution(0, static_cast<int>(COMMON_PREFIXES.size() - 1));
    std::uniform_int_distribution<> suffix_distribution(0, static_cast<int>(COMMON_SUFFIXES.size() - 1));

    for (size_t i = 0; i < count; ++i)
    {
        float rand_val = known_string_distribution(generator);

        if (rand_val < small_known_string_probability)
        {
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                strings.emplace_back(SMALL_KNOWN_STRING);
            }
            else
            {
                strings.emplace_back(SMALL_KNOWN_STRING, static_cast<uint32_t>(std::strlen(SMALL_KNOWN_STRING)), gs::persistent_t{});
            }
            continue;
        }
        else if (rand_val < small_known_string_probability + medium_known_string_probability)
        {
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                strings.emplace_back(MEDIUM_KNOWN_STRING);
            }
            else
            {
                strings.emplace_back(MEDIUM_KNOWN_STRING, static_cast<uint32_t>(std::strlen(MEDIUM_KNOWN_STRING)), gs::persistent_t{});
            }
            continue;
        }
        else if (rand_val < small_known_string_probability + medium_known_string_probability + large_known_string_probability)
        {
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                strings.emplace_back(LARGE_KNOWN_STRING);
            }
            else
            {
                strings.emplace_back(LARGE_KNOWN_STRING, static_cast<uint32_t>(std::strlen(LARGE_KNOWN_STRING)), gs::persistent_t{});
            }
            continue;
        }
        else if (rand_val < small_known_string_probability + medium_known_string_probability + large_known_string_probability + common_pattern_probability)
        {
            // Generate strings with common patterns
            std::string pattern_string = COMMON_PREFIXES[prefix_distribution(generator)];
            size_t remaining_length = std::max(0, static_cast<int>(length_distribution(generator)) - static_cast<int>(pattern_string.length()));
            for (size_t j = 0; j < remaining_length; ++j)
            {
                pattern_string += alphabet[char_distribution(generator)];
            }
            if (rand_val > 0.5f) // 50% chance to add suffix
            {
                pattern_string += COMMON_SUFFIXES[suffix_distribution(generator)];
            }

            if constexpr (std::is_same_v<StringType, std::string>)
            {
                strings.emplace_back(pattern_string);
            }
            else
            {
                strings.emplace_back(pattern_string.c_str(), static_cast<uint32_t>(pattern_string.length()), gs::temporary_t{});
            }
            continue;
        }

        // Generate completely random string
        std::string new_built_string;
        size_t length = length_distribution(generator);
        for (size_t j = 0; j < length; ++j)
        {
            new_built_string += alphabet[char_distribution(generator)];
        }

        if constexpr (std::is_same_v<StringType, std::string>)
        {
            strings.emplace_back(new_built_string);
        }
        else
        {
            strings.emplace_back(new_built_string.c_str(), static_cast<uint32_t>(new_built_string.length()), gs::temporary_t{});
        }
    }
    return strings;
}

template <typename StringType>
void StringStartsWithComparison(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t min_length = static_cast<uint32_t>(state.range(1));
    uint32_t max_length = static_cast<uint32_t>(state.range(2));
    uint32_t seed = static_cast<uint32_t>(state.range(3));

    auto strings = generate_random_strings<StringType>(count, min_length, max_length, seed);

    for (auto _ : state)
    {
        size_t starts_with_count = 0;
        for (const auto &str : strings)
        {
            if (str.starts_with(StringType{"https://"}))
            {
                ++starts_with_count;
            }
        }
        benchmark::DoNotOptimize(starts_with_count);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * count);
}

//BENCHMARK_TEMPLATE(StringStartsWithComparison, std::string)
//    ->Args({1000, 8, 1024, 42})
//    ->Args({10000, 8, 1024, 42})
//    ->Args({100000, 8, 1024, 42})
//    ->Args({500000, 8, 1024, 42})
//    ->Args({1000000, 8, 1024, 42});
//
//BENCHMARK_TEMPLATE(StringStartsWithComparison, gs::german_string)
//    ->Args({1000, 8, 1024, 42})
//    ->Args({10000, 8, 1024, 42})
//    ->Args({100000, 8, 1024, 42})
//    ->Args({500000, 8, 1024, 42})
//    ->Args({1000000, 8, 1024, 42});

template <typename StringType>
void StringEqualityComparison(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t min_length = static_cast<uint32_t>(state.range(1));
    uint32_t max_length = static_cast<uint32_t>(state.range(2));
    uint32_t seed = static_cast<uint32_t>(state.range(3));

    auto strings = generate_random_strings<StringType>(count, min_length, max_length, seed);

    for (auto _ : state)
    {
        size_t equal_count = 0;
        for (size_t i = 0; i < strings.size(); i += 2)
        {
            if (i + 1 < strings.size() && strings[i] == strings[i + 1])
            {
                ++equal_count;
            }
        }
        benchmark::DoNotOptimize(equal_count);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * (strings.size() / 2));
}

BENCHMARK_TEMPLATE(StringEqualityComparison, std::string)
    ->Args({1000, 8, 1024, 42})
    ->Args({10000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42})
    ->Args({500000, 8, 1024, 42})
    ->Args({1000000, 8, 1024, 42});

BENCHMARK_TEMPLATE(StringEqualityComparison, gs::german_string)
    ->Args({1000, 8, 1024, 42})
    ->Args({10000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42})
    ->Args({500000, 8, 1024, 42})
    ->Args({1000000, 8, 1024, 42});

template <typename StringType>
void StringLexicographicComparison(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t min_length = static_cast<uint32_t>(state.range(1));
    uint32_t max_length = static_cast<uint32_t>(state.range(2));
    uint32_t seed = static_cast<uint32_t>(state.range(3));

    auto strings = generate_random_strings<StringType>(count, min_length, max_length, seed);

    for (auto _ : state)
    {
        size_t less_count = 0;
        for (size_t i = 0; i < strings.size(); i += 2)
        {
            if (i + 1 < strings.size() && strings[i] < strings[i + 1])
            {
                ++less_count;
            }
        }
        benchmark::DoNotOptimize(less_count);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * (strings.size() / 2));
}

//BENCHMARK_TEMPLATE(StringLexicographicComparison, std::string)
//    ->Args({1000, 8, 128, 42})
//    ->Args({10000, 8, 128, 42})
//    ->Args({100000, 8, 128, 42})
//    ->Args({500000, 8, 128, 42})
//    ->Args({1000000, 8, 128, 42});
//
//BENCHMARK_TEMPLATE(StringLexicographicComparison, gs::german_string)
//    ->Args({1000, 8, 128, 42})
//    ->Args({10000, 8, 128, 42})
//    ->Args({100000, 8, 128, 42})
//    ->Args({500000, 8, 128, 42})
//    ->Args({1000000, 8, 128, 42});

template <typename StringType>
void StringSorting(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t min_length = static_cast<uint32_t>(state.range(1));
    uint32_t max_length = static_cast<uint32_t>(state.range(2));
    uint32_t seed = static_cast<uint32_t>(state.range(3));

    // Generate the strings once, outside the benchmark loop (like the original)
    auto strings = generate_random_strings<StringType>(count, min_length, max_length, seed);

    for (auto _ : state)
    {
        // Sort forward and backward like the original benchmark for more stable measurements
        std::sort(strings.begin(), strings.end());
        benchmark::DoNotOptimize(strings);
        std::sort(strings.rbegin(), strings.rend());
        benchmark::DoNotOptimize(strings);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK_TEMPLATE(StringSorting, std::string)
    ->Args({1000, 8, 128, 42})
    ->Args({10000, 8, 128, 42})
    ->Args({50000, 8, 128, 42})
    ->Args({100000, 8, 128, 42})
    ->Args({200000, 8, 128, 42});

BENCHMARK_TEMPLATE(StringSorting, gs::german_string)
    ->Args({1000, 8, 128, 42})
    ->Args({10000, 8, 128, 42})
    ->Args({50000, 8, 128, 42})
    ->Args({100000, 8, 128, 42})
    ->Args({200000, 8, 128, 42});

template <typename StringType>
void StringComparisonByLength(benchmark::State &state)
{
    uint32_t length = static_cast<uint32_t>(state.range(0));
    uint32_t seed = static_cast<uint32_t>(state.range(1));

    auto strings = generate_random_strings<StringType>(2000, length, length, seed);

    for (auto _ : state)
    {
        size_t equal_count = 0;
        for (size_t i = 0; i < strings.size(); i += 2)
        {
            if (i + 1 < strings.size() && strings[i] == strings[i + 1])
            {
                ++equal_count;
            }
        }
        benchmark::DoNotOptimize(equal_count);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * (strings.size() / 2));
    state.SetLabel("length=" + std::to_string(length));
}

//BENCHMARK_TEMPLATE(StringComparisonByLength, std::string)
//    ->Arg(4)
//    ->Arg(8)
//    ->Arg(12)
//    ->Arg(16)
//    ->Arg(32)
//    ->Arg(64)
//    ->Arg(128)
//    ->Arg(256)
//    ->Arg(512)
//    ->Arg(1024)
//    ->Arg(2048);
//
//BENCHMARK_TEMPLATE(StringComparisonByLength, gs::german_string)
//    ->Arg(4)
//    ->Arg(8)
//    ->Arg(12)
//    ->Arg(16)
//    ->Arg(32)
//    ->Arg(64)
//    ->Arg(128)
//    ->Arg(256)
//    ->Arg(512)
//    ->Arg(1024)
//    ->Arg(2048);

template <typename StringType>
void StringHashingByLength(benchmark::State& state)
{
    uint32_t length = static_cast<uint32_t>(state.range(0));
    uint32_t seed = static_cast<uint32_t>(state.range(1));

    auto strings = generate_random_strings<StringType>(2000, length, length, seed);

    for (auto _ : state)
    {
        uint64_t ResultingHash = 0;
        for (size_t i = 0; i < strings.size(); i += 2)
        {
			ResultingHash = ResultingHash ^ std::hash<StringType>{}(strings[i]);
        }
        benchmark::DoNotOptimize(ResultingHash);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * (strings.size() / 2));
    state.SetLabel("length=" + std::to_string(length));
}

BENCHMARK_TEMPLATE(StringHashingByLength, std::string)
->Arg(12)
->Arg(16)
->Arg(64)
->Arg(256)
->Arg(512)
->Arg(1024)
->Arg(2048);

BENCHMARK_TEMPLATE(StringHashingByLength, gs::german_string)
->Arg(12)
->Arg(16)
->Arg(64)
->Arg(256)
->Arg(512)
->Arg(1024)
->Arg(2048);

BENCHMARK_MAIN();
