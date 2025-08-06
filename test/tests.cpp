#include <iostream>
#include <random>
#include <string>
#include <limits>
#include <vector>
#include <memory>

#include "german_string.h"

#include <gtest/gtest.h>

constexpr auto SMALL_KNOWN_STRING = "Hello World";
constexpr auto LARGE_KNOWN_STRING = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

template <typename StringType>
std::vector<StringType> generate_random_strings(size_t count, uint32_t min_length, uint32_t max_length, uint32_t seed)
{
    std::vector<StringType> strings;
    strings.reserve(count);
    std::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+[]{}|;:,.<>?";
    std::mt19937 generator(seed);
    std::uniform_int_distribution<> length_distribution(min_length, max_length);
    std::uniform_int_distribution<> char_distribution(0, static_cast<int>(alphabet.size() - 1));

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

struct CountingAllocator : std::allocator<char>
{
    CountingAllocator() = default;

    char *allocate(size_t n)
    {
        ++get_count_allocs();
        return std::allocator<char>::allocate(n);
    }

    void deallocate(char *p, size_t n)
    {
        ++get_count_deallocs();
        std::allocator<char>::deallocate(p, n);
    }

    static void reset()
    {
        get_count_allocs() = 0;
        get_count_deallocs() = 0;
    }

    static size_t &get_count_allocs()
    {
        thread_local size_t count_allocs = 0;
        return count_allocs;
    }

    static size_t &get_count_deallocs()
    {
        thread_local size_t count_deallocs = 0;
        return count_deallocs;
    }
};

// Demonstrate some basic assertions.
TEST(GermanStrings, BasicCorrectness)
{
    size_t count = 1000;
    uint32_t min_length = 8;
    uint32_t max_length = 240;
    uint32_t seed = 42;

    for (size_t outer = 0; outer < 5; ++outer)
    {
        seed = (seed * 18942753) ^ (seed << 5) ^ (seed >> 7); // Random
        auto std_strings = generate_random_strings<std::string>(count, min_length, max_length, seed);
        auto german_strings = generate_random_strings<gs::german_string>(count, min_length, max_length, seed);
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(std_strings[i].length(), german_strings[i].length());
            EXPECT_EQ(std_strings[i].compare(german_strings[i].as_string_view()), 0);
            EXPECT_EQ(std_strings[i], german_strings[i].as_string_view());
        }
    }
}

TEST(GermanStrings, SmallComparison)
{
    gs::german_string small_str1("abc");
    gs::german_string small_str2("bcd");
    gs::german_string small_str3("xyz");
    gs::german_string small_str4("abt");

    EXPECT_FALSE(small_str1 == small_str2);
    EXPECT_FALSE(small_str1 == small_str3);
    EXPECT_FALSE(small_str1 == small_str4);

    auto sv1 = small_str1.as_string_view();
    auto sv2 = small_str2.as_string_view();
    EXPECT_TRUE(sv1 < sv2);
    EXPECT_TRUE(small_str1 < small_str2);
}

TEST(GermanStrings, CompareMethod)
{
    // 1. Basic comparison test cases
    gs::german_string str1("abc");
    gs::german_string str2("abc");
    gs::german_string str3("abcd");
    gs::german_string str4("abd");
    gs::german_string str5("abb");
    gs::german_string empty;
    
    // Equal strings should return 0
    EXPECT_EQ(str1.compare(str2), 0);
    EXPECT_EQ(str2.compare(str1), 0);
    
    // String vs itself should return 0
    EXPECT_EQ(str1.compare(str1), 0);
    
    // Longer string with same prefix should be greater
    EXPECT_LT(str1.compare(str3), 0);  // "abc" < "abcd"
    EXPECT_GT(str3.compare(str1), 0);  // "abcd" > "abc"
    
    // Lexicographically greater string should be greater
    EXPECT_LT(str1.compare(str4), 0);  // "abc" < "abd"
    EXPECT_GT(str4.compare(str1), 0);  // "abd" > "abc"
    
    // Lexicographically smaller string should be smaller
    EXPECT_GT(str1.compare(str5), 0);  // "abc" > "abb"
    EXPECT_LT(str5.compare(str1), 0);  // "abb" < "abc"
    
    // 2. Empty string comparisons
    EXPECT_LT(empty.compare(str1), 0);  // "" < "abc"
    EXPECT_GT(str1.compare(empty), 0);  // "abc" > ""
    EXPECT_EQ(empty.compare(empty), 0); // "" == ""
    
    // 3. SSO edge cases (small string optimization)
    gs::german_string smallStr("Hello");
    gs::german_string mediumStr("Hello, World");      // 12 chars, should be SSO
    gs::german_string largeStr("Hello, World!");      // 13 chars, should not be SSO
    
    EXPECT_LT(smallStr.compare(mediumStr), 0);  // "Hello" < "Hello, World"
    EXPECT_LT(mediumStr.compare(largeStr), 0);  // "Hello, World" < "Hello, World!"
    EXPECT_LT(smallStr.compare(largeStr), 0);   // "Hello" < "Hello, World!"
    
    // 4. Compare with strings of different storage classes
    gs::german_string tempStr(LARGE_KNOWN_STRING, gs::temporary_t{});
    gs::german_string transStr(LARGE_KNOWN_STRING, gs::transient_t{});
    gs::german_string persistStr(LARGE_KNOWN_STRING, gs::persistent_t{});
    
    // All should compare equal regardless of string class
    EXPECT_EQ(tempStr.compare(transStr), 0);
    EXPECT_EQ(tempStr.compare(persistStr), 0);
    EXPECT_EQ(transStr.compare(persistStr), 0);
    
    // 5. Comparing strings with common prefixes
    gs::german_string prefix("Hello");
    gs::german_string withPrefix1("Hello, World");
    gs::german_string withPrefix2("Hello, Alice");
    
    EXPECT_LT(prefix.compare(withPrefix1), 0);     // "Hello" < "Hello, World"
    EXPECT_LT(prefix.compare(withPrefix2), 0);     // "Hello" < "Hello, Alice"
    EXPECT_GT(withPrefix1.compare(withPrefix2), 0); // "Hello, World" > "Hello, Alice"
    
    // 6. Non-ASCII character comparisons
    gs::german_string utf8_1("áéíóú");
    gs::german_string utf8_2("áéíóú");
    gs::german_string utf8_3("áéíóúü");
    
    EXPECT_EQ(utf8_1.compare(utf8_2), 0);  // Equal UTF-8 strings
    EXPECT_LT(utf8_1.compare(utf8_3), 0);  // Shorter UTF-8 string
    
    // 7. Random string comparisons
    size_t count = 100;
    uint32_t min_length = 1;
    uint32_t max_length = 30;
    uint32_t seed = 42;

    auto std_strings = generate_random_strings<std::string>(count, min_length, max_length, seed);
    auto german_strings = generate_random_strings<gs::german_string>(count, min_length, max_length, seed);

    // Verify that german_string.compare matches std::string.compare for the same strings
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            int std_result = std_strings[i].compare(std_strings[j]);
            int german_result = german_strings[i].compare(german_strings[j]);

            // We only care about the sign of the comparison, not the exact value
            if (std_result < 0) {
                EXPECT_LT(german_result, 0);
            } else if (std_result > 0) {
                EXPECT_GT(german_result, 0);
            } else {
                EXPECT_EQ(german_result, 0);
            }
        }
    }
}

TEST(GermanStrings, Sorting)
{
    size_t count = 10;
    uint32_t min_length = 8;
    uint32_t max_length = 240;
    uint32_t seed = 42;
    for (size_t outer = 0; outer < 1; ++outer)
    {
        seed = (seed * 18942753) ^ (seed << 5) ^ (seed >> 7); // Random
        auto std_strings = generate_random_strings<std::string>(count, min_length, max_length, seed);
        std::sort(std_strings.begin(), std_strings.end());
        auto german_strings = generate_random_strings<gs::german_string>(count, min_length, max_length, seed);
        std::sort(german_strings.begin(), german_strings.end());
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(std_strings[i].length(), german_strings[i].length());
            EXPECT_EQ(std_strings[i].compare(german_strings[i].as_string_view()), 0);
            EXPECT_EQ(std_strings[i], german_strings[i].as_string_view());
        }
    }
}

TEST(GermanStrings, Empty)
{
    gs::german_string empty_str;
    EXPECT_TRUE(empty_str.empty());
    EXPECT_EQ(empty_str.length(), 0);
    EXPECT_EQ(empty_str.size(), 0);
    EXPECT_EQ(empty_str.get_class(), gs::string_class::persistent);
}

TEST(GermanStrings, StartsWith) 
{
    using namespace gs::literals;
    auto str1 = "Hello, World!"_gs;
    auto str2 = "Hello"_gs;
    auto str3 = "World"_gs;

    EXPECT_TRUE(str1.starts_with(str2));
    EXPECT_FALSE(str1.starts_with(str3));
}

TEST(GermanStrings, SSO)
{
    CountingAllocator::reset();

    gs::basic_german_string<CountingAllocator> str1("Hello", gs::temporary_t{}); // 5 chars, should be SSO, no allocs even when passing the class
    EXPECT_TRUE(str1.as_string_view() == "Hello");
    EXPECT_EQ(CountingAllocator::get_count_allocs(), 0);

    gs::basic_german_string<CountingAllocator> str2("Hello, World!", gs::temporary_t{}); // 13 chars, should not be SSO, also we force allocation with temporary class here
    EXPECT_TRUE(str2.as_string_view() == "Hello, World!");
    EXPECT_EQ(CountingAllocator::get_count_allocs(), 1);

    gs::basic_german_string<CountingAllocator> str3("Hello, World", gs::temporary_t{}); // 12 chars, should be SSO
    EXPECT_TRUE(str3.as_string_view() == "Hello, World");
    EXPECT_EQ(CountingAllocator::get_count_allocs(), 1);
}

TEST(GermanStrings, Classes)
{
    CountingAllocator::reset();

    {
        // SSO strings are always persistent
        gs::basic_german_string<CountingAllocator> small_persistent_string("Hello");
        EXPECT_EQ(small_persistent_string.get_class(), gs::string_class::persistent);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 0);
    }
    EXPECT_EQ(CountingAllocator::get_count_deallocs(), 0);

    {
        // Literal constructor should default to persistent -- doesn't work now, force persistent
        gs::basic_german_string<CountingAllocator> larger_persistent_string("Hello, World!", gs::persistent_t{});
        EXPECT_EQ(larger_persistent_string.get_class(), gs::string_class::persistent);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 0);
    }
    EXPECT_EQ(CountingAllocator::get_count_deallocs(), 0);

    {
        std::string actual_data_owner(LARGE_KNOWN_STRING);
        gs::basic_german_string<CountingAllocator> larger_transient_string(actual_data_owner.c_str(), gs::transient_t{});
        EXPECT_EQ(larger_transient_string.get_class(), gs::string_class::transient);
        EXPECT_TRUE(larger_transient_string.as_string_view() == actual_data_owner);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 0);
    }
    EXPECT_EQ(CountingAllocator::get_count_deallocs(), 0);

    {
        gs::basic_german_string<CountingAllocator> larger_temp_string(LARGE_KNOWN_STRING, gs::temporary_t{});
        EXPECT_EQ(larger_temp_string.get_class(), gs::string_class::temporary);
        EXPECT_TRUE(larger_temp_string.as_string_view() == LARGE_KNOWN_STRING);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 1);
    }
    EXPECT_EQ(CountingAllocator::get_count_deallocs(), 1);

    {
        gs::basic_german_string<CountingAllocator> larger_temp_string(LARGE_KNOWN_STRING, gs::temporary_t{});
        EXPECT_EQ(larger_temp_string.get_class(), gs::string_class::temporary);
        EXPECT_TRUE(larger_temp_string.as_string_view() == LARGE_KNOWN_STRING);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 2);

        auto moved_temp_string = std::move(larger_temp_string);
        EXPECT_EQ(moved_temp_string.get_class(), gs::string_class::temporary);
        EXPECT_TRUE(moved_temp_string.as_string_view() == LARGE_KNOWN_STRING);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 2);

        // The original string should be in transient state, we only stole the ownership of the data
        EXPECT_EQ(larger_temp_string.get_class(), gs::string_class::transient);
        EXPECT_TRUE(larger_temp_string.as_string_view() == LARGE_KNOWN_STRING);
    }
    EXPECT_EQ(CountingAllocator::get_count_deallocs(), 2);

    // Also test copy constructor and class preservation there
    {
        gs::basic_german_string<CountingAllocator> larger_temp_string(LARGE_KNOWN_STRING, gs::temporary_t{});
        EXPECT_EQ(larger_temp_string.get_class(), gs::string_class::temporary);
        EXPECT_TRUE(larger_temp_string.as_string_view() == LARGE_KNOWN_STRING);
        EXPECT_EQ(CountingAllocator::get_count_allocs(), 3);
	}
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
