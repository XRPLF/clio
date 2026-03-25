#include "util/prometheus/Label.hpp"

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
    EXPECT_EQ(Labels().serialize(), "");
    EXPECT_EQ(Labels({Label("name", "value")}).serialize(), R"JSON({name="value"})JSON");
    EXPECT_EQ(
        Labels({Label("name", "value"), Label("name2", "value2")}).serialize(),
        R"JSON({name="value",name2="value2"})JSON"
    );
}
