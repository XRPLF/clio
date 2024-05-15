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
#include "etl/NFTHelpers.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/beast/core/CurrentThreadName.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/TxMeta.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Account transactions, NFT transactions and NFT data bundled togeher.
 */
struct FormattedTransactionsData {
    std::vector<AccountTransactionsData> accountTxData;
    std::vector<NFTTransactionsData> nfTokenTxData;
    std::vector<NFTsData> nfTokensData;
};

namespace etl::impl {

/**
 * @brief Loads ledger data into the DB
 */
template <typename LoadBalancerType, typename LedgerFetcherType>
class LedgerLoader {
public:
    using GetLedgerResponseType = typename LoadBalancerType::GetLedgerResponseType;
    using OptionalGetLedgerResponseType = typename LoadBalancerType::OptionalGetLedgerResponseType;
    using RawLedgerObjectType = typename LoadBalancerType::RawLedgerObjectType;

private:
    util::Logger log_{"ETL"};

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
        SystemState const& state
    )
        : backend_{std::move(backend)}
        , loadBalancer_{std::move(balancer)}
        , fetcher_{std::ref(fetcher)}
        , state_{std::cref(state)}
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
     * @return The neccessary info to write the account_transactions/account_tx and nft_token_transactions tables
     */
    FormattedTransactionsData
    insertTransactions(ripple::LedgerHeader const& ledger, GetLedgerResponseType& data)
    {
        FormattedTransactionsData result;

        for (auto& txn : *(data.mutable_transactions_list()->mutable_transactions())) {
            std::string* raw = txn.mutable_transaction_blob();

            ripple::SerialIter it{raw->data(), raw->size()};
            ripple::STTx const sttx{it};

            LOG(log_.trace()) << "Inserting transaction = " << sttx.getTransactionID();

            ripple::TxMeta txMeta{sttx.getTransactionID(), ledger.seq, txn.metadata_blob()};

            auto const [nftTxs, maybeNFT] = getNFTDataFromTx(txMeta, sttx);
            result.nfTokenTxData.insert(result.nfTokenTxData.end(), nftTxs.begin(), nftTxs.end());
            if (maybeNFT)
                result.nfTokensData.push_back(*maybeNFT);

            result.accountTxData.emplace_back(txMeta, sttx.getTransactionID());
            static constexpr std::size_t KEY_SIZE = 32;
            std::string keyStr{reinterpret_cast<char const*>(sttx.getTransactionID().data()), KEY_SIZE};
            backend_->writeTransaction(
                std::move(keyStr),
                ledger.seq,
                ledger.closeTime.time_since_epoch().count(),
                std::move(*raw),
                std::move(*txn.mutable_metadata_blob())
            );
        }

        // Remove all but the last NFTsData for each id. unique removes all but the first of a group, so we want to
        // reverse sort by transaction index
        std::sort(result.nfTokensData.begin(), result.nfTokensData.end(), [](NFTsData const& a, NFTsData const& b) {
            return a.tokenID > b.tokenID && a.transactionIndex > b.transactionIndex;
        });

        // Now we can unique the NFTs by tokenID.
        auto last = std::unique(
            result.nfTokensData.begin(),
            result.nfTokensData.end(),
            [](NFTsData const& a, NFTsData const& b) { return a.tokenID == b.tokenID; }
        );
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
    std::optional<ripple::LedgerHeader>
    loadInitialLedger(uint32_t sequence)
    {
        // check that database is actually empty
        auto rng = backend_->hardFetchLedgerRangeNoThrow();
        if (rng) {
            ASSERT(false, "Database is not empty");
            return {};
        }

        // Fetch the ledger from the network. This function will not return until either the fetch is successful, or the
        // server is being shutdown. This only fetches the ledger header and the transactions+metadata
        OptionalGetLedgerResponseType ledgerData{fetcher_.get().fetchData(sequence)};
        if (!ledgerData)
            return {};

        ripple::LedgerHeader lgrInfo = ::util::deserializeHeader(ripple::makeSlice(ledgerData->ledger_header()));

        LOG(log_.debug()) << "Deserialized ledger header. " << ::util::toString(lgrInfo);

        auto timeDiff = ::util::timed<std::chrono::duration<double>>([this, sequence, &lgrInfo, &ledgerData]() {
            backend_->startWrites();

            LOG(log_.debug()) << "Started writes";

            backend_->writeLedger(lgrInfo, std::move(*ledgerData->mutable_ledger_header()));

            LOG(log_.debug()) << "Wrote ledger";
            FormattedTransactionsData insertTxResult = insertTransactions(lgrInfo, *ledgerData);
            LOG(log_.debug()) << "Inserted txns";

            // download the full account state map. This function downloads full
            // ledger data and pushes the downloaded data into the writeQueue.
            // asyncWriter consumes from the queue and inserts the data into the
            // Ledger object. Once the below call returns, all data has been pushed
            // into the queue
            auto edgeKeys = loadBalancer_->loadInitialLedger(sequence);

            size_t numWrites = 0;
            backend_->cache().setFull();

            auto seconds = ::util::timed<std::chrono::seconds>([this, keys = std::move(edgeKeys), sequence, &numWrites](
                                                               ) mutable {
                for (auto& key : keys) {
                    LOG(log_.debug()) << "Writing edge key = " << ripple::strHex(key);
                    auto succ = backend_->cache().getSuccessor(*ripple::uint256::fromVoidChecked(key), sequence);
                    if (succ)
                        backend_->writeSuccessor(std::move(key), sequence, uint256ToString(succ->key));
                }

                ripple::uint256 prev = data::firstKey;
                while (auto cur = backend_->cache().getSuccessor(prev, sequence)) {
                    ASSERT(cur.has_value(), "Succesor for key {} must exist", ripple::strHex(prev));
                    if (prev == data::firstKey)
                        backend_->writeSuccessor(uint256ToString(prev), sequence, uint256ToString(cur->key));

                    if (isBookDir(cur->key, cur->blob)) {
                        auto base = getBookBase(cur->key);
                        // make sure the base is not an actual object
                        if (!backend_->cache().get(base, sequence)) {
                            auto succ = backend_->cache().getSuccessor(base, sequence);
                            ASSERT(succ.has_value(), "Book base {} must have a successor", ripple::strHex(base));
                            if (succ->key == cur->key) {
                                LOG(log_.debug()) << "Writing book successor = " << ripple::strHex(base) << " - "
                                                  << ripple::strHex(cur->key);

                                backend_->writeSuccessor(uint256ToString(base), sequence, uint256ToString(cur->key));
                            }
                        }

                        ++numWrites;
                    }

                    prev = cur->key;
                    static constexpr std::size_t LOG_INTERVAL = 100000;
                    if (numWrites % LOG_INTERVAL == 0 && numWrites != 0)
                        LOG(log_.info()) << "Wrote " << numWrites << " book successors";
                }

                backend_->writeSuccessor(uint256ToString(prev), sequence, uint256ToString(data::lastKey));
                ++numWrites;
            });

            LOG(log_.info()) << "Looping through cache and submitting all writes took " << seconds
                             << " seconds. numWrites = " << std::to_string(numWrites);

            LOG(log_.debug()) << "Loaded initial ledger";

            if (not state_.get().isStopping) {
                backend_->writeAccountTransactions(std::move(insertTxResult.accountTxData));
                backend_->writeNFTs(insertTxResult.nfTokensData);
                backend_->writeNFTTransactions(insertTxResult.nfTokenTxData);
            }

            backend_->finishWrites(sequence);
        });

        LOG(log_.debug()) << "Time to download and store ledger = " << timeDiff;
        return lgrInfo;
    }
};

}  // namespace etl::impl
