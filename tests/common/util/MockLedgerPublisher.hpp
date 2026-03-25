#pragma once

#include "etl/LedgerPublisherInterface.hpp"

#include <gmock/gmock.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <cstdint>
#include <optional>

struct MockLedgerPublisher : public etl::LedgerPublisherInterface {
    MOCK_METHOD(
        bool,
        publish,
        (uint32_t, std::optional<uint32_t>, std::chrono::steady_clock::duration),
        (override)
    );
    MOCK_METHOD(void, publish, (ripple::LedgerHeader const&), ());
    MOCK_METHOD(std::uint32_t, lastPublishAgeSeconds, (), (const));
    MOCK_METHOD(
        std::chrono::time_point<std::chrono::system_clock>,
        getLastPublish,
        (),
        (const, override)
    );
    MOCK_METHOD(std::uint32_t, lastCloseAgeSeconds, (), (const, override));
    MOCK_METHOD(std::optional<uint32_t>, getLastPublishedSequence, (), (const));
};
