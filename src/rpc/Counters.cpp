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

using util::prometheus::Labels;

Counters::MethodInfo::MethodInfo(std::string method)
    : started(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_total_number", method),
          Labels{{{"status", "started"}}},
          fmt::format("Total number of started calls to the method {}", method)))
    , finished(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_total_number", method),
          Labels{{{"status", "finished"}}},
          fmt::format("Total number of finished calls to the method {}", method)))
    , failed(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_total_number", method),
          Labels{{{"status", "failed"}}},
          fmt::format("Total number of failed calls to the method {}", method)))
    , errored(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_total_number", method),
          Labels{{{"status", "errored"}}},
          fmt::format("Total number of errored calls to the method {}", method)))
    , forwarded(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_total_number", method),
          Labels{{{"status", "forwarded"}}},
          fmt::format("Total number of forwarded calls to the method {}", method)))
    , failedForward(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_total_number", method),
          Labels{{{"status", "failed_forward"}}},
          fmt::format("Total number of failed forwarded calls to the method {}", method)))
    , duration(PROMETHEUS().counterInt(
          fmt::format("rpc_{}_method_duration_us", method),
          Labels(),
          fmt::format("Total duration of calls to the method {}", method)))
{
}

Counters::MethodInfo&
Counters::getMethodInfo(std::string const& method)
{
    auto it = methodInfo_.find(method);
    if (it == methodInfo_.end())
    {
        it = methodInfo_.emplace(method, MethodInfo(method)).first;
    }
    return it->second;
}

Counters::Counters(WorkQueue const& wq)
    : tooBusyCounter_(
          PROMETHEUS().counterInt("rpc_too_busy_errors_total_number", Labels(), "Total number of too busy errors"))
    , notReadyCounter_(
          PROMETHEUS().counterInt("rpc_not_ready_total_number", Labels(), "Total number of not ready replyes"))
    , badSyntaxCounter_(
          PROMETHEUS().counterInt("rpc_bad_syntax_total_number", Labels(), "Total number of bad syntax replyes"))
    , unknownCommandCounter_(PROMETHEUS().counterInt(
          "rpc_unknown_command_total_number",
          Labels(),
          "Total number of unknown command replyes"))
    , internalErrorCounter_(
          PROMETHEUS().counterInt("rpc_internal_error_total_number", Labels(), "Total number of internal errors"))
    , workQueue_(std::cref(wq))
    , startupTime_{std::chrono::system_clock::now()} {};

void
Counters::rpcFailed(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = getMethodInfo(method);
    ++counters.started;
    ++counters.failed;
}

void
Counters::rpcErrored(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = getMethodInfo(method);
    ++counters.started;
    ++counters.errored;
}

void
Counters::rpcComplete(std::string const& method, std::chrono::microseconds const& rpcDuration)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = getMethodInfo(method);
    ++counters.started;
    ++counters.finished;
    counters.duration += rpcDuration.count();
}

void
Counters::rpcForwarded(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = getMethodInfo(method);
    ++counters.forwarded;
}

void
Counters::rpcFailedToForward(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo& counters = getMethodInfo(method);
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

    for (auto const& [method, info] : methodInfo_)
    {
        auto counters = boost::json::object{};
        counters[JS(started)] = std::to_string(info.started.value());
        counters[JS(finished)] = std::to_string(info.finished.value());
        counters[JS(errored)] = std::to_string(info.errored.value());
        counters[JS(failed)] = std::to_string(info.failed.value());
        counters["forwarded"] = std::to_string(info.forwarded.value());
        counters["failed_forward"] = std::to_string(info.failedForward.value());
        counters[JS(duration_us)] = std::to_string(info.duration.value());

        rpc[method] = std::move(counters);
    }

    obj["too_busy_errors"] = std::to_string(tooBusyCounter_.value());
    obj["not_ready_errors"] = std::to_string(notReadyCounter_.value());
    obj["bad_syntax_errors"] = std::to_string(badSyntaxCounter_.value());
    obj["unknown_command_errors"] = std::to_string(unknownCommandCounter_.value());
    obj["internal_errors"] = std::to_string(internalErrorCounter_.value());

    obj["work_queue"] = workQueue_.get().report();

    return obj;
}

}  // namespace rpc
