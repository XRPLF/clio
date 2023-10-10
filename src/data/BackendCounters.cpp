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

#include <data/BackendCounters.h>

namespace data {

BackendCounters::PtrType
BackendCounters::make()
{
    struct EnableMakeShared : public BackendCounters
    {
    };
    return std::make_shared<EnableMakeShared>();
}

void
BackendCounters::registerTooBusy()
{
    ++tooBusyCounter_;
}

void
BackendCounters::registerWriteSync()
{
    ++writeSyncCounter_;
}

void
BackendCounters::registerWriteSyncRetry()
{
    ++writeSyncRetryCounter_;
}

void
BackendCounters::registerWriteStarted()
{
    asyncWriteCounters_.registerStarted(1u);
}

void
BackendCounters::registerWriteFinished()
{
    asyncWriteCounters_.registerFinished(1u);
}

void
BackendCounters::registerWriteRetry()
{
    asyncWriteCounters_.registerRetry(1u);
}

void
BackendCounters::registerReadStarted(std::uint64_t const count)
{
    asyncReadCounters_.registerStarted(count);
}

void
BackendCounters::registerReadFinished(std::uint64_t const count)
{
    asyncReadCounters_.registerFinished(count);
}

void
BackendCounters::registerReadRetry(std::uint64_t const count)
{
    asyncReadCounters_.registerRetry(count);
}

void
BackendCounters::registerReadError(std::uint64_t const count)
{
    asyncReadCounters_.registerError(count);
}

boost::json::object
BackendCounters::report() const
{
    boost::json::object result;
    result["too_busy"] = tooBusyCounter_;
    result["write_sync"] = writeSyncCounter_;
    result["write_sync_retry"] = writeSyncRetryCounter_;
    for (auto const& [key, value] : asyncWriteCounters_.report())
        result[key] = value;
    for (auto const& [key, value] : asyncReadCounters_.report())
        result[key] = value;
    return result;
}

BackendCounters::AsyncOperationCounters::AsyncOperationCounters(std::string name) : name_(std::move(name))
{
}

void
BackendCounters::AsyncOperationCounters::registerStarted(std::uint64_t const count)
{
    pendingCounter_ += count;
}

void
BackendCounters::AsyncOperationCounters::registerFinished(std::uint64_t const count)
{
    assert(pendingCounter_ >= count);
    pendingCounter_ -= count;
    completedCounter_ += count;
}

void
BackendCounters::AsyncOperationCounters::registerRetry(std::uint64_t count)
{
    retryCounter_ += count;
}

void
BackendCounters::AsyncOperationCounters::registerError(std::uint64_t count)
{
    assert(pendingCounter_ >= count);
    pendingCounter_ -= count;
    errorCounter_ += count;
}

boost::json::object
BackendCounters::AsyncOperationCounters::report() const
{
    return boost::json::object{
        {name_ + "_pending", pendingCounter_},
        {name_ + "_completed", completedCounter_},
        {name_ + "_retry", retryCounter_},
        {name_ + "_error", errorCounter_}};
}

}  // namespace data
