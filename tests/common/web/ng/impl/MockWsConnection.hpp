#pragma once

#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/WsConnection.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

struct MockWsConnectionImpl : web::ng::impl::WsConnectionBase {
    using WsConnectionBase::WsConnectionBase;

    MOCK_METHOD(bool, wasUpgraded, (), (const, override));

    MOCK_METHOD(void, setTimeout, (std::chrono::steady_clock::duration), (override));

    using SendReturnType = std::expected<void, web::ng::Error>;
    MOCK_METHOD(SendReturnType, send, (web::ng::Response, boost::asio::yield_context), (override));

    using ReceiveReturnType = std::expected<web::ng::Request, web::ng::Error>;
    MOCK_METHOD(ReceiveReturnType, receive, (boost::asio::yield_context), (override));

    MOCK_METHOD(void, close, (boost::asio::yield_context), (override));

    MOCK_METHOD(
        SendReturnType,
        sendShared,
        (std::shared_ptr<std::string>, boost::asio::yield_context),
        (override)
    );
};

using MockWsConnection = testing::NiceMock<MockWsConnectionImpl>;
using MockWsConnectionPtr = std::unique_ptr<testing::NiceMock<MockWsConnectionImpl>>;

using StrictMockWsConnection = testing::StrictMock<MockWsConnectionImpl>;
using StrictMockWsConnectionPtr = std::unique_ptr<testing::StrictMock<MockWsConnectionImpl>>;
