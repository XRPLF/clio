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
#include <etl/NFTHelpers.h>
#include <etl/SystemState.h>
#include <etl/impl/LedgerFetcher.h>
#include <log/Logger.h>
#include <util/LedgerUtils.h>
#include <util/Profiler.h>

#include <ripple/beast/core/CurrentThreadName.h>
#include <ripple/ledger/ReadView.h>

#include <memory>

struct FormattedTransactionsData
{
    std::vector<AccountTransactionsData> accountTxData;
    std::vector<NFTTransactionsData> nfTokenTxData;
    std::vector<NFTsData> nfTokensData;
};

namespace clio::detail {

/**
 * @brief Loads ledger data into the DB
 */
template <typename LoadBalancerType, typename LedgerFetcherType>
class LedgerLoader
{
public:
    using GetLedgerResponseType = typename LoadBalancerType::GetLedgerResponseType;
    using OptionalGetLedgerResponseType = typename LoadBalancerType::OptionalGetLedgerResponseType;
    using RawLedgerObjectType = typename LoadBalancerType::RawLedgerObjectType;

private:
    clio::Logger log_{"ETL"};

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<LoadBalancerType> loadBalancer_;
    std::reference_wrapper<LedgerFetcherType> fetcher_;
    std::reference_wrapper<SystemState const> state_;  // shared state for ETL

public:
    /**
     * @brief Create an instance of the loader
     */
    LedgerLoader(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<LoadBalancerType> balancer,
        LedgerFetcherType& fetcher,
        SystemState const& state)
        : backend_{backend}, loadBalancer_{balancer}, fetcher_{std::ref(fetcher)}, state_{std::cref(state)}
    {
    }

    /**
     * @brief Insert extracted transaction into the ledger
     *
     * Insert all of the extracted transactions into the ledger, returning transactions related to accounts,
     * transactions related to NFTs, and NFTs themselves for later processsing.
     *
     * @param ledger ledger to insert transactions into
     * @param data data extracted from an ETL source
     * @return struct that contains the neccessary info to write to the account_transactions/account_tx and
     * nft_token_transactions tables (mostly transaction hashes, corresponding nodestore hashes and affected accounts)
     */
    FormattedTransactionsData
    insertTransactions(ripple::LedgerInfo const& ledger, GetLedgerResponseType& data)
    {
        FormattedTransactionsData result;

        for (auto& txn : *(data.mutable_transactions_list()->mutable_transactions()))
        {
            std::string* raw = txn.mutable_transaction_blob();

            ripple::SerialIter it{raw->data(), raw->size()};
            std::optional<ripple::STTx> sttxOp;
            try
            {
                sttxOp.emplace(ripple::STTx{it});
            }
            catch (std::runtime_error& e)
            {
                log_.error() << "Failed to deserialize STTx for transaction : " << e.what();
                continue;
            }

            log_.trace() << "Inserting transaction = " << sttxOp->getTransactionID();

            std::optional<ripple::TxMeta> txMetaOp;

            try
            {
                txMetaOp.emplace(ripple::TxMeta{sttxOp->getTransactionID(), ledger.seq, txn.metadata_blob()});
            }
            catch (std::runtime_error& e)
            {
                log_.error() << "Failed to deserializing TxMeta for transaction " << sttxOp->getTransactionID() << " : "
                             << e.what();
                continue;
            }

            auto const [nftTxs, maybeNFT] = getNFTDataFromTx(*txMetaOp, *sttxOp);
            result.nfTokenTxData.insert(result.nfTokenTxData.end(), nftTxs.begin(), nftTxs.end());
            if (maybeNFT)
                result.nfTokensData.push_back(*maybeNFT);

            auto journal = ripple::debugLog();
            result.accountTxData.emplace_back(*txMetaOp, sttxOp->getTransactionID(), journal);
            std::string keyStr{(const char*)sttxOp->getTransactionID().data(), 32};
            backend_->writeTransaction(
                std::move(keyStr),
                ledger.seq,
                ledger.closeTime.time_since_epoch().count(),
                std::move(*raw),
                std::move(*txn.mutable_metadata_blob()));
        }

        // Remove all but the last NFTsData for each id. unique removes all but the first of a group, so we want to
        // reverse sort by transaction index
        std::sort(result.nfTokensData.begin(), result.nfTokensData.end(), [](NFTsData const& a, NFTsData const& b) {
            return a.tokenID > b.tokenID && a.transactionIndex > b.transactionIndex;
        });

        // Now we can unique the NFTs by tokenID.
        auto last = std::unique(
            result.nfTokensData.begin(), result.nfTokensData.end(), [](NFTsData const& a, NFTsData const& b) {
                return a.tokenID == b.tokenID;
            });
        result.nfTokensData.erase(last, result.nfTokensData.end());

        return result;
    }

    /**
     * @brief Download a ledger with specified sequence in full
     *
     * Note: This takes several minutes or longer.
     *
     * @param sequence the sequence of the ledger to download
     * @return The ledger downloaded, with a full transaction and account state map
     */
    std::optional<ripple::LedgerInfo>
    loadInitialLedger(uint32_t sequence)
    {
        // check that database is actually empty
        auto rng = backend_->hardFetchLedgerRangeNoThrow();
        if (rng)
        {
            log_.fatal() << "Database is not empty";
            assert(false);
            return {};
        }

        // Fetch the ledger from the network. This function will not return until either the fetch is successful, or the
        // server is being shutdown. This only fetches the ledger header and the transactions+metadata
        OptionalGetLedgerResponseType ledgerData{fetcher_.get().fetchData(sequence)};
        if (!ledgerData)
            return {};

        ripple::LedgerInfo lgrInfo = util::deserializeHeader(ripple::makeSlice(ledgerData->ledger_header()));

        log_.debug() << "Deserialized ledger header. " << util::toString(lgrInfo);

        auto timeDiff = util::timed<std::chrono::duration<double>>([this, sequence, &lgrInfo, &ledgerData]() {
            backend_->startWrites();

            log_.debug() << "Started writes";

            backend_->writeLedger(lgrInfo, std::move(*ledgerData->mutable_ledger_header()));

            log_.debug() << "Wrote ledger";
            FormattedTransactionsData insertTxResult = insertTransactions(lgrInfo, *ledgerData);
            log_.debug() << "Inserted txns";

            // download the full account state map. This function downloads full
            // ledger data and pushes the downloaded data into the writeQueue.
            // asyncWriter consumes from the queue and inserts the data into the
            // Ledger object. Once the below call returns, all data has been pushed
            // into the queue
            auto [edgeKeys, success] = loadBalancer_->loadInitialLedger(sequence);

            if (success)
            {
                size_t numWrites = 0;
                backend_->cache().setFull();

                auto seconds = util::timed<std::chrono::seconds>([this, edgeKeys = &edgeKeys, sequence, &numWrites]() {
                    for (auto& key : *edgeKeys)
                    {
                        log_.debug() << "Writing edge key = " << ripple::strHex(key);
                        auto succ = backend_->cache().getSuccessor(*ripple::uint256::fromVoidChecked(key), sequence);
                        if (succ)
                            backend_->writeSuccessor(std::move(key), sequence, uint256ToString(succ->key));
                    }

                    ripple::uint256 prev = Backend::firstKey;
                    while (auto cur = backend_->cache().getSuccessor(prev, sequence))
                    {
                        assert(cur);
                        if (prev == Backend::firstKey)
                            backend_->writeSuccessor(uint256ToString(prev), sequence, uint256ToString(cur->key));

                        bool isBookDirFlag = false;

                        try
                        {
                            isBookDirFlag = isBookDir(cur->key, cur->blob);
                        }
                        catch (std::runtime_error const& e)
                        {
                            log_.error() << "Failed to deserialize node: " << cur->key << " : " << e.what();
                            continue;
                        }

                        if (isBookDirFlag)
                        {
                            auto base = getBookBase(cur->key);
                            // make sure the base is not an actual object
                            if (!backend_->cache().get(cur->key, sequence))
                            {
                                auto succ = backend_->cache().getSuccessor(base, sequence);
                                assert(succ);
                                if (succ->key == cur->key)
                                {
                                    log_.debug() << "Writing book successor = " << ripple::strHex(base) << " - "
                                                 << ripple::strHex(cur->key);

                                    backend_->writeSuccessor(
                                        uint256ToString(base), sequence, uint256ToString(cur->key));
                                }
                            }

                            ++numWrites;
                        }

                        prev = std::move(cur->key);
                        if (numWrites % 100000 == 0 && numWrites != 0)
                            log_.info() << "Wrote " << numWrites << " book successors";
                    }

                    backend_->writeSuccessor(uint256ToString(prev), sequence, uint256ToString(Backend::lastKey));
                    ++numWrites;
                });

                log_.info() << "Looping through cache and submitting all writes took " << seconds
                            << " seconds. numWrites = " << std::to_string(numWrites);
            }

            log_.debug() << "Loaded initial ledger";

            if (not state_.get().isStopping)
            {
                backend_->writeAccountTransactions(std::move(insertTxResult.accountTxData));
                backend_->writeNFTs(std::move(insertTxResult.nfTokensData));
                backend_->writeNFTTransactions(std::move(insertTxResult.nfTokenTxData));
            }

            backend_->finishWrites(sequence);
        });

        log_.debug() << "Time to download and store ledger = " << timeDiff;
        return lgrInfo;
    }
};

}  // namespace clio::detail
