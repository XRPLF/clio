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
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/RPCServerHandler.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/dosguard/DOSGuardMock.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace web;
using namespace util::config;

namespace {

constexpr auto kMinSeq = 10;
constexpr auto kMaxSeq = 30;

}  // namespace

struct MockWsBase : public web::ConnectionBase {
    std::string message;
    boost::beast::http::status lastStatus = boost::beast::http::status::unknown;
    size_t slowDownCallsCounter{0};

    void
    send(std::shared_ptr<std::string> msgType) override
    {
        message += std::string(*msgType);
        lastStatus = boost::beast::http::status::ok;
    }

    void
    send(
        std::string&& msg,  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        boost::beast::http::status status = boost::beast::http::status::ok
    ) override
    {
        message += msg;
        lastStatus = status;
    }

    void
    sendSlowDown(std::string const&) override
    {
        ++slowDownCallsCounter;
    }

    SubscriptionContextPtr
    makeSubscriptionContext(util::TagDecoratorFactory const&) override
    {
        return {};
    }

    MockWsBase(util::TagDecoratorFactory const& factory)
        : web::ConnectionBase(factory, "localhost.fake.ip")
    {
    }
};

struct WebRPCServerHandlerTest : util::prometheus::WithPrometheus,
                                 MockBackendTest,
                                 SyncAsioContextTest {
    util::config::ClioConfigDefinition cfg{
        {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
        {"api_version.default",
         ConfigValue{ConfigType::Integer}.defaultValue(rpc::kApiVersionDefault)},
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(rpc::kApiVersionMin)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(rpc::kApiVersionMax)}
    };
    std::shared_ptr<MockAsyncRPCEngine> rpcEngine = std::make_shared<MockAsyncRPCEngine>();
    std::shared_ptr<MockETLService> etl = std::make_shared<MockETLService>();
    DOSGuardStrictMock dosguard;
    std::shared_ptr<util::TagDecoratorFactory> tagFactory =
        std::make_shared<util::TagDecoratorFactory>(cfg);
    std::shared_ptr<RPCServerHandler<MockAsyncRPCEngine>> handler =
        std::make_shared<RPCServerHandler<MockAsyncRPCEngine>>(
            cfg,
            backend_,
            rpcEngine,
            etl,
            dosguard
        );
    std::shared_ptr<MockWsBase> session = std::make_shared<MockWsBase>(*tagFactory);
};

TEST_F(WebRPCServerHandlerTest, HTTPDefaultPath)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResult = "{}";
    static constexpr auto kResponse = R"JSON({
        "result": {
            "status": "success"
        },
        "warnings": [
            {
                "id": 2001,
                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
            }
        ]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPRejectedByDosguard)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(false));

    (*handler)(kRequest, session);
    EXPECT_EQ(session->slowDownCallsCounter, 1);
}

TEST_F(WebRPCServerHandlerTest, HTTPRejectedByDosguardAfterParsing)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_))
        .WillOnce(testing::Return(false));

    (*handler)(kRequest, session);
    EXPECT_EQ(session->slowDownCallsCounter, 1);
}

TEST_F(WebRPCServerHandlerTest, WsNormalPath)
{
    session->upgraded = true;
    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99,
        "api_version": 2
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResult = "{}";
    static constexpr auto kResponse = R"JSON({
        "result": {},
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
    })JSON";
    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsRejectedByDosguard)
{
    session->upgraded = true;
    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99,
        "api_version": 2
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(false));

    (*handler)(kRequest, session);
    EXPECT_EQ(session->slowDownCallsCounter, 1);
}

TEST_F(WebRPCServerHandlerTest, WsRejectedByDosguardAfterParsing)
{
    session->upgraded = true;
    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99,
        "api_version": 2
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(false));

    (*handler)(kRequest, session);
    EXPECT_EQ(session->slowDownCallsCounter, 1);
}

TEST_F(WebRPCServerHandlerTest, HTTPForwardedPath)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    // Note: forwarding always goes thru WS API
    static constexpr auto kResult = R"JSON({
        "result": {
            "index": 1
        },
        "forwarded": true
    })JSON";
    static constexpr auto kResponse = R"JSON({
        "result": {
                "index": 1,
                "status": "success"
        },
        "forwarded": true,
        "warnings": [
            {
                "id": 2001,
                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
            }
        ]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPForwardedErrorPath)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    // Note: forwarding always goes thru WS API
    static constexpr auto kResult = R"JSON({
        "error": "error",
        "error_code": 123,
        "error_message": "error message",
        "status": "error",
        "type": "response",
        "forwarded": true
    })JSON";
    static constexpr auto kResponse = R"JSON({
        "result": {
            "error": "error",
            "error_code": 123,
            "error_message": "error message",
            "status": "error",
            "type": "response"
        },
        "forwarded": true,
        "warnings": [
            {
                "id": 2001,
                "message": "This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
            }
        ]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsForwardedPath)
{
    session->upgraded = true;
    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    // Note: forwarding always goes thru WS API
    static constexpr auto kResult = R"JSON({
        "result": {
            "index": 1
        },
        "forwarded": true
    })JSON";
    static constexpr auto kResponse = R"JSON({
        "result": {
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsForwardedErrorPath)
{
    session->upgraded = true;
    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    // Note: forwarding always goes thru WS API
    static constexpr auto kResult = R"JSON({
        "error": "error",
        "error_code": 123,
        "error_message": "error message",
        "status": "error",
        "type": "response",
        "forwarded": true
    })JSON";
    // WS error responses, unlike their successful counterpart, contain everything on top level
    // without "result"
    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));

    // Forwarded errors counted as successful:
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);
    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPErrorPath)
{
    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kRequestJson = R"JSON({
        "method": "ledger",
        "params": [
            {
            "ledger_index": "xx"
            }
        ]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(
        dosguard, request(session->clientIp(), boost::json::parse(kRequestJson).as_object())
    )
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(
            testing::Return(
                rpc::Result{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "ledgerIndexMalformed"}
                }
            )
        );

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequestJson, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsErrorPath)
{
    session->upgraded = true;
    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kRequestJson = R"JSON({
        "command": "ledger",
        "ledger_index": "xx",
        "id": "123",
        "api_version": 2
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(
        dosguard, request(session->clientIp(), boost::json::parse(kRequestJson).as_object())
    )
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(
            testing::Return(
                rpc::Result{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "ledgerIndexMalformed"}
                }
            )
        );

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    (*handler)(kRequestJson, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPNotReady)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyNotReady).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsNotReady)
{
    session->upgraded = true;

    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99
    })JSON";

    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyNotReady).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPBadSyntaxWhenRequestSubscribe)
{
    static constexpr auto kRequest = R"JSON({"method": "subscribe"})JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_)).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPMissingCommand)
{
    static constexpr auto kRequest = R"JSON({"method2": "server_info"})JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse = "Null method";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_)).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(session->message, kResponse);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPCommandNotString)
{
    static constexpr auto kRequest = R"JSON({"method": 1})JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse = "method is not string";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_)).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(session->message, kResponse);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPCommandIsEmpty)
{
    static constexpr auto kRequest = R"JSON({"method": ""})JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse = "method is empty";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_)).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(session->message, kResponse);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WsMissingCommand)
{
    session->upgraded = true;
    static constexpr auto kRequest = R"JSON({
        "command2": "server_info",
        "id": 99
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse = R"JSON({
        "error": "missingCommand",
        "error_code": 6001,
        "error_message": "Method/Command is not specified or is not a string.",
        "status": "error",
        "type": "response",
        "id": 99,
        "request": {
            "command2": "server_info",
            "id": 99
        }
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPParamsUnparsableNotArray)
{
    static constexpr auto kResponse = "params unparsable";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kRequestJson = R"JSON({
        "method": "ledger",
        "params": "wrong"
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_)).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequestJson, session);
    EXPECT_EQ(session->message, kResponse);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPParamsUnparsableArrayWithDigit)
{
    static constexpr auto kResponse = "params unparsable";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kRequestJson = R"JSON({
        "method": "ledger",
        "params": [1]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), testing::_)).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequestJson, session);
    EXPECT_EQ(session->message, kResponse);
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, HTTPInternalError)
{
    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kRequestJson = R"JSON({
        "method": "ledger",
        "params": [{}]
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(
        dosguard, request(session->clientIp(), boost::json::parse(kRequestJson).as_object())
    )
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .Times(1)
        .WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*handler)(kRequestJson, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsInternalError)
{
    session->upgraded = true;

    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kRequestJson = R"JSON({
        "command": "ledger",
        "id": "123"
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(
        dosguard, request(session->clientIp(), boost::json::parse(kRequestJson).as_object())
    )
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyInternalError).Times(1);
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .Times(1)
        .WillOnce(testing::Throw(std::runtime_error("MyError")));

    (*handler)(kRequestJson, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPOutDated)
{
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResult = "{}";
    static constexpr auto kResponse = R"JSON({
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsOutdated)
{
    session->upgraded = true;

    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResult = "{}";
    static constexpr auto kResponse = R"JSON({
        "result": {},
        "id": 99,
        "status": "success",
        "type": "response",
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
    })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(rpc::Result{boost::json::parse(kResult).as_object()}));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, WsTooBusy)
{
    session->upgraded = true;

    auto localRpcEngine = std::make_shared<MockRPCEngine>();
    auto localHandler = std::make_shared<RPCServerHandler<MockRPCEngine>>(
        cfg, backend_, localRpcEngine, etl, dosguard
    );
    static constexpr auto kRequest = R"JSON({
        "command": "server_info",
        "id": 99
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse =
        R"JSON({
            "error": "tooBusy",
            "error_code": 9,
            "error_message": "The server is too busy to help you now.",
            "status": "error",
            "type": "response"
        })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*localRpcEngine, notifyTooBusy).Times(1);
    EXPECT_CALL(*localRpcEngine, post).WillOnce(testing::Return(false));

    (*localHandler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPTooBusy)
{
    auto localRpcEngine = std::make_shared<MockRPCEngine>();
    auto localHandler = std::make_shared<RPCServerHandler<MockRPCEngine>>(
        cfg, backend_, localRpcEngine, etl, dosguard
    );
    static constexpr auto kRequest = R"JSON({
        "method": "server_info",
        "params": [{}]
    })JSON";

    backend_->setRange(kMinSeq, kMaxSeq);

    static constexpr auto kResponse =
        R"JSON({
            "error": "tooBusy",
            "error_code": 9,
            "error_message": "The server is too busy to help you now.",
            "status": "error",
            "type": "response"
        })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(kRequest).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*localRpcEngine, notifyTooBusy).Times(1);
    EXPECT_CALL(*localRpcEngine, post).WillOnce(testing::Return(false));

    (*localHandler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

TEST_F(WebRPCServerHandlerTest, HTTPRequestNotJson)
{
    static constexpr auto kRequest = "not json";
    static constexpr auto kResponsePrefix = "Unable to parse JSON from the request";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_THAT(session->message, testing::StartsWith(kResponsePrefix));
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_F(WebRPCServerHandlerTest, WsRequestNotJson)
{
    session->upgraded = true;
    static constexpr auto kRequest = "not json";
    static constexpr auto kResponse =
        R"JSON({
            "error": "badSyntax",
            "error_code": 1,
            "error_message": "Syntax error.",
            "status": "error",
            "type": "response"
        })JSON";

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(kRequest, session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(kResponse));
}

struct InvalidAPIVersionTestBundle {
    std::string testName;
    std::string version;
    std::string wsMessage;
};

// parameterized test cases for parameters check
struct WebRPCServerHandlerInvalidAPIVersionParamTest
    : public WebRPCServerHandlerTest,
      public testing::WithParamInterface<InvalidAPIVersionTestBundle> {};

auto
generateInvalidVersions()
{
    return std::vector<InvalidAPIVersionTestBundle>{
        {.testName = "v0",
         .version = "0",
         .wsMessage = fmt::format(
             "Requested API version is lower than minimum supported ({})", rpc::kApiVersionMin
         )},
        {.testName = "v4",
         .version = "4",
         .wsMessage = fmt::format(
             "Requested API version is higher than maximum supported ({})", rpc::kApiVersionMax
         )},
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
    tests::util::kNameGenerator
);

TEST_P(WebRPCServerHandlerInvalidAPIVersionParamTest, HTTPInvalidAPIVersion)
{
    auto request = fmt::format(
        R"JSON({{
            "method": "server_info",
            "params": [{{
                "api_version": {}
            }}]
        }})JSON",
        GetParam().version
    );

    backend_->setRange(kMinSeq, kMaxSeq);

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(request).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);
    EXPECT_EQ(session->message, "invalid_API_version");
    EXPECT_EQ(session->lastStatus, boost::beast::http::status::bad_request);
}

TEST_P(WebRPCServerHandlerInvalidAPIVersionParamTest, WSInvalidAPIVersion)
{
    session->upgraded = true;
    auto request = fmt::format(
        R"JSON({{
            "method": "server_info",
            "api_version": {}
        }})JSON",
        GetParam().version
    );

    backend_->setRange(kMinSeq, kMaxSeq);

    EXPECT_CALL(dosguard, isOk(session->clientIp())).WillOnce(testing::Return(true));
    EXPECT_CALL(dosguard, request(session->clientIp(), boost::json::parse(request).as_object()))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(*rpcEngine, notifyBadSyntax).Times(1);

    (*handler)(request, session);

    auto response = boost::json::parse(session->message);
    EXPECT_TRUE(response.is_object());

    EXPECT_TRUE(response.as_object().contains("error"));
    EXPECT_EQ(response.at("error").as_string(), "invalid_API_version");

    EXPECT_TRUE(response.as_object().contains("error_code"));
    EXPECT_EQ(
        response.at("error_code").as_int64(),
        static_cast<int64_t>(rpc::ClioError::RpcInvalidApiVersion)
    );

    EXPECT_TRUE(response.as_object().contains("error_message"));
    EXPECT_EQ(response.at("error_message").as_string(), GetParam().wsMessage);
}
