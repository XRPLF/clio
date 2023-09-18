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

#include <data/cassandra/Concepts.h>
#include <data/cassandra/Handle.h>
#include <data/cassandra/Types.h>
#include <data/cassandra/impl/RetryPolicy.h>
#include <util/Expected.h>
#include <util/log/Logger.h>

#include <boost/asio.hpp>

#include <functional>
#include <memory>

namespace data::cassandra::detail {

/**
 * @brief A query executor with a changable retry policy
 *
 * Note: this is a bit of an anti-pattern and should be done differently
 * eventually.
 *
 * Currently it's basically a saner implementation of the previous design that
 * was used in production without much issue but was using raw new/delete and
 * could leak easily. This version is slightly better but the overall design is
 * flawed and should be reworked.
 */
template <
    typename StatementType,
    typename HandleType = Handle,
    SomeRetryPolicy RetryPolicyType = ExponentialBackoffRetryPolicy>
class AsyncExecutor : public std::enable_shared_from_this<AsyncExecutor<StatementType, HandleType, RetryPolicyType>>
{
    using FutureWithCallbackType = typename HandleType::FutureWithCallbackType;
    using CallbackType = std::function<void(typename HandleType::ResultOrErrorType)>;

    util::Logger log_{"Backend"};

    StatementType data_;
    RetryPolicyType retryPolicy_;
    CallbackType onComplete_;

    // does not exist during initial construction, hence optional
    std::optional<FutureWithCallbackType> future_;
    std::mutex mtx_;

public:
    /**
     * @brief Create a new instance of the AsyncExecutor and execute it.
     */
    static void
    run(boost::asio::io_context& ioc, HandleType const& handle, StatementType&& data, CallbackType&& onComplete)
    {
        // this is a helper that allows us to use std::make_shared below
        struct EnableMakeShared : public AsyncExecutor<StatementType, HandleType, RetryPolicyType>
        {
            EnableMakeShared(boost::asio::io_context& ioc, StatementType&& data, CallbackType&& onComplete)
                : AsyncExecutor(ioc, std::move(data), std::move(onComplete))
            {
            }
        };

        auto ptr = std::make_shared<EnableMakeShared>(ioc, std::move(data), std::move(onComplete));
        ptr->execute(handle);
    }

private:
    AsyncExecutor(boost::asio::io_context& ioc, StatementType&& data, CallbackType&& onComplete)
        : data_{std::move(data)}, retryPolicy_{ioc}, onComplete_{std::move(onComplete)}
    {
    }

    void
    execute(HandleType const& handle)
    {
        auto self = this->shared_from_this();

        // lifetime is extended by capturing self ptr
        auto handler = [this, &handle, self](auto&& res) mutable {
            if (res)
            {
                onComplete_(std::forward<decltype(res)>(res));
            }
            else
            {
                if (retryPolicy_.shouldRetry(res.error()))
                    retryPolicy_.retry([self, &handle]() { self->execute(handle); });
                else
                    onComplete_(std::forward<decltype(res)>(res));  // report error
            }

            self = nullptr;  // explicitly decrement refcount
        };

        std::scoped_lock lck{mtx_};
        future_.emplace(handle.asyncExecute(data_, std::move(handler)));
    }
};

}  // namespace data::cassandra::detail
