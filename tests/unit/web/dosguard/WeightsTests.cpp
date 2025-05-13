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

#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/dosguard/Weights.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

using namespace web::dosguard;

struct TestParams {
    std::string testName;
    std::string requestJson;
    size_t expectedWeight;
};

class WeightsTest : public ::testing::TestWithParam<TestParams> {
protected:
    size_t const defaultWeight_{10};
    std::unordered_map<std::string, Weights::Entry> weightsMap_{
        {"only_weight", {.weight = 20, .weightLedgerCurrent = std::nullopt, .weightLedgerValidated = std::nullopt}},
        {"with_current_weight", {.weight = 30, .weightLedgerCurrent = 35, .weightLedgerValidated = std::nullopt}},
        {"with_validated_weight", {.weight = 40, .weightLedgerCurrent = std::nullopt, .weightLedgerValidated = 45}},
        {"with_both_weights", {.weight = 50, .weightLedgerCurrent = 55, .weightLedgerValidated = 60}},
    };
    Weights weights_{defaultWeight_, weightsMap_};
};

TEST_P(WeightsTest, RequestWeight)
{
    auto const& params = GetParam();
    auto request = boost::json::parse(params.requestJson).as_object();
    EXPECT_EQ(weights_.requestWeight(request), params.expectedWeight);
}

INSTANTIATE_TEST_SUITE_P(
    WeightsTests,
    WeightsTest,
    ::testing::Values(
        TestParams{.testName = "EmptyObject", .requestJson = "{}", .expectedWeight = 10},
        TestParams{.testName = "NonStringMethod", .requestJson = R"json({"method": 123})json", .expectedWeight = 10},
        TestParams{.testName = "NonStringCommand", .requestJson = R"json({"command": 123})json", .expectedWeight = 10},

        TestParams{
            .testName = "UnknownMethodName",
            .requestJson = R"json({"method": "unknown_method"})json",
            .expectedWeight = 10
        },
        TestParams{
            .testName = "UnknownCommandName",
            .requestJson = R"json({"command": "unknown_command"})json",
            .expectedWeight = 10
        },

        TestParams{
            .testName = "OnlyWeight_NoLedgerIndex",
            .requestJson = R"json({"method": "only_weight"})json",
            .expectedWeight = 20
        },
        TestParams{
            .testName = "OnlyWeight_CurrentLedgerIndex",
            .requestJson = R"json({"method": "only_weight", "ledger_index": "current"})json",
            .expectedWeight = 20
        },
        TestParams{
            .testName = "OnlyWeight_ValidatedLedgerIndex",
            .requestJson = R"json({"method": "only_weight", "ledger_index": "validated"})json",
            .expectedWeight = 20
        },
        TestParams{
            .testName = "OnlyWeight_ClosedLedgerIndex",
            .requestJson = R"json({"method": "only_weight", "ledger_index": "closed"})json",
            .expectedWeight = 20
        },
        TestParams{
            .testName = "OnlyWeight_NumericLedgerIndex",
            .requestJson = R"json({"method": "only_weight", "ledger_index": "123"})json",
            .expectedWeight = 20
        },
        TestParams{
            .testName = "OnlyWeight_OtherStringLedgerIndex",
            .requestJson = R"json({"method": "only_weight", "ledger_index": "some_string"})json",
            .expectedWeight = 20
        },

        // With Current Weight
        TestParams{
            .testName = "WithCurrentWeight_NoLedgerIndex",
            .requestJson = R"json({"method": "with_current_weight"})json",
            .expectedWeight = 30
        },
        TestParams{
            .testName = "WithCurrentWeight_CurrentLedgerIndex",
            .requestJson = R"json({"method": "with_current_weight", "ledger_index": "current"})json",
            .expectedWeight = 35
        },
        TestParams{
            .testName = "WithCurrentWeight_ValidatedLedgerIndex",
            .requestJson = R"json({"method": "with_current_weight", "ledger_index": "validated"})json",
            .expectedWeight = 30
        },

        // With Validated Weight
        TestParams{
            .testName = "WithValidatedWeight_NoLedgerIndex",
            .requestJson = R"json({"method": "with_validated_weight"})json",
            .expectedWeight = 40
        },
        TestParams{
            .testName = "WithValidatedWeight_CurrentLedgerIndex",
            .requestJson = R"json({"method": "with_validated_weight", "ledger_index": "current"})json",
            .expectedWeight = 40
        },
        TestParams{
            .testName = "WithValidatedWeight_ValidatedLedgerIndex",
            .requestJson = R"json({"method": "with_validated_weight", "ledger_index": "validated"})json",
            .expectedWeight = 45
        },

        // With Both Weights
        TestParams{
            .testName = "WithBothWeights_NoLedgerIndex",
            .requestJson = R"json({"method": "with_both_weights"})json",
            .expectedWeight = 50
        },
        TestParams{
            .testName = "WithBothWeights_CurrentLedgerIndex",
            .requestJson = R"json({"method": "with_both_weights", "ledger_index": "current"})json",
            .expectedWeight = 55
        },
        TestParams{
            .testName = "WithBothWeights_ValidatedLedgerIndex",
            .requestJson = R"json({"method": "with_both_weights", "ledger_index": "validated"})json",
            .expectedWeight = 60
        },

        // Using Command
        TestParams{
            .testName = "UsingCommand_NoLedgerIndex",
            .requestJson = R"json({"command": "with_both_weights"})json",
            .expectedWeight = 50
        },
        TestParams{
            .testName = "UsingCommand_CurrentLedgerIndex",
            .requestJson = R"json({"command": "with_both_weights", "ledger_index": "current"})json",
            .expectedWeight = 55
        },
        TestParams{
            .testName = "UsingCommand_ValidatedLedgerIndex",
            .requestJson = R"json({"command": "with_both_weights", "ledger_index": "validated"})json",
            .expectedWeight = 60
        },

        // With Params Array
        TestParams{
            .testName = "WithParamsArray_CurrentLedgerIndex",
            .requestJson = R"json({"method": "with_both_weights", "params": [{"ledger_index": "current"}]})json",
            .expectedWeight = 55
        },
        TestParams{
            .testName = "WithParamsArray_ValidatedLedgerIndex",
            .requestJson = R"json({"method": "with_both_weights", "params": [{"ledger_index": "validated"}]})json",
            .expectedWeight = 60
        },
        TestParams{
            .testName = "WithParamsArray_WithCommand",
            .requestJson = R"json({"command": "with_both_weights", "params": [{"ledger_index": "current"}]})json",
            .expectedWeight = 55
        }
    ),
    [](::testing::TestParamInfo<TestParams> const& info) { return info.param.testName; }
);

TEST(WeightsMakeTest, CreateFromConfig)
{
    util::config::ClioConfigDefinition mockConfig{
        {"dos_guard.__ng_default_weight", util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(10)
        },
        {"dos_guard.__ng_weights.[].method",
         util::config::Array{util::config::ConfigValue{util::config::ConfigType::String}}},
        {"dos_guard.__ng_weights.[].weight",
         util::config::Array{util::config::ConfigValue{util::config::ConfigType::Integer}}},
        {"dos_guard.__ng_weights.[].weight_ledger_current",
         util::config::Array{util::config::ConfigValue{util::config::ConfigType::Integer}.optional()}},
        {"dos_guard.__ng_weights.[].weight_ledger_validated",
         util::config::Array{util::config::ConfigValue{util::config::ConfigType::Integer}.optional()}}
    };
    std::string const configStr = R"json(
    {
        "dos_guard": {
            "__ng_default_weight": 15,
            "__ng_weights": [
                {
                    "method": "method1",
                    "weight": 25,
                    "weight_ledger_current": 30
                },
                {
                    "method": "method2",
                    "weight": 35,
                    "weight_ledger_validated": 40
                },
                {
                    "method": "method3",
                    "weight": 45,
                    "weight_ledger_current": 50,
                    "weight_ledger_validated": 55
                }
            ]
        }
    }
    )json";

    auto const configJson = boost::json::parse(configStr).as_object();

    ASSERT_FALSE(mockConfig.parse(util::config::ConfigFileJson(configJson)).has_value());

    Weights const weights = Weights::make(mockConfig);

    auto request = boost::json::parse(R"json({"method": "unknown_method"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 15);

    request = boost::json::parse(R"json({"method": "method1"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 25);

    request = boost::json::parse(R"json({"method": "method1", "ledger_index": "current"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 30);

    request = boost::json::parse(R"json({"method": "method1", "ledger_index": "validated"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 25);

    request = boost::json::parse(R"json({"method": "method2"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 35);

    request = boost::json::parse(R"json({"method": "method2", "ledger_index": "current"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 35);

    request = boost::json::parse(R"json({"method": "method2", "ledger_index": "validated"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 40);

    request = boost::json::parse(R"json({"method": "method3"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 45);

    request = boost::json::parse(R"json({"method": "method3", "ledger_index": "current"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 50);

    request = boost::json::parse(R"json({"method": "method3", "ledger_index": "validated"})json").as_object();
    EXPECT_EQ(weights.requestWeight(request), 55);
}
