//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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
