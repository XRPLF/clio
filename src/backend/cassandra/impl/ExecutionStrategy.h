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

#include <backend/cassandra/Handle.h>
#include <backend/cassandra/Types.h>
#include <backend/cassandra/impl/AsyncExecutor.h>
#include <log/Logger.h>
#include <util/Expected.h>

#include <boost/asio/async_result.hpp>
#include <boost/asio/spawn.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace Backend::Cassandra::detail {

/**
 * @brief Implements async and sync querying against the cassandra DB with
 * support for throttling.
 *
 * Note: A lot of the code that uses yield is repeated below. This is ok for now
 * because we are hopefully going to be getting rid of it entirely later on.
 */
template <typename HandleType = Handle>
class DefaultExecutionStrategy
{
    clio::Logger log_{"Backend"};

    std::uint32_t maxWriteRequestsOutstanding_;
    std::atomic_uint32_t numWriteRequestsOutstanding_ = 0;

    std::uint32_t maxReadRequestsOutstanding_;
    std::atomic_uint32_t numReadRequestsOutstanding_ = 0;

    std::mutex throttleMutex_;
    std::condition_variable throttleCv_;

    std::mutex syncMutex_;
    std::condition_variable syncCv_;

    boost::asio::io_context ioc_;
    std::optional<boost::asio::io_service::work> work_;

    std::reference_wrapper<HandleType const> handle_;
    std::thread thread_;

public:
    using ResultOrErrorType = typename HandleType::ResultOrErrorType;
    using StatementType = typename HandleType::StatementType;
    using PreparedStatementType = typename HandleType::PreparedStatementType;
    using FutureType = typename HandleType::FutureType;
    using FutureWithCallbackType = typename HandleType::FutureWithCallbackType;
    using ResultType = typename HandleType::ResultType;

    using CompletionTokenType = boost::asio::yield_context;
    using FunctionType = void(boost::system::error_code);
    using AsyncResultType =
        boost::asio::async_result<CompletionTokenType, FunctionType>;
    using HandlerType = typename AsyncResultType::completion_handler_type;

    DefaultExecutionStrategy(Settings settings, HandleType const& handle)
        : maxWriteRequestsOutstanding_{settings.maxWriteRequestsOutstanding}
        , maxReadRequestsOutstanding_{settings.maxReadRequestsOutstanding}
        , work_{ioc_}
        , handle_{std::cref(handle)}
        , thread_{[this]() { ioc_.run(); }}
    {
        log_.info() << "Max write requests outstanding is "
                    << maxWriteRequestsOutstanding_
                    << "; Max read requests outstanding is "
                    << maxReadRequestsOutstanding_;
    }

    ~DefaultExecutionStrategy()
    {
        work_.reset();
        ioc_.stop();
        thread_.join();
    }

    /**
     * @brief Wait for all async writes to finish before unblocking
     */
    void
    sync()
    {
        log_.debug() << "Waiting to sync all writes...";
        std::unique_lock<std::mutex> lck(syncMutex_);
        syncCv_.wait(lck, [this]() { return finishedAllWriteRequests(); });
        log_.debug() << "Sync done.";
    }

    bool
    isTooBusy() const
    {
        return numReadRequestsOutstanding_ >= maxReadRequestsOutstanding_;
    }

    /**
     * @brief Blocking query execution used for writing data
     *
     * Retries forever sleeping for 5 milliseconds between attempts.
     */
    ResultOrErrorType
    writeSync(StatementType const& statement)
    {
        while (true)
        {
            if (auto res = handle_.get().execute(statement); res)
            {
                return res;
            }
            else
            {
                log_.warn()
                    << "Cassandra sync write error, retrying: " << res.error();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    /**
     * @brief Blocking query execution used for writing data
     *
     * Retries forever sleeping for 5 milliseconds between attempts.
     */
    template <typename... Args>
    ResultOrErrorType
    writeSync(PreparedStatementType const& preparedStatement, Args&&... args)
    {
        return writeSync(preparedStatement.bind(std::forward<Args>(args)...));
    }

    /**
     * @brief Non-blocking query execution used for writing data
     *
     * Retries forever with retry policy specified by @ref AsyncExecutor
     *
     * @param prepradeStatement Statement to prepare and execute
     * @param args Args to bind to the prepared statement
     * @throw DatabaseTimeout on timeout
     */
    template <typename... Args>
    void
    write(PreparedStatementType const& preparedStatement, Args&&... args)
    {
        auto statement = preparedStatement.bind(std::forward<Args>(args)...);
        incrementOutstandingRequestCount();

        // Note: lifetime is controlled by std::shared_from_this internally
        AsyncExecutor<decltype(statement), HandleType>::run(
            ioc_, handle_.get(), std::move(statement), [this](auto const&) {
                decrementOutstandingRequestCount();
            });
    }

    /**
     * @brief Non-blocking batched query execution used for writing data
     *
     * Retries forever with retry policy specified by @ref AsyncExecutor.
     *
     * @param statements Vector of statements to execute as a batch
     * @throw DatabaseTimeout on timeout
     */
    void
    write(std::vector<StatementType> const& statements)
    {
        incrementOutstandingRequestCount();

        // Note: lifetime is controlled by std::shared_from_this internally
        AsyncExecutor<decltype(statements), HandleType>::run(
            ioc_, handle_.get(), statements, [this](auto const&) {
                decrementOutstandingRequestCount();
            });
    }

    /**
     * @brief Coroutine-based query execution used for reading data.
     *
     * Retries forever until successful or throws an exception on timeout.
     *
     * @param token Completion token (yield_context)
     * @param prepradeStatement Statement to prepare and execute
     * @param args Args to bind to the prepared statement
     * @throw DatabaseTimeout on timeout
     * @return ResultType or error wrapped in Expected
     */
    template <typename... Args>
    [[maybe_unused]] ResultOrErrorType
    read(
        CompletionTokenType token,
        PreparedStatementType const& preparedStatement,
        Args&&... args)
    {
        return read(token, preparedStatement.bind(std::forward<Args>(args)...));
    }

    /**
     * @brief Coroutine-based query execution used for reading data.
     *
     * Retries forever until successful or throws an exception on timeout.
     *
     * @param token Completion token (yield_context)
     * @param statements Statements to execute in a batch
     * @throw DatabaseTimeout on timeout
     * @return ResultType or error wrapped in Expected
     */
    [[maybe_unused]] ResultOrErrorType
    read(
        CompletionTokenType token,
        std::vector<StatementType> const& statements)
    {
        auto handler = HandlerType{token};
        auto result = AsyncResultType{handler};

        // todo: perhaps use policy instead
        while (true)
        {
            numReadRequestsOutstanding_ += statements.size();

            auto const future = handle_.get().asyncExecute(
                statements, [handler](auto&&) mutable {
                    boost::asio::post(
                        boost::asio::get_associated_executor(handler),
                        [handler]() mutable {
                            handler(boost::system::error_code{});
                        });
                });

            // suspend coroutine until completion handler is called
            result.get();

            numReadRequestsOutstanding_ -= statements.size();

            // it's safe to call blocking get on future here as we already
            // waited for the coroutine to resume above.
            if (auto res = future.get(); res)
            {
                return res;
            }
            else
            {
                log_.error()
                    << "Failed batch read in coroutine: " << res.error();
                throwErrorIfNeeded(res.error());
            }
        }
    }

    /**
     * @brief Coroutine-based query execution used for reading data.
     *
     * Retries forever until successful or throws an exception on timeout.
     *
     * @param token Completion token (yield_context)
     * @param statement Statement to execute
     * @throw DatabaseTimeout on timeout
     * @return ResultType or error wrapped in Expected
     */
    [[maybe_unused]] ResultOrErrorType
    read(CompletionTokenType token, StatementType const& statement)
    {
        auto handler = HandlerType{token};
        auto result = AsyncResultType{handler};

        // todo: perhaps use policy instead
        while (true)
        {
            ++numReadRequestsOutstanding_;

            auto const future = handle_.get().asyncExecute(
                statement, [handler](auto const&) mutable {
                    boost::asio::post(
                        boost::asio::get_associated_executor(handler),
                        [handler]() mutable {
                            handler(boost::system::error_code{});
                        });
                });

            // suspend coroutine until completion handler is called
            result.get();

            --numReadRequestsOutstanding_;

            // it's safe to call blocking get on future here as we already
            // waited for the coroutine to resume above.
            if (auto res = future.get(); res)
            {
                return res;
            }
            else
            {
                log_.error() << "Failed read in coroutine: " << res.error();
                throwErrorIfNeeded(res.error());
            }
        }
    }

    /**
     * @brief Coroutine-based query execution used for reading data.
     *
     * Attempts to execute each statement. On any error the whole vector will be
     * discarded and exception will be thrown.
     *
     * @param token Completion token (yield_context)
     * @param statements Statements to execute
     * @throw DatabaseTimeout on db error
     * @return Vector of results
     */
    std::vector<ResultType>
    readEach(
        CompletionTokenType token,
        std::vector<StatementType> const& statements)
    {
        auto handler = HandlerType{token};
        auto result = AsyncResultType{handler};

        std::atomic_bool hadError = false;
        std::atomic_int numOutstanding = statements.size();
        numReadRequestsOutstanding_ += statements.size();

        auto futures = std::vector<FutureWithCallbackType>{};
        futures.reserve(numOutstanding);

        // used as the handler for each async statement individually
        auto executionHandler =
            [handler, &hadError, &numOutstanding](auto const& res) mutable {
                if (not res)
                    hadError = true;

                // when all async operations complete unblock the result
                if (--numOutstanding == 0)
                    boost::asio::post(
                        boost::asio::get_associated_executor(handler),
                        [handler]() mutable {
                            handler(boost::system::error_code{});
                        });
            };

        std::transform(
            std::cbegin(statements),
            std::cend(statements),
            std::back_inserter(futures),
            [this, &executionHandler](auto const& statement) {
                return handle_.get().asyncExecute(statement, executionHandler);
            });

        // suspend coroutine until completion handler is called
        result.get();

        numReadRequestsOutstanding_ -= statements.size();

        if (hadError)
            throw DatabaseTimeout{};

        std::vector<ResultType> results;
        results.reserve(futures.size());

        // it's safe to call blocking get on futures here as we already
        // waited for the coroutine to resume above.
        std::transform(
            std::make_move_iterator(std::begin(futures)),
            std::make_move_iterator(std::end(futures)),
            std::back_inserter(results),
            [](auto&& future) {
                auto entry = future.get();
                auto&& res = entry.value();
                return std::move(res);
            });

        assert(futures.size() == statements.size());
        assert(results.size() == statements.size());
        return results;
    }

private:
    void
    incrementOutstandingRequestCount()
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!canAddWriteRequest())
            {
                log_.trace() << "Max outstanding requests reached. "
                             << "Waiting for other requests to finish";
                throttleCv_.wait(
                    lck, [this]() { return canAddWriteRequest(); });
            }
        }
        ++numWriteRequestsOutstanding_;
    }

    void
    decrementOutstandingRequestCount()
    {
        // sanity check
        if (numWriteRequestsOutstanding_ == 0)
        {
            assert(false);
            throw std::runtime_error("decrementing num outstanding below 0");
        }
        size_t cur = (--numWriteRequestsOutstanding_);
        {
            // mutex lock required to prevent race condition around spurious
            // wakeup
            std::lock_guard lck(throttleMutex_);
            throttleCv_.notify_one();
        }
        if (cur == 0)
        {
            // mutex lock required to prevent race condition around spurious
            // wakeup
            std::lock_guard lck(syncMutex_);
            syncCv_.notify_one();
        }
    }

    bool
    canAddWriteRequest() const
    {
        return numWriteRequestsOutstanding_ < maxWriteRequestsOutstanding_;
    }

    bool
    finishedAllWriteRequests() const
    {
        return numWriteRequestsOutstanding_ == 0;
    }

    void
    throwErrorIfNeeded(CassandraError err) const
    {
        if (err.isTimeout())
            throw DatabaseTimeout();

        if (err.isInvalidQuery())
            throw std::runtime_error("Invalid query");
    }
};

}  // namespace Backend::Cassandra::detail
