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

#include "util/IotaIterator.hpp"

#include <gtest/gtest.h>

TEST(IotaIteratorTests, RamdonAccess)
{
    util::IotaIterator it(1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*(it += 1), 2);
    EXPECT_EQ(*(it -= 1), 1);
    EXPECT_EQ(*(it++), 1);
    EXPECT_EQ(*(it--), 2);
    EXPECT_EQ(*(++it), 2);
    EXPECT_EQ(*(--it), 1);
    util::IotaIterator it2(2);
    EXPECT_FALSE(it == it2);
    EXPECT_TRUE(it != it2);
    EXPECT_TRUE(it < it2);
    EXPECT_TRUE(it <= it2);
    EXPECT_FALSE(it > it2);
    EXPECT_FALSE(it >= it2);
    EXPECT_EQ(it2 - it, 1);
    EXPECT_EQ(it2 - 1, it);
    EXPECT_EQ(it + 1, it2);
    EXPECT_EQ(it2[2], util::IotaIterator(4));
}
