#include "util/Concepts.hpp"

#include <gtest/gtest.h>

TEST(ConceptTests, SomeNumberType)
{
    static_assert(util::SomeNumberType<int>);
    static_assert(!util::SomeNumberType<bool>);
    static_assert(util::SomeNumberType<char>);
    static_assert(!util::SomeNumberType<int const>);
}

TEST(ConceptTests, hasNoDuplicates)
{
    static_assert(util::hasNoDuplicates(1, 2, 3, 4, 5));
    static_assert(!util::hasNoDuplicates(1, 2, 3, 4, 5, 5));
}

struct TestA {
    static constexpr auto kName = "TestA";
};

struct AnotherA {
    static constexpr auto kName = "TestA";
};

struct TestB {
    static constexpr auto kName = "TestB";
};

TEST(ConceptTests, hasNoDuplicateNames)
{
    static_assert(util::hasNoDuplicateNames<TestA, TestB>());
    static_assert(!util::hasNoDuplicateNames<TestA, AnotherA, TestB>());
    static_assert(!util::hasNoDuplicateNames<TestA, TestB, AnotherA>());
}
