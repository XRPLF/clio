//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include <util/prometheus/Label.h>

#include <gtest/gtest.h>

using namespace util::prometheus;

TEST(LabelTests, operatorLower)
{
    EXPECT_LT(Label("aaa", "b"), Label("bbb", "a"));
    EXPECT_LT(Label("name", "a"), Label("name", "b"));
}

TEST(LabelTests, operatorEquals)
{
    EXPECT_EQ(Label("aaa", "b"), Label("aaa", "b"));
    EXPECT_NE(Label("aaa", "b"), Label("aaa", "c"));
    EXPECT_NE(Label("aaa", "b"), Label("bbb", "b"));
}

TEST(LabelTests, serialize)
{
    EXPECT_EQ(Label("name", "value").serialize(), R"(name="value")");
    EXPECT_EQ(Label("name", "value\n").serialize(), R"(name="value\n")");
    EXPECT_EQ(Label("name", "value\\").serialize(), R"(name="value\\")");
    EXPECT_EQ(Label("name", "value\"").serialize(), R"(name="value\"")");
}

TEST(LabelsTest, serialize)
{
    EXPECT_EQ(Labels({Label("name", "value")}).serialize(), R"({name="value"})");
    EXPECT_EQ(
        Labels({Label("name", "value"), Label("name2", "value2")}).serialize(), R"({name="value",name2="value2"})");
}
