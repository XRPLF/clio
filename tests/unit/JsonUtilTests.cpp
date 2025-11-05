//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

TEST(JsonUtils, RemoveSecrets)
{
    auto json = boost::json::parse(R"JSON({
        "secret": "snoopy",
        "seed": "woodstock",
        "seed_hex": "charlie",
        "passphrase": "lucy"
    })JSON")
                    .as_object();

    auto json2 = util::removeSecret(json);
    EXPECT_EQ(json2.at("secret").as_string(), "*");
    EXPECT_EQ(json2.at("seed").as_string(), "*");
    EXPECT_EQ(json2.at("seed_hex").as_string(), "*");
    EXPECT_EQ(json2.at("passphrase").as_string(), "*");

    json = boost::json::parse(R"JSON({
        "params": [
            {
                "secret": "snoopy",
                "seed": "woodstock",
                "seed_hex": "charlie",
                "passphrase": "lucy"
            }
        ]
    })JSON")
               .as_object();

    json2 = util::removeSecret(json);
    EXPECT_TRUE(json2.contains("params"));
    EXPECT_TRUE(json2.at("params").is_array());
    EXPECT_TRUE(!json2.at("params").as_array().empty());
    json2 = json2.at("params").as_array()[0].as_object();
    EXPECT_EQ(json2.at("secret").as_string(), "*");
    EXPECT_EQ(json2.at("seed").as_string(), "*");
    EXPECT_EQ(json2.at("seed_hex").as_string(), "*");
    EXPECT_EQ(json2.at("passphrase").as_string(), "*");
}

TEST(JsonUtils, integralValueAs)
{
    auto const expectedResultUint64 = static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1u;
    auto const uint64Json = boost::json::value(expectedResultUint64);
    EXPECT_EQ(util::integralValueAs<int32_t>(uint64Json), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(util::integralValueAs<uint32_t>(uint64Json), expectedResultUint64);
    EXPECT_EQ(util::integralValueAs<int64_t>(uint64Json), expectedResultUint64);
    EXPECT_EQ(util::integralValueAs<uint64_t>(uint64Json), expectedResultUint64);

    auto const expectedResultInt64 = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1u;
    auto const int64Json = boost::json::value(expectedResultInt64);
    EXPECT_EQ(util::integralValueAs<int32_t>(int64Json), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(util::integralValueAs<uint32_t>(int64Json), expectedResultInt64);
    EXPECT_EQ(util::integralValueAs<int64_t>(int64Json), expectedResultInt64);
    EXPECT_EQ(util::integralValueAs<uint64_t>(int64Json), expectedResultInt64);

    auto const doubleJson = boost::json::value(3.14);
    EXPECT_THROW(util::integralValueAs<int>(doubleJson), std::logic_error);

    auto const stringJson = boost::json::value("not a number");
    EXPECT_THROW(util::integralValueAs<int>(stringJson), std::logic_error);
}

TEST(JsonUtils, getLedgerIndex)
{
    auto const emptyJson = boost::json::value();
    EXPECT_THROW(std::ignore = util::getLedgerIndex(emptyJson), std::logic_error);

    auto const boolJson = boost::json::value(true);
    EXPECT_THROW(std::ignore = util::getLedgerIndex(emptyJson), std::logic_error);

    auto const numberJson = boost::json::value(12345);
    auto ledgerIndex = util::getLedgerIndex(numberJson);
    EXPECT_TRUE(ledgerIndex.has_value());
    EXPECT_EQ(ledgerIndex.value(), 12345u);

    auto const validStringJson = boost::json::value("12345");
    ledgerIndex = util::getLedgerIndex(validStringJson);
    EXPECT_TRUE(ledgerIndex.has_value());
    EXPECT_EQ(ledgerIndex.value(), 12345u);

    auto const invalidStringJson = boost::json::value("invalid123");
    EXPECT_THROW(std::ignore = util::getLedgerIndex(invalidStringJson), std::invalid_argument);

    auto const validatedJson = boost::json::value("validated");
    ledgerIndex = util::getLedgerIndex(validatedJson);
    EXPECT_FALSE(ledgerIndex.has_value());
}
