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

#include "rpc/Errors.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/impl/ErrorHandling.hpp"
#include "web/interface/ConnectionBaseMock.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

using namespace web::impl;
using namespace web;
using namespace util::config;

struct ErrorHandlingTests : NoLoggerFixture {
    util::TagDecoratorFactory tagFactory_{ClioConfigDefinition{
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
    }};
    std::string const clientIp_ = "some ip";
    ConnectionBaseStrictMockPtr connection_ =
        std::make_shared<testing::StrictMock<ConnectionBaseMock>>(tagFactory_, clientIp_);
};

struct ErrorHandlingComposeErrorTestBundle {
    std::string testName;
    bool connectionUpgraded;
    std::optional<boost::json::object> request;
    boost::json::object expectedResult;
};

struct ErrorHandlingComposeErrorTest : ErrorHandlingTests,
                                       testing::WithParamInterface<ErrorHandlingComposeErrorTestBundle> {};

TEST_P(ErrorHandlingComposeErrorTest, composeError)
{
    connection_->upgraded = GetParam().connectionUpgraded;
    ErrorHelper const errorHelper{connection_, GetParam().request};
    auto const result = errorHelper.composeError(rpc::RippledError::rpcNOT_READY);
    EXPECT_EQ(boost::json::serialize(result), boost::json::serialize(GetParam().expectedResult));
}

INSTANTIATE_TEST_CASE_P(
    ErrorHandlingComposeErrorTestGroup,
    ErrorHandlingComposeErrorTest,
    testing::ValuesIn(
        {ErrorHandlingComposeErrorTestBundle{
             "NoRequest_UpgradedConnection",
             true,
             std::nullopt,
             {{"error", "notReady"},
              {"error_code", 13},
              {"error_message", "Not ready to handle this request."},
              {"status", "error"},
              {"type", "response"}}
         },
         ErrorHandlingComposeErrorTestBundle{
             "NoRequest_NotUpgradedConnection",
             false,
             std::nullopt,
             {{"result",
               {{"error", "notReady"},
                {"error_code", 13},
                {"error_message", "Not ready to handle this request."},
                {"status", "error"},
                {"type", "response"}}}}
         },
         ErrorHandlingComposeErrorTestBundle{
             "Request_UpgradedConnection",
             true,
             boost::json::object{{"id", 1}, {"api_version", 2}},
             {{"error", "notReady"},
              {"error_code", 13},
              {"error_message", "Not ready to handle this request."},
              {"status", "error"},
              {"type", "response"},
              {"id", 1},
              {"api_version", 2},
              {"request", {{"id", 1}, {"api_version", 2}}}}
         },
         ErrorHandlingComposeErrorTestBundle{
             "Request_NotUpgradedConnection",
             false,
             boost::json::object{{"id", 1}, {"api_version", 2}},
             {{"result",
               {{"error", "notReady"},
                {"error_code", 13},
                {"error_message", "Not ready to handle this request."},
                {"status", "error"},
                {"type", "response"},
                {"id", 1},
                {"request", {{"id", 1}, {"api_version", 2}}}}}}
         }}
    ),
    tests::util::NameGenerator
);

struct ErrorHandlingSendErrorTestBundle {
    std::string testName;
    bool connectionUpgraded;
    rpc::Status status;
    std::string expectedMessage;
    boost::beast::http::status expectedStatus;
};

struct ErrorHandlingSendErrorTest : ErrorHandlingTests,
                                    testing::WithParamInterface<ErrorHandlingSendErrorTestBundle> {};

TEST_P(ErrorHandlingSendErrorTest, sendError)
{
    connection_->upgraded = GetParam().connectionUpgraded;
    ErrorHelper const errorHelper{connection_};

    EXPECT_CALL(*connection_, send(std::string{GetParam().expectedMessage}, GetParam().expectedStatus));
    errorHelper.sendError(GetParam().status);
}

INSTANTIATE_TEST_CASE_P(
    ErrorHandlingSendErrorTestGroup,
    ErrorHandlingSendErrorTest,
    testing::ValuesIn({
        ErrorHandlingSendErrorTestBundle{
            "UpgradedConnection",
            true,
            rpc::Status{rpc::RippledError::rpcTOO_BUSY},
            R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})",
            boost::beast::http::status::ok
        },
        ErrorHandlingSendErrorTestBundle{
            "NotUpgradedConnection_InvalidApiVersion",
            false,
            rpc::Status{rpc::ClioError::rpcINVALID_API_VERSION},
            "invalid_API_version",
            boost::beast::http::status::bad_request
        },
        ErrorHandlingSendErrorTestBundle{
            "NotUpgradedConnection_CommandIsMissing",
            false,
            rpc::Status{rpc::ClioError::rpcCOMMAND_IS_MISSING},
            "Null method",
            boost::beast::http::status::bad_request
        },
        ErrorHandlingSendErrorTestBundle{
            "NotUpgradedConnection_CommandIsEmpty",
            false,
            rpc::Status{rpc::ClioError::rpcCOMMAND_IS_EMPTY},
            "method is empty",
            boost::beast::http::status::bad_request
        },
        ErrorHandlingSendErrorTestBundle{
            "NotUpgradedConnection_CommandNotString",
            false,
            rpc::Status{rpc::ClioError::rpcCOMMAND_NOT_STRING},
            "method is not string",
            boost::beast::http::status::bad_request
        },
        ErrorHandlingSendErrorTestBundle{
            "NotUpgradedConnection_ParamsUnparseable",
            false,
            rpc::Status{rpc::ClioError::rpcPARAMS_UNPARSEABLE},
            "params unparseable",
            boost::beast::http::status::bad_request
        },
        ErrorHandlingSendErrorTestBundle{
            "NotUpgradedConnection_RippledError",
            false,
            rpc::Status{rpc::RippledError::rpcTOO_BUSY},
            R"({"result":{"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"}})",
            boost::beast::http::status::bad_request
        },
    }),
    tests::util::NameGenerator
);

TEST_F(ErrorHandlingTests, sendInternalError)
{
    ErrorHelper const errorHelper{connection_};

    EXPECT_CALL(
        *connection_,
        send(
            std::string{
                R"({"result":{"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"}})"
            },
            boost::beast::http::status::internal_server_error
        )
    );
    errorHelper.sendInternalError();
}

TEST_F(ErrorHandlingTests, sendNotReadyError)
{
    ErrorHelper const errorHelper{connection_};
    EXPECT_CALL(
        *connection_,
        send(
            std::string{
                R"({"result":{"error":"notReady","error_code":13,"error_message":"Not ready to handle this request.","status":"error","type":"response"}})"
            },
            boost::beast::http::status::ok
        )
    );
    errorHelper.sendNotReadyError();
}

TEST_F(ErrorHandlingTests, sendTooBusyError_UpgradedConnection)
{
    connection_->upgraded = true;
    ErrorHelper const errorHelper{connection_};
    EXPECT_CALL(
        *connection_,
        send(
            std::string{
                R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})"
            },
            boost::beast::http::status::ok
        )
    );
    errorHelper.sendTooBusyError();
}

TEST_F(ErrorHandlingTests, sendTooBusyError_NotUpgradedConnection)
{
    connection_->upgraded = false;
    ErrorHelper const errorHelper{connection_};
    EXPECT_CALL(
        *connection_,
        send(
            std::string{
                R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})"
            },
            boost::beast::http::status::service_unavailable
        )
    );
    errorHelper.sendTooBusyError();
}

TEST_F(ErrorHandlingTests, sendJsonParsingError_UpgradedConnection)
{
    connection_->upgraded = true;
    ErrorHelper const errorHelper{connection_};
    EXPECT_CALL(
        *connection_,
        send(
            std::string{
                R"({"error":"badSyntax","error_code":1,"error_message":"Syntax error.","status":"error","type":"response"})"
            },
            boost::beast::http::status::ok
        )
    );
    errorHelper.sendJsonParsingError();
}

TEST_F(ErrorHandlingTests, sendJsonParsingError_NotUpgradedConnection)
{
    connection_->upgraded = false;
    ErrorHelper const errorHelper{connection_};
    EXPECT_CALL(
        *connection_,
        send(std::string{"Unable to parse JSON from the request"}, boost::beast::http::status::bad_request)
    );
    errorHelper.sendJsonParsingError();
}
