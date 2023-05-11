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
#include <backend/cassandra/Concepts.h>
#include <backend/cassandra/Handle.h>
#include <backend/cassandra/Schema.h>
#include <backend/cassandra/SettingsProvider.h>
#include <backend/cassandra/impl/ExecutionStrategy.h>
#include <log/Logger.h>
#include <util/Profiler.h>

#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <boost/asio/spawn.hpp>

namespace Backend::Cassandra {

/**
 * @brief Implements @ref BackendInterface for Cassandra/Scylladb
 *
 * Note: this is a safer and more correct rewrite of the original implementation
 * of the backend. We deliberately did not change the interface for now so that
 * other parts such as ETL do not have to change at all.
 * Eventually we should change the interface so that it does not have to know
 * about yield_context.
 */
template <SomeSettingsProvider SettingsProviderType, SomeExecutionStrategy ExecutionStrategy>
class BasicCassandraBackend : public BackendInterface
{
    clio::Logger log_{"Backend"};

    SettingsProviderType settingsProvider_;
    Schema<SettingsProviderType> schema_;
    Handle handle_;

    // have to be mutable because BackendInterface constness :(
    mutable ExecutionStrategy executor_;

    std::atomic_uint32_t ledgerSequence_ = 0u;

public:
    /**
     * @brief Create a new cassandra/scylla backend instance.
     *
     * @param settingsProvider
     */
    BasicCassandraBackend(SettingsProviderType settingsProvider)
        : settingsProvider_{std::move(settingsProvider)}
        , schema_{settingsProvider_}
        , handle_{settingsProvider_.getSettings()}
        , executor_{settingsProvider_.getSettings(), handle_}
    {
        if (auto const res = handle_.connect(); not res)
            throw std::runtime_error("Could not connect to Cassandra: " + res.error());

        if (auto const res = handle_.execute(schema_.createKeyspace); not res)
        {
            // on datastax, creation of keyspaces can be configured to only be done thru the admin interface.
            // this does not mean that the keyspace does not already exist tho.
            if (res.error().code() != CASS_ERROR_SERVER_UNAUTHORIZED)
                throw std::runtime_error("Could not create keyspace: " + res.error());
        }

        if (auto const res = handle_.executeEach(schema_.createSchema); not res)
            throw std::runtime_error("Could not create schema: " + res.error());

        schema_.prepareStatements(handle_);
        log_.info() << "Created (revamped) CassandraBackend";
    }

    /*! Not used in this implementation */
    void
    open([[maybe_unused]] bool readOnly) override
    {
    }

    /*! Not used in this implementation */
    void
    close() override
    {
    }

    TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context& yield) const override
    {
        auto rng = fetchLedgerRange();
        if (!rng)
            return {{}, {}};

        Statement statement = [this, forward, &account]() {
            if (forward)
                return schema_->selectAccountTxForward.bind(account);
            else
                return schema_->selectAccountTx.bind(account);
        }();

        auto cursor = cursorIn;
        if (cursor)
        {
            statement.bindAt(1, cursor->asTuple());
            log_.debug() << "account = " << ripple::strHex(account) << " tuple = " << cursor->ledgerSequence
                         << cursor->transactionIndex;
        }
        else
        {
            auto const seq = forward ? rng->minSequence : rng->maxSequence;
            auto const placeHolder = forward ? 0u : std::numeric_limits<std::uint32_t>::max();

            statement.bindAt(1, std::make_tuple(placeHolder, placeHolder));
            log_.debug() << "account = " << ripple::strHex(account) << " idx = " << seq << " tuple = " << placeHolder;
        }

        // FIXME: Limit is a hack to support uint32_t properly for the time
        // being. Should be removed later and schema updated to use proper
        // types.
        statement.bindAt(2, Limit{limit});
        auto const res = executor_.read(yield, statement);
        auto const& results = res.value();
        if (not results.hasRows())
        {
            log_.debug() << "No rows returned";
            return {};
        }

        std::vector<ripple::uint256> hashes = {};
        auto numRows = results.numRows();
        log_.info() << "num_rows = " << numRows;

        for (auto [hash, data] : extract<ripple::uint256, std::tuple<uint32_t, uint32_t>>(results))
        {
            hashes.push_back(hash);
            if (--numRows == 0)
            {
                log_.debug() << "Setting cursor";
                cursor = data;

                // forward queries by ledger/tx sequence `>=`
                // so we have to advance the index by one
                if (forward)
                    ++cursor->transactionIndex;
            }
        }

        auto const txns = fetchTransactions(hashes, yield);
        log_.debug() << "Txns = " << txns.size();

        if (txns.size() == limit)
        {
            log_.debug() << "Returning cursor";
            return {txns, cursor};
        }

        return {txns, {}};
    }

    bool
    doFinishWrites() override
    {
        // wait for other threads to finish their writes
        executor_.sync();

        if (!range)
        {
            executor_.writeSync(schema_->updateLedgerRange, ledgerSequence_, false, ledgerSequence_);
        }

        if (not executeSyncUpdate(schema_->updateLedgerRange.bind(ledgerSequence_, true, ledgerSequence_ - 1)))
        {
            log_.warn() << "Update failed for ledger " << ledgerSequence_;
            return false;
        }

        log_.info() << "Committed ledger " << ledgerSequence_;
        return true;
    }

    void
    writeLedger(ripple::LedgerInfo const& ledgerInfo, std::string&& header) override
    {
        executor_.write(schema_->insertLedgerHeader, ledgerInfo.seq, std::move(header));

        executor_.write(schema_->insertLedgerHash, ledgerInfo.hash, ledgerInfo.seq);

        ledgerSequence_ = ledgerInfo.seq;
    }

    std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const override
    {
        if (auto const res = executor_.read(yield, schema_->selectLatestLedger); res)
        {
            if (auto const& result = res.value(); result)
            {
                if (auto const maybeValue = result.template get<uint32_t>(); maybeValue)
                    return maybeValue;

                log_.error() << "Could not fetch latest ledger - no rows";
                return std::nullopt;
            }

            log_.error() << "Could not fetch latest ledger - no result";
        }
        else
        {
            log_.error() << "Could not fetch latest ledger: " << res.error();
        }

        return std::nullopt;
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(std::uint32_t const sequence, boost::asio::yield_context& yield) const override
    {
        log_.trace() << __func__ << " call for seq " << sequence;

        auto const res = executor_.read(yield, schema_->selectLedgerBySeq, sequence);
        if (res)
        {
            if (auto const& result = res.value(); result)
            {
                if (auto const maybeValue = result.template get<std::vector<unsigned char>>(); maybeValue)
                {
                    return deserializeHeader(ripple::makeSlice(*maybeValue));
                }

                log_.error() << "Could not fetch ledger by sequence - no rows";
                return std::nullopt;
            }

            log_.error() << "Could not fetch ledger by sequence - no result";
        }
        else
        {
            log_.error() << "Could not fetch ledger by sequence: " << res.error();
        }

        return std::nullopt;
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(ripple::uint256 const& hash, boost::asio::yield_context& yield) const override
    {
        log_.trace() << __func__ << " call";

        if (auto const res = executor_.read(yield, schema_->selectLedgerByHash, hash); res)
        {
            if (auto const& result = res.value(); result)
            {
                if (auto const maybeValue = result.template get<uint32_t>(); maybeValue)
                    return fetchLedgerBySequence(*maybeValue, yield);

                log_.error() << "Could not fetch ledger by hash - no rows";
                return std::nullopt;
            }

            log_.error() << "Could not fetch ledger by hash - no result";
        }
        else
        {
            log_.error() << "Could not fetch ledger by hash: " << res.error();
        }

        return std::nullopt;
    }

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const override
    {
        log_.trace() << __func__ << " call";

        if (auto const res = executor_.read(yield, schema_->selectLedgerRange); res)
        {
            auto const& results = res.value();
            if (not results.hasRows())
            {
                log_.debug() << "Could not fetch ledger range - no rows";
                return std::nullopt;
            }

            // TODO: this is probably a good place to use user type in
            // cassandra instead of having two rows with bool flag. or maybe at
            // least use tuple<int, int>?
            LedgerRange range;
            std::size_t idx = 0;
            for (auto [seq] : extract<uint32_t>(results))
            {
                if (idx == 0)
                    range.maxSequence = range.minSequence = seq;
                else if (idx == 1)
                    range.maxSequence = seq;

                ++idx;
            }

            if (range.minSequence > range.maxSequence)
                std::swap(range.minSequence, range.maxSequence);

            log_.debug() << "After hardFetchLedgerRange range is " << range.minSequence << ":" << range.maxSequence;
            return range;
        }
        else
        {
            log_.error() << "Could not fetch ledger range: " << res.error();
        }

        return std::nullopt;
    }

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(std::uint32_t const ledgerSequence, boost::asio::yield_context& yield) const override
    {
        log_.trace() << __func__ << " call";
        auto hashes = fetchAllTransactionHashesInLedger(ledgerSequence, yield);
        return fetchTransactions(hashes, yield);
    }

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(std::uint32_t const ledgerSequence, boost::asio::yield_context& yield)
        const override
    {
        log_.trace() << __func__ << " call";
        auto start = std::chrono::system_clock::now();
        auto const res = executor_.read(yield, schema_->selectAllTransactionHashesInLedger, ledgerSequence);

        if (not res)
        {
            log_.error() << "Could not fetch all transaction hashes: " << res.error();
            return {};
        }

        auto const& result = res.value();
        if (not result.hasRows())
        {
            log_.error() << "Could not fetch all transaction hashes - no rows; ledger = "
                         << std::to_string(ledgerSequence);
            return {};
        }

        std::vector<ripple::uint256> hashes;
        for (auto [hash] : extract<ripple::uint256>(result))
            hashes.push_back(std::move(hash));

        auto end = std::chrono::system_clock::now();
        log_.debug() << "Fetched " << hashes.size() << " transaction hashes from Cassandra in "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " milliseconds";

        return hashes;
    }

    std::optional<NFT>
    fetchNFT(ripple::uint256 const& tokenID, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield)
        const override
    {
        log_.trace() << __func__ << " call";

        auto const res = executor_.read(yield, schema_->selectNFT, tokenID, ledgerSequence);
        if (not res)
            return std::nullopt;

        if (auto const maybeRow = res->template get<uint32_t, ripple::AccountID, bool>(); maybeRow)
        {
            auto [seq, owner, isBurned] = *maybeRow;
            auto result = std::make_optional<NFT>(tokenID, seq, owner, isBurned);

            // now fetch URI. Usually we will have the URI even for burned NFTs,
            // but if the first ledger on this clio included NFTokenBurn
            // transactions we will not have the URIs for any of those tokens.
            // In any other case not having the URI indicates something went
            // wrong with our data.
            //
            // TODO - in the future would be great for any handlers that use
            // this could inject a warning in this case (the case of not having
            // a URI because it was burned in the first ledger) to indicate that
            // even though we are returning a blank URI, the NFT might have had
            // one.
            auto uriRes = executor_.read(yield, schema_->selectNFTURI, tokenID, ledgerSequence);
            if (uriRes)
            {
                if (auto const maybeUri = uriRes->template get<ripple::Blob>(); maybeUri)
                    result->uri = *maybeUri;
            }

            return result;
        }

        log_.error() << "Could not fetch NFT - no rows";
        return std::nullopt;
    }

    TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& tokenID,
        std::uint32_t const limit,
        bool const forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context& yield) const override
    {
        log_.trace() << __func__ << " call";

        auto rng = fetchLedgerRange();
        if (!rng)
            return {{}, {}};

        Statement statement = [this, forward, &tokenID]() {
            if (forward)
                return schema_->selectNFTTxForward.bind(tokenID);
            else
                return schema_->selectNFTTx.bind(tokenID);
        }();

        auto cursor = cursorIn;
        if (cursor)
        {
            statement.bindAt(1, cursor->asTuple());
            log_.debug() << "token_id = " << ripple::strHex(tokenID) << " tuple = " << cursor->ledgerSequence
                         << cursor->transactionIndex;
        }
        else
        {
            auto const seq = forward ? rng->minSequence : rng->maxSequence;
            auto const placeHolder = forward ? 0 : std::numeric_limits<std::uint32_t>::max();

            statement.bindAt(1, std::make_tuple(placeHolder, placeHolder));
            log_.debug() << "token_id = " << ripple::strHex(tokenID) << " idx = " << seq << " tuple = " << placeHolder;
        }

        statement.bindAt(2, Limit{limit});

        auto const res = executor_.read(yield, statement);
        auto const& results = res.value();
        if (not results.hasRows())
        {
            log_.debug() << "No rows returned";
            return {};
        }

        std::vector<ripple::uint256> hashes = {};
        auto numRows = results.numRows();
        log_.info() << "num_rows = " << numRows;

        for (auto [hash, data] : extract<ripple::uint256, std::tuple<uint32_t, uint32_t>>(results))
        {
            hashes.push_back(hash);
            if (--numRows == 0)
            {
                log_.debug() << "Setting cursor";
                cursor = data;

                // forward queries by ledger/tx sequence `>=`
                // so we have to advance the index by one
                if (forward)
                    ++cursor->transactionIndex;
            }
        }

        auto const txns = fetchTransactions(hashes, yield);
        log_.debug() << "NFT Txns = " << txns.size();

        if (txns.size() == limit)
        {
            log_.debug() << "Returning cursor";
            return {txns, cursor};
        }

        return {txns, {}};
    }

    NFTsAndCursor
    fetchNFTsByIssuer(
        ripple::AccountID const& issuer,
        std::optional<std::uint32_t> const& taxon,
        std::uint32_t const ledgerSequence,
        std::uint32_t const limit,
        std::optional<ripple::uint256> const& cursorIn,
        boost::asio::yield_context& yield) const override
    {
        NFTsAndCursor ret;

        Statement idQueryStatement = [&taxon, &issuer, &cursorIn, &limit, this]() {
            if (taxon.has_value())
            {
                auto r = schema_->selectNFTIDsByIssuerTaxon.bind(issuer);
                r.bindAt(1, *taxon);
                r.bindAt(2, cursorIn.value_or(ripple::uint256(0)));
                r.bindAt(3, limit);
                return r;
            }

            auto r = schema_->selectNFTIDsByIssuer.bind(issuer);
            r.bindAt(
                1,
                std::make_tuple(
                    cursorIn.has_value() ? ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn)) : 0,
                    cursorIn.value_or(ripple::uint256(0))));
            r.bindAt(2, limit);
            return r;
        }();

        auto const& idQueryResults = executor_.read(yield, idQueryStatement).value();
        std::vector<ripple::uint256> nftIDs;
        for (auto const [nftID] : extract<ripple::uint256>(idQueryResults))
            nftIDs.push_back(nftID);
    }

    return nftIDs;
}

std::optional<Blob>
doFetchLedgerObject(ripple::uint256 const& key, std::uint32_t const sequence, boost::asio::yield_context& yield)
    const override
{
    Statement statement = [&taxon, &issuer, &cursorIn, this]() {
        if (taxon.has_value())
        {
            auto r = schema_->selectNFTIDsByIssuerTaxon.bind(issuer);
            r.bindAt(1, *taxon);
            return r;
        }

        auto r = schema_->selectNFTIDsByIssuer.bind(issuer);
        if (cursorIn.has_value())
            r.bindAt(1, ripple::nft::toUInt32(ripple::nft::getTaxon(*cursorIn)));
        else
            r.bindAt(1, 0);
        return r;
    }();

    if (cursorIn.has_value())
        statement.bindAt(2, *cursorIn);
    else
        statement.bindAt(2, 0x00);

    statement.bindAt(3, limit);

    auto const res = executor_.read(yield, statement);
    auto const& results = res.value();
    std::vector<ripple::uint256> nftIDs;

    for (auto const [nftID] : extract<ripple::uint256>(results))
    {
        nftIDs.push_back(nftID);
    }

    if (nftIDs.size() == 0)
        return ret;

    if (nftIDs.size() == limit)
        ret.cursor = nftIDs.back();

    /// TODO these two queries should happen in parallel
    auto nftQueryStatement = schema_->selectNFTBulk.bind(nftIDs);
    nftQueryStatement.bindAt(1, ledgerSequence);

    auto const& nftQueryResults = executor_.read(yield, nftQueryStatement).value();

    auto nftURIQueryStatement = schema_->selectNFTURIBulk.bind(nftIDs);
    nftURIQueryStatement.bindAt(1, ledgerSequence);

    auto const& nftURIQueryResults = executor_.read(yield, nftURIQueryStatement).value();
    ///
    //

    std::unordered_map<std::string, Blob> nftURIMap;
    for (auto const [nftID, uri] : extract<ripple::uint256, Blob>(nftURIQueryResults))
        nftURIMap.insert({ripple::strHex(nftID), uri});

    for (auto const [nftID, seq, owner, isBurned] :
         extract<ripple::uint256, std::uint32_t, ripple::AccountID, bool>(nftQueryResults))
    {
        NFT nft;
        nft.tokenID = nftID;
        nft.ledgerSequence = seq;
        nft.owner = owner;
        nft.isBurned = isBurned;
        if (nftURIMap.contains(ripple::strHex(nft.tokenID)))
            nft.uri = nftURIMap.at(ripple::strHex(nft.tokenID));
        ret.nfts.push_back(nft);
    }

    return ret;
}

std::optional<Blob>
doFetchLedgerObject(ripple::uint256 const& key, std::uint32_t const sequence, boost::asio::yield_context& yield)
    const override
{
    log_.debug() << "Fetching ledger object for seq " << sequence << ", key = " << ripple::to_string(key);
    if (auto const res = executor_.read(yield, schema_->selectObject, key, sequence); res)
    {
        if (auto const result = res->template get<Blob>(); result)
        {
            if (result->size())
                return *result;
        }
        else
        {
            log_.debug() << "Could not fetch ledger object - no rows";
        }
    }
    else
    {
        log_.error() << "Could not fetch ledger object: " << res.error();
    }

    return std::nullopt;
}

std::optional<TransactionAndMetadata>
fetchTransaction(ripple::uint256 const& hash, boost::asio::yield_context& yield) const override
{
    log_.trace() << __func__ << " call";

    if (auto const res = executor_.read(yield, schema_->selectTransaction, hash); res)
    {
        if (auto const maybeValue = res->template get<Blob, Blob, uint32_t, uint32_t>(); maybeValue)
        {
            auto [transaction, meta, seq, date] = *maybeValue;
            return std::make_optional<TransactionAndMetadata>(transaction, meta, seq, date);
        }
        else
        {
            log_.debug() << "Could not fetch transaction - no rows";
        }
    }
    else
    {
        log_.error() << "Could not fetch transaction: " << res.error();
    }

    return std::nullopt;
}

std::optional<ripple::uint256>
doFetchSuccessorKey(ripple::uint256 key, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield)
    const override
{
    log_.trace() << __func__ << " call";

    if (auto const res = executor_.read(yield, schema_->selectSuccessor, key, ledgerSequence); res)
    {
        if (auto const result = res->template get<ripple::uint256>(); result)
        {
            if (*result == lastKey)
                return std::nullopt;
            return *result;
        }
        else
        {
            log_.debug() << "Could not fetch successor - no rows";
        }
    }
    else
    {
        log_.error() << "Could not fetch successor: " << res.error();
    }

    return std::nullopt;
}

std::vector<TransactionAndMetadata>
fetchTransactions(std::vector<ripple::uint256> const& hashes, boost::asio::yield_context& yield) const override
{
    log_.trace() << __func__ << " call";

    if (hashes.size() == 0)
        return {};

    auto const numHashes = hashes.size();
    std::vector<TransactionAndMetadata> results;
    results.reserve(numHashes);

    std::vector<Statement> statements;
    statements.reserve(numHashes);

    auto const timeDiff = util::timed([this, &yield, &results, &hashes, &statements]() {
        // TODO: seems like a job for "hash IN (list of hashes)" instead?
        std::transform(
            std::cbegin(hashes), std::cend(hashes), std::back_inserter(statements), [this](auto const& hash) {
                return schema_->selectTransaction.bind(hash);
            });

        auto const entries = executor_.readEach(yield, statements);
        std::transform(
            std::cbegin(entries),
            std::cend(entries),
            std::back_inserter(results),
            [](auto const& res) -> TransactionAndMetadata {
                if (auto const maybeRow = res.template get<Blob, Blob, uint32_t, uint32_t>(); maybeRow)
                    return *maybeRow;
                else
                    return {};
            });
    });

    assert(numHashes == results.size());
    log_.debug() << "Fetched " << numHashes << " transactions from Cassandra in " << timeDiff << " milliseconds";
    return results;
}

std::vector<Blob>
doFetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const override
{
    log_.trace() << __func__ << " call";

    if (keys.size() == 0)
        return {};

    auto const numKeys = keys.size();
    log_.trace() << "Fetching " << numKeys << " objects";

    std::vector<Blob> results;
    results.reserve(numKeys);

    std::vector<Statement> statements;
    statements.reserve(numKeys);

    // TODO: seems like a job for "key IN (list of keys)" instead?
    std::transform(
        std::cbegin(keys), std::cend(keys), std::back_inserter(statements), [this, &sequence](auto const& key) {
            return schema_->selectObject.bind(key, sequence);
        });

    auto const entries = executor_.readEach(yield, statements);
    std::transform(std::cbegin(entries), std::cend(entries), std::back_inserter(results), [](auto const& res) -> Blob {
        if (auto const maybeValue = res.template get<Blob>(); maybeValue)
            return *maybeValue;
        else
            return {};
    });

    log_.trace() << "Fetched " << numKeys << " objects";
    return results;
}

std::vector<LedgerObject>
fetchLedgerDiff(std::uint32_t const ledgerSequence, boost::asio::yield_context& yield) const override
{
    log_.trace() << __func__ << " call";

    auto const [keys, timeDiff] = util::timed([this, &ledgerSequence, &yield]() -> std::vector<ripple::uint256> {
        auto const res = executor_.read(yield, schema_->selectDiff, ledgerSequence);
        if (not res)
        {
            log_.error() << "Could not fetch ledger diff: " << res.error() << "; ledger = " << ledgerSequence;
            return {};
        }

        auto const& results = res.value();
        if (not results)
        {
            log_.error() << "Could not fetch ledger diff - no rows; ledger = " << ledgerSequence;
            return {};
        }

        std::vector<ripple::uint256> keys;
        for (auto [key] : extract<ripple::uint256>(results))
            keys.push_back(key);

        return keys;
    });

    // one of the above errors must have happened
    if (keys.empty())
        return {};

    log_.debug() << "Fetched " << keys.size() << " diff hashes from Cassandra in " << timeDiff << " milliseconds";

    auto const objs = fetchLedgerObjects(keys, ledgerSequence, yield);
    std::vector<LedgerObject> results;
    results.reserve(keys.size());

    std::transform(
        std::cbegin(keys),
        std::cend(keys),
        std::cbegin(objs),
        std::back_inserter(results),
        [](auto const& key, auto const& obj) {
            return LedgerObject{key, obj};
        });

    return results;
}

void
doWriteLedgerObject(std::string&& key, std::uint32_t const seq, std::string&& blob) override
{
    log_.trace() << " Writing ledger object " << key.size() << ":" << seq << " [" << blob.size() << " bytes]";

    if (range)
        executor_.write(schema_->insertDiff, seq, key);

    executor_.write(schema_->insertObject, std::move(key), seq, std::move(blob));
}

void
writeSuccessor(std::string&& key, std::uint32_t const seq, std::string&& successor) override
{
    log_.trace() << "Writing successor. key = " << key.size() << " bytes. "
                 << " seq = " << std::to_string(seq) << " successor = " << successor.size() << " bytes.";
    assert(key.size() != 0);
    assert(successor.size() != 0);

    executor_.write(schema_->insertSuccessor, std::move(key), seq, std::move(successor));
}

void
writeAccountTransactions(std::vector<AccountTransactionsData>&& data) override
{
    std::vector<Statement> statements;
    statements.reserve(data.size() * 10);  // assume 10 transactions avg

    for (auto& record : data)
    {
        std::transform(
            std::begin(record.accounts),
            std::end(record.accounts),
            std::back_inserter(statements),
            [this, &record](auto&& account) {
                return schema_->insertAccountTx.bind(
                    std::move(account), std::make_tuple(record.ledgerSequence, record.transactionIndex), record.txHash);
            });
    }

    executor_.write(std::move(statements));
}

void
writeNFTTransactions(std::vector<NFTTransactionsData>&& data) override
{
    std::vector<Statement> statements;
    statements.reserve(data.size());

    std::transform(std::cbegin(data), std::cend(data), std::back_inserter(statements), [this](auto const& record) {
        return schema_->insertNFTTx.bind(
            record.tokenID, std::make_tuple(record.ledgerSequence, record.transactionIndex), record.txHash);
    });

    executor_.write(std::move(statements));
}

void
writeTransaction(
    std::string&& hash,
    std::uint32_t const seq,
    std::uint32_t const date,
    std::string&& transaction,
    std::string&& metadata) override
{
    log_.trace() << "Writing txn to cassandra";

    executor_.write(schema_->insertLedgerTransaction, seq, hash);
    executor_.write(
        schema_->insertTransaction, std::move(hash), seq, date, std::move(transaction), std::move(metadata));
}

void
writeNFTs(std::vector<NFTsData>&& data) override
{
    std::vector<Statement> statements;
    statements.reserve(data.size() * 3);

    for (NFTsData const& record : data)
    {
        statements.push_back(
            schema_->insertNFT.bind(record.tokenID, record.ledgerSequence, record.owner, record.isBurned));

        // If `uri` is set (and it can be set to an empty uri), we know this
        // is a net-new NFT. That is, this NFT has not been seen before by
        // us _OR_ it is in the extreme edge case of a re-minted NFT ID with
        // the same NFT ID as an already-burned token. In this case, we need
        // to record the URI and link to the issuer_nf_tokens table.
        if (record.uri)
        {
            statements.push_back(schema_->insertIssuerNFT.bind(
                ripple::nft::getIssuer(record.tokenID),
                static_cast<uint32_t>(ripple::nft::getTaxon(record.tokenID)),
                record.tokenID));
            statements.push_back(schema_->insertNFTURI.bind(record.tokenID, record.ledgerSequence, record.uri.value()));
        }
    }

    executor_.write(std::move(statements));
}

void
startWrites() const override
{
    // Note: no-op in original implementation too.
    // probably was used in PG to start a transaction or smth.
}

/*! Unused in this implementation */
bool
doOnlineDelete(std::uint32_t const numLedgersToKeep, boost::asio::yield_context& yield) const override
{
    log_.trace() << __func__ << " call";
    return true;
}

bool
isTooBusy() const override
{
    return executor_.isTooBusy();
}

private:
bool
executeSyncUpdate(Statement statement)
{
    auto const res = executor_.writeSync(statement);
    auto maybeSuccess = res->template get<bool>();
    if (not maybeSuccess)
    {
        log_.error() << "executeSyncUpdate - error getting result - no row";
        return false;
    }

    if (not maybeSuccess.value())
    {
        log_.warn() << "Update failed. Checking if DB state is what we expect";

        // error may indicate that another writer wrote something.
        // in this case let's just compare the current state of things
        // against what we were trying to write in the first place and
        // use that as the source of truth for the result.
        auto rng = hardFetchLedgerRangeNoThrow();
        return rng && rng->maxSequence == ledgerSequence_;
    }

    return true;
}
};

using CassandraBackend = BasicCassandraBackend<SettingsProvider, detail::DefaultExecutionStrategy<>>;

}  // namespace Backend::Cassandra
