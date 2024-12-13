//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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

struct MockWsConnectionImpl : web::ng::impl::WsConnectionBase {
    using WsConnectionBase::WsConnectionBase;

    MOCK_METHOD(bool, wasUpgraded, (), (const, override));

    MOCK_METHOD(void, setTimeout, (std::chrono::steady_clock::duration), (override));

    using SendReturnType = std::optional<web::ng::Error>;
    MOCK_METHOD(SendReturnType, send, (web::ng::Response, boost::asio::yield_context), (override));

    using ReceiveReturnType = std::expected<web::ng::Request, web::ng::Error>;
    MOCK_METHOD(ReceiveReturnType, receive, (boost::asio::yield_context), (override));

    MOCK_METHOD(void, close, (boost::asio::yield_context));

    using SendBufferReturnType = std::optional<web::ng::Error>;
    MOCK_METHOD(SendBufferReturnType, sendBuffer, (boost::asio::const_buffer, boost::asio::yield_context), (override));
};

using MockWsConnection = testing::NiceMock<MockWsConnectionImpl>;
using MockWsConnectionPtr = std::unique_ptr<testing::NiceMock<MockWsConnectionImpl>>;

using StrictMockWsConnection = testing::StrictMock<MockWsConnectionImpl>;
using StrictMockWsConnectionPtr = std::unique_ptr<testing::StrictMock<MockWsConnectionImpl>>;
