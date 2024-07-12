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

#include "data/AmendmentCenter.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Feature.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;
constexpr static auto SEQ = 30;
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

class RPCFeatureHandlerTest : public HandlerBaseTest {
protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr;
};

struct RPCFeatureHandlerParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct RPCFeatureHandlerParamTest : public RPCFeatureHandlerTest,
                                    public testing::WithParamInterface<RPCFeatureHandlerParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<RPCFeatureHandlerParamTestCaseBundle>{
        // Note: on rippled this and below returns "badFeature"
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeFeatureBool", R"({"feature": true})", "invalidParams", "Invalid parameters."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeFeatureInt", R"({"feature": 42})", "invalidParams", "Invalid parameters."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeFeatureDouble", R"({"feature": 4.2})", "invalidParams", "Invalid parameters."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeFeatureNull", R"({"feature": null})", "invalidParams", "Invalid parameters."
        },
        // Note: this and below internal errors on rippled
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeFeatureObj", R"({"feature": {}})", "invalidParams", "Invalid parameters."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeFeatureArray", R"({"feature": []})", "invalidParams", "Invalid parameters."
        },
        // "vetoed" should always be blocked
        RPCFeatureHandlerParamTestCaseBundle{
            "VetoedPassed",
            R"({"feature": "foo", "vetoed": true})",
            "noPermission",
            "The admin portion of feature API is not available through Clio."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeVetoedString",
            R"({"feature": "foo", "vetoed": "test"})",
            "noPermission",
            "The admin portion of feature API is not available through Clio."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeVetoedInt",
            R"({"feature": "foo", "vetoed": 42})",
            "noPermission",
            "The admin portion of feature API is not available through Clio."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeVetoedDouble",
            R"({"feature": "foo", "vetoed": 4.2})",
            "noPermission",
            "The admin portion of feature API is not available through Clio."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeVetoedObject",
            R"({"feature": "foo", "vetoed": {}})",
            "noPermission",
            "The admin portion of feature API is not available through Clio."
        },
        RPCFeatureHandlerParamTestCaseBundle{
            "InvalidTypeVetoedArray",
            R"({"feature": "foo", "vetoed": []})",
            "noPermission",
            "The admin portion of feature API is not available through Clio."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCFeatureGroup1,
    RPCFeatureHandlerParamTest,
    testing::ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(RPCFeatureHandlerParamTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCFeatureHandlerTest, LedgerNotExistViaIntSequence)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, testing::_)).WillOnce(testing::Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const req = boost::json::parse(fmt::format(
            R"({{
                "ledger_index": {}
            }})",
            RANGEMAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCFeatureHandlerTest, LedgerNotExistViaStringSequence)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, testing::_)).WillOnce(testing::Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const req = boost::json::parse(fmt::format(
            R"({{
                "ledger_index": "{}"
            }})",
            RANGEMAX
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCFeatureHandlerTest, LedgerNotExistViaHash)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, testing::_))
        .WillOnce(testing::Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const req = boost::json::parse(fmt::format(
            R"({{
                "ledger_hash": "{}"
            }})",
            LEDGERHASH
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCFeatureHandlerTest, AlwaysNoPermissionForVetoed)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const output =
            handler.process(boost::json::parse(R"({"vetoed": true, "feature": "foo"})"), Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "noPermission");
        EXPECT_EQ(
            err.at("error_message").as_string(), "The admin portion of feature API is not available through Clio."
        );
    });
}

TEST_F(RPCFeatureHandlerTest, SuccessPathViaNameWithSingleSupportedAndEnabledResult)
{
    backend->setRange(10, 30);

    auto const all = std::vector<data::Amendment>{
        {
            .name = Amendments::fixUniversalNumber,
            .feature = data::Amendment::GetAmendmentId(Amendments::fixUniversalNumber),
            .isSupportedByXRPL = true,
            .isSupportedByClio = true,
        },
        {
            .name = Amendments::fixRemoveNFTokenAutoTrustLine,
            .feature = data::Amendment::GetAmendmentId(Amendments::fixRemoveNFTokenAutoTrustLine),
            .isSupportedByXRPL = true,
            .isSupportedByClio = true,
        }
    };
    auto const keys = std::vector<data::AmendmentKey>{Amendments::fixUniversalNumber};
    auto const enabled = std::vector<bool>{true};

    EXPECT_CALL(*mockAmendmentCenterPtr, getAll).WillOnce(testing::ReturnRef(all));
    EXPECT_CALL(*mockAmendmentCenterPtr, isEnabled(testing::_, keys, SEQ)).WillOnce(testing::Return(enabled));

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    auto const expectedOutput = fmt::format(
        R"({{
            "2E2FB9CF8A44EB80F4694D38AADAE9B8B7ADAFD2F092E10068E61C98C4F092B0": 
            {{
                "name": "fixUniversalNumber", 
                "enabled": true, 
                "supported": true
            }},
            "ledger_hash": "{}", 
            "ledger_index": {}, 
            "validated": true
        }})",
        LEDGERHASH,
        SEQ
    );

    runSpawn([this, &expectedOutput](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const output = handler.process(boost::json::parse(R"({"feature": "fixUniversalNumber"})"), Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(expectedOutput));
    });
}

TEST_F(RPCFeatureHandlerTest, SuccessPathViaHashWithSingleResult)
{
    backend->setRange(10, 30);

    auto const all = std::vector<data::Amendment>{
        {
            .name = Amendments::fixUniversalNumber,
            .feature = data::Amendment::GetAmendmentId(Amendments::fixUniversalNumber),
            .isSupportedByXRPL = true,
            .isSupportedByClio = true,
        },
        {
            .name = Amendments::fixRemoveNFTokenAutoTrustLine,
            .feature = data::Amendment::GetAmendmentId(Amendments::fixRemoveNFTokenAutoTrustLine),
            .isSupportedByXRPL = true,
            .isSupportedByClio = true,
        }
    };
    auto const keys = std::vector<data::AmendmentKey>{Amendments::fixUniversalNumber};
    auto const enabled = std::vector<bool>{true};

    EXPECT_CALL(*mockAmendmentCenterPtr, getAll).WillOnce(testing::ReturnRef(all));
    EXPECT_CALL(*mockAmendmentCenterPtr, isEnabled(testing::_, keys, SEQ)).WillOnce(testing::Return(enabled));

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    auto const expectedOutput = fmt::format(
        R"({{
            "2E2FB9CF8A44EB80F4694D38AADAE9B8B7ADAFD2F092E10068E61C98C4F092B0": 
            {{
                "name": "fixUniversalNumber", 
                "enabled": true, 
                "supported": true
            }},
            "ledger_hash": "{}", 
            "ledger_index": {}, 
            "validated": true
        }})",
        LEDGERHASH,
        SEQ
    );

    runSpawn([this, &expectedOutput](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const output = handler.process(
            boost::json::parse(R"({"feature": "2E2FB9CF8A44EB80F4694D38AADAE9B8B7ADAFD2F092E10068E61C98C4F092B0"})"),
            Context{yield}
        );

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(expectedOutput));
    });
}

TEST_F(RPCFeatureHandlerTest, BadFeaturePath)
{
    backend->setRange(10, 30);

    auto const all = std::vector<data::Amendment>{{
        .name = Amendments::fixUniversalNumber,
        .feature = data::Amendment::GetAmendmentId(Amendments::fixUniversalNumber),
        .isSupportedByXRPL = true,
        .isSupportedByClio = true,
    }};
    auto const keys = std::vector<data::AmendmentKey>{"nonexistent"};
    EXPECT_CALL(*mockAmendmentCenterPtr, getAll).WillOnce(testing::ReturnRef(all));

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const output = handler.process(boost::json::parse(R"({"feature": "nonexistent"})"), Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badFeature");
        EXPECT_EQ(err.at("error_message").as_string(), "Feature unknown or invalid.");
    });
}

TEST_F(RPCFeatureHandlerTest, SuccessPathWithMultipleResults)
{
    backend->setRange(10, 30);

    auto const all = std::vector<data::Amendment>{
        {
            .name = Amendments::fixUniversalNumber,
            .feature = data::Amendment::GetAmendmentId(Amendments::fixUniversalNumber),
            .isSupportedByXRPL = true,
            .isSupportedByClio = true,
        },
        {
            .name = Amendments::fixRemoveNFTokenAutoTrustLine,
            .feature = data::Amendment::GetAmendmentId(Amendments::fixRemoveNFTokenAutoTrustLine),
            .isSupportedByXRPL = true,
            .isSupportedByClio = false,
        }
    };
    auto const keys =
        std::vector<data::AmendmentKey>{Amendments::fixUniversalNumber, Amendments::fixRemoveNFTokenAutoTrustLine};
    auto const enabled = std::vector<bool>{true, false};

    EXPECT_CALL(*mockAmendmentCenterPtr, getAll).WillOnce(testing::ReturnRef(all));
    EXPECT_CALL(*mockAmendmentCenterPtr, isEnabled(testing::_, keys, SEQ)).WillOnce(testing::Return(enabled));

    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    auto const amendments =
        CreateAmendmentsObject({Amendments::fixUniversalNumber, Amendments::fixRemoveNFTokenAutoTrustLine});

    auto const expectedOutput = fmt::format(
        R"({{
            "features": {{
                "2E2FB9CF8A44EB80F4694D38AADAE9B8B7ADAFD2F092E10068E61C98C4F092B0": 
                {{
                    "name": "fixUniversalNumber", 
                    "enabled": true, 
                    "supported": true
                }},
                "DF8B4536989BDACE3F934F29423848B9F1D76D09BE6A1FCFE7E7F06AA26ABEAD":
                {{
                    "name": "fixRemoveNFTokenAutoTrustLine", 
                    "enabled": false, 
                    "supported": false
                }}
            }},
            "ledger_hash": "{}", 
            "ledger_index": {}, 
            "validated": true
        }})",
        LEDGERHASH,
        SEQ
    );

    runSpawn([this, &expectedOutput](auto yield) {
        auto const handler = AnyHandler{FeatureHandler{backend, mockAmendmentCenterPtr}};
        auto const output = handler.process(boost::json::parse(R"({})"), Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, boost::json::parse(expectedOutput));
    });
}
