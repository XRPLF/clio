//------------------------------------------------------------------------------
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

#include "util/AssignRandomPort.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestHttpSyncClient.hpp"
#include "util/config/Config.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"
#include "web/DOSGuard.hpp"
#include "web/IntervalSweepHandler.hpp"
#include "web/Server.hpp"
#include "web/WhitelistHandler.hpp"
#include "web/impl/AdminVerificationStrategy.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/system/system_error.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace util;
using namespace web::impl;
using namespace web;

boost::json::value
generateJSONWithDynamicPort(std::string_view port)
{
    return boost::json::parse(fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {}
            }},
            "dos_guard": {{
                "max_fetches": 100,
                "sweep_interval": 1000,
                "max_connections": 2,
                "max_requests": 3,
                "whitelist": ["127.0.0.1"]
            }}
        }})JSON",
        port
    ));
}

boost::json::value
generateJSONDataOverload(std::string_view port)
{
    return boost::json::parse(fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {}
            }},
            "dos_guard": {{
                "max_fetches": 100,
                "sweep_interval": 1000,
                "max_connections": 2,
                "max_requests": 1
            }}
        }})JSON",
        port
    ));
}

boost::json::value
addSslConfig(boost::json::value config)
{
    config.as_object()["ssl_key_file"] = TEST_DATA_SSL_KEY_PATH;
    config.as_object()["ssl_cert_file"] = TEST_DATA_SSL_CERT_PATH;
    return config;
}

struct WebServerTest : NoLoggerFixture {
    ~WebServerTest() override
    {
        work.reset();
        ctx.stop();
        if (runner->joinable())
            runner->join();
    }

    WebServerTest()
    {
        work.emplace(ctx);  // make sure ctx does not stop on its own
        runner.emplace([this] { ctx.run(); });
    }

    // this ctx is for dos timer
    boost::asio::io_context ctxSync;
    std::string const port = std::to_string(tests::util::generateFreePort());
    Config cfg{generateJSONWithDynamicPort(port)};
    IntervalSweepHandler sweepHandler = web::IntervalSweepHandler{cfg, ctxSync};
    WhitelistHandler whitelistHandler = web::WhitelistHandler{cfg};
    DOSGuard dosGuard = web::DOSGuard{cfg, whitelistHandler, sweepHandler};

    Config cfgOverload{generateJSONDataOverload(port)};
    IntervalSweepHandler sweepHandlerOverload = web::IntervalSweepHandler{cfgOverload, ctxSync};
    WhitelistHandler whitelistHandlerOverload = web::WhitelistHandler{cfgOverload};
    DOSGuard dosGuardOverload = web::DOSGuard{cfgOverload, whitelistHandlerOverload, sweepHandlerOverload};
    // this ctx is for http server
    boost::asio::io_context ctx;

private:
    std::optional<boost::asio::io_service::work> work;
    std::optional<std::thread> runner;
};

class EchoExecutor {
public:
    void
    operator()(std::string const& reqStr, std::shared_ptr<web::ConnectionBase> const& ws)
    {
        ws->send(std::string(reqStr), http::status::ok);
    }

    void
    operator()(boost::beast::error_code /* ec */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
    }
};

class ExceptionExecutor {
public:
    void
    operator()(std::string const& /* req */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
        throw std::runtime_error("MyError");
    }

    void
    operator()(boost::beast::error_code /* ec */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
    }
};

namespace {

template <class Executor>
std::shared_ptr<web::HttpServer<Executor>>
makeServerSync(
    util::Config const& config,
    boost::asio::io_context& ioc,
    web::DOSGuard& dosGuard,
    std::shared_ptr<Executor> const& handler
)
{
    auto server = std::shared_ptr<web::HttpServer<Executor>>();
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    boost::asio::dispatch(ioc.get_executor(), [&]() mutable {
        server = web::make_HttpServer(config, ioc, dosGuard, handler);
        {
            std::lock_guard const lk(m);
            ready = true;
        }
        cv.notify_one();
    });
    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return ready; });
    }
    return server;
}

}  // namespace

TEST_F(WebServerTest, Http)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const res = HttpSyncClient::syncPost("localhost", port, R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
}

TEST_F(WebServerTest, Ws)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
}

TEST_F(WebServerTest, HttpInternalError)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const res = HttpSyncClient::syncPost("localhost", port, R"({})");
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"})"
    );
}

TEST_F(WebServerTest, WsInternalError)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(R"({"id":"id1"})");
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","id":"id1","request":{"id":"id1"}})"
    );
}

TEST_F(WebServerTest, WsInternalErrorNotJson)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost("not json");
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","request":"not json"})"
    );
}

TEST_F(WebServerTest, IncompleteSslConfig)
{
    auto e = std::make_shared<EchoExecutor>();

    auto jsonConfig = generateJSONWithDynamicPort(port);
    jsonConfig.as_object()["ssl_key_file"] = TEST_DATA_SSL_KEY_PATH;

    auto const server = makeServerSync(Config{jsonConfig}, ctx, dosGuard, e);
    EXPECT_EQ(server, nullptr);
}

TEST_F(WebServerTest, WrongSslConfig)
{
    auto e = std::make_shared<EchoExecutor>();

    auto jsonConfig = generateJSONWithDynamicPort(port);
    jsonConfig.as_object()["ssl_key_file"] = TEST_DATA_SSL_KEY_PATH;
    jsonConfig.as_object()["ssl_cert_file"] = "wrong_path";

    auto const server = makeServerSync(Config{jsonConfig}, ctx, dosGuard, e);
    EXPECT_EQ(server, nullptr);
}

TEST_F(WebServerTest, Https)
{
    auto e = std::make_shared<EchoExecutor>();
    cfg = Config{addSslConfig(generateJSONWithDynamicPort(port))};
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const res = HttpsSyncClient::syncPost("localhost", port, R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
}

TEST_F(WebServerTest, Wss)
{
    auto e = std::make_shared<EchoExecutor>();
    cfg = Config{addSslConfig(generateJSONWithDynamicPort(port))};
    auto server = makeServerSync(cfg, ctx, dosGuard, e);
    WebServerSslSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
}

TEST_F(WebServerTest, HttpRequestOverload)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    auto res = HttpSyncClient::syncPost("localhost", port, R"({})");
    EXPECT_EQ(res, "{}");
    res = HttpSyncClient::syncPost("localhost", port, R"({})");
    EXPECT_EQ(
        res,
        R"({"error":"slowDown","error_code":10,"error_message":"You are placing too much load on the server.","status":"error","type":"response"})"
    );
}

TEST_F(WebServerTest, WsRequestOverload)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto res = wsClient.syncPost(R"({})");
    wsClient.disconnect();
    EXPECT_EQ(res, "{}");
    WebSocketSyncClient wsClient2;
    wsClient2.connect("localhost", port);
    res = wsClient2.syncPost(R"({})");
    wsClient2.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"slowDown","error_code":10,"error_message":"You are placing too much load on the server.","status":"error","type":"response","request":{}})"
    );
}

TEST_F(WebServerTest, HttpPayloadOverload)
{
    std::string const s100(100, 'a');
    auto e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    auto const res = HttpSyncClient::syncPost("localhost", port, fmt::format(R"({{"payload":"{}"}})", s100));
    EXPECT_EQ(
        res,
        R"({"payload":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","warning":"load","warnings":[{"id":2003,"message":"You are about to be rate limited"}]})"
    );
}

TEST_F(WebServerTest, WsPayloadOverload)
{
    std::string const s100(100, 'a');
    auto e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(fmt::format(R"({{"payload":"{}"}})", s100));
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"payload":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","warning":"load","warnings":[{"id":2003,"message":"You are about to be rate limited"}]})"
    );
}

TEST_F(WebServerTest, WsTooManyConnection)
{
    auto e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    // max connection is 2, exception should happen when the third connection is made
    WebSocketSyncClient wsClient1;
    wsClient1.connect("localhost", port);
    WebSocketSyncClient wsClient2;
    wsClient2.connect("localhost", port);
    bool exceptionThrown = false;
    try {
        WebSocketSyncClient wsClient3;
        wsClient3.connect("localhost", port);
    } catch (boost::system::system_error const& ex) {
        exceptionThrown = true;
        EXPECT_EQ(ex.code(), boost::beast::websocket::error::upgrade_declined);
    }
    wsClient1.disconnect();
    wsClient2.disconnect();
    EXPECT_TRUE(exceptionThrown);
}

std::string
JSONServerConfigWithAdminPassword(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret"
            }}
        }})JSON",
        port
    );
}

std::string
JSONServerConfigWithLocalAdmin(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {},
                "local_admin": true
            }}
        }})JSON",
        port
    );
}

std::string
JSONServerConfigWithBothAdminPasswordAndLocalAdminFalse(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret",
                "local_admin": false
            }}
        }})JSON",
        port
    );
}

std::string
JSONServerConfigWithNoSpecifiedAdmin(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {}
            }}
        }})JSON",
        port
    );
}

// get this value from online sha256 generator
static auto constexpr SecretSha256 = "2bb80d537b1da3e38bd30361aa855686bde0eacd7162fef6a25fe97bf527a25b";

class AdminCheckExecutor {
public:
    void
    operator()(std::string const& reqStr, std::shared_ptr<web::ConnectionBase> const& ws)
    {
        auto response = fmt::format("{} {}", reqStr, ws->isAdmin() ? "admin" : "user");
        ws->send(std::move(response), http::status::ok);
    }

    void
    operator()(boost::beast::error_code /* ec */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
    }
};

struct WebServerAdminTestParams {
    std::string config;
    std::vector<WebHeader> headers;
    std::string expectedResponse;
};

class WebServerAdminTest : public WebServerTest, public ::testing::WithParamInterface<WebServerAdminTestParams> {};

TEST_P(WebServerAdminTest, WsAdminCheck)
{
    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(GetParam().config)};
    auto server = makeServerSync(serverConfig, ctx, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    uint32_t const webServerPort = serverConfig.value<uint32_t>("server.port");
    wsClient.connect("localhost", std::to_string(webServerPort), GetParam().headers);
    std::string const request = "Why hello";
    auto const res = wsClient.syncPost(request);
    wsClient.disconnect();
    EXPECT_EQ(res, fmt::format("{} {}", request, GetParam().expectedResponse));
}

TEST_P(WebServerAdminTest, HttpAdminCheck)
{
    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(GetParam().config)};
    auto server = makeServerSync(serverConfig, ctx, dosGuardOverload, e);
    std::string const request = "Why hello";
    uint32_t const webServerPort = serverConfig.value<uint32_t>("server.port");
    auto const res = HttpSyncClient::syncPost("localhost", std::to_string(webServerPort), request, GetParam().headers);
    EXPECT_EQ(res, fmt::format("{} {}", request, GetParam().expectedResponse));
}

INSTANTIATE_TEST_CASE_P(
    WebServerAdminTestsSuit,
    WebServerAdminTest,
    ::testing::Values(
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, "")},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, "s")},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, SecretSha256)},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(
                http::field::authorization,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecretSha256)
            )},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithBothAdminPasswordAndLocalAdminFalse(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, SecretSha256)},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithBothAdminPasswordAndLocalAdminFalse(tests::util::generateFreePort()),
            .headers = {WebHeader(
                http::field::authorization,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecretSha256)
            )},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(
                http::field::authentication_info,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecretSha256)
            )},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithLocalAdmin(tests::util::generateFreePort()),
            .headers = {},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithNoSpecifiedAdmin(tests::util::generateFreePort()),
            .headers = {},
            .expectedResponse = "admin"
        }

    )
);

TEST_F(WebServerTest, AdminErrorCfgTestBothAdminPasswordAndLocalAdminSet)
{
    uint32_t webServerPort = tests::util::generateFreePort();
    std::string const JSONServerConfigWithBothAdminPasswordAndLocalAdmin = fmt::format(
        R"JSON({{
        "server":{{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret",
                "local_admin": true
            }}
    }})JSON",
        webServerPort
    );

    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithBothAdminPasswordAndLocalAdmin)};
    EXPECT_THROW(web::make_HttpServer(serverConfig, ctx, dosGuardOverload, e), std::logic_error);
}

TEST_F(WebServerTest, AdminErrorCfgTestBothAdminPasswordAndLocalAdminFalse)
{
    uint32_t webServerPort = tests::util::generateFreePort();
    std::string const JSONServerConfigWithNoAdminPasswordAndLocalAdminFalse = fmt::format(
        R"JSON({{
        "server": {{
            "ip": "0.0.0.0",
            "port": {},
            "local_admin": false
        }}
    }})JSON",
        webServerPort
    );

    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithNoAdminPasswordAndLocalAdminFalse)};
    EXPECT_THROW(web::make_HttpServer(serverConfig, ctx, dosGuardOverload, e), std::logic_error);
}

struct WebServerPrometheusTest : util::prometheus::WithPrometheus, WebServerTest {};

TEST_F(WebServerPrometheusTest, rejectedWithoutAdminPassword)
{
    auto e = std::make_shared<EchoExecutor>();
    uint32_t const webServerPort = tests::util::generateFreePort();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithAdminPassword(webServerPort))};
    auto server = makeServerSync(serverConfig, ctx, dosGuard, e);
    auto const res = HttpSyncClient::syncGet("localhost", std::to_string(webServerPort), "", "/metrics");
    EXPECT_EQ(res, "Only admin is allowed to collect metrics");
}

TEST_F(WebServerPrometheusTest, rejectedIfPrometheusIsDisabled)
{
    uint32_t webServerPort = tests::util::generateFreePort();
    std::string const JSONServerConfigWithDisabledPrometheus = fmt::format(
        R"JSON({{
        "server":{{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret"
            }},
        "prometheus": {{ "enabled": false }}
    }})JSON",
        webServerPort
    );

    auto e = std::make_shared<EchoExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithDisabledPrometheus)};
    PrometheusService::init(serverConfig);
    auto server = makeServerSync(serverConfig, ctx, dosGuard, e);
    auto const res = HttpSyncClient::syncGet(
        "localhost",
        std::to_string(webServerPort),
        "",
        "/metrics",
        {WebHeader(
            http::field::authorization,
            fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecretSha256)
        )}
    );
    EXPECT_EQ(res, "Prometheus is disabled in clio config");
}

TEST_F(WebServerPrometheusTest, validResponse)
{
    uint32_t const webServerPort = tests::util::generateFreePort();
    auto& testCounter = PrometheusService::counterInt("test_counter", util::prometheus::Labels());
    ++testCounter;
    auto e = std::make_shared<EchoExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithAdminPassword(webServerPort))};
    auto server = makeServerSync(serverConfig, ctx, dosGuard, e);
    auto const res = HttpSyncClient::syncGet(
        "localhost",
        std::to_string(webServerPort),
        "",
        "/metrics",
        {WebHeader(
            http::field::authorization,
            fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecretSha256)
        )}
    );
    EXPECT_EQ(res, "# TYPE test_counter counter\ntest_counter 1\n\n");
}
