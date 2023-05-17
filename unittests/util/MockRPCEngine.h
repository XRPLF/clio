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
#include <rpc/common/Types.h>
#include <webserver/Context.h>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include <string>

struct MockAsyncRPCEngine
{
public:
    MockAsyncRPCEngine()
    {
        work_.emplace(ioc_);  // make sure ctx does not stop on its own
        runner_.emplace([this] { ioc_.run(); });
    }

    ~MockAsyncRPCEngine()
    {
        work_.reset();
        ioc_.stop();
        if (runner_->joinable())
            runner_->join();
    }

    template <typename Fn>
    bool
    post(Fn&& func, std::string const& ip)
    {
        boost::asio::spawn(ioc_, [f = std::move(func)](auto yield) { f(yield); });
        return true;
    }

    MOCK_METHOD(void, notifyComplete, (std::string const&, std::chrono::microseconds const&), ());
    MOCK_METHOD(void, notifyErrored, (std::string const&), ());
    MOCK_METHOD(void, notifyForwarded, (std::string const&), ());
    MOCK_METHOD(RPC::Result, buildResponse, (Web::Context const&), ());

private:
    boost::asio::io_context ioc_;
    std::optional<boost::asio::io_service::work> work_;
    std::optional<std::thread> runner_;
};

struct MockRPCEngine
{
public:
    MOCK_METHOD(bool, post, (std::function<void(boost::asio::yield_context)>&&, std::string const&), ());
    MOCK_METHOD(void, notifyComplete, (std::string const&, std::chrono::microseconds const&), ());
    MOCK_METHOD(void, notifyErrored, (std::string const&), ());
    MOCK_METHOD(void, notifyForwarded, (std::string const&), ());
    MOCK_METHOD(RPC::Result, buildResponse, (Web::Context const&), ());
};
