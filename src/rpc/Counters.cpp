//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <rpc/Counters.h>
#include <rpc/RPC.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

void
Counters::initializeCounter(std::string const& method)
{
    std::shared_lock lk(mutex_);
    if (methodInfo_.count(method) == 0)
    {
        lk.unlock();
        std::scoped_lock ulk(mutex_);

        // This calls the default constructor for methodInfo of the method.
        methodInfo_[method];
    }
}

void
Counters::rpcErrored(std::string const& method)
{
    // if (!validHandler(method))
    //     return;

    initializeCounter(method);

    std::shared_lock lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    counters.started++;
    counters.errored++;
}

void
Counters::rpcComplete(std::string const& method, std::chrono::microseconds const& rpcDuration)
{
    // if (!validHandler(method))
    //     return;

    initializeCounter(method);

    std::shared_lock lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    counters.started++;
    counters.finished++;
    counters.duration += rpcDuration.count();
}

void
Counters::rpcForwarded(std::string const& method)
{
    // if (!validHandler(method))
    //     return;

    initializeCounter(method);

    std::shared_lock lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    counters.forwarded++;
}

boost::json::object
Counters::report() const
{
    std::shared_lock lk(mutex_);
    boost::json::object obj = {};
    obj[JS(rpc)] = boost::json::object{};
    auto& rpc = obj[JS(rpc)].as_object();

    for (auto const& [method, info] : methodInfo_)
    {
        boost::json::object counters = {};
        counters[JS(started)] = std::to_string(info.started);
        counters[JS(finished)] = std::to_string(info.finished);
        counters[JS(errored)] = std::to_string(info.errored);
        counters["forwarded"] = std::to_string(info.forwarded);
        counters[JS(duration_us)] = std::to_string(info.duration);

        rpc[method] = std::move(counters);
    }
    obj["work_queue"] = workQueue_.get().report();

    return obj;
}

}  // namespace RPC
