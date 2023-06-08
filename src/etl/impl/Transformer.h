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

#include <backend/BackendInterface.h>
#include <etl/SystemState.h>
#include <etl/impl/LedgerLoader.h>
#include <log/Logger.h>
#include <util/LedgerUtils.h>
#include <util/Profiler.h>

#include <ripple/beast/core/CurrentThreadName.h>
#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <thread>

namespace clio::detail {

/*
 * TODO:
 *
 * 1) loading of data into db should not really be part of transform right?
 * 2) can we just prepare the data and give it to the loader afterwards?
 * 3) how to deal with cache update that is needed to write successors if neighbours not included?
 */

/**
 * @brief Transformer thread that prepares new ledger out of raw data from GRPC
 */
template <typename DataPipeType, typename LedgerLoaderType, typename LedgerPublisherType>
class Transformer
{
    using DataType = typename LedgerLoaderType::DataType;

    clio::Logger log_{"ETL"};

    std::reference_wrapper<DataPipeType> pipe_;
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<LedgerLoaderType> loader_;
    std::reference_wrapper<LedgerPublisherType> publisher_;
    uint32_t startSequence_;
    std::reference_wrapper<SystemState> state_;  // shared state for ETL

    std::thread thread_;

public:
    /**
     * @brief Create an instance of the transformer
     *
     * This spawns a new thread that reads from the data pipe and writes ledgers to the DB using LedgerLoader and
     * LedgerPublisher.
     */
    Transformer(
        DataPipeType& pipe,
        std::shared_ptr<BackendInterface> backend,
        LedgerLoaderType& loader,
        LedgerPublisherType& publisher,
        uint32_t startSequence,
        SystemState& state)
        : pipe_(std::ref(pipe))
        , backend_{backend}
        , loader_(std::ref(loader))
        , publisher_(std::ref(publisher))
        , startSequence_{startSequence}
        , state_{std::ref(state)}
    {
        thread_ = std::thread([this]() { process(); });
    }

    /**
     * @brief Joins the transformer thread
     */
    ~Transformer()
    {
        if (thread_.joinable())
            thread_.join();
    }

    /**
     * @brief Block calling thread until transformer thread exits
     */
    void
    waitTillFinished()
    {
        assert(thread_.joinable());
        thread_.join();
    }

private:
    bool
    isStopping() const
    {
        return state_.get().isStopping;
    }

    void
    process()
    {
        beast::setCurrentThreadName("ETLService transform");
        uint32_t currentSequence = startSequence_;

        while (not state_.get().writeConflict)
        {
            auto fetchResponse = pipe_.get().popNext(currentSequence);
            ++currentSequence;

            // if fetchResponse is an empty optional, the extracter thread has stopped and the transformer should
            // stop as well
            if (!fetchResponse)
                break;

            if (isStopping())
                continue;

            auto const numTxns = fetchResponse->transactions_list().transactions_size();
            auto const numObjects = fetchResponse->ledger_objects().objects_size();
            auto const start = std::chrono::system_clock::now();
            auto [lgrInfo, success] = buildNextLedger(*fetchResponse);
            auto const end = std::chrono::system_clock::now();

            auto duration = ((end - start).count()) / 1000000000.0;
            if (success)
                log_.info() << "Load phase of etl : "
                            << "Successfully wrote ledger! Ledger info: " << util::toString(lgrInfo)
                            << ". txn count = " << numTxns << ". object count = " << numObjects
                            << ". load time = " << duration << ". load txns per second = " << numTxns / duration
                            << ". load objs per second = " << numObjects / duration;
            else
                log_.error() << "Error writing ledger. " << util::toString(lgrInfo);

            // success is false if the ledger was already written
            if (success)
                publisher_.get().publish(lgrInfo);

            state_.get().writeConflict = !success;
        }
    }

    // TODO update this documentation
    /**
     * @brief Build the next ledger using the previous ledger and the extracted data.
     * @note rawData should be data that corresponds to the ledger immediately following the previous seq.
     *
     * @param rawData data extracted from an ETL source
     * @return the newly built ledger and data to write to the database
     */
    std::pair<ripple::LedgerInfo, bool>
    buildNextLedger(DataType& rawData)
    {
        log_.debug() << "Beginning ledger update";
        ripple::LedgerInfo lgrInfo = util::deserializeHeader(ripple::makeSlice(rawData.ledger_header()));

        log_.debug() << "Deserialized ledger header. " << util::toString(lgrInfo);
        backend_->startWrites();
        log_.debug() << "started writes";

        backend_->writeLedger(lgrInfo, std::move(*rawData.mutable_ledger_header()));
        log_.debug() << "wrote ledger header";

        writeSuccessors(lgrInfo, rawData);
        updateCache(lgrInfo, rawData);

        log_.debug() << "Inserted/modified/deleted all objects. Number of objects = "
                     << rawData.ledger_objects().objects_size();

        FormattedTransactionsData insertTxResult = loader_.get().insertTransactions(lgrInfo, rawData);

        log_.debug() << "Inserted all transactions. Number of transactions  = "
                     << rawData.transactions_list().transactions_size();

        backend_->writeAccountTransactions(std::move(insertTxResult.accountTxData));
        backend_->writeNFTs(std::move(insertTxResult.nfTokensData));
        backend_->writeNFTTransactions(std::move(insertTxResult.nfTokenTxData));

        log_.debug() << "wrote account_tx";

        auto [success, duration] =
            util::timed<std::chrono::duration<double>>([&]() { return backend_->finishWrites(lgrInfo.seq); });

        log_.debug() << "Finished writes. took " << std::to_string(duration);
        log_.debug() << "Finished ledger update. " << util::toString(lgrInfo);

        return {lgrInfo, success};
    }

    /**
     * @brief Update cache from new ledger data.
     *
     * @param lgrInfo Ledger info
     * @param rawData Ledger data from GRPC
     */
    void
    updateCache(ripple::LedgerInfo const& lgrInfo, DataType& rawData)
    {
        std::vector<Backend::LedgerObject> cacheUpdates;
        cacheUpdates.reserve(rawData.ledger_objects().objects_size());

        // TODO change these to unordered_set
        std::set<ripple::uint256> bookSuccessorsToCalculate;
        std::set<ripple::uint256> modified;

        for (auto& obj : *(rawData.mutable_ledger_objects()->mutable_objects()))
        {
            auto key = ripple::uint256::fromVoidChecked(obj.key());
            assert(key);

            cacheUpdates.push_back({*key, {obj.mutable_data()->begin(), obj.mutable_data()->end()}});
            log_.debug() << "key = " << ripple::strHex(*key) << " - mod type = " << obj.mod_type();

            if (obj.mod_type() != org::xrpl::rpc::v1::RawLedgerObject::MODIFIED && !rawData.object_neighbors_included())
            {
                log_.debug() << "object neighbors not included. using cache";

                if (!backend_->cache().isFull() || backend_->cache().latestLedgerSequence() != lgrInfo.seq - 1)
                    throw std::runtime_error("Cache is not full, but object neighbors were not included");

                auto const blob = obj.mutable_data();
                bool checkBookBase = false;
                bool const isDeleted = (blob->size() == 0);

                if (isDeleted)
                {
                    auto old = backend_->cache().get(*key, lgrInfo.seq - 1);
                    assert(old);
                    checkBookBase = isBookDir(*key, *old);
                }
                else
                {
                    checkBookBase = isBookDir(*key, *blob);
                }

                if (checkBookBase)
                {
                    log_.debug() << "Is book dir. key = " << ripple::strHex(*key);

                    auto bookBase = getBookBase(*key);
                    auto oldFirstDir = backend_->cache().getSuccessor(bookBase, lgrInfo.seq - 1);
                    assert(oldFirstDir);

                    // We deleted the first directory, or we added a directory prior to the old first directory
                    if ((isDeleted && key == oldFirstDir->key) || (!isDeleted && key < oldFirstDir->key))
                    {
                        log_.debug() << "Need to recalculate book base successor. base = " << ripple::strHex(bookBase)
                                     << " - key = " << ripple::strHex(*key) << " - isDeleted = " << isDeleted
                                     << " - seq = " << lgrInfo.seq;
                        bookSuccessorsToCalculate.insert(bookBase);
                    }
                }
            }

            if (obj.mod_type() == org::xrpl::rpc::v1::RawLedgerObject::MODIFIED)
                modified.insert(*key);

            backend_->writeLedgerObject(std::move(*obj.mutable_key()), lgrInfo.seq, std::move(*obj.mutable_data()));
        }

        backend_->cache().update(cacheUpdates, lgrInfo.seq);

        // rippled didn't send successor information, so use our cache
        if (!rawData.object_neighbors_included())
        {
            log_.debug() << "object neighbors not included. using cache";
            if (!backend_->cache().isFull() || backend_->cache().latestLedgerSequence() != lgrInfo.seq)
                throw std::runtime_error("Cache is not full, but object neighbors were not included");

            for (auto const& obj : cacheUpdates)
            {
                if (modified.count(obj.key))
                    continue;

                auto lb = backend_->cache().getPredecessor(obj.key, lgrInfo.seq);
                if (!lb)
                    lb = {Backend::firstKey, {}};

                auto ub = backend_->cache().getSuccessor(obj.key, lgrInfo.seq);
                if (!ub)
                    ub = {Backend::lastKey, {}};

                if (obj.blob.size() == 0)
                {
                    log_.debug() << "writing successor for deleted object " << ripple::strHex(obj.key) << " - "
                                 << ripple::strHex(lb->key) << " - " << ripple::strHex(ub->key);

                    backend_->writeSuccessor(uint256ToString(lb->key), lgrInfo.seq, uint256ToString(ub->key));
                }
                else
                {
                    backend_->writeSuccessor(uint256ToString(lb->key), lgrInfo.seq, uint256ToString(obj.key));
                    backend_->writeSuccessor(uint256ToString(obj.key), lgrInfo.seq, uint256ToString(ub->key));

                    log_.debug() << "writing successor for new object " << ripple::strHex(lb->key) << " - "
                                 << ripple::strHex(obj.key) << " - " << ripple::strHex(ub->key);
                }
            }

            for (auto const& base : bookSuccessorsToCalculate)
            {
                auto succ = backend_->cache().getSuccessor(base, lgrInfo.seq);
                if (succ)
                {
                    backend_->writeSuccessor(uint256ToString(base), lgrInfo.seq, uint256ToString(succ->key));

                    log_.debug() << "Updating book successor " << ripple::strHex(base) << " - "
                                 << ripple::strHex(succ->key);
                }
                else
                {
                    backend_->writeSuccessor(uint256ToString(base), lgrInfo.seq, uint256ToString(Backend::lastKey));

                    log_.debug() << "Updating book successor " << ripple::strHex(base) << " - "
                                 << ripple::strHex(Backend::lastKey);
                }
            }
        }
    }

    /**
     * @brief Write successors info into DB
     *
     * @param lgrInfo Ledger info
     * @param rawData Ledger data from GRPC
     */
    void
    writeSuccessors(ripple::LedgerInfo const& lgrInfo, DataType& rawData)
    {
        // Write successor info, if included from rippled
        if (rawData.object_neighbors_included())
        {
            log_.debug() << "object neighbors included";

            for (auto& obj : *(rawData.mutable_book_successors()))
            {
                auto firstBook = std::move(*obj.mutable_first_book());
                if (!firstBook.size())
                    firstBook = uint256ToString(Backend::lastKey);
                log_.debug() << "writing book successor " << ripple::strHex(obj.book_base()) << " - "
                             << ripple::strHex(firstBook);

                backend_->writeSuccessor(std::move(*obj.mutable_book_base()), lgrInfo.seq, std::move(firstBook));
            }

            for (auto& obj : *(rawData.mutable_ledger_objects()->mutable_objects()))
            {
                if (obj.mod_type() != org::xrpl::rpc::v1::RawLedgerObject::MODIFIED)
                {
                    std::string* predPtr = obj.mutable_predecessor();
                    if (!predPtr->size())
                        *predPtr = uint256ToString(Backend::firstKey);
                    std::string* succPtr = obj.mutable_successor();
                    if (!succPtr->size())
                        *succPtr = uint256ToString(Backend::lastKey);

                    if (obj.mod_type() == org::xrpl::rpc::v1::RawLedgerObject::DELETED)
                    {
                        log_.debug() << "Modifying successors for deleted object " << ripple::strHex(obj.key()) << " - "
                                     << ripple::strHex(*predPtr) << " - " << ripple::strHex(*succPtr);

                        backend_->writeSuccessor(std::move(*predPtr), lgrInfo.seq, std::move(*succPtr));
                    }
                    else
                    {
                        log_.debug() << "adding successor for new object " << ripple::strHex(obj.key()) << " - "
                                     << ripple::strHex(*predPtr) << " - " << ripple::strHex(*succPtr);

                        backend_->writeSuccessor(std::move(*predPtr), lgrInfo.seq, std::string{obj.key()});
                        backend_->writeSuccessor(std::string{obj.key()}, lgrInfo.seq, std::move(*succPtr));
                    }
                }
                else
                    log_.debug() << "object modified " << ripple::strHex(obj.key());
            }
        }
    }
};

}  // namespace clio::detail
