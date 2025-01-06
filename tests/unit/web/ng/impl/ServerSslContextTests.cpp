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

#include "util/NameGenerator.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/ng/impl/ServerSslContext.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <test_data/SslCert.hpp>

#include <optional>
#include <string>

using namespace web::ng::impl;
using namespace util::config;

struct MakeServerSslContextFromConfigTestBundle {
    std::string testName;
    std::optional<std::string> certFile;
    std::optional<std::string> keyFile;
    std::optional<std::string> expectedError;
    bool expectContext;

    boost::json::value
    configJson() const
    {
        boost::json::object result;
        if (certFile.has_value()) {
            result["ssl_cert_file"] = *certFile;
        }

        if (keyFile.has_value()) {
            result["ssl_key_file"] = *keyFile;
        }
        return result;
    }
};

struct MakeServerSslContextFromConfigTest : testing::TestWithParam<MakeServerSslContextFromConfigTestBundle> {};

TEST_P(MakeServerSslContextFromConfigTest, makeFromConfig)
{
    ConfigFileJson const js{GetParam().configJson().as_object()};
    // generate cert and key file
    auto config = ClioConfigDefinition{
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()}
    };
    auto const errors = config.parse(js);
    ASSERT_TRUE(!errors.has_value());

    auto const expectedServerSslContext = makeServerSslContext(config);
    if (GetParam().expectedError.has_value()) {
        ASSERT_FALSE(expectedServerSslContext.has_value());
        EXPECT_THAT(expectedServerSslContext.error(), testing::HasSubstr(*GetParam().expectedError));
    } else {
        EXPECT_EQ(expectedServerSslContext.value().has_value(), GetParam().expectContext);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MakeServerSslContextFromConfigTest,
    MakeServerSslContextFromConfigTest,
    testing::ValuesIn(
        {MakeServerSslContextFromConfigTestBundle{
             .testName = "NoCertNoKey",
             .certFile = std::nullopt,
             .keyFile = std::nullopt,
             .expectedError = std::nullopt,
             .expectContext = false
         },
         MakeServerSslContextFromConfigTestBundle{
             .testName = "CertOnly",
             .certFile = "some_path",
             .keyFile = std::nullopt,
             .expectedError = "Config entries 'ssl_cert_file' and 'ssl_key_file' must be set or unset together.",
             .expectContext = false
         },
         MakeServerSslContextFromConfigTestBundle{
             .testName = "KeyOnly",
             .certFile = std::nullopt,
             .keyFile = "some_path",
             .expectedError = "Config entries 'ssl_cert_file' and 'ssl_key_file' must be set or unset together.",
             .expectContext = false
         },
         MakeServerSslContextFromConfigTestBundle{
             .testName = "BothKeyAndCert",
             .certFile = "some_path",
             .keyFile = "some_other_path",
             .expectedError = "Can't read SSL certificate",
             .expectContext = false
         }}
    ),
    tests::util::kNAME_GENERATOR
);

struct MakeServerSslContextFromConfigRealFilesTest : testing::Test {};

TEST_F(MakeServerSslContextFromConfigRealFilesTest, WrongKeyFile)
{
    auto const certFile = tests::sslCertFile();

    ClioConfigDefinition const config{
        {"ssl_cert_file", ConfigValue{ConfigType::String}.defaultValue(certFile.path)},
        {"ssl_key_file", ConfigValue{ConfigType::String}.defaultValue("some_path")}
    };
    auto const expectedServerSslContext = makeServerSslContext(config);
    ASSERT_FALSE(expectedServerSslContext.has_value());
    EXPECT_THAT(expectedServerSslContext.error(), testing::HasSubstr("Can't read SSL key"));
}

TEST_F(MakeServerSslContextFromConfigRealFilesTest, BothFilesValid)
{
    auto const certFile = tests::sslCertFile();
    auto const keyFile = tests::sslKeyFile();

    ClioConfigDefinition const config{
        {"ssl_cert_file", ConfigValue{ConfigType::String}.defaultValue(certFile.path)},
        {"ssl_key_file", ConfigValue{ConfigType::String}.defaultValue(keyFile.path)}
    };
    auto const expectedServerSslContext = makeServerSslContext(config);
    EXPECT_TRUE(expectedServerSslContext.has_value());
}

struct MakeServerSslContextFromDataTestBundle {
    std::string testName;
    std::string certData;
    std::string keyData;
    bool expectedSuccess;
};

struct MakeServerSslContextFromDataTest : testing::TestWithParam<MakeServerSslContextFromDataTestBundle> {};

TEST_P(MakeServerSslContextFromDataTest, makeFromData)
{
    auto const& data = GetParam();
    auto const expectedServerSslContext = makeServerSslContext(data.certData, data.keyData);
    EXPECT_EQ(expectedServerSslContext.has_value(), data.expectedSuccess);
}

INSTANTIATE_TEST_SUITE_P(
    MakeServerSslContextFromDataTest,
    MakeServerSslContextFromDataTest,
    testing::ValuesIn(
        {MakeServerSslContextFromDataTestBundle{
             .testName = "EmptyData",
             .certData = "",
             .keyData = "",
             .expectedSuccess = false
         },
         MakeServerSslContextFromDataTestBundle{
             .testName = "CertOnly",
             .certData = std::string{tests::sslCert()},
             .keyData = "",
             .expectedSuccess = false
         },
         MakeServerSslContextFromDataTestBundle{
             .testName = "KeyOnly",
             .certData = "",
             .keyData = std::string{tests::sslKey()},
             .expectedSuccess = false
         },
         MakeServerSslContextFromDataTestBundle{
             .testName = "BothKeyAndCert",
             .certData = std::string{tests::sslCert()},
             .keyData = std::string{tests::sslKey()},
             .expectedSuccess = true
         }}
    ),
    tests::util::kNAME_GENERATOR
);
