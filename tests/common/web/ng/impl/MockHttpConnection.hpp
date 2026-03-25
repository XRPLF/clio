#pragma once

#include "util/Taggable.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <optional>

struct MockHttpConnectionImpl : web::ng::impl::UpgradableConnection {
    using UpgradableConnection::UpgradableConnection;

    MOCK_METHOD(bool, wasUpgraded, (), (const, override));

    MOCK_METHOD(void, setTimeout, (std::chrono::steady_clock::duration), (override));

    using SendReturnType = std::expected<void, web::ng::Error>;
    MOCK_METHOD(SendReturnType, send, (web::ng::Response, boost::asio::yield_context), (override));

    MOCK_METHOD(
        SendReturnType,
        sendRaw,
        (boost::beast::http::response<boost::beast::http::string_body>, boost::asio::yield_context),
        (override)
    );

    using ReceiveReturnType = std::expected<web::ng::Request, web::ng::Error>;
    MOCK_METHOD(ReceiveReturnType, receive, (boost::asio::yield_context), (override));

    MOCK_METHOD(void, close, (boost::asio::yield_context), (override));

    using IsUpgradeRequestedReturnType = std::expected<bool, web::ng::Error>;
    MOCK_METHOD(
        IsUpgradeRequestedReturnType,
        isUpgradeRequested,
        (boost::asio::yield_context),
        (override)
    );

    using UpgradeReturnType = std::expected<web::ng::ConnectionPtr, web::ng::Error>;
    using OptionalSslContext = std::optional<boost::asio::ssl::context>;
    MOCK_METHOD(
        UpgradeReturnType,
        upgrade,
        (util::TagDecoratorFactory const& tagDecoratorFactory, boost::asio::yield_context yield),
        (override)
    );
};

using MockHttpConnection = testing::NiceMock<MockHttpConnectionImpl>;
using MockHttpConnectionPtr = std::unique_ptr<testing::NiceMock<MockHttpConnectionImpl>>;

using StrictMockHttpConnection = testing::StrictMock<MockHttpConnectionImpl>;
using StrictMockHttpConnectionPtr = std::unique_ptr<testing::StrictMock<MockHttpConnectionImpl>>;
