#include <random>
#include <string>
#include <limits>
#include <vector>

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

// Demonstrate some basic assertions.
TEST(GermanStrings, BasicCorrectness) {
  size_t count = 100;
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
