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

#include "rpc/Errors.h"
#include "util/Fixtures.h"
#include "util/MockETLService.h"
#include "util/MockRPCEngine.h"
#include "util/Taggable.h"
#include "util/config/Config.h"
#include "web/RPCServerHandler.h"
#include "web/interface/ConnectionBase.h"

#include <boost/beast/http/status.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/protocol/ErrorCodes.h>

#include <memory>
#include <stdexcept>
#include <string>

using namespace feed;
using namespace web;

constexpr static auto MINSEQ = 10;
constexpr static auto MAXSEQ = 30;

struct MockWsBase : public web::ConnectionBase {
    std::string message;
    boost::beast::http::status lastStatus = boost::beast::http::status::unknown;

    void
    send(std::shared_ptr<std::string> msg_type) override
    {
        message += std::string(*msg_type);
        lastStatus = boost::beast::http::status::ok;
    }

    void
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    send(std::string&& msg, boost::beast::http::status status = boost::beast::http::status::ok) override
    {
        message += msg;
        lastStatus = status;
    }

    MockWsBase(util::TagDecoratorFactory const& factory) : web::ConnectionBase(factory, "localhost.fake.ip")
    {
    }
};

class WebRPCServerHandlerTest : public MockBackendTest, public SyncAsioContextTest {
protected:
    void
    SetUp() override
    {
        MockBackendTest::SetUp();

        etl = std::make_shared<MockETLService>();
        rpcEngine = std::make_shared<MockAsyncRPCEngine>();
        tagFactory = std::make_shared<util::TagDecoratorFactory>(cfg);
        session = std::make_shared<MockWsBase>(*tagFactory);
        handler = std::make_shared<RPCServerHandler<MockAsyncRPCEngine, MockETLService>>(cfg, backend, rpcEngine, etl);
    }

    void
    TearDown() override
    {
        MockBackendTest::TearDown();
    }

    std::shared_ptr<MockAsyncRPCEngine> rpcEngine;
    std::shared_ptr<MockETLService> etl;
    std::shared_ptr<util::TagDecoratorFactory> tagFactory;
    std::shared_ptr<RPCServerHandler<MockAsyncRPCEngine, MockETLService>> handler;
    std::shared_ptr<MockWsBase> session;
    util::Config cfg;
};

TEST_F(WebRPCServerHandlerTest, HTTPDefaultPath)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
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
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsNormalPath)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99,
                                        "api_version": 2
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
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
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPForwardedPath)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr result = R"({
                                        "result": {
                                            "index": 1
                                        },
                                        "forwarded": true
                                    })";
    static auto constexpr response = R"({
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
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsForwardedPath)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr result = R"({
                                        "result": {
                                            "index": 1
                                        },
                                        "forwarded": true
                                   })";
    static auto constexpr response = R"({
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
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPErrorPath)
{
    static auto constexpr response = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [
                                                {
                                                "ledger_index": "xx"
                                                }
                                            ]
                                        })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Status{rpc::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}));

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(requestJSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsErrorPath)
{
    session->upgraded = true;
    static auto constexpr response = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr requestJSON = R"({
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123",
                                            "api_version": 2
                                        })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Status{rpc::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}));

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(requestJSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPNotReady)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    static auto constexpr response = R"({
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

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsNotReady)
{
    session->upgraded = true;

    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    static auto constexpr response = R"({
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

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPInvalidAPIVersion)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{
                                            "api_version": null
                                        }]
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = "invalid_API_version";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(session->message, response);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WSInvalidAPIVersion)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "api_version": null
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = R"({
                                        "error": "invalid_API_version",
                                        "error_code": 6000,
                                        "error_message": "API version must be an integer",
                                        "status": "error",
                                        "type": "response",
                                        "request": {
                                            "method": "server_info",
                                            "api_version": null
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPBadSyntaxWhenRequestSubscribe)
{
    static auto constexpr request = R"({"method": "subscribe"})";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = R"({
                                        "result": {
                                            "error": "badSyntax",
                                            "error_code": 1,
                                            "error_message": "Subscribe and unsubscribe are only allowed or websocket.",
                                            "status": "error",
                                            "type": "response",
                                            "request": {
                                                "method": "subscribe",
                                                "params": [{}]
                                            }
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPMissingCommand)
{
    static auto constexpr request = R"({"method2": "server_info"})";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = "Null method";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(session->message, response);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPCommandNotString)
{
    static auto constexpr request = R"({"method": 1})";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = "method is not string";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(session->message, response);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPCommandIsEmpty)
{
    static auto constexpr request = R"({"method": ""})";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = "method is empty";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(session->message, response);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WsMissingCommand)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "command2": "server_info",
                                        "id": 99
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response = R"({
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

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPParamsUnparseableNotArray)
{
    static auto constexpr response = "params unparseable";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": "wrong"
                                        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(requestJSON, session);
    EXPECT_EQ(session->message, response);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPParamsUnparseableArrayWithDigit)
{
    static auto constexpr response = "params unparseable";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [1]
                                        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(requestJSON, session);
    EXPECT_EQ(session->message, response);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPInternalError)
{
    static auto constexpr response = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [{}]
                                        })";

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*handler)(requestJSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsInternalError)
{
    session->upgraded = true;

    static auto constexpr response = R"({
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

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr requestJSON = R"({
                                            "command": "ledger",
                                            "id": "123"
                                        })";

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*handler)(requestJSON, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPOutDated)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
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
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsOutdated)
{
    session->upgraded = true;

    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
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
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, WsTooBusy)
{
    session->upgraded = true;

    auto localRpcEngine = std::make_shared<MockRPCEngine>();
    auto localHandler =
        std::make_shared<RPCServerHandler<MockRPCEngine, MockETLService>>(cfg, backend, localRpcEngine, etl);
    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response =
        R"({
            "error": "tooBusy",
            "error_code": 9,
            "error_message": "The server is too busy to help you now.",
            "status": "error",
            "type": "response"
        })";

    EXPECT_CALL(*localRpcEngine, notifyTooBusy).Times(1);
    EXPECT_CALL(*localRpcEngine, post).WillOnce(testing::Return(false));

    (*localHandler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPTooBusy)
{
    auto localRpcEngine = std::make_shared<MockRPCEngine>();
    auto localHandler =
        std::make_shared<RPCServerHandler<MockRPCEngine, MockETLService>>(cfg, backend, localRpcEngine, etl);
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    backend->setRange(MINSEQ, MAXSEQ);

    static auto constexpr response =
        R"({
            "error": "tooBusy",
            "error_code": 9,
            "error_message": "The server is too busy to help you now.",
            "status": "error",
            "type": "response"
        })";

    EXPECT_CALL(*localRpcEngine, notifyTooBusy).Times(1);
    EXPECT_CALL(*localRpcEngine, post).WillOnce(testing::Return(false));

    (*localHandler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCServerHandlerTest, HTTPRequestNotJson)
{
    static auto constexpr request = "not json";
    static auto constexpr responsePrefix = "Unable to parse JSON from the request";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_THAT(session->message, testing::StartsWith(responsePrefix));
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WsRequestNotJson)
{
    session->upgraded = true;
    static auto constexpr request = "not json";
    static auto constexpr response =
        R"({
            "error": "badSyntax",
            "error_code": 1,
            "error_message": "Syntax error.",
            "status": "error",
            "type": "response"
        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}
