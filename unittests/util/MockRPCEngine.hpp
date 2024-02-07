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

#pragma once
#include "rpc/common/Types.hpp"
#include "web/Context.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>

struct MockAsyncRPCEngine {
    template <typename Fn>
    bool
    post(Fn&& func, [[maybe_unused]] std::string const& ip = "")
    {
        using namespace boost::asio;
        io_context ioc;

        spawn(ioc, [handler = std::forward<Fn>(func), _ = make_work_guard(ioc)](auto yield) mutable {
            handler(yield);
            ;
        });

        ioc.run();
        return true;
    }

    MOCK_METHOD(void, notifyComplete, (std::string const&, std::chrono::microseconds const&), ());
    MOCK_METHOD(void, notifyFailed, (std::string const&), ());
    MOCK_METHOD(void, notifyErrored, (std::string const&), ());
    MOCK_METHOD(void, notifyForwarded, (std::string const&), ());
    MOCK_METHOD(void, notifyFailedToForward, (std::string const&), ());
    MOCK_METHOD(void, notifyNotReady, (), ());
    MOCK_METHOD(void, notifyBadSyntax, (), ());
    MOCK_METHOD(void, notifyTooBusy, (), ());
    MOCK_METHOD(void, notifyUnknownCommand, (), ());
    MOCK_METHOD(void, notifyInternalError, (), ());
    MOCK_METHOD(rpc::Result, buildResponse, (web::Context const&), ());
};

struct MockRPCEngine {
    MOCK_METHOD(bool, post, (std::function<void(boost::asio::yield_context)>&&, std::string const&), ());
    MOCK_METHOD(void, notifyComplete, (std::string const&, std::chrono::microseconds const&), ());
    MOCK_METHOD(void, notifyErrored, (std::string const&), ());
    MOCK_METHOD(void, notifyForwarded, (std::string const&), ());
    MOCK_METHOD(void, notifyFailedToForward, (std::string const&), ());
    MOCK_METHOD(void, notifyNotReady, (), ());
    MOCK_METHOD(void, notifyBadSyntax, (), ());
    MOCK_METHOD(void, notifyTooBusy, (), ());
    MOCK_METHOD(void, notifyUnknownCommand, (), ());
    MOCK_METHOD(void, notifyInternalError, (), ());
    MOCK_METHOD(rpc::Result, buildResponse, (web::Context const&), ());
};
