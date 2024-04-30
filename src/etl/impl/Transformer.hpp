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

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlock.hpp"
#include "etl/impl/LedgerLoader.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <grpcpp/grpcpp.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/beast/core/CurrentThreadName.h>
#include <ripple/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>
#include <ripple/protocol/LedgerHeader.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace etl::impl {

/*
 * TODO:
 *
 * 1) loading of data into db should not really be part of transform right?
 * 2) can we just prepare the data and give it to the loader afterwards?
 * 3) how to deal with cache update that is needed to write successors if neighbours not included?
 */

/**
 * @brief Transformer thread that prepares new ledger out of raw data from GRPC.
 */
template <
    typename DataPipeType,
    typename LedgerLoaderType,
    typename LedgerPublisherType,
    typename AmendmentBlockHandlerType>
class Transformer {
    using GetLedgerResponseType = typename LedgerLoaderType::GetLedgerResponseType;
    using RawLedgerObjectType = typename LedgerLoaderType::RawLedgerObjectType;

    util::Logger log_{"ETL"};

    std::reference_wrapper<DataPipeType> pipe_;
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<LedgerLoaderType> loader_;
    std::reference_wrapper<LedgerPublisherType> publisher_;
    std::reference_wrapper<AmendmentBlockHandlerType> amendmentBlockHandler_;

    uint32_t startSequence_;
    std::reference_wrapper<SystemState> state_;  // shared state for ETL

    std::thread thread_;

public:
    /**
     * @brief Create an instance of the transformer.
     *
     * This spawns a new thread that reads from the data pipe and writes ledgers to the DB using LedgerLoader and
     * LedgerPublisher.
     */
    Transformer(
        DataPipeType& pipe,
        std::shared_ptr<BackendInterface> backend,
        LedgerLoaderType& loader,
        LedgerPublisherType& publisher,
        AmendmentBlockHandlerType& amendmentBlockHandler,
        uint32_t startSequence,
        SystemState& state
    )
        : pipe_{std::ref(pipe)}
        , backend_{std::move(backend)}
        , loader_{std::ref(loader)}
        , publisher_{std::ref(publisher)}
        , amendmentBlockHandler_{std::ref(amendmentBlockHandler)}
        , startSequence_{startSequence}
        , state_{std::ref(state)}
    {
        thread_ = std::thread([this]() { process(); });
    }

    /**
     * @brief Joins the transformer thread.
     */
    ~Transformer()
    {
        if (thread_.joinable())
            thread_.join();
    }

    /**
     * @brief Block calling thread until transformer thread exits.
     */
    void
    waitTillFinished()
    {
        ASSERT(thread_.joinable(), "Transformer thread must be joinable");
        thread_.join();
    }

private:
    void
    process()
    {
        beast::setCurrentThreadName("ETLService transform");
        uint32_t currentSequence = startSequence_;

        while (not hasWriteConflict()) {
            auto fetchResponse = pipe_.get().popNext(currentSequence);
            ++currentSequence;

            // if fetchResponse is an empty optional, the extracter thread has stopped and the transformer should
            // stop as well
            if (!fetchResponse)
                break;

            if (isStopping())
                continue;

            auto const start = std::chrono::system_clock::now();
            auto [lgrInfo, success] = buildNextLedger(*fetchResponse);

            if (success) {
                auto const numTxns = fetchResponse->transactions_list().transactions_size();
                auto const numObjects = fetchResponse->ledger_objects().objects_size();
                auto const end = std::chrono::system_clock::now();
                auto const duration = ((end - start).count()) / 1000000000.0;

                LOG(log_.info()) << "Load phase of etl : " << "Successfully wrote ledger! Ledger info: "
                                 << util::toString(lgrInfo) << ". txn count = " << numTxns
                                 << ". object count = " << numObjects << ". load time = " << duration
                                 << ". load txns per second = " << numTxns / duration
                                 << ". load objs per second = " << numObjects / duration;

                // success is false if the ledger was already written
                publisher_.get().publish(lgrInfo);
            } else {
                LOG(log_.error()) << "Error writing ledger. " << util::toString(lgrInfo);
            }

            setWriteConflict(not success);
        }
    }

    /**
     * @brief Build the next ledger using the previous ledger and the extracted data.
     * @note rawData should be data that corresponds to the ledger immediately following the previous seq.
     *
     * @param rawData Data extracted from an ETL source
     * @return The newly built ledger and data to write to the database
     */
    std::pair<ripple::LedgerHeader, bool>
    buildNextLedger(GetLedgerResponseType& rawData)
    {
        LOG(log_.debug()) << "Beginning ledger update";
        ripple::LedgerHeader lgrInfo = ::util::deserializeHeader(ripple::makeSlice(rawData.ledger_header()));

        LOG(log_.debug()) << "Deserialized ledger header. " << ::util::toString(lgrInfo);
        backend_->startWrites();
        backend_->writeLedger(lgrInfo, std::move(*rawData.mutable_ledger_header()));

        writeSuccessors(lgrInfo, rawData);
        std::optional<FormattedTransactionsData> insertTxResultOp;
        try {
            updateCache(lgrInfo, rawData);

            LOG(log_.debug()) << "Inserted/modified/deleted all objects. Number of objects = "
                              << rawData.ledger_objects().objects_size();

            insertTxResultOp.emplace(loader_.get().insertTransactions(lgrInfo, rawData));
        } catch (std::runtime_error const& e) {
            LOG(log_.fatal()) << "Failed to build next ledger: " << e.what();

            amendmentBlockHandler_.get().onAmendmentBlock();
            return {ripple::LedgerHeader{}, false};
        }

        LOG(log_.debug()) << "Inserted all transactions. Number of transactions  = "
                          << rawData.transactions_list().transactions_size();

        backend_->writeAccountTransactions(std::move(insertTxResultOp->accountTxData));
        backend_->writeNFTs(insertTxResultOp->nfTokensData);
        backend_->writeNFTTransactions(insertTxResultOp->nfTokenTxData);

        auto [success, duration] =
            ::util::timed<std::chrono::duration<double>>([&]() { return backend_->finishWrites(lgrInfo.seq); });

        LOG(log_.debug()) << "Finished writes. Total time: " << std::to_string(duration);
        LOG(log_.debug()) << "Finished ledger update: " << ::util::toString(lgrInfo);

        return {lgrInfo, success};
    }

    /**
     * @brief Update cache from new ledger data.
     *
     * @param lgrInfo Ledger info
     * @param rawData Ledger data from GRPC
     */
    void
    updateCache(ripple::LedgerHeader const& lgrInfo, GetLedgerResponseType& rawData)
    {
        std::vector<data::LedgerObject> cacheUpdates;
        cacheUpdates.reserve(rawData.ledger_objects().objects_size());

        // TODO change these to unordered_set
        std::set<ripple::uint256> bookSuccessorsToCalculate;
        std::set<ripple::uint256> modified;

        for (auto& obj : *(rawData.mutable_ledger_objects()->mutable_objects())) {
            auto key = ripple::uint256::fromVoidChecked(obj.key());
            ASSERT(key.has_value(), "Failed to deserialize key from void");

            cacheUpdates.push_back({*key, {obj.mutable_data()->begin(), obj.mutable_data()->end()}});
            LOG(log_.debug()) << "key = " << ripple::strHex(*key) << " - mod type = " << obj.mod_type();

            if (obj.mod_type() != RawLedgerObjectType::MODIFIED && !rawData.object_neighbors_included()) {
                LOG(log_.debug()) << "object neighbors not included. using cache";

                if (!backend_->cache().isFull() || backend_->cache().latestLedgerSequence() != lgrInfo.seq - 1)
                    throw std::logic_error("Cache is not full, but object neighbors were not included");

                auto const blob = obj.mutable_data();
                auto checkBookBase = false;
                auto const isDeleted = (blob->size() == 0);

                if (isDeleted) {
                    auto const old = backend_->cache().get(*key, lgrInfo.seq - 1);
                    ASSERT(old.has_value(), "Deleted object {} must be in cache", ripple::strHex(*key));
                    checkBookBase = isBookDir(*key, *old);
                } else {
                    checkBookBase = isBookDir(*key, *blob);
                }

                if (checkBookBase) {
                    LOG(log_.debug()) << "Is book dir. Key = " << ripple::strHex(*key);

                    auto const bookBase = getBookBase(*key);
                    auto const oldFirstDir = backend_->cache().getSuccessor(bookBase, lgrInfo.seq - 1);
                    ASSERT(
                        oldFirstDir.has_value(),
                        "Book base must have a successor for lgrInfo.seq - 1 = {}",
                        lgrInfo.seq - 1
                    );

                    // We deleted the first directory, or we added a directory prior to the old first
                    // directory
                    if ((isDeleted && key == oldFirstDir->key) || (!isDeleted && key < oldFirstDir->key)) {
                        LOG(log_.debug())
                            << "Need to recalculate book base successor. base = " << ripple::strHex(bookBase)
                            << " - key = " << ripple::strHex(*key) << " - isDeleted = " << isDeleted
                            << " - seq = " << lgrInfo.seq;
                        bookSuccessorsToCalculate.insert(bookBase);
                    }
                }
            }

            if (obj.mod_type() == RawLedgerObjectType::MODIFIED)
                modified.insert(*key);

            backend_->writeLedgerObject(std::move(*obj.mutable_key()), lgrInfo.seq, std::move(*obj.mutable_data()));
        }

        backend_->cache().update(cacheUpdates, lgrInfo.seq);

        // rippled didn't send successor information, so use our cache
        if (!rawData.object_neighbors_included()) {
            LOG(log_.debug()) << "object neighbors not included. using cache";
            if (!backend_->cache().isFull() || backend_->cache().latestLedgerSequence() != lgrInfo.seq)
                throw std::logic_error("Cache is not full, but object neighbors were not included");

            for (auto const& obj : cacheUpdates) {
                if (modified.contains(obj.key))
                    continue;

                auto lb = backend_->cache().getPredecessor(obj.key, lgrInfo.seq);
                if (!lb)
                    lb = {data::firstKey, {}};

                auto ub = backend_->cache().getSuccessor(obj.key, lgrInfo.seq);
                if (!ub)
                    ub = {data::lastKey, {}};

                if (obj.blob.empty()) {
                    LOG(log_.debug()) << "writing successor for deleted object " << ripple::strHex(obj.key) << " - "
                                      << ripple::strHex(lb->key) << " - " << ripple::strHex(ub->key);

                    backend_->writeSuccessor(uint256ToString(lb->key), lgrInfo.seq, uint256ToString(ub->key));
                } else {
                    backend_->writeSuccessor(uint256ToString(lb->key), lgrInfo.seq, uint256ToString(obj.key));
                    backend_->writeSuccessor(uint256ToString(obj.key), lgrInfo.seq, uint256ToString(ub->key));

                    LOG(log_.debug()) << "writing successor for new object " << ripple::strHex(lb->key) << " - "
                                      << ripple::strHex(obj.key) << " - " << ripple::strHex(ub->key);
                }
            }

            for (auto const& base : bookSuccessorsToCalculate) {
                auto succ = backend_->cache().getSuccessor(base, lgrInfo.seq);
                if (succ) {
                    backend_->writeSuccessor(uint256ToString(base), lgrInfo.seq, uint256ToString(succ->key));

                    LOG(log_.debug()) << "Updating book successor " << ripple::strHex(base) << " - "
                                      << ripple::strHex(succ->key);
                } else {
                    backend_->writeSuccessor(uint256ToString(base), lgrInfo.seq, uint256ToString(data::lastKey));

                    LOG(log_.debug()) << "Updating book successor " << ripple::strHex(base) << " - "
                                      << ripple::strHex(data::lastKey);
                }
            }
        }
    }

    /**
     * @brief Write successors info into DB.
     *
     * @param lgrInfo Ledger info
     * @param rawData Ledger data from GRPC
     */
    void
    writeSuccessors(ripple::LedgerHeader const& lgrInfo, GetLedgerResponseType& rawData)
    {
        // Write successor info, if included from rippled
        if (rawData.object_neighbors_included()) {
            LOG(log_.debug()) << "object neighbors included";

            for (auto& obj : *(rawData.mutable_book_successors())) {
                auto firstBook = std::move(*obj.mutable_first_book());
                if (!firstBook.size())
                    firstBook = uint256ToString(data::lastKey);
                LOG(log_.debug()) << "writing book successor " << ripple::strHex(obj.book_base()) << " - "
                                  << ripple::strHex(firstBook);

                backend_->writeSuccessor(std::move(*obj.mutable_book_base()), lgrInfo.seq, std::move(firstBook));
            }

            for (auto& obj : *(rawData.mutable_ledger_objects()->mutable_objects())) {
                if (obj.mod_type() != RawLedgerObjectType::MODIFIED) {
                    std::string* predPtr = obj.mutable_predecessor();
                    if (predPtr->empty())
                        *predPtr = uint256ToString(data::firstKey);
                    std::string* succPtr = obj.mutable_successor();
                    if (succPtr->empty())
                        *succPtr = uint256ToString(data::lastKey);

                    if (obj.mod_type() == RawLedgerObjectType::DELETED) {
                        LOG(log_.debug()) << "Modifying successors for deleted object " << ripple::strHex(obj.key())
                                          << " - " << ripple::strHex(*predPtr) << " - " << ripple::strHex(*succPtr);

                        backend_->writeSuccessor(std::move(*predPtr), lgrInfo.seq, std::move(*succPtr));
                    } else {
                        LOG(log_.debug()) << "adding successor for new object " << ripple::strHex(obj.key()) << " - "
                                          << ripple::strHex(*predPtr) << " - " << ripple::strHex(*succPtr);

                        backend_->writeSuccessor(std::move(*predPtr), lgrInfo.seq, std::string{obj.key()});
                        backend_->writeSuccessor(std::string{obj.key()}, lgrInfo.seq, std::move(*succPtr));
                    }
                } else
                    LOG(log_.debug()) << "object modified " << ripple::strHex(obj.key());
            }
        }
    }

    /** @return true if the transformer is stopping; false otherwise */
    bool
    isStopping() const
    {
        return state_.get().isStopping;
    }

    /** @return true if there was a write conflict; false otherwise */
    bool
    hasWriteConflict() const
    {
        return state_.get().writeConflict;
    }

    /**
     * @brief Sets the write conflict flag.
     *
     * @param conflict The value to set
     */
    void
    setWriteConflict(bool conflict)
    {
        state_.get().writeConflict = conflict;
    }
};

}  // namespace etl::impl
