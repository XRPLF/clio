#pragma once

#include "data/LedgerCacheLoadingState.hpp"

#include <gmock/gmock.h>

#include <memory>

struct MockLedgerCacheLoadingStateBase : data::LedgerCacheLoadingStateInterface {
    MOCK_METHOD(void, allowLoading, (), (override));
    MOCK_METHOD(bool, isLoadingAllowed, (), (const, override));
    MOCK_METHOD(void, waitForLoadingAllowed, (), (const, override));
    MOCK_METHOD(bool, isCurrentlyLoading, (), (const, override));
    MOCK_METHOD(
        std::unique_ptr<data::LedgerCacheLoadingStateInterface>,
        clone,
        (),
        (const, override)
    );
};

using MockLedgerCacheLoadingState = testing::StrictMock<MockLedgerCacheLoadingStateBase>;
using NiceMockLedgerCacheLoadingState = testing::NiceMock<MockLedgerCacheLoadingStateBase>;
