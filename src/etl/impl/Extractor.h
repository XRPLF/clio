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

#include <log/Logger.h>
#include <util/Profiler.h>

#include <ripple/beast/core/CurrentThreadName.h>
#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <mutex>
#include <thread>

namespace clio::detail {

/**
 * @brief Extractor thread that is fetching GRPC data and enqueue it on the DataPipeType
 */
template <typename DataPipeType, typename NetworkValidatedLedgersType, typename LedgerFetcherType>
class Extractor
{
    clio::Logger log_{"ETL"};

    std::reference_wrapper<DataPipeType> pipe_;
    std::shared_ptr<NetworkValidatedLedgersType> networkValidatedLedgers_;
    std::reference_wrapper<LedgerFetcherType> ledgerFetcher_;
    uint32_t startSequence_;
    std::optional<uint32_t> finishSequence_;
    std::reference_wrapper<SystemState const> state_;  // shared state for ETL

    std::thread thread_;

public:
    Extractor(
        DataPipeType& pipe,
        std::shared_ptr<NetworkValidatedLedgersType> networkValidatedLedgers,
        LedgerFetcherType& ledgerFetcher,
        uint32_t startSequence,
        std::optional<uint32_t> finishSequence,
        SystemState const& state)
        : pipe_(std::ref(pipe))
        , networkValidatedLedgers_{networkValidatedLedgers}
        , ledgerFetcher_{std::ref(ledgerFetcher)}
        , startSequence_{startSequence}
        , finishSequence_{finishSequence}
        , state_{std::cref(state)}
    {
        thread_ = std::thread([this]() { process(); });
    }

    ~Extractor()
    {
        if (thread_.joinable())
            thread_.join();
    }

    void
    waitTillFinished()
    {
        assert(thread_.joinable());
        thread_.join();
    }

private:
    void
    process()
    {
        beast::setCurrentThreadName("ETLService extract");

        double totalTime = 0.0;
        auto currentSequence = startSequence_;
        auto transformQueue = pipe_.get().getExtractor(currentSequence);

        // Two stopping conditions:
        // - if there is a write conflict in the load thread, the ETL mechanism should stop.
        // - if the entire server is shutting down - this can be detected in a variety of ways.
        while ((not finishSequence_ || currentSequence <= *finishSequence_) && not state_.get().writeConflict &&
               not isStopping() && networkValidatedLedgers_->waitUntilValidatedByNetwork(currentSequence))
        {
            auto [fetchResponse, time] = util::timed<std::chrono::duration<double>>(
                [this, currentSequence]() { return ledgerFetcher_.get().fetchDataAndDiff(currentSequence); });
            totalTime += time;

            // if the fetch is unsuccessful, stop. fetchLedger only returns false if the server is shutting down, or if
            // the ledger was found in the database (which means another process already wrote the ledger that this
            // process was trying to extract; this is a form of a write conflict). Otherwise, fetchLedgerDataAndDiff
            // will keep trying to fetch the specified ledger until successful.
            if (!fetchResponse)
                break;

            auto const tps = fetchResponse->transactions_list().transactions_size() / time;
            log_.info() << "Extract phase time = " << time << "; Extract phase tps = " << tps
                        << "; Avg extract time = " << totalTime / (currentSequence - startSequence_ + 1)
                        << "; seq = " << currentSequence;

            transformQueue->push(std::move(fetchResponse));
            currentSequence += pipe_.get().numQueues();

            if (finishSequence_ && currentSequence > *finishSequence_)
                break;
        }

        // empty optional tells the transformer to shut down
        transformQueue->push({});
    }

    bool
    isStopping() const
    {
        return state_.get().isStopping;
    }
};

}  // namespace clio::detail
