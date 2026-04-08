#include "util/requests/impl/SslContext.hpp"

#include <gtest/gtest.h>

using namespace util::requests::impl;

TEST(SslContext, Create)
{
    auto ctx = makeClientSslContext();
    EXPECT_TRUE(ctx);
}
