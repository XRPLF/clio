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

#include <util/Fixtures.h>

#include <rpc/Counters.h>
#include <rpc/JS.h>

#include <boost/json.hpp>
#include <gtest/gtest.h>

using namespace rpc;

class RPCCountersTest : public NoLoggerFixture {
protected:
    WorkQueue queue{4u, 1024u};  // todo: mock instead
    Counters counters{queue};
};

TEST_F(RPCCountersTest, CheckThatCountersAddUp)
{
    for (auto i = 0u; i < 512u; ++i) {
        counters.rpcErrored("error");
        counters.rpcComplete("complete", std::chrono::milliseconds{1u});
        counters.rpcForwarded("forward");
        counters.rpcFailedToForward("failedToForward");
        counters.rpcFailed("failed");
        counters.onTooBusy();
        counters.onNotReady();
        counters.onBadSyntax();
        counters.onUnknownCommand();
        counters.onInternalError();
    }

    auto const report = counters.report();
    auto const& rpc = report.at(JS(rpc)).as_object();

    EXPECT_STREQ(rpc.at("error").as_object().at(JS(started)).as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("error").as_object().at(JS(finished)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("error").as_object().at(JS(errored)).as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("error").as_object().at("forwarded").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("error").as_object().at("failed_forward").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("error").as_object().at(JS(failed)).as_string().c_str(), "0");

    EXPECT_STREQ(rpc.at("complete").as_object().at(JS(started)).as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("complete").as_object().at(JS(finished)).as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("complete").as_object().at(JS(errored)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("complete").as_object().at("forwarded").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("complete").as_object().at("failed_forward").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("complete").as_object().at(JS(failed)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("complete").as_object().at(JS(duration_us)).as_string().c_str(), "512000");  // 1000 per call

    EXPECT_STREQ(rpc.at("forward").as_object().at(JS(started)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("forward").as_object().at(JS(finished)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("forward").as_object().at(JS(errored)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("forward").as_object().at("forwarded").as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("forward").as_object().at("failed_forward").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("forward").as_object().at(JS(failed)).as_string().c_str(), "0");

    EXPECT_STREQ(rpc.at("failed").as_object().at(JS(started)).as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("failed").as_object().at(JS(finished)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failed").as_object().at(JS(errored)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failed").as_object().at("forwarded").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failed").as_object().at("failed_forward").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failed").as_object().at(JS(failed)).as_string().c_str(), "512");

    EXPECT_STREQ(rpc.at("failedToForward").as_object().at(JS(started)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failedToForward").as_object().at(JS(finished)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failedToForward").as_object().at(JS(errored)).as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failedToForward").as_object().at("forwarded").as_string().c_str(), "0");
    EXPECT_STREQ(rpc.at("failedToForward").as_object().at("failed_forward").as_string().c_str(), "512");
    EXPECT_STREQ(rpc.at("failedToForward").as_object().at(JS(failed)).as_string().c_str(), "0");

    EXPECT_STREQ(report.at("too_busy_errors").as_string().c_str(), "512");
    EXPECT_STREQ(report.at("not_ready_errors").as_string().c_str(), "512");
    EXPECT_STREQ(report.at("bad_syntax_errors").as_string().c_str(), "512");
    EXPECT_STREQ(report.at("unknown_command_errors").as_string().c_str(), "512");
    EXPECT_STREQ(report.at("internal_errors").as_string().c_str(), "512");

    EXPECT_EQ(report.at("work_queue"), queue.report());  // Counters report includes queue report
}
