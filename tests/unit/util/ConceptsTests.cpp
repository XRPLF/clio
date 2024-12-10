//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
    static constexpr auto name = "TestA";
};

struct AnotherA {
    static constexpr auto name = "TestA";
};

struct TestB {
    static constexpr auto name = "TestB";
};

TEST(ConceptTests, hasNoDuplicateNames)
{
    static_assert(util::hasNoDuplicateNames<TestA, TestB>());
    static_assert(!util::hasNoDuplicateNames<TestA, AnotherA, TestB>());
    static_assert(!util::hasNoDuplicateNames<TestA, TestB, AnotherA>());
}
