#pragma once

#include "data/BackendInterface.hpp"
#include "etl/LedgerFetcherInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "util/log/Logger.hpp"

#include <grpcpp/grpcpp.h>
#include <xrpl/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace etl::impl {

/**
 * @brief GRPC Ledger data fetcher
 */
class LedgerFetcher : public LedgerFetcherInterface {
private:
    util::Logger log_{"ETL"};

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<LoadBalancerInterface> loadBalancer_;

public:
    /**
     * @brief Create an instance of the fetcher
     */
    LedgerFetcher(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<LoadBalancerInterface> balancer
    )
        : backend_(std::move(backend)), loadBalancer_(std::move(balancer))
    {
    }

    /**
     * @brief Extract data for a particular ledger from an ETL source
     *
     * This function continuously tries to extract the specified ledger (using all available ETL
     * sources) until the extraction succeeds, or the server shuts down.
     *
     * @param sequence sequence of the ledger to extract
     * @return Ledger header and transaction+metadata blobs; Empty optional if the server is
     * shutting down
     */
    [[nodiscard]] OptionalGetLedgerResponseType
    fetchData(uint32_t sequence) override
    {
        LOG(log_.debug()) << "Attempting to fetch ledger with sequence = " << sequence;

        auto response = loadBalancer_->fetchLedger(sequence, false, false);
        if (response)
            LOG(log_.trace()) << "GetLedger reply = " << response->DebugString();
        return response;
    }

    /**
     * @brief Extract diff data for a particular ledger from an ETL source.
     *
     * This function continuously tries to extract the specified ledger (using all available ETL
     * sources) until the extraction succeeds, or the server shuts down.
     *
     * @param sequence sequence of the ledger to extract
     * @return Ledger data diff between sequance and parent; Empty optional if the server is
     * shutting down
     */
    [[nodiscard]] OptionalGetLedgerResponseType
    fetchDataAndDiff(uint32_t sequence) override
    {
        LOG(log_.debug()) << "Attempting to fetch ledger with sequence = " << sequence;

        auto const isCacheFull = backend_->cache().isFull();
        auto const isLedgerCached = backend_->cache().latestLedgerSequence() >= sequence;
        if (isLedgerCached) {
            LOG(log_.info()) << sequence
                             << " is already cached, the current latest seq in cache is "
                             << backend_->cache().latestLedgerSequence() << " and the cache is "
                             << (isCacheFull ? "full" : "not full");
        }

        auto response = loadBalancer_->fetchLedger(sequence, true, !isCacheFull || isLedgerCached);
        if (response)
            LOG(log_.trace()) << "GetLedger reply = " << response->DebugString();

        return response;
    }
};

}  // namespace etl::impl
