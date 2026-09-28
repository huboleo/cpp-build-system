#include "string_utils.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using Words = std::vector<std::string>;

TEST(StringUtils, TrimRemovesSurroundingWhitespace) {
    EXPECT_EQ(string_utils::trim("  a b \t\n"), "a b");
    EXPECT_EQ(string_utils::trim("word"), "word");
}

TEST(StringUtils, TrimOfBlankTextIsEmpty) {
    EXPECT_EQ(string_utils::trim(""), "");
    EXPECT_EQ(string_utils::trim(" \t\n "), "");
}

TEST(StringUtils, SplitSeparatesOnAnyWhitespace) {
    EXPECT_EQ(string_utils::split("a  b\tc\nd"), (Words{"a", "b", "c", "d"}));
    EXPECT_EQ(string_utils::split("  leading and trailing  "), (Words{"leading", "and", "trailing"}));
}

TEST(StringUtils, SplitOfBlankTextIsEmpty) {
    EXPECT_TRUE(string_utils::split("").empty());
    EXPECT_TRUE(string_utils::split(" \n\t ").empty());
}
