#ifndef RIPPLE_APP_REPORTING_POSTGRESBACKEND_H_INCLUDED
#define RIPPLE_APP_REPORTING_POSTGRESBACKEND_H_INCLUDED
#include <boost/json.hpp>
#include <backend/BackendInterface.h>

namespace Backend {
class PostgresBackend : public BackendInterface
{
private:
    mutable size_t numRowsInObjectsBuffer_ = 0;
    mutable std::stringstream objectsBuffer_;
    mutable std::stringstream keysBuffer_;
    mutable std::stringstream transactionsBuffer_;
    mutable std::stringstream accountTxBuffer_;
    std::shared_ptr<PgPool> pgPool_;
    mutable PgQuery writeConnection_;
    mutable bool abortWrite_ = false;
    mutable boost::asio::thread_pool pool_{16};
    uint32_t writeInterval_ = 1000000;

public:
    PostgresBackend(boost::json::object const& config);

    std::optional<uint32_t>
    fetchLatestLedgerSequence() const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(ripple::uint256 const& hash) const override;

    std::optional<LedgerRange>
    fetchLedgerRange() const override;

    std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence)
        const override;

    // returns a transaction, metadata pair
    std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const override;

    LedgerPage
    doFetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes) const override;

    std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const override;

    std::pair<
        std::vector<TransactionAndMetadata>,
        std::optional<AccountTransactionsCursor>>
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        std::optional<AccountTransactionsCursor> const& cursor) const override;

    void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader,
        bool isFirst) const override;

    void
    doWriteLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const override;

    void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        std::string&& transaction,
        std::string&& metadata) const override;

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) const override;

    void
    open(bool readOnly) override;

    void
    close() override;

    void
    startWrites() const override;

    bool
    doFinishWrites() const override;

    bool
    doOnlineDelete(uint32_t numLedgersToKeep) const override;
    bool
    writeKeys(
        std::unordered_set<ripple::uint256> const& keys,
        KeyIndex const& index,
        bool isAsync = false) const override;
};
}  // namespace Backend
#endif
