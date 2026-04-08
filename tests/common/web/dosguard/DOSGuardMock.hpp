#pragma once

#include "web/dosguard/DOSGuardInterface.hpp"

#include <boost/json/object.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <string>
#include <string_view>

struct DOSGuardMockImpl : web::dosguard::DOSGuardInterface {
    MOCK_METHOD(bool, isWhiteListed, (std::string_view const ip), (const, noexcept, override));
    MOCK_METHOD(bool, isOk, (std::string const& ip), (const, noexcept, override));
    MOCK_METHOD(void, increment, (std::string const& ip), (noexcept, override));
    MOCK_METHOD(void, decrement, (std::string const& ip), (noexcept, override));
    MOCK_METHOD(bool, add, (std::string const& ip, uint32_t size), (noexcept, override));
    MOCK_METHOD(
        bool,
        request,
        (std::string const& ip, boost::json::object const& request),
        (override)
    );
    MOCK_METHOD(void, clear, (), (noexcept, override));
};

using DOSGuardMock = testing::NiceMock<DOSGuardMockImpl>;
using DOSGuardStrictMock = testing::StrictMock<DOSGuardMockImpl>;
