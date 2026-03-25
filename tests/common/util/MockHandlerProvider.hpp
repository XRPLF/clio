#pragma once

#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/HandlerProvider.hpp"

#include <gmock/gmock.h>

#include <optional>
#include <string>

struct MockHandlerProvider : public rpc::HandlerProvider {
public:
    MOCK_METHOD(bool, contains, (std::string const&), (const, override));
    MOCK_METHOD(
        std::optional<rpc::AnyHandler>,
        getHandler,
        (std::string const&),
        (const, override)
    );
    MOCK_METHOD(bool, isClioOnly, (std::string const&), (const, override));
};
