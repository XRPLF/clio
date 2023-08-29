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

#include <data/BackendInterface.h>
#include <etl/SystemState.h>
#include <util/LedgerUtils.h>
#include <util/Profiler.h>
#include <util/log/Logger.h>

#include <ripple/protocol/LedgerHeader.h>

#include <chrono>

namespace etl::detail {

/**
 * @brief Publishes ledgers in a synchronized fashion.
 *
 * If ETL is started far behind the network, ledgers will be written and published very rapidly. Monitoring processes
 * will publish ledgers as they are written. However, to publish a ledger, the monitoring process needs to read all of
 * the transactions for that ledger from the database. Reading the transactions from the database requires network
 * calls, which can be slow. It is imperative however that the monitoring processes keep up with the writer, else the
 * monitoring processes will not be able to detect if the writer failed. Therefore, publishing each ledger (which
 * includes reading all of the transactions from the database) is done from the application wide asio io_service, and a
 * strand is used to ensure ledgers are published in order.
 */
template <typename SubscriptionManagerType>
class LedgerPublisher
{
    util::Logger log_{"ETL"};

    boost::asio::strand<boost::asio::io_context::executor_type> publishStrand_;

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManagerType> subscriptions_;
    std::reference_wrapper<SystemState const> state_;  // shared state for ETL

    std::chrono::time_point<ripple::NetClock> lastCloseTime_;
    mutable std::shared_mutex closeTimeMtx_;

    std::chrono::time_point<std::chrono::system_clock> lastPublish_;
    mutable std::shared_mutex publishTimeMtx_;

    std::optional<uint32_t> lastPublishedSequence_;
    mutable std::shared_mutex lastPublishedSeqMtx_;

public:
    /**
     * @brief Create an instance of the publisher
     */
    LedgerPublisher(
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        SystemState const& state)
        : publishStrand_{boost::asio::make_strand(ioc)}
        , backend_{backend}
        , subscriptions_{subscriptions}
        , state_{std::cref(state)}
    {
    }

    /**
     * @brief Attempt to read the specified ledger from the database, and then publish that ledger to the ledgers
     * stream.
     *
     * @param ledgerSequence the sequence of the ledger to publish
     * @param maxAttempts the number of times to attempt to read the ledger from the database. 1 attempt per second
     * @return whether the ledger was found in the database and published
     */
    bool
    publish(uint32_t ledgerSequence, std::optional<uint32_t> maxAttempts)
    {
        LOG(log_.info()) << "Attempting to publish ledger = " << ledgerSequence;
        size_t numAttempts = 0;
        while (not state_.get().isStopping)
        {
            auto range = backend_->hardFetchLedgerRangeNoThrow();

            if (!range || range->maxSequence < ledgerSequence)
            {
                LOG(log_.debug()) << "Trying to publish. Could not find "
                                     "ledger with sequence = "
                                  << ledgerSequence;

                // We try maxAttempts times to publish the ledger, waiting one second in between each attempt.
                if (maxAttempts && numAttempts >= maxAttempts)
                {
                    LOG(log_.debug()) << "Failed to publish ledger after " << numAttempts << " attempts.";
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                ++numAttempts;
                continue;
            }
            else
            {
                auto lgr = data::retryOnTimeout([&] { return backend_->syncFetchLedgerBySequence(ledgerSequence); });
                assert(lgr);
                publish(*lgr);

                return true;
            }
        }
        return false;
    }

    /**
     * @brief Publish the passed in ledger
     *
     * All ledgers are published thru publishStrand_ which ensures that all publishes are performed in a serial fashion.
     *
     * @param lgrInfo the ledger to publish
     */
    void
    publish(ripple::LedgerHeader const& lgrInfo)
    {
        boost::asio::post(publishStrand_, [this, lgrInfo = lgrInfo]() {
            LOG(log_.info()) << "Publishing ledger " << std::to_string(lgrInfo.seq);

            if (!state_.get().isWriting)
            {
                LOG(log_.info()) << "Updating cache";

                std::vector<data::LedgerObject> diff =
                    data::retryOnTimeout([&] { return backend_->syncFetchLedgerDiff(lgrInfo.seq); });

                backend_->cache().update(diff, lgrInfo.seq);  // todo: inject cache to update, don't use backend cache
                backend_->updateRange(lgrInfo.seq);
            }

            setLastClose(lgrInfo.closeTime);
            auto age = lastCloseAgeSeconds();

            // if the ledger closed over 10 minutes ago, assume we are still catching up and don't publish
            // TODO: this probably should be a strategy
            if (age < 600)
            {
                std::optional<ripple::Fees> fees =
                    data::retryOnTimeout([this, &lgrInfo] { return backend_->syncFetchFees(lgrInfo.seq); });

                std::vector<data::TransactionAndMetadata> transactions = data::retryOnTimeout(
                    [this, &lgrInfo] { return backend_->syncFetchAllTransactionsInLedger(lgrInfo.seq); });

                auto ledgerRange = backend_->fetchLedgerRange();
                assert(ledgerRange);
                assert(fees);

                std::string range =
                    std::to_string(ledgerRange->minSequence) + "-" + std::to_string(ledgerRange->maxSequence);

                subscriptions_->pubLedger(lgrInfo, *fees, range, transactions.size());

                for (auto& txAndMeta : transactions)
                    subscriptions_->pubTransaction(txAndMeta, lgrInfo);

                subscriptions_->pubBookChanges(lgrInfo, transactions);

                setLastPublishTime();
                LOG(log_.info()) << "Published ledger " << std::to_string(lgrInfo.seq);
            }
            else
                LOG(log_.info()) << "Skipping publishing ledger " << std::to_string(lgrInfo.seq);
        });

        // we track latest publish-requested seq, not necessarily already published
        setLastPublishedSequence(lgrInfo.seq);
    }

    /**
     * @brief Get time passed since last publish, in seconds
     */
    std::uint32_t
    lastPublishAgeSeconds() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - getLastPublish())
            .count();
    }

    /**
     * @brief Get last publish time as a time point
     */
    std::chrono::time_point<std::chrono::system_clock>
    getLastPublish() const
    {
        std::shared_lock lck(publishTimeMtx_);
        return lastPublish_;
    }

    /**
     * @brief Get time passed since last ledger close, in seconds
     */
    std::uint32_t
    lastCloseAgeSeconds() const
    {
        std::shared_lock lck(closeTimeMtx_);
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                       .count();
        auto closeTime = lastCloseTime_.time_since_epoch().count();
        if (now < (rippleEpochStart + closeTime))
            return 0;
        return now - (rippleEpochStart + closeTime);
    }

    std::optional<uint32_t>
    getLastPublishedSequence() const
    {
        std::scoped_lock lck(lastPublishedSeqMtx_);
        return lastPublishedSequence_;
    }

private:
    void
    setLastClose(std::chrono::time_point<ripple::NetClock> lastCloseTime)
    {
        std::scoped_lock lck(closeTimeMtx_);
        lastCloseTime_ = lastCloseTime;
    }

    void
    setLastPublishTime()
    {
        std::scoped_lock lck(publishTimeMtx_);
        lastPublish_ = std::chrono::system_clock::now();
    }

    void
    setLastPublishedSequence(std::optional<uint32_t> lastPublishedSequence)
    {
        std::scoped_lock lck(lastPublishedSeqMtx_);
        lastPublishedSequence_ = lastPublishedSequence;
    }
};

}  // namespace etl::detail
