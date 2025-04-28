#include <random>
#include <string>
#include <limits>
#include <vector>
#include <memory>

#include "german_string.h"

#include <gtest/gtest.h>

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

    constexpr float small_known_string_probability = 0.1f; // 10% chance to generate the small known string
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

struct CountingAllocator : std::allocator<char> {
    CountingAllocator() = default;

    char* allocate(size_t n) {
        ++get_count_allocs();
        return std::allocator<char>::allocate(n);
    }

    void deallocate(char* p, size_t n) {
        ++get_count_deallocs();
        std::allocator<char>::deallocate(p, n);
    }

    static void reset() {
        get_count_allocs() = 0;
        get_count_deallocs() = 0;
    }

    static size_t& get_count_allocs() {
        thread_local size_t count_allocs = 0;
        return count_allocs;
    }

    static size_t& get_count_deallocs() {
        thread_local size_t count_deallocs = 0;
        return count_deallocs;
    }
};

// Demonstrate some basic assertions.
TEST(GermanStrings, BasicCorrectness) {
  size_t count = 1000;
  size_t min_length = 8;
  size_t max_length = 240;
  uint32_t seed = 42;
  auto std_strings = generate_random_strings<std::string>(count, min_length, max_length, seed);
  auto german_strings = generate_random_strings<gs::german_string>(count, min_length, max_length, seed);

  for (size_t i = 0; i < count; ++i)
  {
      EXPECT_EQ(std_strings[i].length(), german_strings[i].length());
      EXPECT_EQ(std_strings[i].compare(german_strings[i].as_string_view()), 0);
      EXPECT_EQ(std_strings[i], german_strings[i].as_string_view());
  }
}

TEST(GermanStrings, SSO) {
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

TEST(GermanStrings, Classes) {
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
}
