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
#include <util/prometheus/Http.h>

#include <gtest/gtest.h>

using namespace util::prometheus;
namespace http = boost::beast::http;

struct PrometheusCheckRequestTestsParams
{
    std::string testName;
    http::verb method;
    std::string target;
    bool isAdmin;
    bool expected;
};

struct PrometheusCheckRequestTests : public ::testing::TestWithParam<PrometheusCheckRequestTestsParams>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<PrometheusCheckRequestTestsParams>(info.param);
            return bundle.testName;
        }
    };
};

TEST_P(PrometheusCheckRequestTests, isPrometheusRequest)
{
    boost::beast::http::request<boost::beast::http::string_body> req;
    req.method(GetParam().method);
    req.target(GetParam().target);
    EXPECT_EQ(isPrometheusRequest(req, GetParam().isAdmin), GetParam().expected);
}

INSTANTIATE_TEST_CASE_P(
    PrometheusHttpTests,
    PrometheusCheckRequestTests,
    ::testing::ValuesIn({
        PrometheusCheckRequestTestsParams{"validRequest", http::verb::get, "/metrics", true, true},
        PrometheusCheckRequestTestsParams{"notAdmin", http::verb::get, "/metrics", false, false},
        PrometheusCheckRequestTestsParams{"wrongMethod", http::verb::post, "/metrics", true, false},
        PrometheusCheckRequestTestsParams{"wrongTarget", http::verb::get, "/", true, false},
    }),
    PrometheusCheckRequestTests::NameGenerator());

struct PrometheusHandleRequestTests : ::testing::Test
{
    PrometheusHandleRequestTests()
    {
        PrometheusSingleton::init();
    }
    http::request<http::string_body> const req{http::verb::get, "/metrics", 11};
};

TEST_F(PrometheusHandleRequestTests, emptyResponse)
{
    auto response = handlePrometheusRequest(req);
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response[http::field::content_type], "text/plain; version=0.0.4");
    EXPECT_EQ(response.body(), "");
}

TEST_F(PrometheusHandleRequestTests, responseWithCounter)
{
    const auto counterName = "test_counter";
    const Labels labels{{{"label1", "value1"}, Label{"label2", "value2"}}};
    const auto description = "test_description";

    auto& counter = PROMETHEUS().counterInt(counterName, labels, description);
    ++counter;
    counter += 3;

    auto response = handlePrometheusRequest(req);
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response[http::field::content_type], "text/plain; version=0.0.4");
    const auto expectedBody =
        fmt::format("# HELP {0} {1}\n# TYPE {0} counter\n{0}{2} 4\n\n", counterName, description, labels.serialize());
    EXPECT_EQ(response.body(), expectedBody);
}

TEST_F(PrometheusHandleRequestTests, responseWithGauge)
{
    const auto gaugeName = "test_gauge";
    const Labels labels{{{"label2", "value2"}, Label{"label3", "value3"}}};
    const auto description = "test_description_gauge";

    auto& gauge = PROMETHEUS().gaugeInt(gaugeName, labels, description);
    ++gauge;
    gauge -= 3;

    auto response = handlePrometheusRequest(req);
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response[http::field::content_type], "text/plain; version=0.0.4");
    const auto expectedBody =
        fmt::format("# HELP {0} {1}\n# TYPE {0} gauge\n{0}{2} -2\n\n", gaugeName, description, labels.serialize());
    EXPECT_EQ(response.body(), expectedBody);
}

TEST_F(PrometheusHandleRequestTests, responseWithCounterAndGauge)
{
    const auto counterName = "test_counter";
    const Labels counterLabels{{{"label1", "value1"}, Label{"label2", "value2"}}};
    const auto counterDescription = "test_description";

    auto& counter = PROMETHEUS().counterInt(counterName, counterLabels, counterDescription);
    ++counter;
    counter += 3;

    const auto gaugeName = "test_gauge";
    const Labels gaugeLabels{{{"label2", "value2"}, Label{"label3", "value3"}}};
    const auto gaugeDescription = "test_description_gauge";

    auto& gauge = PROMETHEUS().gaugeInt(gaugeName, gaugeLabels, gaugeDescription);
    ++gauge;
    gauge -= 3;

    auto response = handlePrometheusRequest(req);

    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response[http::field::content_type], "text/plain; version=0.0.4");
    const auto expectedBody = fmt::format(
        "# HELP {3} {4}\n# TYPE {3} gauge\n{3}{5} -2\n\n"
        "# HELP {0} {1}\n# TYPE {0} counter\n{0}{2} 4\n\n",
        counterName,
        counterDescription,
        counterLabels.serialize(),
        gaugeName,
        gaugeDescription,
        gaugeLabels.serialize());
    const auto anotherExpectedBody = fmt::format(
        "# HELP {0} {1}\n# TYPE {0} counter\n{0}{2} 4\n\n"
        "# HELP {3} {4}\n# TYPE {3} gauge\n{3}{5} -2\n\n",
        counterName,
        counterDescription,
        counterLabels.serialize(),
        gaugeName,
        gaugeDescription,
        gaugeLabels.serialize());
    EXPECT_TRUE(response.body() == expectedBody || response.body() == anotherExpectedBody);
}