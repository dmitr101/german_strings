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

// 1. Construction Benchmarks
template <typename StringType>
void StringConstruction(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t string_length = static_cast<uint32_t>(state.range(1));
    uint32_t seed = static_cast<uint32_t>(state.range(2));

    auto random_strings = generate_random_strings<std::string>(count, string_length, string_length, seed);

    for (auto _ : state)
    {
        std::vector<StringType> strings;
        strings.reserve(count);
        for (const auto &str : random_strings)
        {
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                strings.emplace_back(str);
            }
            else
            {
                strings.emplace_back(str.c_str(), static_cast<uint32_t>(str.length()), gs::temporary_t{});
            }
        }
        benchmark::DoNotOptimize(strings);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * count);
    state.SetBytesProcessed(state.iterations() * count * string_length);
}

BENCHMARK_TEMPLATE(StringConstruction, std::string)
    ->Args({1000, 16, 42})
    ->Args({1000, 64, 42})
    ->Args({1000, 256, 42})
    ->Args({10000, 16, 42})
    ->Args({10000, 64, 42});

BENCHMARK_TEMPLATE(StringConstruction, gs::german_string)
    ->Args({1000, 16, 42})
    ->Args({1000, 64, 42})
    ->Args({1000, 256, 42})
    ->Args({10000, 16, 42})
    ->Args({10000, 64, 42});

// 2. Move Construction Benchmarks (since copy construction is not available)
template <typename StringType>
void StringMoveConstruction(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t string_length = static_cast<uint32_t>(state.range(1));
    uint32_t seed = static_cast<uint32_t>(state.range(2));

    for (auto _ : state)
    {
        std::vector<StringType> source_strings;
        source_strings.reserve(count);

        // First create source strings
        auto random_strings = generate_random_strings<std::string>(count, string_length, string_length, seed);
        for (const auto &str : random_strings)
        {
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                source_strings.emplace_back(str);
            }
            else
            {
                source_strings.emplace_back(str.c_str(), static_cast<uint32_t>(str.length()), gs::temporary_t{});
            }
        }

        // Then move them
        std::vector<StringType> moved_strings;
        moved_strings.reserve(count);
        for (auto &str : source_strings)
        {
            moved_strings.emplace_back(std::move(str));
        }

        benchmark::DoNotOptimize(moved_strings);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * count);
    state.SetBytesProcessed(state.iterations() * count * string_length);
}

BENCHMARK_TEMPLATE(StringMoveConstruction, std::string)
    ->Args({1000, 16, 42})
    ->Args({1000, 256, 42})
    ->Args({10000, 64, 42});

BENCHMARK_TEMPLATE(StringMoveConstruction, gs::german_string)
    ->Args({1000, 16, 42})
    ->Args({1000, 256, 42})
    ->Args({10000, 64, 42});

// 3. String Equality Comparison Performance
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
    ->Args({100000, 8, 1024, 42});

BENCHMARK_TEMPLATE(StringEqualityComparison, gs::german_string)
    ->Args({1000, 8, 1024, 42})
    ->Args({10000, 8, 1024, 42})
    ->Args({100000, 8, 1024, 42});

// 4. String Comparison (Lexicographic Ordering)
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

BENCHMARK_TEMPLATE(StringLexicographicComparison, std::string)
    ->Args({1000, 8, 128, 42})
    ->Args({10000, 8, 128, 42})
    ->Args({100000, 8, 128, 42});

BENCHMARK_TEMPLATE(StringLexicographicComparison, gs::german_string)
    ->Args({1000, 8, 128, 42})
    ->Args({10000, 8, 128, 42})
    ->Args({100000, 8, 128, 42});

// 5. String Sorting Performance
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
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK_TEMPLATE(StringSorting, std::string)
    ->Args({1000, 8, 128, 42})
    ->Args({10000, 8, 128, 42})
    ->Args({50000, 8, 128, 42});

BENCHMARK_TEMPLATE(StringSorting, gs::german_string)
    ->Args({1000, 8, 128, 42})
    ->Args({10000, 8, 128, 42})
    ->Args({50000, 8, 128, 42});

// 6. String Length Scenarios
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

BENCHMARK_TEMPLATE(StringComparisonByLength, std::string)
    ->Arg(4)
    ->Arg(8)
    ->Arg(12)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512)
    ->Arg(1024);

BENCHMARK_TEMPLATE(StringComparisonByLength, gs::german_string)
    ->Arg(4)
    ->Arg(8)
    ->Arg(12)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512)
    ->Arg(1024);

// 7. String Class Type Comparison (German String specific)
void GermanStringClassComparison(benchmark::State &state)
{
    size_t count = state.range(0);
    uint32_t string_length = static_cast<uint32_t>(state.range(1));
    uint32_t seed = static_cast<uint32_t>(state.range(2));
    int string_class_type = static_cast<int>(state.range(3)); // 0=temporary, 1=persistent, 2=transient

    auto template_strings = generate_random_strings<std::string>(count, string_length, string_length, seed);

    for (auto _ : state)
    {
        std::vector<gs::german_string> strings;
        strings.reserve(count);

        for (const auto &str : template_strings)
        {
            switch (string_class_type)
            {
            case 0:
                strings.emplace_back(str.c_str(), static_cast<uint32_t>(str.length()), gs::temporary_t{});
                break;
            case 1:
                strings.emplace_back(str.c_str(), static_cast<uint32_t>(str.length()), gs::persistent_t{});
                break;
            case 2:
                strings.emplace_back(str.c_str(), static_cast<uint32_t>(str.length()), gs::transient_t{});
                break;
            }
        }
        benchmark::DoNotOptimize(strings);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * count);
    state.SetLabel(string_class_type == 0 ? "temporary" : string_class_type == 1 ? "persistent"
                                                                                 : "transient");
}

BENCHMARK(GermanStringClassComparison)
    ->Args({1000, 64, 42, 0})   // temporary
    ->Args({1000, 64, 42, 1})   // persistent
    ->Args({1000, 64, 42, 2})   // transient
    ->Args({10000, 64, 42, 0})  // temporary
    ->Args({10000, 64, 42, 1})  // persistent
    ->Args({10000, 64, 42, 2}); // transient

// 8. Prefix Comparison Benchmark (German String optimization)
void GermanStringPrefixComparison(benchmark::State &state)
{
    uint32_t string_length = static_cast<uint32_t>(state.range(0));
    uint32_t shared_prefix_length = static_cast<uint32_t>(state.range(1));
    uint32_t seed = static_cast<uint32_t>(state.range(2));

    std::string prefix(shared_prefix_length, 'A');
    std::string suffix1(string_length - shared_prefix_length, 'B');
    std::string suffix2(string_length - shared_prefix_length, 'C');

    std::string str1 = prefix + suffix1;
    std::string str2 = prefix + suffix2;

    gs::german_string gs_str1(str1.c_str(), static_cast<uint32_t>(str1.length()), gs::persistent_t{});
    gs::german_string gs_str2(str2.c_str(), static_cast<uint32_t>(str2.length()), gs::persistent_t{});

    for (auto _ : state)
    {
        for (int i = 0; i < 1000; ++i)
        {
            bool result = (gs_str1 < gs_str2);
            benchmark::DoNotOptimize(result);
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * 1000);
    state.SetLabel("prefix=" + std::to_string(shared_prefix_length) + "/" + std::to_string(string_length));
}

BENCHMARK(GermanStringPrefixComparison)
    ->Args({16, 0, 42})   // No shared prefix
    ->Args({16, 4, 42})   // 4-byte shared prefix (fits in prefix optimization)
    ->Args({16, 8, 42})   // 8-byte shared prefix
    ->Args({32, 0, 42})   // No shared prefix, longer strings
    ->Args({32, 4, 42})   // 4-byte shared prefix, longer strings
    ->Args({32, 16, 42})  // 16-byte shared prefix, longer strings
    ->Args({64, 0, 42})   // No shared prefix, much longer strings
    ->Args({64, 4, 42})   // 4-byte shared prefix, much longer strings
    ->Args({64, 32, 42}); // 32-byte shared prefix, much longer strings

// 9. Small String Optimization Boundary
void GermanStringSmallStringBoundary(benchmark::State &state)
{
    uint32_t string_length = static_cast<uint32_t>(state.range(0));

    std::string test_string(string_length, 'A');

    for (auto _ : state)
    {
        for (int i = 0; i < 1000; ++i)
        {
            gs::german_string gs_str(test_string.c_str(), string_length, gs::persistent_t{});
            benchmark::DoNotOptimize(gs_str);
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * 1000);
    state.SetLabel(string_length <= 12 ? "small" : "large");
}

BENCHMARK(GermanStringSmallStringBoundary)
    ->Arg(1)
    ->Arg(4)
    ->Arg(8)
    ->Arg(11)
    ->Arg(12)
    ->Arg(13)
    ->Arg(16)
    ->Arg(24)
    ->Arg(32)
    ->Arg(64);

BENCHMARK_MAIN();
