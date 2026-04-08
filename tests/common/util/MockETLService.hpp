#pragma once

#include "etl/ETLServiceInterface.hpp"
#include "etl/ETLState.hpp"

#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <optional>

struct MockETLService : etl::ETLServiceInterface {
    MOCK_METHOD(void, run, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(boost::json::object, getInfo, (), (const, override));
    MOCK_METHOD(std::uint32_t, lastCloseAgeSeconds, (), (const, override));
    MOCK_METHOD(bool, isAmendmentBlocked, (), (const, override));
    MOCK_METHOD(bool, isCorruptionDetected, (), (const, override));
    MOCK_METHOD(std::optional<etl::ETLState>, getETLState, (), (const, override));
};
