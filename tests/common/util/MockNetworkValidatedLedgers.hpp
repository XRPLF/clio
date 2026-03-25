#pragma once

#include "etl/NetworkValidatedLedgersInterface.hpp"

#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <memory>
#include <optional>

struct MockNetworkValidatedLedgers : public etl::NetworkValidatedLedgersInterface {
    MOCK_METHOD(void, push, (uint32_t), (override));
    MOCK_METHOD(std::optional<uint32_t>, getMostRecent, (), (override));
    MOCK_METHOD(bool, waitUntilValidatedByNetwork, (uint32_t, std::optional<uint32_t>), (override));
    MOCK_METHOD(
        boost::signals2::scoped_connection,
        subscribe,
        (etl::NetworkValidatedLedgersInterface::SignalType::slot_type const& subscriber),
        (override)
    );
};

template <template <typename> typename MockType>
struct MockNetworkValidatedLedgersPtrImpl {
    std::shared_ptr<MockType<MockNetworkValidatedLedgers>> ptr =
        std::make_shared<MockType<MockNetworkValidatedLedgers>>();

    operator std::shared_ptr<etl::NetworkValidatedLedgersInterface>() const
    {
        return ptr;
    }

    MockType<MockNetworkValidatedLedgers>&
    operator*()
    {
        return *ptr;
    }
};

using MockNetworkValidatedLedgersPtr = MockNetworkValidatedLedgersPtrImpl<testing::NiceMock>;
using StrictMockNetworkValidatedLedgersPtr =
    MockNetworkValidatedLedgersPtrImpl<testing::StrictMock>;
