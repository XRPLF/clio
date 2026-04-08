#include "util/StringHash.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace util;

TEST(StringHashTest, HashesConsistently)
{
    StringHash const hasher;

    std::string const stdString = "test string";
    std::string_view const strView = "test string";
    char const* cString = "test string";

    EXPECT_EQ(hasher(stdString), hasher(strView));
    EXPECT_EQ(hasher(stdString), hasher(cString));
    EXPECT_EQ(hasher(strView), hasher(cString));
}

TEST(StringHashTest, TransparentLookup)
{
    std::unordered_set<std::string, StringHash, std::equal_to<>> const stringSet{"hello world"};

    std::string const stdString = "hello world";
    std::string_view const strView = "hello world";
    char const* cString = "hello world";

    EXPECT_TRUE(stringSet.contains(stdString));
    EXPECT_TRUE(stringSet.contains(strView));
    EXPECT_TRUE(stringSet.contains(cString));

    EXPECT_FALSE(stringSet.contains("goodbye world"));
}

TEST(StringHashTest, EmptyStrings)
{
    StringHash const hasher;

    std::string const emptyStdString;
    std::string_view const emptyStrView;
    char const* emptyCString = "";

    EXPECT_EQ(hasher(emptyStdString), hasher(emptyStrView));
    EXPECT_EQ(hasher(emptyStdString), hasher(emptyCString));
    EXPECT_EQ(hasher(emptyStrView), hasher(emptyCString));
}
