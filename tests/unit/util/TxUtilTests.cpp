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

#include "util/JsonUtils.hpp"
#include "util/TxUtils.hpp"

#include <gtest/gtest.h>
#include <ripple/protocol/TxFormats.h>

#include <algorithm>
#include <cstddef>
#include <iterator>

TEST(TxUtilTests, txTypesInLowercase)
{
    auto const& types = util::getTxTypesInLowercase();
    ASSERT_TRUE(
        std::size_t(std::distance(ripple::TxFormats::getInstance().begin(), ripple::TxFormats::getInstance().end())) ==
        types.size()
    );

    std::for_each(
        ripple::TxFormats::getInstance().begin(),
        ripple::TxFormats::getInstance().end(),
        [&](auto const& pair) { EXPECT_TRUE(types.find(util::toLower(pair.getName())) != types.end()); }
    );
}
