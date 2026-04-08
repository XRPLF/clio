#pragma once

#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <optional>

struct MockConnectionMetadataImpl : web::ng::ConnectionMetadata {
    using web::ng::ConnectionMetadata::ConnectionMetadata;
    MOCK_METHOD(bool, wasUpgraded, (), (const, override));
};

using MockConnectionMetadata = testing::NiceMock<MockConnectionMetadataImpl>;
using StrictMockConnectionMetadata = testing::StrictMock<MockConnectionMetadataImpl>;

struct MockConnectionImpl : web::ng::Connection {
    using web::ng::Connection::Connection;

    MOCK_METHOD(bool, wasUpgraded, (), (const, override));

    MOCK_METHOD(void, setTimeout, (std::chrono::steady_clock::duration), (override));

    using SendReturnType = std::expected<void, web::ng::Error>;
    MOCK_METHOD(SendReturnType, send, (web::ng::Response, boost::asio::yield_context), (override));

    using ReceiveReturnType = std::expected<web::ng::Request, web::ng::Error>;
    MOCK_METHOD(ReceiveReturnType, receive, (boost::asio::yield_context), (override));

    MOCK_METHOD(void, close, (boost::asio::yield_context), (override));
};

using MockConnection = testing::NiceMock<MockConnectionImpl>;
using MockConnectionPtr = std::unique_ptr<testing::NiceMock<MockConnectionImpl>>;

using StrictMockConnection = testing::StrictMock<MockConnectionImpl>;
using StrictMockConnectionPtr = std::unique_ptr<testing::StrictMock<MockConnectionImpl>>;
