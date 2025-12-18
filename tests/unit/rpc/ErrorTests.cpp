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
#include "util/NameGenerator.hpp"

#include <boost/json/fwd.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value_to.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

using namespace rpc;
using namespace std;

namespace {

template <typename ErrorCodeType>
void
check(boost::json::object const& j, std::string_view error, ErrorCodeType errorCode, std::string_view errorMessage)
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

    EXPECT_EQ(boost::json::value_to<std::string>(j.at("status")), "error");
    EXPECT_EQ(boost::json::value_to<std::string>(j.at("type")), "response");

    EXPECT_EQ(boost::json::value_to<std::string>(j.at("error")), error.data());
    EXPECT_EQ(j.at("error_code").as_uint64(), static_cast<uint64_t>(errorCode));
    EXPECT_EQ(boost::json::value_to<std::string>(j.at("error_message")), errorMessage.data());
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
        RippledError::rpcWRONG_NETWORK,
        RippledError::rpcACT_MALFORMED,
        RippledError::rpcBAD_MARKET,
        ClioError::RpcMalformedCurrency,
    };

    for (auto const& ec : errors)
        EXPECT_TRUE(Status{ec});
}

TEST(RPCErrorsTest, StatusEquals)
{
    EXPECT_EQ(Status{RippledError::rpcUNKNOWN}, Status{RippledError::rpcUNKNOWN});
    EXPECT_NE(Status{RippledError::rpcUNKNOWN}, Status{RippledError::rpcINTERNAL});
}

TEST(RPCErrorsTest, SuccessToJSON)
{
    auto const status = Status{RippledError::rpcSUCCESS};
    check(makeError(status), "unknown", RippledError::rpcSUCCESS, "An unknown error code.");
}

TEST(RPCErrorsTest, RippledErrorToJSON)
{
    auto const status = Status{RippledError::rpcINVALID_PARAMS};
    check(makeError(status), "invalidParams", RippledError::rpcINVALID_PARAMS, "Invalid parameters.");
}

TEST(RPCErrorsTest, RippledErrorFromStringToJSON)
{
    auto const j = makeError(Status{"veryCustomError"});
    EXPECT_EQ(boost::json::value_to<std::string>(j.at("error")), "veryCustomError");
}

TEST(RPCErrorsTest, RippledErrorToJSONCustomMessage)
{
    auto const status = Status{RippledError::rpcINVALID_PARAMS, "custom"};
    check(makeError(status), "invalidParams", RippledError::rpcINVALID_PARAMS, "custom");
}

TEST(RPCErrorsTest, RippledErrorToJSONCustomStrCodeAndMessage)
{
    auto const status = Status{RippledError::rpcINVALID_PARAMS, "customCode", "customMessage"};
    check(makeError(status), "customCode", RippledError::rpcINVALID_PARAMS, "customMessage");
}

TEST(RPCErrorsTest, ClioErrorToJSON)
{
    auto const status = Status{ClioError::RpcMalformedCurrency};
    check(makeError(status), "malformedCurrency", ClioError::RpcMalformedCurrency, "Malformed currency.");
}

TEST(RPCErrorsTest, ClioErrorToJSONCustomMessage)
{
    auto const status = Status{ClioError::RpcMalformedCurrency, "custom"};
    check(makeError(status), "malformedCurrency", ClioError::RpcMalformedCurrency, "custom");
}

TEST(RPCErrorsTest, ClioErrorToJSONCustomStrCodeAndMessage)
{
    auto const status = Status{ClioError::RpcMalformedCurrency, "customCode", "customMessage"};
    check(makeError(status), "customCode", ClioError::RpcMalformedCurrency, "customMessage");
}

TEST(RPCErrorsTest, InvalidClioErrorToJSON)
{
    EXPECT_ANY_THROW((void)makeError(static_cast<ClioError>(999999)));
}

struct WarningCodeTestBundle {
    std::string testName;
    WarningCode code;
    std::string message;
};

struct WarningCodeTest : public ::testing::TestWithParam<WarningCodeTestBundle> {};

INSTANTIATE_TEST_SUITE_P(
    WarningCodeTestGroup,
    WarningCodeTest,
    testing::Values(
        WarningCodeTestBundle{"Unknown", WarningCode::WarnUnknown, "Unknown warning"},
        WarningCodeTestBundle{
            "Clio",
            WarningCode::WarnRpcClio,
            "This is a clio server. clio only serves validated data. If you want to talk to rippled, include "
            "'ledger_index':'current' in your request"
        },
        WarningCodeTestBundle{"Outdated", WarningCode::WarnRpcOutdated, "This server may be out of date"},
        WarningCodeTestBundle{"RateLimit", WarningCode::WarnRpcRateLimit, "You are about to be rate limited"},
        WarningCodeTestBundle{
            "Deprecated",
            WarningCode::WarnRpcDeprecated,
            "Some fields from your request are deprecated. Please check the documentation at "
            "https://xrpl.org/docs/references/http-websocket-apis/ and update your request."
        }
    ),
    tests::util::kNAME_GENERATOR
);

TEST_P(WarningCodeTest, WarningToJSON)
{
    auto j = makeWarning(GetParam().code);
    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("message"));

    EXPECT_TRUE(j.at("id").is_int64());
    EXPECT_TRUE(j.at("message").is_string());

    EXPECT_EQ(j.at("id").as_int64(), static_cast<int64_t>(GetParam().code));
    EXPECT_EQ(boost::json::value_to<std::string>(j.at("message")), GetParam().message);
}

TEST(RPCErrorsTest, InvalidWarningToJSON)
{
    auto notSanitizedMakeWarning = []() __attribute__((no_sanitize("undefined"))) {
        return makeWarning(static_cast<WarningCode>(999999));
    };
    EXPECT_ANY_THROW((void)notSanitizedMakeWarning());
}

struct StatusStreamTestBundle {
    std::string testName;
    rpc::Status status;
    std::string expectedOutput;
};

struct RPCErrorsStatusStreamTest : public ::testing::TestWithParam<StatusStreamTestBundle> {
protected:
    std::ostringstream oss_;
};

TEST_P(RPCErrorsStatusStreamTest, StatusStreamOperator)
{
    auto const param = GetParam();
    oss_ << param.status;
    EXPECT_EQ(oss_.str(), param.expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(
    RPCErrorsTest,
    RPCErrorsStatusStreamTest,
    testing::Values(
        StatusStreamTestBundle{
            .testName = "EmptyStatus",
            .status = Status{},
            .expectedOutput = "Code: 0, Message: An unknown error code."
        },
        StatusStreamTestBundle{
            .testName = "StatusWithRippledError",
            .status = Status{RippledError::rpcSUCCESS},
            .expectedOutput = "Code: 0, Message: An unknown error code."
        },
        StatusStreamTestBundle{
            .testName = "StatusWithClioError",
            .status = Status{ClioError::RpcParamsUnparsable},
            .expectedOutput = "Code: 6004, Message: Params must be an array holding exactly one object."
        },
        StatusStreamTestBundle{
            .testName = "StatusWithCodeAndExtraInfo",
            .status = Status{ClioError::EtlConnectionError, boost::json::object{}},
            .expectedOutput = "Code: 7000, Message: Couldn't connect to rippled., Extra Info: {}"
        },
        StatusStreamTestBundle{
            .testName = "StatusWithMessage",
            .status = Status{"test message."},
            .expectedOutput = "Code: -1, Message: test message."
        },
        StatusStreamTestBundle{
            .testName = "StatusWithRippledErrorAndMessage",
            .status = Status{RippledError::rpcSUCCESS, "test message."},
            .expectedOutput = "Code: 0, Message: test message."
        },
        StatusStreamTestBundle{
            .testName = "StatusWithClioErrorAndMessage",
            .status = Status{ClioError::RpcParamsUnparsable, "Missing params array."},
            .expectedOutput = "Code: 6004, Message: Missing params array."
        },
        StatusStreamTestBundle{
            .testName = "StatusWithCodeErrorMessage",
            .status = Status{ClioError::EtlInvalidResponse, "invalidResponse", "Rippled returned an invalid response."},
            .expectedOutput = "Code: 7003, Error: invalidResponse, Message: Rippled returned an invalid response."
        }
    ),
    tests::util::kNAME_GENERATOR
);
