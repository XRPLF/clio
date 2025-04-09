//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/VaultInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/NameGenerator.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;
namespace json = boost::json;

struct RPCVaultInfoHandlerTest : HandlerBaseTest {
    RPCVaultInfoHandlerTest()
    {
        backend_->setRange(10, 30);
    }

protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
};

struct VaultInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct VaultInfoParameterTest : RPCVaultInfoHandlerTest, WithParamInterface<VaultInfoParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<VaultInfoParamTestCaseBundle>{
        VaultInfoParamTestCaseBundle{
            .testName = "MissingVaultField",
            .testJson = R"({
                "method": "vault_info",
                "params": [{
                    "idk" : "idk"
                }]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Required field vault missing"
        },
        VaultInfoParamTestCaseBundle{
            .testName = "MissingOwnerInVault",
            .testJson = R"({
                "method": "vault_info",
                "params": [
                    {
                        "vault": {
                            "seq": 4
                        }
                    }
                ]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "MissingSeqInVault",
            .testJson = R"({
                "method": "vault_info",
                "params": [
                    {
                        "vault": {
                            "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
                        }
                    }
                ]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "SeqNotAnInteger",
            .testJson = R"({
                "method": "vault_info",
                "params": [
                    {
                        "vault": {
                            "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
                            "seq": "asdf"
                        }
                    }
                ]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "OwnerNotAString",
            .testJson = R"({
                "method": "vault_info",
                "params": [
                    {
                        "vault": {
                            "owner": true,
                             "seq": 3

                        }
                    }
                ]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        },
        VaultInfoParamTestCaseBundle{
            .testName = "OwnerNotAHexString",
            .testJson = R"({
                "method": "vault_info",
                "params": [
                    {
                        "vault": {
                            "owner": "asdf",
                             "seq": 3

                        }
                    }
                ]
            })",
            .expectedError = "malformedRequest",
            .expectedErrorMessage = "Malformed request."
        }
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountInfoGroup1,
    VaultInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(VaultInfoParameterTest, InvalidParams)
{
    auto const testBundle = VaultInfoParameterTest::GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{VaultInfoHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}
