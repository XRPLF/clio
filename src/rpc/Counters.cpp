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

#include "rpc/Counters.hpp"

#include "rpc/JS.hpp"
#include "rpc/WorkQueue.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/json/object.hpp>
#include <fmt/core.h>
#include <xrpl/protocol/jss.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace rpc {

using util::prometheus::Label;
using util::prometheus::Labels;

Counters::MethodInfo::MethodInfo(std::string const& method)
    : started(PrometheusService::counterInt(
          "rpc_method_total_number",
          Labels{{{"status", "started"}, {"method", method}}},
          fmt::format("Total number of started calls to the method {}", method)
      ))
    , finished(PrometheusService::counterInt(
          "rpc_method_total_number",
          Labels{{{"status", "finished"}, {"method", method}}},
          fmt::format("Total number of finished calls to the method {}", method)
      ))
    , failed(PrometheusService::counterInt(
          "rpc_method_total_number",
          Labels{{{"status", "failed"}, {"method", method}}},
          fmt::format("Total number of failed calls to the method {}", method)
      ))
    , errored(PrometheusService::counterInt(
          "rpc_method_total_number",
          Labels{{{"status", "errored"}, {"method", method}}},
          fmt::format("Total number of errored calls to the method {}", method)
      ))
    , forwarded(PrometheusService::counterInt(
          "rpc_method_total_number",
          Labels{{{"status", "forwarded"}, {"method", method}}},
          fmt::format("Total number of forwarded calls to the method {}", method)
      ))
    , failedForward(PrometheusService::counterInt(
          "rpc_method_total_number",
          Labels{{{"status", "failed_forward"}, {"method", method}}},
          fmt::format("Total number of failed forwarded calls to the method {}", method)
      ))
    , duration(PrometheusService::counterInt(
          "rpc_method_duration_us",
          Labels({util::prometheus::Label{"method", method}}),
          fmt::format("Total duration of calls to the method {}", method)
      ))
{
}

Counters::MethodInfo&
Counters::getMethodInfo(std::string const& method)
{
    auto it = methodInfo_.find(method);
    if (it == methodInfo_.end()) {
        it = methodInfo_.emplace(method, MethodInfo(method)).first;
    }
    return it->second;
}

Counters::Counters(WorkQueue const& wq)
    : tooBusyCounter_(PrometheusService::counterInt(
          "rpc_error_total_number",
          Labels({Label{"error_type", "too_busy"}}),
          "Total number of too busy errors"
      ))
    , notReadyCounter_(PrometheusService::counterInt(
          "rpc_error_total_number",
          Labels({Label{"error_type", "not_ready"}}),
          "Total number of not ready replyes"
      ))
    , badSyntaxCounter_(PrometheusService::counterInt(
          "rpc_error_total_number",
          Labels({Label{"error_type", "bad_syntax"}}),
          "Total number of bad syntax replyes"
      ))
    , unknownCommandCounter_(PrometheusService::counterInt(
          "rpc_error_total_number",
          Labels({Label{"error_type", "unknown_command"}}),
          "Total number of unknown command replyes"
      ))
    , internalErrorCounter_(PrometheusService::counterInt(
          "rpc_error_total_number",
          Labels({Label{"error_type", "internal_error"}}),
          "Total number of internal errors"
      ))
    , workQueue_(std::cref(wq))
    , startupTime_{std::chrono::system_clock::now()}
{
}

void
Counters::rpcFailed(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo const& counters = getMethodInfo(method);
    ++counters.started.get();
    ++counters.failed.get();
}

void
Counters::rpcErrored(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo const& counters = getMethodInfo(method);
    ++counters.started.get();
    ++counters.errored.get();
}

void
Counters::rpcComplete(std::string const& method, std::chrono::microseconds const& rpcDuration)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo const& counters = getMethodInfo(method);
    ++counters.started.get();
    ++counters.finished.get();
    counters.duration.get() += rpcDuration.count();
}

void
Counters::rpcForwarded(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo const& counters = getMethodInfo(method);
    ++counters.forwarded.get();
}

void
Counters::rpcFailedToForward(std::string const& method)
{
    std::scoped_lock const lk(mutex_);
    MethodInfo const& counters = getMethodInfo(method);
    ++counters.failedForward.get();
}

void
Counters::onTooBusy()
{
    ++tooBusyCounter_.get();
}

void
Counters::onNotReady()
{
    ++notReadyCounter_.get();
}

void
Counters::onBadSyntax()
{
    ++badSyntaxCounter_.get();
}

void
Counters::onUnknownCommand()
{
    ++unknownCommandCounter_.get();
}

void
Counters::onInternalError()
{
    ++internalErrorCounter_.get();
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
        counters[JS(started)] = std::to_string(info.started.get().value());
        counters[JS(finished)] = std::to_string(info.finished.get().value());
        counters[JS(errored)] = std::to_string(info.errored.get().value());
        counters[JS(failed)] = std::to_string(info.failed.get().value());
        counters["forwarded"] = std::to_string(info.forwarded.get().value());
        counters["failed_forward"] = std::to_string(info.failedForward.get().value());
        counters[JS(duration_us)] = std::to_string(info.duration.get().value());

        rpc[method] = std::move(counters);
    }

    obj["too_busy_errors"] = std::to_string(tooBusyCounter_.get().value());
    obj["not_ready_errors"] = std::to_string(notReadyCounter_.get().value());
    obj["bad_syntax_errors"] = std::to_string(badSyntaxCounter_.get().value());
    obj["unknown_command_errors"] = std::to_string(unknownCommandCounter_.get().value());
    obj["internal_errors"] = std::to_string(internalErrorCounter_.get().value());

    obj["work_queue"] = workQueue_.get().report();

    return obj;
}

}  // namespace rpc
