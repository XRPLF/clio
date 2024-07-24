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

#include "util/AccountUtils.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/tokens.h>

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";

TEST(AccountUtils, parseBase58Wrapper)
{
    EXPECT_FALSE(util::parseBase58Wrapper<ripple::AccountID>("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp!"));
    EXPECT_TRUE(util::parseBase58Wrapper<ripple::AccountID>(ACCOUNT));

    EXPECT_TRUE(util::parseBase58Wrapper<ripple::SecretKey>(
        ripple::TokenType::NodePrivate, "paQmjZ37pKKPMrgadBLsuf9ab7Y7EUNzh27LQrZqoexpAs31nJi"
    ));
    EXPECT_FALSE(util::parseBase58Wrapper<ripple::SecretKey>(
        ripple::TokenType::NodePrivate, "??paQmjZ37pKKPMrgadBLsuf9ab7Y7EUNzh27LQrZqoexpAs31n"
    ));
}
