#pragma once
#include "rpc/common/Types.hpp"
#include "util/Spawn.hpp"
#include "web/Context.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

struct MockAsyncRPCEngine {
    template <typename Fn>
    bool
    post(Fn&& func, [[maybe_unused]] std::string const& ip = "")
    {
        boost::asio::io_context ioc;

        util::spawn(
            ioc, [handler = std::forward<Fn>(func), _ = make_work_guard(ioc)](auto yield) mutable {
                handler(yield);
            }
        );

        ioc.run();
        return true;
    }

    MOCK_METHOD(void, notifyComplete, (std::string const&, std::chrono::microseconds const&), ());
    void
    notifyComplete(
        web::Context const& ctx,
        std::chrono::microseconds const& duration,
        bool isForwarded
    )
    {
        notifyComplete(ctx.method, duration);
        if (not isForwarded)
            recordLedgerMetrics(ctx.params, ctx.range.maxSequence);
    }
    MOCK_METHOD(void, notifyFailed, (std::string const&), ());
    MOCK_METHOD(void, notifyErrored, (std::string const&), ());
    MOCK_METHOD(void, notifyForwarded, (std::string const&), ());
    MOCK_METHOD(void, notifyFailedToForward, (std::string const&), ());
    MOCK_METHOD(void, notifyNotReady, (), ());
    MOCK_METHOD(void, notifyBadSyntax, (), ());
    MOCK_METHOD(void, notifyTooBusy, (), ());
    MOCK_METHOD(void, notifyUnknownCommand, (), ());
    MOCK_METHOD(void, notifyInternalError, (), ());
    MOCK_METHOD(void, recordLedgerMetrics, (boost::json::object const&, std::uint32_t), ());
    MOCK_METHOD(rpc::Result, buildResponse, (web::Context const&), ());
};

struct MockRPCEngine {
    MOCK_METHOD(
        bool,
        post,
        (std::function<void(boost::asio::yield_context)>&&, std::string const&),
        ()
    );
    MOCK_METHOD(void, notifyComplete, (std::string const&, std::chrono::microseconds const&), ());
    void
    notifyComplete(
        web::Context const& ctx,
        std::chrono::microseconds const& duration,
        bool isForwarded
    )
    {
        notifyComplete(ctx.method, duration);
        if (not isForwarded)
            recordLedgerMetrics(ctx.params, ctx.range.maxSequence);
    }
    MOCK_METHOD(void, notifyErrored, (std::string const&), ());
    MOCK_METHOD(void, notifyForwarded, (std::string const&), ());
    MOCK_METHOD(void, notifyFailedToForward, (std::string const&), ());
    MOCK_METHOD(void, notifyNotReady, (), ());
    MOCK_METHOD(void, notifyBadSyntax, (), ());
    MOCK_METHOD(void, notifyTooBusy, (), ());
    MOCK_METHOD(void, notifyUnknownCommand, (), ());
    MOCK_METHOD(void, notifyInternalError, (), ());
    MOCK_METHOD(void, recordLedgerMetrics, (boost::json::object const&, std::uint32_t), ());
    MOCK_METHOD(rpc::Result, buildResponse, (web::Context const&), ());
};
