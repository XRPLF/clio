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

#include "etl/impl/SubscriptionSourceDependencies.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockSubscriptionManager.hpp"

#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

using namespace etl::impl;
using testing::StrictMock;

struct SubscriptionSourceDependenciesTest : testing::Test {
    std::shared_ptr<StrictMock<MockNetworkValidatedLedgers>> networkValidatedLedgers_ =
        std::make_shared<StrictMock<MockNetworkValidatedLedgers>>();

    std::shared_ptr<StrictMock<MockSubscriptionManager>> subscriptionManager_ =
        std::make_shared<StrictMock<MockSubscriptionManager>>();

    SubscriptionSourceDependencies dependencies_{networkValidatedLedgers_, subscriptionManager_};
};

TEST_F(SubscriptionSourceDependenciesTest, ForwardProposedTransaction)
{
    boost::json::object const txJson = {{"tx", "json"}};
    EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(txJson));
    dependencies_.forwardProposedTransaction(txJson);
}

TEST_F(SubscriptionSourceDependenciesTest, ForwardValidation)
{
    boost::json::object const validationJson = {{"validation", "json"}};
    EXPECT_CALL(*subscriptionManager_, forwardValidation(validationJson));
    dependencies_.forwardValidation(validationJson);
}

TEST_F(SubscriptionSourceDependenciesTest, ForwardManifest)
{
    boost::json::object const manifestJson = {{"manifest", "json"}};
    EXPECT_CALL(*subscriptionManager_, forwardManifest(manifestJson));
    dependencies_.forwardManifest(manifestJson);
}

TEST_F(SubscriptionSourceDependenciesTest, PushValidatedLedger)
{
    uint32_t const idx = 42;
    EXPECT_CALL(*networkValidatedLedgers_, push(idx));
    dependencies_.pushValidatedLedger(idx);
}
