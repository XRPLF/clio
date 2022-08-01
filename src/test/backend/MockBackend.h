#ifndef MOCK_BACKEND_H
#define MOCK_BACKEND_H

#include <clio/backend/BackendInterface.h>
#include <map>

namespace Backend {

namespace test {
class ordered_pair
{
    std::pair<std::uint32_t, std::uint32_t> pair_ = {0, 0};

public:
    ordered_pair(std::pair<std::uint32_t, std::uint32_t> const& pair)
        : pair_(pair)
    {
    }

    ordered_pair(TransactionsCursor const& cursor)
        : pair_(std::pair{cursor.ledgerSequence, cursor.transactionIndex})
    {
    }

    ordered_pair&
    operator=(TransactionsCursor const& cursor)
    {
        pair_.first = cursor.ledgerSequence;
        pair_.second = cursor.transactionIndex;
        return *this;
    }

    friend bool
    operator<(ordered_pair const& lhs, ordered_pair const& rhs)
    {
        if (lhs.pair_.first == rhs.pair_.first)
            return lhs.pair_.second < rhs.pair_.second;

        return lhs.pair_.first < rhs.pair_.first;
    }

    std::uint32_t
    first() const
    {
        return pair_.first;
    }

    std::uint32_t
    second() const
    {
        return pair_.first;
    }
};
}  // namespace test

/*
 * MockBackend is a mock for the backend class, so we can
 * run unit tests without requiring a connection to a real cassandra
 * instance.
 */
class MockBackend : public BackendInterface
{
private:
    std::atomic_bool open_ = false;
    std::map<ripple::uint256, std::uint32_t> ledgerSeqByHash = {};
    std::map<std::uint32_t, std::string> ledgersBySeq = {};
    std::map<std::string, TransactionAndMetadata> txs = {};
    std::map<std::uint32_t, std::set<std::string>> ledgerTxs = {};
    std::map<std::string, std::map<std::uint32_t, std::string>> objects = {};
    std::map<std::string, std::map<std::uint32_t, std::string>> successor = {};
    std::map<std::uint32_t, std::vector<Backend::LedgerObject>> diff = {};
    std::map<
        ripple::AccountID,
        std::map<Backend::test::ordered_pair, ripple::uint256>>
        accountTx = {};

public:
    MockBackend(Application const& app) : BackendInterface(app)
    {
    }

    ~MockBackend() override
    {
    }

    // Setup all of the necessary components for talking to the database.
    // Create the table if it doesn't exist already
    // @param createIfMissing ignored
    void
    open(bool readOnly) override
    {
        open_ = true;
    }

    // Close the connection to the database
    void
    close() override
    {
        open_ = false;
    }

    bool
    doFinishWrites() override
    {
        return true;
    }

    void
    writeLedger(ripple::LedgerInfo const& ledgerInfo, std::string&& header)
        override
    {
        ledgerSeqByHash[ledgerInfo.hash] = ledgerInfo.seq;
        ledgersBySeq[ledgerInfo.seq] = header;
    }

    void
    writeSuccessor(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& suc) override
    {
        auto& keyMap = successor[ripple::strHex(key)];
        keyMap[seq] = suc;
    }

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) override
    {
        for (auto const& datum : data)
        {
            for (auto const& account : datum.accounts)
            {
                accountTx[account][std::pair{
                    datum.ledgerSequence, datum.transactionIndex}] =
                    datum.txHash;
            }
        }
    }

    void
    writeTransaction(
        std::string&& hash,
        std::uint32_t const seq,
        std::uint32_t const date,
        std::string&& transaction,
        std::string&& metadata) override
    {
        Backend::Blob tx{
            transaction.data(), transaction.data() + transaction.size()};
        Backend::Blob meta{metadata.data(), metadata.data() + metadata.size()};

        TransactionAndMetadata txn = {tx, meta, seq, date};
        txs[hash] = std::move(txn);

        if (!ledgerTxs.contains(seq))
            ledgerTxs[seq] = {};

        ledgerTxs[seq].insert(hash);
    }

    void
    doWriteLedgerObject(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& blob) override
    {
        objects[ripple::strHex(key)][seq] = std::string{blob};

        if (!diff.contains(seq))
            diff[seq] = {};

        ripple::uint256 index = ripple::uint256::fromVoid(key.data());
        Backend::Blob b{blob.data(), blob.data() + blob.size()};
        Backend::LedgerObject obj{index, b};
        diff[seq].push_back(obj);
    }

    void
    startWrites() const override
    {
    }

    std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const override
    {
        if (ledgersBySeq.empty())
            return {};

        return ledgersBySeq.rbegin()->first;
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override
    {
        if (!ledgersBySeq.contains(sequence))
            return {};

        std::string blob = ledgersBySeq.at(sequence);
        return deserializeHeader(ripple::makeSlice(blob));
    }

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override
    {
        if (!ledgerSeqByHash.contains(hash))
            return {};

        auto seq = ledgerSeqByHash.at(hash);
        return fetchLedgerBySequence(seq, yield);
    }

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const override
    {
        if (ledgersBySeq.empty())
            return {};

        LedgerRange rng;

        rng.minSequence = ledgersBySeq.begin()->first;
        rng.maxSequence = ledgersBySeq.rbegin()->first;

        return rng;
    }

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override
    {
        auto hashes = fetchAllTransactionHashesInLedger(ledgerSequence, yield);

        return fetchTransactions(hashes, yield);
    }

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override
    {
        if (!ledgerTxs.contains(ledgerSequence))
            return {};

        auto set = ledgerTxs.at(ledgerSequence);

        std::vector<ripple::uint256> result;

        for (auto const& hash : set)
        {
            auto h = ripple::uint256::fromVoid(hash.data());
            result.push_back(h);
        }

        return result;
    }

    // Synchronously fetch the object with key key, as of ledger with sequence
    // sequence
    std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override
    {
        std::string keyStr = ripple::strHex(key);
        if (!objects.contains(keyStr))
            return {};

        auto& history = objects.at(keyStr);

        auto lb = history.lower_bound(sequence);

        auto returnIfNotEmpty =
            [](std::string const& s) -> std::optional<Blob> {
            Blob b{s.data(), s.data() + s.size()};

            if (b.size() == 0)
                return {};

            return b;
        };

        if (lb->first == sequence)
            return returnIfNotEmpty(lb->second);

        if (lb != history.begin())
            lb--;

        if (lb->first <= sequence)
            return returnIfNotEmpty(lb->second);

        return {};
    }

    std::optional<TransactionAndMetadata>
    fetchTransaction(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override
    {
        std::string strHash{hash.data(), hash.data() + hash.size()};
        if (!txs.contains(strHash))
            return {};

        return txs.at(strHash);
    }

    std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override
    {
        std::string keyStr = ripple::strHex(key);
        if (!successor.contains(keyStr))
            return {};

        auto& history = successor.at(keyStr);

        auto lb = history.lower_bound(ledgerSequence);

        auto returnIfNotEmpty =
            [](std::string const& s) -> std::optional<ripple::uint256> {
            auto result = ripple::uint256::fromVoid(s.data());

            if (result == Backend::lastKey)
                return {};

            return result;
        };

        if (lb->first == ledgerSequence)
            return returnIfNotEmpty(lb->second);

        if (lb != history.begin())
            lb--;

        if (lb->first <= ledgerSequence)
            return returnIfNotEmpty(lb->second);

        return {};
    }

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes,
        boost::asio::yield_context& yield) const override
    {
        std::vector<TransactionAndMetadata> result = {};

        for (auto const& hash : hashes)
        {
            std::string strHash{hash.data(), hash.data() + hash.size()};
            result.push_back(txs.at(strHash));
        }

        return result;
    }

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override
    {
        std::vector<Blob> result = {};
        for (auto const& key : keys)
        {
            ripple::uint256 keyUint = ripple::uint256::fromVoid(key.data());
            auto obj = doFetchLedgerObject(keyUint, sequence, yield);
            if (obj)
                result.push_back(*obj);
        }

        return result;
    }

    std::vector<LedgerObject>
    fetchLedgerDiff(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override
    {
        return diff.at(ledgerSequence);
    }

    TransactionsAndCursor
    doAccountTransactionsReverse(
        std::map<Backend::test::ordered_pair, ripple::uint256> const& txs,
        std::uint32_t limit,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const
    {
        std::vector<ripple::uint256> hashes = {};

        Backend::test::ordered_pair c(std::pair<uint32_t, uint32_t>{0, 0});

        if (cursor)
            c = *cursor;

        TransactionsAndCursor result;
        for (auto it = txs.upper_bound(c); it != txs.end(); ++it)
        {
            hashes.push_back(it->second);

            if (--limit == 0)
            {
                result.cursor = TransactionsCursor{};
                result.cursor->ledgerSequence = it->first.first();
                result.cursor->transactionIndex = it->first.second() - 1;
            }
        }

        result.txns = fetchTransactions(hashes, yield);

        return result;
    }

    TransactionsAndCursor
    doAccountTransactionsForward(
        std::map<Backend::test::ordered_pair, ripple::uint256> const& txs,
        std::uint32_t limit,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const
    {
        std::vector<ripple::uint256> hashes = {};

        std::uint32_t max = std::numeric_limits<std::uint32_t>::max();
        Backend::test::ordered_pair c(std::pair<uint32_t, uint32_t>{max, max});

        if (cursor)
            c = *cursor;

        TransactionsAndCursor result;

        for (auto it = std::make_reverse_iterator(txs.lower_bound(c));
             it != txs.rend();
             ++it)
        {
            hashes.push_back(it->second);

            if (--limit == 0)
            {
                result.cursor = TransactionsCursor{};
                result.cursor->ledgerSequence = it->first.first();
                result.cursor->transactionIndex = it->first.second() + 1;
            }
        }

        result.txns = fetchTransactions(hashes, yield);

        return result;
    }

    TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const override
    {
        auto const& txs = accountTx.at(account);

        if (forward)
            return doAccountTransactionsForward(txs, limit, cursor, yield);
        else
            return doAccountTransactionsReverse(txs, limit, cursor, yield);
    }

    bool
    doOnlineDelete(
        std::uint32_t const numLedgersToKeep,
        boost::asio::yield_context& yield) const override
    {
        // This will remain unimplemented until we add online_delete.
        return true;
    }

    virtual std::optional<NFT>
    fetchNFT(
        ripple::uint256 const& id,
        std::uint32_t seq, 
        boost::asio::yield_context& yield) const override
    {
        return {};
    }

    virtual TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& key,
        std::uint32_t seq, 
        bool forward, 
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const override
    {
        return TransactionsAndCursor{};
    }

    virtual void
    writeNFTs(std::vector<NFTsData>&& data) override
    {
        
    }

    virtual void 
    writeNFTTransactions(std::vector<NFTTransactionsData>&& data) override
    {

    }
};

}  // namespace Backend

#endif  // MOCK_BACKEND_H