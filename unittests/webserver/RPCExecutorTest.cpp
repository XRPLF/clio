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

#include <util/Fixtures.h>
#include <util/MockETLService.h>
#include <util/MockRPCEngine.h>
#include <webserver/RPCExecutor.h>

#include <chrono>
#include <gtest/gtest.h>

using namespace std::chrono_literals;

constexpr static auto MINSEQ = 10;
constexpr static auto MAXSEQ = 30;

struct MockWsBase : public Server::ConnectionBase
{
    std::string message;

    void
    send(std::shared_ptr<std::string> msg_type) override
    {
        message += std::string(msg_type->data());
    }

    void
    send(std::string&& msg, boost::beast::http::status status = boost::beast::http::status::ok) override
    {
        message += std::string(msg.data());
    }

    MockWsBase(util::TagDecoratorFactory const& factory) : Server::ConnectionBase(factory, "localhost.fake.ip")
    {
    }
};

class WebRPCExecutorTest : public MockBackendTest
{
protected:
    void
    SetUp() override
    {
        MockBackendTest::SetUp();

        etl = std::make_shared<MockETLService>();
        rpcEngine = std::make_shared<MockAsyncRPCEngine>();
        tagFactory = std::make_shared<util::TagDecoratorFactory>(cfg);
        subManager = std::make_shared<SubscriptionManager>(cfg, mockBackendPtr);
        session = std::make_shared<MockWsBase>(*tagFactory);
        rpcExecutor = std::make_shared<RPCExecutor<MockAsyncRPCEngine, MockETLService>>(
            cfg, mockBackendPtr, rpcEngine, etl, subManager);
    }

    void
    TearDown() override
    {
        MockBackendTest::TearDown();
    }

    std::shared_ptr<MockAsyncRPCEngine> rpcEngine;
    std::shared_ptr<MockETLService> etl;
    std::shared_ptr<SubscriptionManager> subManager;
    std::shared_ptr<util::TagDecoratorFactory> tagFactory;
    std::shared_ptr<RPCExecutor<MockAsyncRPCEngine, MockETLService>> rpcExecutor;
    std::shared_ptr<MockWsBase> session;
    clio::Config cfg;
};

TEST_F(WebRPCExecutorTest, HTTPDefaultPath)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                            "status":"success"
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsNormalPath)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                        },
                                        "id":99,
                                        "status":"success",
                                        "type":"response",
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPForwardedPath)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

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
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                            ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsForwardedPath)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

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
                                        "id":99,
                                        "status":"success",
                                        "type":"response",
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPErrorPath)
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
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [
                                                {
                                                "ledger_index": "xx"
                                                }
                                            ]
                                        })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}));

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*rpcExecutor)(std::move(requestJSON), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsErrorPath)
{
    session->upgraded = true;
    static auto constexpr response = R"({
                                        "id": "123",
                                        "error": "invalidParams",
                                        "error_code": 31,
                                        "error_message": "ledgerIndexMalformed",
                                        "status": "error",
                                        "type": "response",
                                        "request": {
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123"
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123"
                                        })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}));

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*rpcExecutor)(std::move(requestJSON), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPNotReady)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    static auto constexpr response = R"({
                                        "result":{
                                            "error":"notReady",
                                            "error_code":13,
                                            "error_message":"Not ready to handle this request.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method":"server_info",
                                                "params":[
                                                    {
                                                    
                                                    }
                                                ]
                                            }
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyNotReady).Times(1);

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsNotReady)
{
    session->upgraded = true;

    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    static auto constexpr response = R"({
                                        "error":"notReady",
                                        "error_code":13,
                                        "error_message":"Not ready to handle this request.",
                                        "status":"error",
                                        "type":"response",
                                        "id":99,
                                        "request":{
                                            "command":"server_info",
                                            "id":99
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyNotReady).Times(1);

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPBadSyntax)
{
    static auto constexpr request = R"({"method2": "server_info"})";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response = R"({
                                        "result":{
                                            "error":"badSyntax",
                                            "error_code":1,
                                            "error_message":"Syntax error.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method2":"server_info",
                                                "params":[{}]
                                            }
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPBadSyntaxWhenRequestSubscribe)
{
    static auto constexpr request = R"({"method": "subscribe"})";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response = R"({
                                        "result":{
                                            "error":"badSyntax",
                                            "error_code":1,
                                            "error_message":"Syntax error.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method":"subscribe",
                                                "params":[{}]
                                            }
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsBadSyntax)
{
    session->upgraded = true;
    static auto constexpr request = R"({
                                        "command2": "server_info",
                                        "id": 99
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response = R"({
                                        "error":"badSyntax",
                                        "error_code":1,
                                        "error_message":"Syntax error.",
                                        "status":"error",
                                        "type":"response",
                                        "id":99,
                                        "request":{
                                            "command2":"server_info",
                                            "id":99
                                        }
                                    })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPInternalError)
{
    static auto constexpr response = R"({
                                        "result": {
                                            "error":"internal",
                                            "error_code":73,
                                            "error_message":"Internal error.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method": "ledger",
                                                "params": [
                                                    {

                                                    }
                                                ]
                                            }
                                        }
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [
                                                {

                                                }
                                            ]
                                        })";

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*rpcExecutor)(std::move(requestJSON), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsInternalError)
{
    session->upgraded = true;

    static auto constexpr response = R"({
                                        "error":"internal",
                                        "error_code":73,
                                        "error_message":"Internal error.",
                                        "status":"error",
                                        "type":"response",
                                        "id":"123",
                                        "request":{
                                            "command":"ledger",
                                            "id":"123"
                                        }
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "command": "ledger",
                                            "id": "123"
                                        })";

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*rpcExecutor)(std::move(requestJSON), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPOutDated)
{
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                            "status":"success"
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            },
                                            {
                                                "id":2002,
                                                "message":"This server may be out of date"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsOutdated)
{
    session->upgraded = true;

    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                        },
                                        "id":99,
                                        "status":"success",
                                        "type":"response",
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            },
                                            {
                                                "id":2002,
                                                "message":"This server may be out of date"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*rpcExecutor)(std::move(request), session);
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsTooBusy)
{
    session->upgraded = true;

    auto rpcEngine2 = std::make_shared<MockRPCEngine>();
    auto rpcExecutor2 =
        std::make_shared<RPCExecutor<MockRPCEngine, MockETLService>>(cfg, mockBackendPtr, rpcEngine2, etl, subManager);
    static auto constexpr request = R"({
                                        "command": "server_info",
                                        "id": 99
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response =
        R"({
            "error":"tooBusy",
            "error_code":9,
            "error_message":"The server is too busy to help you now.",
            "status":"error",
            "type":"response"
        })";

    EXPECT_CALL(*rpcEngine2, notifyTooBusy).Times(1);
    EXPECT_CALL(*rpcEngine2, post).WillOnce(testing::Return(false));

    (*rpcExecutor2)(std::move(request), session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPTooBusy)
{
    auto rpcEngine2 = std::make_shared<MockRPCEngine>();
    auto rpcExecutor2 =
        std::make_shared<RPCExecutor<MockRPCEngine, MockETLService>>(cfg, mockBackendPtr, rpcEngine2, etl, subManager);
    static auto constexpr request = R"({
                                        "method": "server_info",
                                        "params": [{}]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response =
        R"({
            "error":"tooBusy",
            "error_code":9,
            "error_message":"The server is too busy to help you now.",
            "status":"error",
            "type":"response"
        })";

    EXPECT_CALL(*rpcEngine2, notifyTooBusy).Times(1);
    EXPECT_CALL(*rpcEngine2, post).WillOnce(testing::Return(false));

    (*rpcExecutor2)(std::move(request), session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPRequestNotJson)
{
    static auto constexpr request = "not json";
    static auto constexpr response =
        R"({
            "error":"badSyntax",
            "error_code":1,
            "error_message":"Syntax error.",
            "status":"error",
            "type":"response"
        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*rpcExecutor)(std::move(request), session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, WsRequestNotJson)
{
    session->upgraded = true;
    static auto constexpr request = "not json";
    static auto constexpr response =
        R"({
            "error":"badSyntax",
            "error_code":1,
            "error_message":"Syntax error.",
            "status":"error",
            "type":"response"
        })";

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*rpcExecutor)(std::move(request), session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}
