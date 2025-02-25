//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etlng/impl/Monitor.hpp"

#include "data/BackendInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "util/Assert.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/log/Logger.hpp"

#include <boost/signals2/connection.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace etlng::impl {
Monitor::Monitor(
    util::async::AnyExecutionContext ctx,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<etl::NetworkValidatedLedgersInterface> validatedLedgers,
    uint32_t startSequence
)
    : strand_(ctx.makeStrand())
    , backend_(std::move(backend))
    , validatedLedgers_(std::move(validatedLedgers))
    , nextSequence_(startSequence)
{
}

Monitor::~Monitor()
{
    stop();
}

void
Monitor::run(std::chrono::steady_clock::duration repeatInterval)
{
    ASSERT(not repeatedTask_.has_value(), "Monitor attempted to run more than once");
    LOG(log_.debug()) << "Starting monitor";

    repeatedTask_ = strand_.executeRepeatedly(repeatInterval, std::bind_front(&Monitor::doWork, this));
    subscription_ = validatedLedgers_->subscribe(std::bind_front(&Monitor::onNextSequence, this));
}

void
Monitor::stop()
{
    if (repeatedTask_.has_value())
        repeatedTask_->abort();

    repeatedTask_ = std::nullopt;
}

boost::signals2::scoped_connection
Monitor::subscribe(SignalType::slot_type const& subscriber)
{
    return notificationChannel_.connect(subscriber);
}

void
Monitor::onNextSequence(uint32_t seq)
{
    LOG(log_.debug()) << "rippled published sequence " << seq;
    repeatedTask_->invoke();  // force-invoke immediately
}

void
Monitor::doWork()
{
    if (auto rng = backend_->hardFetchLedgerRangeNoThrow(); rng) {
        while (rng->maxSequence >= nextSequence_)
            notificationChannel_(nextSequence_++);
    }
}

}  // namespace etlng::impl
