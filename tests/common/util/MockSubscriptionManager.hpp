#pragma once

#include "data/Types.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

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
        (xrpl::LedgerHeader const&, xrpl::Fees const&, std::string const&, std::uint32_t),
        (override)
    );

    MOCK_METHOD(
        void,
        pubBookChanges,
        (xrpl::LedgerHeader const&, std::vector<data::TransactionAndMetadata> const&),
        (override)
    );

    MOCK_METHOD(void, unsubLedger, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(
        void,
        pubTransaction,
        (data::TransactionAndMetadata const&, xrpl::LedgerHeader const&),
        (override)
    );

    MOCK_METHOD(
        void,
        subAccount,
        (xrpl::AccountID const&, feed::SubscriberSharedPtr const&),
        (override)
    );

    MOCK_METHOD(
        void,
        unsubAccount,
        (xrpl::AccountID const&, feed::SubscriberSharedPtr const&),
        (override)
    );

    MOCK_METHOD(void, subBook, (xrpl::Book const&, feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubBook, (xrpl::Book const&, feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subBookChanges, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubBookChanges, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subManifest, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubManifest, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, subValidation, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubValidation, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, forwardProposedTransaction, (boost::json::object const&), (override));

    MOCK_METHOD(void, forwardManifest, (boost::json::object const&), (override));

    MOCK_METHOD(void, forwardValidation, (boost::json::object const&), (override));

    MOCK_METHOD(
        void,
        subProposedAccount,
        (xrpl::AccountID const&, feed::SubscriberSharedPtr const&),
        (override)
    );

    MOCK_METHOD(
        void,
        unsubProposedAccount,
        (xrpl::AccountID const&, feed::SubscriberSharedPtr const&),
        (override)
    );

    MOCK_METHOD(void, subProposedTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(void, unsubProposedTransactions, (feed::SubscriberSharedPtr const&), (override));

    MOCK_METHOD(boost::json::object, report, (), (const, override));

    MOCK_METHOD(void, setNetworkID, (uint32_t), (override));

    MOCK_METHOD(uint32_t, getNetworkID, (), (const, override));

    MOCK_METHOD(void, stop, (), (override));
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
using StrictMockSubscriptionManagerSharedPtr =
    MockSubscriptionManagerSharedPtrImpl<testing::StrictMock>;
