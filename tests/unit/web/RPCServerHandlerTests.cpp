/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "rpc/Errors.hpp"
#include "rpc/common/APIVersion.hpp"
#include "rpc/common/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockETLService.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockRPCEngine.hpp"
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/RPCServerHandler.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace web;
using namespace util::config;

namespace {

constexpr auto kMIN_SEQ = 10;
constexpr auto kMAX_SEQ = 30;

}  // namespace

struct MockWsBase : public web::ConnectionBase {
    std::string message;
    boost::beast::http::status lastStatus = boost::beast::http::status::unknown;

    void
    send(std::shared_ptr<std::string> msgType) override
    {
        message += std::string(*msgType);
        lastStatus = boost::beast::http::status::ok;
    }

    void
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    send(std::string&& msg, boost::beast::http::status status = boost::beast::http::status::ok) override
    {
        message += msg;
        lastStatus = status;
    }

    SubscriptionContextPtr
    makeSubscriptionContext(util::TagDecoratorFactory const&) override
    {
        return {};
    }

    MockWsBase(util::TagDecoratorFactory const& factory) : web::ConnectionBase(factory, "localhost.fake.ip")
    {
    }
};

struct WebRPCServerHandlerTest : util::prometheus::WithPrometheus, MockBackendTest, SyncAsioContextTest {
    util::config::ClioConfigDefinition cfg{
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
        {"api_version.default", ConfigValue{ConfigType::Integer}.defaultValue(rpc::kAPI_VERSION_DEFAULT)},
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(rpc::kAPI_VERSION_MIN)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(rpc::kAPI_VERSION_MAX)}
    };
    std::shared_ptr<MockAsyncRPCEngine> rpcEngine = std::make_shared<MockAsyncRPCEngine>();
    std::shared_ptr<MockETLService> etl = std::make_shared<MockETLService>();
    std::shared_ptr<util::TagDecoratorFactory> tagFactory = std::make_shared<util::TagDecoratorFactory>(cfg);
    std::shared_ptr<RPCServerHandler<MockAsyncRPCEngine, MockETLService>> handler =
        std::make_shared<RPCServerHandler<MockAsyncRPCEngine, MockETLService>>(cfg, backend_, rpcEngine, etl);
    std::shared_ptr<MockWsBase> session = std::make_shared<MockWsBase>(*tagFactory);
};

TEST_F(WebRPCServerHandlerTest, HTTPDefaultPath)
{
    static constexpr auto kREQUEST = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESULT = "{}";
    static constexpr auto kRESPONSE = R"({
                                        "result": {
                                            "status": "success"
                                        },
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsNormalPath)
{
    session->upgraded = true;
    static constexpr auto kREQUEST = R"({
                                        "command": "server_info",
                                        "id": 99,
                                        "api_version": 2
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESULT = "{}";
    static constexpr auto kRESPONSE = R"({
                                        "result":{},
                                        "id": 99,
                                        "status": "success",
                                        "type": "response",
                                        "api_version": 2,
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPForwardedPath)
{
    static constexpr auto kREQUEST = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    // Note: forwarding always goes thru WS API
    static constexpr auto kRESULT = R"({
                                        "result": {
                                            "index": 1
                                        },
                                        "forwarded": true
                                    })";
    static constexpr auto kRESPONSE = R"({
                                        "result":{
                                                "index": 1,
                                                "status": "success"
                                        },
                                        "forwarded": true,
                                        "warnings":[
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPForwardedErrorPath)
{
    static constexpr auto kREQUEST = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    // Note: forwarding always goes thru WS API
    static constexpr auto kRESULT = R"({
                                        "error": "error",
                                        "error_code": 123,
                                        "error_message": "error message",
                                        "status": "error",
                                        "type": "response",
                                        "forwarded": true
                                    })";
    static constexpr auto kRESPONSE = R"({
                                        "result":{
                                            "error": "error",
                                            "error_code": 123,
                                            "error_message": "error message",
                                            "status": "error",
                                            "type": "response"
                                        },
                                        "forwarded": true,
                                        "warnings":[
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsForwardedPath)
{
    session->upgraded = true;
    static constexpr auto kREQUEST = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    // Note: forwarding always goes thru WS API
    static constexpr auto kRESULT = R"({
                                        "result": {
                                            "index": 1
                                        },
                                        "forwarded": true
                                   })";
    static constexpr auto kRESPONSE = R"({
                                        "result":{
                                            "index": 1
                                        },
                                        "forwarded": true,
                                        "id": 99,
                                        "status": "success",
                                        "type": "response",
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsForwardedErrorPath)
{
    session->upgraded = true;
    static constexpr auto kREQUEST = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    // Note: forwarding always goes thru WS API
    static constexpr auto kRESULT = R"({
                                        "error": "error",
                                        "error_code": 123,
                                        "error_message": "error message",
                                        "status": "error",
                                        "type": "response",
                                        "forwarded": true
                                   })";
    // WS error responses, unlike their successful counterpart, contain everything on top level without "result"
    static constexpr auto kRESPONSE = R"({
                                        "error": "error",
                                        "error_code": 123,
                                        "error_message": "error message",
                                        "status": "error",
                                        "type": "response",
                                        "forwarded": true,
                                        "id": 99,
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));

    // Forwarded errors counted as successful:
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);
    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPErrorPath)
{
    static constexpr auto kRESPONSE = R"({
                                        "result": {
                                            "error": "invalidParams",
                                            "error_code": 31,
                                            "error_message": "ledgerIndexMalformed",
                                            "status": "error",
                                            "type": "response",
                                            "request": {
                                                "method": "ledger",
                                                "params": [
                                                    {
                                                        "ledger_index": "xx"
                                                    }
                                                ]
                                            }
                                        },
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kREQUEST_JSON = R"({
                                            "method": "ledger",
                                            "params": [
                                                {
                                                "ledger_index": "xx"
                                                }
                                            ]
                                        })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{rpc::Status{rpc::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}}
        ));

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST_JSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsErrorPath)
{
    session->upgraded = true;
    static constexpr auto kRESPONSE = R"({
                                        "id": "123",
                                        "error": "invalidParams",
                                        "error_code": 31,
                                        "error_message": "ledgerIndexMalformed",
                                        "status": "error",
                                        "type": "response",
                                        "api_version": 2,
                                        "request": {
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123",
                                            "api_version": 2
                                        },
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kREQUEST_JSON = R"({
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123",
                                            "api_version": 2
                                        })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{rpc::Status{rpc::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}}
        ));

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kREQUEST_JSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPNotReady)
{
    static constexpr auto kREQUEST = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    static constexpr auto kRESPONSE = R"({
                                        "result": {
                                            "error": "notReady",
                                            "error_code": 13,
                                            "error_message": "Not ready to handle this request.",
                                            "status": "error",
                                            "type": "response",
                                            "request": {
                                                "method": "server_info",
                                                "params": [{}]
                                            }
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyNotReady).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsNotReady)
{
    session->upgraded = true;

    static constexpr auto kREQUEST = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    static constexpr auto kRESPONSE = R"({
                                        "error": "notReady",
                                        "error_code": 13,
                                        "error_message": "Not ready to handle this request.",
                                        "status": "error",
                                        "type": "response",
                                        "id": 99,
                                        "request": {
                                            "command": "server_info",
                                            "id": 99
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyNotReady).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPBadSyntaxWhenRequestSubscribe)
{
    static constexpr auto kREQUEST = R"({"method": "subscribe"})";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE = R"({
                                        "result": {
                                            "error": "badSyntax",
                                            "error_code": 1,
                                            "error_message": "Subscribe and unsubscribe are only allowed for websocket.",
                                            "status": "error",
                                            "type": "response",
                                            "request": {
                                                "method": "subscribe",
                                                "params": [{}]
                                            }
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPMissingCommand)
{
    static constexpr auto kREQUEST = R"({"method2": "server_info"})";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE = "Null method";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(session->message, kRESPONSE);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPCommandNotString)
{
    static constexpr auto kREQUEST = R"({"method": 1})";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE = "method is not string";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(session->message, kRESPONSE);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPCommandIsEmpty)
{
    static constexpr auto kREQUEST = R"({"method": ""})";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE = "method is empty";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(session->message, kRESPONSE);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WsMissingCommand)
{
    session->upgraded = true;
    static constexpr auto kREQUEST = R"({
                                        "command2": "server_info",
                                        "id": 99
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE = R"({
                                        "error": "missingCommand",
                                        "error_code": 6001,
                                        "error_message": "Method/Command is not specified or is not a string.",
                                        "status": "error",
                                        "type": "response",
                                        "id": 99,
                                        "request":{
                                            "command2": "server_info",
                                            "id": 99
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPParamsUnparseableNotArray)
{
    static constexpr auto kRESPONSE = "params unparseable";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kREQUEST_JSON = R"({
                                            "method": "ledger",
                                            "params": "wrong"
                                        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST_JSON, session);
    EXPECT_EQ(session->message, kRESPONSE);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPParamsUnparseableArrayWithDigit)
{
    static constexpr auto kRESPONSE = "params unparseable";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kREQUEST_JSON = R"({
                                            "method": "ledger",
                                            "params": [1]
                                        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST_JSON, session);
    EXPECT_EQ(session->message, kRESPONSE);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPInternalError)
{
    static constexpr auto kRESPONSE = R"({
                                        "result": {
                                            "error": "internal",
                                            "error_code": 73,
                                            "error_message": "Internal error.",
                                            "status": "error",
                                            "type": "response",
                                            "request": {
                                                "method": "ledger",
                                                "params": [{}]
                                            }
                                        }
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kREQUEST_JSON = R"({
                                            "method": "ledger",
                                            "params": [{}]
                                        })";

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*handler)(kREQUEST_JSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsInternalError)
{
    session->upgraded = true;

    static constexpr auto kRESPONSE = R"({
                                        "error": "internal",
                                        "error_code": 73,
                                        "error_message": "Internal error.",
                                        "status": "error",
                                        "type": "response",
                                        "id": "123",
                                        "request": {
                                            "command": "ledger",
                                            "id": "123"
                                        }
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kREQUEST_JSON = R"({
                                            "command": "ledger",
                                            "id": "123"
                                        })";

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*handler)(kREQUEST_JSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPOutDated)
{
    static constexpr auto kREQUEST = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESULT = "{}";
    static constexpr auto kRESPONSE = R"({
                                        "result": {
                                            "status": "success"
                                        },
                                        "warnings": [
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            },
                                            {
                                                "id": 2002,
                                                "message": "This server may be out of date"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsOutdated)
{
    session->upgraded = true;

    static constexpr auto kREQUEST = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESULT = "{}";
    static constexpr auto kRESPONSE = R"({
                                        "result":{},
                                        "id": 99,
                                        "status": "success",
                                        "type": "response",
                                        "warnings":[
                                            {
                                                "id": 2001,
                                                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            },
                                            {
                                                "id": 2002,
                                                "message": "This server may be out of date"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kRESULT).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, WsTooBusy)
{
    session->upgraded = true;

    auto localRpcEngine = std::make_shared<MockRPCEngine>();
    auto localHandler =
        std::make_shared<RPCServerHandler<MockRPCEngine, MockETLService>>(cfg, backend_, localRpcEngine, etl);
    static constexpr auto kREQUEST = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE =
        R"({
            "error": "tooBusy",
            "error_code": 9,
            "error_message": "The server is too busy to help you now.",
            "status": "error",
            "type": "response"
        })";

    EXPECT_CALL(*localRpcEngine, notifyTooBusy).Times(1);
    EXPECT_CALL(*localRpcEngine, post).WillOnce(testing::Return(false));

    (*localHandler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPTooBusy)
{
    auto localRpcEngine = std::make_shared<MockRPCEngine>();
    auto localHandler =
        std::make_shared<RPCServerHandler<MockRPCEngine, MockETLService>>(cfg, backend_, localRpcEngine, etl);
    static constexpr auto kREQUEST = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    static constexpr auto kRESPONSE =
        R"({
            "error": "tooBusy",
            "error_code": 9,
            "error_message": "The server is too busy to help you now.",
            "status": "error",
            "type": "response"
        })";

    EXPECT_CALL(*localRpcEngine, notifyTooBusy).Times(1);
    EXPECT_CALL(*localRpcEngine, post).WillOnce(testing::Return(false));

    (*localHandler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

TEST_F(WebRPCServerHandlerTest, HTTPRequestNotJson)
{
    static constexpr auto kREQUEST = "not json";
    static constexpr auto kRESPONSE_PREFIX = "Unable to parse JSON from the request";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_THAT(session->message, testing::StartsWith(kRESPONSE_PREFIX));
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WsRequestNotJson)
{
    session->upgraded = true;
    static constexpr auto kREQUEST = "not json";
    static constexpr auto kRESPONSE =
        R"({
            "error": "badSyntax",
            "error_code": 1,
            "error_message": "Syntax error.",
            "status": "error",
            "type": "response"
        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kREQUEST, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kRESPONSE));
}

struct InvalidAPIVersionTestBundle {
    std::string testName;
    std::string version;
    std::string wsMessage;
};

// parameterized test cases for parameters check
struct WebRPCServerHandlerInvalidAPIVersionParamTest : public WebRPCServerHandlerTest,
                                                       public testing::WithParamInterface<InvalidAPIVersionTestBundle> {
};

auto
generateInvalidVersions()
{
    return std::vector<InvalidAPIVersionTestBundle>{
        {.testName = "v0",
         .version = "0",
         .wsMessage = fmt::format("Requested API version is lower than minimum supported ({})", rpc::kAPI_VERSION_MIN)},
        {.testName = "v4",
         .version = "4",
         .wsMessage = fmt::format("Requested API version is higher than maximum supported ({})", rpc::kAPI_VERSION_MAX)
        },
        {.testName = "null", .version = "null", .wsMessage = "API version must be an integer"},
        {.testName = "str", .version = "\"bogus\"", .wsMessage = "API version must be an integer"},
        {.testName = "bool", .version = "false", .wsMessage = "API version must be an integer"},
        {.testName = "double", .version = "12.34", .wsMessage = "API version must be an integer"},
    };
}

INSTANTIATE_TEST_CASE_P(
    WebRPCServerHandlerAPIVersionGroup,
    WebRPCServerHandlerInvalidAPIVersionParamTest,
    testing::ValuesIn(generateInvalidVersions()),
    tests::util::kNAME_GENERATOR
);

TEST_P(WebRPCServerHandlerInvalidAPIVersionParamTest, HTTPInvalidAPIVersion)
{
    auto request = fmt::format(
        R"({{
            "method": "server_info",
            "params": [{{
                "api_version": {}
            }}]
        }})",
        GetParam().version
    );

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(session->message, "invalid_API_version");
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_P(WebRPCServerHandlerInvalidAPIVersionParamTest, WSInvalidAPIVersion)
{
    session->upgraded = true;
    auto request = fmt::format(
        R"({{
            "method": "server_info",
            "api_version": {}
        }})",
        GetParam().version
    );

    backend_->setRange(kMIN_SEQ, kMAX_SEQ);

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);

    auto response = boost::json::parse(session->message);
    EXPECT_TRUE(response.is_object());
    EXPECT_TRUE(response.as_object().contains("error"));
    EXPECT_EQ(response.at("error").as_string(), "invalid_API_version");
    EXPECT_TRUE(response.as_object().contains("error_message"));
    EXPECT_EQ(response.at("error_message").as_string(), GetParam().wsMessage);
    EXPECT_TRUE(response.as_object().contains("error_code"));
    EXPECT_EQ(response.at("error_code").as_int64(), static_cast<int64_t>(rpc::ClioError::RpcInvalidApiVersion));
}
