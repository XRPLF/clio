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
    mutable size_t numRowsInSuccessorBuffer_ = 0;
    mutable std::stringstream successorBuffer_;
    mutable std::stringstream transactionsBuffer_;
    mutable std::stringstream accountTxBuffer_;
    std::shared_ptr<PgPool> pgPool_;
    mutable PgQuery writeConnection_;
    mutable bool abortWrite_ = false;
    mutable boost::asio::thread_pool pool_{16};
    uint32_t writeInterval_ = 1000000;
    uint32_t inProcessLedger = 0;
    std::unordered_set<std::string> successors_;

public:
    PostgresBackend(boost::json::object const& config);

    std::optional<uint32_t>
    fetchLatestLedgerSequence() const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(ripple::uint256 const& hash) const override;

    std::optional<Blob>
    doFetchLedgerObject(ripple::uint256 const& key, uint32_t sequence)
        const override;

    // returns a transaction, metadata pair
    std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const override;

    std::vector<LedgerObject>
    fetchLedgerDiff(uint32_t ledgerSequence) const override;

    std::optional<LedgerRange>
    hardFetchLedgerRange() const override;

    std::optional<ripple::uint256>
    doFetchSuccessorKey(ripple::uint256 key, uint32_t ledgerSequence)
        const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes) const override;

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const override;

    AccountTransactions
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        bool forward,
        std::optional<AccountTransactionsCursor> const& cursor) const override;

    void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader) override;

    void
    doWriteLedgerObject(std::string&& key, uint32_t seq, std::string&& blob)
        override;

    void
    writeSuccessor(std::string&& key, uint32_t seq, std::string&& successor)
        override;

    void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        uint32_t date,
        std::string&& transaction,
        std::string&& metadata) override;

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) override;

    void
    open(bool readOnly) override;

    void
    close() override;

    void
    startWrites() override;

    bool
    doFinishWrites() override;

    bool
    doOnlineDelete(uint32_t numLedgersToKeep) const override;
};
}  // namespace Backend
#endif
