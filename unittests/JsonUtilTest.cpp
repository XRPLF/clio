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

#include <util/JsonUtils.h>

#include <boost/json.hpp>
#include <gtest/gtest.h>

TEST(JsonUtils, RemoveSecrets)
{
    auto json = boost::json::parse(R"({
        "secret": "snoopy",
        "seed": "woodstock",
        "seed_hex": "charlie",
        "passphrase": "lucy"
    })")
                    .as_object();

    auto json2 = util::removeSecret(json);
    EXPECT_FALSE(json2.contains("secret"));
    EXPECT_FALSE(json2.contains("seed"));
    EXPECT_FALSE(json2.contains("seed_hex"));
    EXPECT_FALSE(json2.contains("passphrase"));

    json = boost::json::parse(R"({
        "params": [
            {
                "secret": "snoopy",
                "seed": "woodstock",
                "seed_hex": "charlie",
                "passphrase": "lucy"
            }
        ]
    })")
               .as_object();

    json2 = util::removeSecret(json);
    EXPECT_TRUE(json2.contains("params"));
    EXPECT_TRUE(json2.at("params").is_array());
    EXPECT_TRUE(json2.at("params").as_array().size() > 0);
    json2 = json2.at("params").as_array()[0].as_object();
    EXPECT_FALSE(json2.contains("secret"));
    EXPECT_FALSE(json2.contains("seed"));
    EXPECT_FALSE(json2.contains("seed_hex"));
    EXPECT_FALSE(json2.contains("passphrase"));
}
