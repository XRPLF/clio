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
    boost::asio::io_context& ioc_;
    uint32_t writeInterval_ = 1000000;
    uint32_t inProcessLedger = 0;
    std::unordered_set<std::string> successors_;

public:
    PostgresBackend(
        boost::asio::io_context& ioc,
        boost::json::object const& config);

    std::optional<uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        uint32_t sequence,
        boost::asio::yield_context& yield) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override;

    std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        uint32_t sequence,
        boost::asio::yield_context& yield) const override;

    // returns a transaction, metadata pair
    std::optional<TransactionAndMetadata>
    fetchTransaction(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<LedgerObject>
    fetchLedgerDiff(
        std::uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::optional<LedgerRange>
    hardFetchLedgerRange() const override;

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const override;

    std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        uint32_t ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes,
        boost::asio::yield_context& yield) const override;

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence,
        boost::asio::yield_context& yield) const override;

    AccountTransactions
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        bool forward,
        std::optional<AccountTransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const override;

    void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader,
        boost::asio::yield_context& yield) override;

    void
    doWriteLedgerObject(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        boost::asio::yield_context& yield) override;

    void
    writeSuccessor(
        std::string&& key,
        uint32_t seq,
        std::string&& successor,
        boost::asio::yield_context& yield) override;

    void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        uint32_t date,
        std::string&& transaction,
        std::string&& metadata,
        boost::asio::yield_context& yield) override;

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data,
        boost::asio::yield_context& yield) override;

    void
    open(bool readOnly) override;

    void
    close() override;

    void
    startWrites(boost::asio::yield_context& yield) const override;

    bool
    doFinishWrites(boost::asio::yield_context& yield) const override;

    bool
    doOnlineDelete(
        uint32_t numLedgersToKeep,
        boost::asio::yield_context& yield) const override;

};
}  // namespace Backend
#endif
