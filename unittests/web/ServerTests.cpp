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

#include "util/Fixtures.h"
#include "util/MockPrometheus.h"
#include "util/TestHttpSyncClient.h"
#include "util/config/Config.h"
#include "util/prometheus/Label.h"
#include "util/prometheus/Prometheus.h"
#include "web/DOSGuard.h"
#include "web/IntervalSweepHandler.h"
#include "web/Server.h"
#include "web/WhitelistHandler.h"
#include "web/impl/AdminVerificationStrategy.h"
#include "web/interface/ConnectionBase.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/json/parse.hpp>
#include <boost/system/system_error.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace util;
using namespace web::detail;
using namespace web;

constexpr static auto JSONData = R"JSON(
    {
        "server":{
            "ip":"0.0.0.0",
            "port":8888
        },
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1000,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": ["127.0.0.1"]
        }
    }
)JSON";

constexpr static auto JSONDataOverload = R"JSON(
    {
        "server":{
            "ip":"0.0.0.0",
            "port":8888
        },
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1000,
            "max_connections": 2,
            "max_requests": 1
        }
    }
)JSON";

// for testing, we use a self-signed certificate
std::optional<ssl::context>
parseCertsForTest()
{
    std::string const key = R"(-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAqP3K4WDIhk63zbxSoN8tJqRZD3W0IWFMwCluZchUwsHPxEC4
32sPk58YonynY5nGtTeSGhedSqHD0gFBLcU/su4dSsj+kgGgJwKmiPmoQiTpzEmd
g2Kqrnrw6QAilyhyMgjo6lYOiCsLU2qdnXcN8AOaAD9wtqNdcoFFQJD9vU9uKA8x
evwIF7OgpUyERlnj5ILTGlwzOr1IochpxG08JD22C9ZlSLB2DTGbW4x8OvdobAtC
tKU+x9hRbgaAN/jgHze+CrN3Bq48RY2S51Pe/VrDnTAWoDJ/VVFvv8z4niAC5dYC
oAdB6Zut11bUTspqp8MWt3gzEp3Z1cKs83ftaQIDAQABAoIBAGXZH48Zz4DyrGA4
YexG1WV2o55np/p+M82Uqs55IGyIdnmnMESmt6qWtjgnvJKQuWu6ZDmJhejW+bf1
vZyiRrPGQq0x2guRIz6foFLpdHj42lee/mmS659gxRUIWdCUNc7mA8pHt1Zl6tuJ
ZBjlCedfpE8F7R6F8unx8xTozaRr4ZbOVnqB8YWjyuIDUnujsxKdKFASZJAEzRjh
+lScXAdEYTaswgTWFFGKzwTjH/Yfv4y3LwE0RmR/1e+eQmQ7Z4C0HhjYe3EYXAvk
naH2QFZaYVhu7x/+oLPetIzFJOZn61iDhUtGYdvQVvF8qQCPqeuKeLcS9X5my9aK
nfLUryECgYEA3ZZGffe6Me6m0ZX/zwT5NbZpZCJgeALGLZPg9qulDVf8zHbDRsdn
K6Mf/Xhy3DCfSwdwcuAKz/r+4tPFyNUJR+Y2ltXaVl72iY3uJRdriNrEbZ47Ez4z
dhtEmDrD7C+7AusErEgjas+AKXkp1tovXrXUiVfRytBtoKqrym4IjJUCgYEAwzxz
fTuE2nrIwFkvg0p9PtrCwkw8dnzhBeNnzFdPOVAiHCfnNcaSOWWTkGHIkGLoORqs
fqfZCD9VkqRwsPDaSSL7vhX3oHuerDipdxOjaXVjYa7YjM6gByzo62hnG6BcQHC7
zrj7iqjnMdyNLtXcPu6zm/j5iIOLWXMevK/OVIUCgYAey4e4cfk6f0RH1GTczIAl
6tfyxqRJiXkpVGfrYCdsF1JWyBqTd5rrAZysiVTNLSS2NK54CJL4HJXXyD6wjorf
pyrnA4l4f3Ib49G47exP9Ldf1KG5JufX/iomTeR0qp1+5lKb7tqdOYFCQkiCR4hV
zUdgXwgU+6qArbd6RpiBkQKBgQCSen5jjQ5GJS0NM1y0cmS5jcPlpvEOLO9fTZiI
9VCZPYf5++46qHr42T73aoXh3nNAtMSKWkA5MdtwJDPwbSQ5Dyg1G6IoI9eOewya
LH/EFbC0j0wliLkD6SvvwurpDU1pg6tElAEVrVeYT1MVupp+FPVopkoBpEAeooKD
KpvxSQKBgQDP9fNJIpuX3kaudb0pI1OvuqBYTrTExMx+JMR+Sqf0HUwavpeCn4du
O2R4tGOOkGAX/0/actRXptFk23ucHnSIwcW6HYgDM3tDBP7n3GYdu5CSE1eiR5k7
Zl3fuvbMYcmYKgutFcRj+8NvzRWT2suzGU2x4PiPX+fh5kpvmMdvLA==
-----END RSA PRIVATE KEY-----)";
    std::string const cert = R"(-----BEGIN CERTIFICATE-----
MIIDrjCCApagAwIBAgIJAOE4Hv/P8CO3MA0GCSqGSIb3DQEBCwUAMDkxEjAQBgNV
BAMMCTEyNy4wLjAuMTELMAkGA1UEBhMCVVMxFjAUBgNVBAcMDVNhbiBGcmFuc2lz
Y28wHhcNMjMwNTE4MTUwMzEwWhcNMjQwNTE3MTUwMzEwWjBrMQswCQYDVQQGEwJV
UzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNU2FuIEZyYW5zaXNjbzEN
MAsGA1UECgwEVGVzdDEMMAoGA1UECwwDRGV2MRIwEAYDVQQDDAkxMjcuMC4wLjEw
ggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCo/crhYMiGTrfNvFKg3y0m
pFkPdbQhYUzAKW5lyFTCwc/EQLjfaw+TnxiifKdjmca1N5IaF51KocPSAUEtxT+y
7h1KyP6SAaAnAqaI+ahCJOnMSZ2DYqquevDpACKXKHIyCOjqVg6IKwtTap2ddw3w
A5oAP3C2o11ygUVAkP29T24oDzF6/AgXs6ClTIRGWePkgtMaXDM6vUihyGnEbTwk
PbYL1mVIsHYNMZtbjHw692hsC0K0pT7H2FFuBoA3+OAfN74Ks3cGrjxFjZLnU979
WsOdMBagMn9VUW+/zPieIALl1gKgB0Hpm63XVtROymqnwxa3eDMSndnVwqzzd+1p
AgMBAAGjgYYwgYMwUwYDVR0jBEwwSqE9pDswOTESMBAGA1UEAwwJMTI3LjAuMC4x
MQswCQYDVQQGEwJVUzEWMBQGA1UEBwwNU2FuIEZyYW5zaXNjb4IJAKu2wr50Pfbq
MAkGA1UdEwQCMAAwCwYDVR0PBAQDAgTwMBQGA1UdEQQNMAuCCTEyNy4wLjAuMTAN
BgkqhkiG9w0BAQsFAAOCAQEArEjC1DmJ6q0735PxGkOmjWNsfnw8c2Zl1Z4idKfn
svEFtegNLU7tCu4aKunxlCHWiFVpunr4X67qH1JiE93W0JADnRrPxvywiqR6nUcO
p6HII/kzOizUXk59QMc1GLIIR6LDlNEeDlUbIc2DH8DPrRFBuIMYy4lf18qyfiUb
8Jt8nLeAzbhA21wI6BVhEt8G/cgIi88mPifXq+YVHrJE01jUREHRwl/MMildqxgp
LLuOOuPuy2d+HqjKE7z00j28Uf7gZK29bGx1rK+xH6veAr4plKBavBr8WWpAoUG+
PAMNb1i80cMsjK98xXDdr+7Uvy5M4COMwA5XHmMZDEW8Jw==
-----END CERTIFICATE-----)";
    ssl::context ctx{ssl::context::tlsv12};
    ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);
    ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));
    ctx.use_private_key(boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);
    return ctx;
}

class WebServerTest : public NoLoggerFixture {
public:
    ~WebServerTest() override
    {
        work.reset();
        ctx.stop();
        if (runner->joinable())
            runner->join();
    }

protected:
    WebServerTest()
    {
        work.emplace(ctx);  // make sure ctx does not stop on its own
        runner.emplace([this] { ctx.run(); });
    }

    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
    }

    // this ctx is for dos timer
    boost::asio::io_context ctxSync;
    Config cfg{boost::json::parse(JSONData)};
    IntervalSweepHandler sweepHandler = web::IntervalSweepHandler{cfg, ctxSync};
    WhitelistHandler whitelistHandler = web::WhitelistHandler{cfg};
    DOSGuard dosGuard = web::DOSGuard{cfg, whitelistHandler, sweepHandler};

    Config cfgOverload{boost::json::parse(JSONDataOverload)};
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
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> const& sslCtx,
    web::DOSGuard& dosGuard,
    std::shared_ptr<Executor> const& handler
)
{
    auto server = std::shared_ptr<web::HttpServer<Executor>>();
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    boost::asio::dispatch(ioc.get_executor(), [&]() mutable {
        server = web::make_HttpServer(config, ioc, sslCtx, dosGuard, handler);
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
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncPost("localhost", "8888", R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
}

TEST_F(WebServerTest, Ws)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888");
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
}

TEST_F(WebServerTest, HttpInternalError)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncPost("localhost", "8888", R"({})");
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"})"
    );
}

TEST_F(WebServerTest, WsInternalError)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888");
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
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888");
    auto const res = wsClient.syncPost("not json");
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","request":"not json"})"
    );
}

TEST_F(WebServerTest, Https)
{
    auto e = std::make_shared<EchoExecutor>();
    auto sslCtx = parseCertsForTest();
    auto const ctxSslRef = sslCtx ? std::optional<std::reference_wrapper<ssl::context>>{sslCtx.value()} : std::nullopt;
    auto const server = makeServerSync(cfg, ctx, ctxSslRef, dosGuard, e);
    auto const res = HttpsSyncClient::syncPost("localhost", "8888", R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
}

TEST_F(WebServerTest, Wss)
{
    auto e = std::make_shared<EchoExecutor>();
    auto sslCtx = parseCertsForTest();
    auto const ctxSslRef = sslCtx ? std::optional<std::reference_wrapper<ssl::context>>{sslCtx.value()} : std::nullopt;

    auto server = makeServerSync(cfg, ctx, ctxSslRef, dosGuard, e);
    WebServerSslSyncClient wsClient;
    wsClient.connect("localhost", "8888");
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
}

TEST_F(WebServerTest, HttpRequestOverload)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuardOverload, e);
    auto res = HttpSyncClient::syncPost("localhost", "8888", R"({})");
    EXPECT_EQ(res, "{}");
    res = HttpSyncClient::syncPost("localhost", "8888", R"({})");
    EXPECT_EQ(
        res,
        R"({"error":"slowDown","error_code":10,"error_message":"You are placing too much load on the server.","status":"error","type":"response"})"
    );
}

TEST_F(WebServerTest, WsRequestOverload)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, std::nullopt, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888");
    auto res = wsClient.syncPost(R"({})");
    wsClient.disconnect();
    EXPECT_EQ(res, "{}");
    WebSocketSyncClient wsClient2;
    wsClient2.connect("localhost", "8888");
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
    auto server = makeServerSync(cfg, ctx, std::nullopt, dosGuardOverload, e);
    auto const res = HttpSyncClient::syncPost("localhost", "8888", fmt::format(R"({{"payload":"{}"}})", s100));
    EXPECT_EQ(
        res,
        R"({"payload":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","warning":"load","warnings":[{"id":2003,"message":"You are about to be rate limited"}]})"
    );
}

TEST_F(WebServerTest, WsPayloadOverload)
{
    std::string const s100(100, 'a');
    auto e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, std::nullopt, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888");
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
    auto server = makeServerSync(cfg, ctx, std::nullopt, dosGuardOverload, e);
    // max connection is 2, exception should happen when the third connection is made
    WebSocketSyncClient wsClient1;
    wsClient1.connect("localhost", "8888");
    WebSocketSyncClient wsClient2;
    wsClient2.connect("localhost", "8888");
    bool exceptionThrown = false;
    try {
        WebSocketSyncClient wsClient3;
        wsClient3.connect("localhost", "8888");
    } catch (boost::system::system_error const& ex) {
        exceptionThrown = true;
        EXPECT_EQ(ex.code(), boost::beast::websocket::error::upgrade_declined);
    }
    wsClient1.disconnect();
    wsClient2.disconnect();
    EXPECT_TRUE(exceptionThrown);
}

static auto constexpr JSONServerConfigWithAdminPassword = R"JSON(
    {
        "server":{
            "ip": "0.0.0.0",
            "port": 8888,
            "admin_password": "secret"
        }
    }
)JSON";

static auto constexpr JSONServerConfigWithLocalAdmin = R"JSON(
    {
        "server":{
            "ip": "0.0.0.0",
            "port": 8888,
            "local_admin": true
        }
    }
)JSON";

static auto constexpr JSONServerConfigWithBothAdminPasswordAndLocalAdminFalse = R"JSON(
    {
        "server":{
            "ip": "0.0.0.0",
            "port": 8888,
            "admin_password": "secret",
            "local_admin": false
        }
    }
)JSON";

static auto constexpr JSONServerConfigWithNoSpecifiedAdmin = R"JSON(
    {
        "server":{
            "ip": "0.0.0.0",
            "port": 8888
        }
    }
)JSON";

// get this value from online sha256 generator
static auto constexpr SecertSha256 = "2bb80d537b1da3e38bd30361aa855686bde0eacd7162fef6a25fe97bf527a25b";

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
    auto server = makeServerSync(serverConfig, ctx, std::nullopt, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888", GetParam().headers);
    std::string const request = "Why hello";
    auto const res = wsClient.syncPost(request);
    wsClient.disconnect();
    EXPECT_EQ(res, fmt::format("{} {}", request, GetParam().expectedResponse));
}

TEST_P(WebServerAdminTest, HttpAdminCheck)
{
    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(GetParam().config)};
    auto server = makeServerSync(serverConfig, ctx, std::nullopt, dosGuardOverload, e);
    std::string const request = "Why hello";
    auto const res = HttpSyncClient::syncPost("localhost", "8888", request, GetParam().headers);
    EXPECT_EQ(res, fmt::format("{} {}", request, GetParam().expectedResponse));
}

INSTANTIATE_TEST_CASE_P(
    WebServerAdminTestsSuit,
    WebServerAdminTest,
    ::testing::Values(
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword,
            .headers = {},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword,
            .headers = {WebHeader(http::field::authorization, "")},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword,
            .headers = {WebHeader(http::field::authorization, "s")},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword,
            .headers = {WebHeader(http::field::authorization, SecertSha256)},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword,
            .headers = {WebHeader(
                http::field::authorization,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecertSha256)
            )},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithBothAdminPasswordAndLocalAdminFalse,
            .headers = {WebHeader(http::field::authorization, SecertSha256)},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithBothAdminPasswordAndLocalAdminFalse,
            .headers = {WebHeader(
                http::field::authorization,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecertSha256)
            )},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = JSONServerConfigWithAdminPassword,
            .headers = {WebHeader(
                http::field::authentication_info,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecertSha256)
            )},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{.config = JSONServerConfigWithLocalAdmin, .headers = {}, .expectedResponse = "admin"},
        WebServerAdminTestParams{
            .config = JSONServerConfigWithNoSpecifiedAdmin,
            .headers = {},
            .expectedResponse = "admin"
        }

    )
);

TEST_F(WebServerTest, AdminErrorCfgTestBothAdminPasswordAndLocalAdminSet)
{
    static auto constexpr JSONServerConfigWithBothAdminPasswordAndLocalAdmin = R"JSON(
        {
            "server":{
                "ip": "0.0.0.0",
                "port": 8888,
                "admin_password": "secret",
                "local_admin": true
            }
        }
    )JSON";

    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithBothAdminPasswordAndLocalAdmin)};
    EXPECT_THROW(web::make_HttpServer(serverConfig, ctx, std::nullopt, dosGuardOverload, e), std::logic_error);
}

TEST_F(WebServerTest, AdminErrorCfgTestBothAdminPasswordAndLocalAdminFalse)
{
    static auto constexpr JSONServerConfigWithNoAdminPasswordAndLocalAdminFalse = R"JSON(
        {
            "server":{
                "ip": "0.0.0.0",
                "port": 8888,
                "local_admin": false
            }
        }
    )JSON";

    auto e = std::make_shared<AdminCheckExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithNoAdminPasswordAndLocalAdminFalse)};
    EXPECT_THROW(web::make_HttpServer(serverConfig, ctx, std::nullopt, dosGuardOverload, e), std::logic_error);
}

struct WebServerPrometheusTest : util::prometheus::WithPrometheus, WebServerTest {};

TEST_F(WebServerPrometheusTest, rejectedWithoutAdminPassword)
{
    auto e = std::make_shared<EchoExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithAdminPassword)};
    auto server = makeServerSync(serverConfig, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncGet("localhost", "8888", "", "/metrics");
    EXPECT_EQ(res, "Only admin is allowed to collect metrics");
}

TEST_F(WebServerPrometheusTest, rejectedIfPrometheusIsDisabled)
{
    static auto constexpr JSONServerConfigWithDisabledPrometheus = R"JSON(
        {
            "server": {
                "ip": "0.0.0.0",
                "port": 8888,
                "admin_password": "secret"
            },
            "prometheus": { "enabled": false }
        }
    )JSON";

    auto e = std::make_shared<EchoExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithDisabledPrometheus)};
    PrometheusService::init(serverConfig);
    auto server = makeServerSync(serverConfig, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncGet(
        "localhost",
        "8888",
        "",
        "/metrics",
        {WebHeader(
            http::field::authorization,
            fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecertSha256)
        )}
    );
    EXPECT_EQ(res, "Prometheus is disabled in clio config");
}

TEST_F(WebServerPrometheusTest, validResponse)
{
    auto& testCounter = PrometheusService::counterInt("test_counter", util::prometheus::Labels());
    ++testCounter;
    auto e = std::make_shared<EchoExecutor>();
    Config const serverConfig{boost::json::parse(JSONServerConfigWithAdminPassword)};
    auto server = makeServerSync(serverConfig, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncGet(
        "localhost",
        "8888",
        "",
        "/metrics",
        {WebHeader(
            http::field::authorization,
            fmt::format("{}{}", PasswordAdminVerificationStrategy::passwordPrefix, SecertSha256)
        )}
    );
    EXPECT_EQ(res, "# TYPE test_counter counter\ntest_counter 1\n\n");
}
