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

#pragma once

#include "data/Types.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Book.h>
#include <ripple/protocol/Fees.h>
#include <ripple/protocol/LedgerHeader.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct MockSubscriptionManager : feed::SubscriptionManagerInterface {
    MOCK_METHOD(
        boost::json::object,
        subLedger,
        (boost::asio::yield_context, feed::SubscriberSharedPtr const&),
        (override)
    );

    MOCK_METHOD(
        void,
        pubLedger,
        (ripple::LedgerHeader const&, ripple::Fees const&, std::string const&, std::uint32_t),
        (const, override)
    );

    MOCK_METHOD(
        void,
        pubBookChanges,
        (ripple::LedgerInfo const&, std::vector<data::TransactionAndMetadata> const&),
        (const, override)
    );

    MOCK_METHOD(void, unsubLedger, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subTransactions, (feed::SubscriberSharedPtr const&, std::uint32_t), (override));

    MOCK_METHOD(void, unsubTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, pubTransaction, (data::TransactionAndMetadata const&, ripple::LedgerInfo const&), (override));

    MOCK_METHOD(
        void,
        subAccount,
        (ripple::AccountID const&, feed::SubscriberSharedPtr const&, std::uint32_t),
        (override)
    );

    MOCK_METHOD(void, unsubAccount, (ripple::AccountID const&, feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subBook, (ripple::Book const&, feed::SubscriberSharedPtr const&, std::uint32_t), (override));

    MOCK_METHOD(void, unsubBook, (ripple::Book const&, feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subBookChanges, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubBookChanges, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subManifest, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubManifest, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subValidation, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubValidation, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, forwardProposedTransaction, (boost::json::object const&), (override));

    MOCK_METHOD(void, forwardManifest, (boost::json::object const&), (const, override));

    MOCK_METHOD(void, forwardValidation, (boost::json::object const&), (const, override));

    MOCK_METHOD(void, subProposedAccount, (ripple::AccountID const&, feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubProposedAccount, (ripple::AccountID const&, feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subProposedTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubProposedTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(boost::json::object, report, (), (const, override));
};

template <template <typename> typename MockType = ::testing::NiceMock>
struct MockSubscriptionManagerSharedPtrImpl {
    std::shared_ptr<MockType<MockSubscriptionManager>> subscriptionManagerMock =
        std::make_shared<MockType<MockSubscriptionManager>>();

    operator std::shared_ptr<feed::SubscriptionManagerInterface>()
    {
        return subscriptionManagerMock;
    }

    MockType<MockSubscriptionManager>&
    operator*()
    {
        return *subscriptionManagerMock;
    }
};

using MockSubscriptionManagerSharedPtr = MockSubscriptionManagerSharedPtrImpl<>;
using StrictMockSubscriptionManagerSharedPtr = MockSubscriptionManagerSharedPtrImpl<testing::StrictMock>;
