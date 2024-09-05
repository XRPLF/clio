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

#include "web/impl/ServerSslContext.hpp"

#include <gtest/gtest.h>

using namespace web::impl;

TEST(ServerSslContext, makeServerSslContext)
{
    auto const sslContext = makeServerSslContext(TEST_DATA_SSL_CERT_PATH, TEST_DATA_SSL_KEY_PATH);
    ASSERT_TRUE(sslContext);
}

TEST(ServerSslContext, makeServerSslContext_WrongCertPath)
{
    auto const sslContext = makeServerSslContext("wrong_path", TEST_DATA_SSL_KEY_PATH);
    ASSERT_FALSE(sslContext);
}

TEST(ServerSslContext, makeServerSslContext_WrongKeyPath)
{
    auto const sslContext = makeServerSslContext(TEST_DATA_SSL_CERT_PATH, "wrong_path");
    ASSERT_FALSE(sslContext);
}

TEST(ServerSslContext, makeServerSslContext_CertKeyMismatch)
{
    auto const sslContext = makeServerSslContext(TEST_DATA_SSL_KEY_PATH, TEST_DATA_SSL_CERT_PATH);
    ASSERT_FALSE(sslContext);
}
