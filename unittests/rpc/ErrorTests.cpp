//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "rpc/Errors.hpp"

#include <boost/json/object.hpp>
#include <gtest/gtest.h>
#include <ripple/protocol/ErrorCodes.h>

#include <cstdint>
#include <string_view>

using namespace rpc;
using namespace std;

namespace {
void
check(boost::json::object const& j, std::string_view error, uint32_t errorCode, std::string_view errorMessage)
{
    EXPECT_TRUE(j.contains("error"));
    EXPECT_TRUE(j.contains("error_code"));
    EXPECT_TRUE(j.contains("error_message"));
    EXPECT_TRUE(j.contains("status"));
    EXPECT_TRUE(j.contains("type"));

    EXPECT_TRUE(j.at("error").is_string());
    EXPECT_TRUE(j.at("error_code").is_uint64());
    EXPECT_TRUE(j.at("error_message").is_string());
    EXPECT_TRUE(j.at("status").is_string());
    EXPECT_TRUE(j.at("type").is_string());

    EXPECT_STREQ(j.at("status").as_string().c_str(), "error");
    EXPECT_STREQ(j.at("type").as_string().c_str(), "response");

    EXPECT_STREQ(j.at("error").as_string().c_str(), error.data());
    EXPECT_EQ(j.at("error_code").as_uint64(), errorCode);
    EXPECT_STREQ(j.at("error_message").as_string().c_str(), errorMessage.data());
}
}  // namespace

TEST(RPCErrorsTest, StatusAsBool)
{
    // Only rpcSUCCESS status should return false
    EXPECT_FALSE(Status{RippledError::rpcSUCCESS});

    // true should be returned for any error state, we just test a few
    CombinedError const errors[]{
        RippledError::rpcINVALID_PARAMS,
        RippledError::rpcUNKNOWN_COMMAND,
        RippledError::rpcTOO_BUSY,
        RippledError::rpcNO_NETWORK,
        RippledError::rpcACT_MALFORMED,
        RippledError::rpcBAD_MARKET,
        ClioError::rpcMALFORMED_CURRENCY,
    };

    for (auto const& ec : errors)
        EXPECT_TRUE(Status{ec});
}

TEST(RPCErrorsTest, SuccessToJSON)
{
    auto const status = Status{RippledError::rpcSUCCESS};
    check(makeError(status), "unknown", 0, "An unknown error code.");
}

TEST(RPCErrorsTest, RippledErrorToJSON)
{
    auto const status = Status{RippledError::rpcINVALID_PARAMS};
    check(makeError(status), "invalidParams", 31, "Invalid parameters.");
}

TEST(RPCErrorsTest, RippledErrorFromStringToJSON)
{
    auto const j = makeError(Status{"veryCustomError"});
    EXPECT_STREQ(j.at("error").as_string().c_str(), "veryCustomError");
}

TEST(RPCErrorsTest, RippledErrorToJSONCustomMessage)
{
    auto const status = Status{RippledError::rpcINVALID_PARAMS, "custom"};
    check(makeError(status), "invalidParams", 31, "custom");
}

TEST(RPCErrorsTest, RippledErrorToJSONCustomStrCodeAndMessage)
{
    auto const status = Status{RippledError::rpcINVALID_PARAMS, "customCode", "customMessage"};
    check(makeError(status), "customCode", 31, "customMessage");
}

TEST(RPCErrorsTest, ClioErrorToJSON)
{
    auto const status = Status{ClioError::rpcMALFORMED_CURRENCY};
    check(makeError(status), "malformedCurrency", 5000, "Malformed currency.");
}

TEST(RPCErrorsTest, ClioErrorToJSONCustomMessage)
{
    auto const status = Status{ClioError::rpcMALFORMED_CURRENCY, "custom"};
    check(makeError(status), "malformedCurrency", 5000, "custom");
}

TEST(RPCErrorsTest, ClioErrorToJSONCustomStrCodeAndMessage)
{
    auto const status = Status{ClioError::rpcMALFORMED_CURRENCY, "customCode", "customMessage"};
    check(makeError(status), "customCode", 5000, "customMessage");
}

TEST(RPCErrorsTest, InvalidClioErrorToJSON)
{
    EXPECT_ANY_THROW((void)makeError(static_cast<ClioError>(999999)));
}

TEST(RPCErrorsTest, WarningToJSON)
{
    auto j = makeWarning(WarningCode::warnRPC_OUTDATED);
    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("message"));

    EXPECT_TRUE(j.at("id").is_int64());
    EXPECT_TRUE(j.at("message").is_string());

    EXPECT_EQ(j.at("id").as_int64(), static_cast<uint32_t>(WarningCode::warnRPC_OUTDATED));
    EXPECT_STREQ(j.at("message").as_string().c_str(), "This server may be out of date");
}

TEST(RPCErrorsTest, InvalidWarningToJSON)
{
    EXPECT_ANY_THROW((void)makeWarning(static_cast<WarningCode>(999999)));
}
