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

struct WeightsTest : public ::testing::Test {
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

TEST_F(WeightsTest, RequestWeightNoMethodOrCommand)
{
    EXPECT_EQ(weights_.requestWeight(boost::json::object{}), defaultWeight_);
    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"method", 123}}), defaultWeight_);
    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"command", 123}}), defaultWeight_);
}

TEST_F(WeightsTest, RequestWeightUnknownMethod)
{
    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"method", "unknown_method"}}), defaultWeight_);
    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"command", "unknown_command"}}), defaultWeight_);
}

TEST_F(WeightsTest, RequestWeightOnlyBaseWeight)
{
    auto const& entry = weightsMap_.at("only_weight");

    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"method", "only_weight"}}), entry.weight);
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "only_weight"}, {"ledger_index", "current"}}),
        entry.weight
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "only_weight"}, {"ledger_index", "validated"}}),
        entry.weight
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "only_weight"}, {"ledger_index", "closed"}}), entry.weight
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "only_weight"}, {"ledger_index", 123}}), entry.weight
    );  // ledger_index not a string
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "only_weight"}, {"ledger_index", "some_string"}}),
        entry.weight
    );
}

TEST_F(WeightsTest, RequestWeightWithCurrentWeight)
{
    auto const& entry = weightsMap_.at("with_current_weight");

    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"method", "with_current_weight"}}), entry.weight);
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "with_current_weight"}, {"ledger_index", "current"}}),
        entry.weightLedgerCurrent.value()
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "with_current_weight"}, {"ledger_index", "validated"}}),
        entry.weight
    );
}

TEST_F(WeightsTest, RequestWeightWithValidatedWeight)
{
    auto const& entry = weightsMap_.at("with_validated_weight");

    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"method", "with_validated_weight"}}), entry.weight);
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "with_validated_weight"}, {"ledger_index", "current"}}),
        entry.weight
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "with_validated_weight"}, {"ledger_index", "validated"}}),
        entry.weightLedgerValidated.value()
    );
}

TEST_F(WeightsTest, RequestWeightWithBothWeights)
{
    auto const& entry = weightsMap_.at("with_both_weights");

    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"method", "with_both_weights"}}), entry.weight);
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "with_both_weights"}, {"ledger_index", "current"}}),
        entry.weightLedgerCurrent.value()
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"method", "with_both_weights"}, {"ledger_index", "validated"}}),
        entry.weightLedgerValidated.value()
    );
}

TEST_F(WeightsTest, RequestWeightUsingCommand)
{
    auto const& entry = weightsMap_.at("with_both_weights");

    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"command", "with_both_weights"}}), entry.weight);
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"command", "with_both_weights"}, {"ledger_index", "current"}}),
        entry.weightLedgerCurrent.value()
    );
    EXPECT_EQ(
        weights_.requestWeight(boost::json::object{{"command", "with_both_weights"}, {"ledger_index", "validated"}}),
        entry.weightLedgerValidated.value()
    );
    EXPECT_EQ(weights_.requestWeight(boost::json::object{{"command", "unknown_method"}}), defaultWeight_);
}

TEST_F(WeightsTest, RequestWeightWithParamsArray)
{
    auto const& entry = weightsMap_.at("with_both_weights");

    // Test the case where ledger_index is in the params array
    auto req = boost::json::object{
        {"method", "with_both_weights"},
        {"params", boost::json::array{{boost::json::object{{"ledger_index", "current"}}}}}
    };
    EXPECT_EQ(weights_.requestWeight(req), entry.weightLedgerCurrent.value());

    req = boost::json::object{
        {"method", "with_both_weights"},
        {"params", boost::json::array{{boost::json::object{{"ledger_index", "validated"}}}}}
    };
    EXPECT_EQ(weights_.requestWeight(req), entry.weightLedgerValidated.value());

    // Test with command instead of method
    req = boost::json::object{
        {"command", "with_both_weights"},
        {"params", boost::json::array{{boost::json::object{{"ledger_index", "current"}}}}}
    };
    EXPECT_EQ(weights_.requestWeight(req), entry.weightLedgerCurrent.value());
}
