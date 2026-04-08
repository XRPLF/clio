#include "util/AccountUtils.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/tokens.h>

namespace {
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
}  // namespace

TEST(AccountUtils, parseBase58Wrapper)
{
    EXPECT_FALSE(util::parseBase58Wrapper<ripple::AccountID>("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp!"));
    EXPECT_TRUE(util::parseBase58Wrapper<ripple::AccountID>(kACCOUNT));

    EXPECT_TRUE(
        util::parseBase58Wrapper<ripple::SecretKey>(
            ripple::TokenType::NodePrivate, "paQmjZ37pKKPMrgadBLsuf9ab7Y7EUNzh27LQrZqoexpAs31nJi"
        )
    );
    EXPECT_FALSE(
        util::parseBase58Wrapper<ripple::SecretKey>(
            ripple::TokenType::NodePrivate, "??paQmjZ37pKKPMrgadBLsuf9ab7Y7EUNzh27LQrZqoexpAs31n"
        )
    );
}
