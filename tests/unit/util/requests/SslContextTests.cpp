#include "util/requests/impl/SslContext.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

using namespace util::requests::impl;

TEST(SslContext, Create)
{
    auto& ctx = getClientSslContext();
    EXPECT_NE(ctx.native_handle(), nullptr);
}

TEST(SslContext, IsCached)
{
    auto& first = getClientSslContext();
    auto& second = getClientSslContext();

    // Same shared instance is returned on every call
    EXPECT_EQ(&first, &second);
}

TEST(SslContext, InitSucceedsWithSystemCertificates)
{
    EXPECT_TRUE(initClientSslContext().has_value());
}

TEST(SslContext, MakeWithoutRootCertificateReturnsError)
{
    auto const ctx = makeClientSslContext(std::nullopt);

    ASSERT_FALSE(ctx.has_value());
    EXPECT_THAT(ctx.error().message(), testing::HasSubstr("could not find root certificate"));
}
