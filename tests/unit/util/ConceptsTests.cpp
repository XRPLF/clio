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

#include <list>
#include <string>
#include <vector>

TEST(ConceptsTests, SomeNumberType)
{
    static_assert(util::SomeNumberType<int>);
    static_assert(!util::SomeNumberType<int const>);
    static_assert(!util::SomeNumberType<bool>);
    static_assert(util::SomeNumberType<float>);
}

TEST(ConceptsTests, IsInstanceOfV)
{
    static_assert(util::IsInstanceOfV<std::vector, std::vector<int>>);
    static_assert(!util::IsInstanceOfV<std::vector, std::list<int>>);
    static_assert(util::IsInstanceOfV<std::list, std::list<int>>);
    static_assert(!util::IsInstanceOfV<std::list, std::vector<int>>);
}

TEST(ConceptsTests, Hashable)
{
    static_assert(util::Hashable<int>);
    static_assert(util::Hashable<std::string>);
    static_assert(!util::Hashable<std::vector<int>>);
    static_assert(!util::Hashable<std::list<int>>);
}
