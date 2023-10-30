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
#include <rpc/JS.h>
#include <rpc/RPCHelpers.h>

namespace rpc {

void
Counters::rpcFailed(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    ++counters.started;
    ++counters.failed;
}

void
Counters::rpcErrored(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    ++counters.started;
    ++counters.errored;
}

void
Counters::rpcComplete(std::string const& method, std::chrono::microseconds const& rpcDuration)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    ++counters.started;
    ++counters.finished;
    counters.duration += rpcDuration.count();
}

void
Counters::rpcForwarded(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    ++counters.forwarded;
}

void
Counters::rpcFailedToForward(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    ++counters.failedForward;
}

void
Counters::onTooBusy()
{
    ++tooBusyCounter_;
}

void
Counters::onNotReady()
{
    ++notReadyCounter_;
}

void
Counters::onBadSyntax()
{
    ++badSyntaxCounter_;
}

void
Counters::onUnknownCommand()
{
    ++unknownCommandCounter_;
}

void
Counters::onInternalError()
{
    ++internalErrorCounter_;
}

std::chrono::seconds
Counters::uptime() const
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startupTime_);
}

boost::json::object
Counters::report() const
{
    std::scoped_lock const lk(mutex_);
    auto obj = boost::json::object{};

    obj[JS(rpc)] = boost::json::object{};
    auto& rpc = obj[JS(rpc)].as_object();

    for (auto const& [method, info] : methodInfo_) {
        auto counters = boost::json::object{};
        counters[JS(started)] = std::to_string(info.started);
        counters[JS(finished)] = std::to_string(info.finished);
        counters[JS(errored)] = std::to_string(info.errored);
        counters[JS(failed)] = std::to_string(info.failed);
        counters["forwarded"] = std::to_string(info.forwarded);
        counters["failed_forward"] = std::to_string(info.failedForward);
        counters[JS(duration_us)] = std::to_string(info.duration);

        rpc[method] = std::move(counters);
    }

    obj["too_busy_errors"] = std::to_string(tooBusyCounter_);
    obj["not_ready_errors"] = std::to_string(notReadyCounter_);
    obj["bad_syntax_errors"] = std::to_string(badSyntaxCounter_);
    obj["unknown_command_errors"] = std::to_string(unknownCommandCounter_);
    obj["internal_errors"] = std::to_string(internalErrorCounter_);

    obj["work_queue"] = workQueue_.get().report();

    return obj;
}

}  // namespace rpc
