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
    std::uint32_t writeInterval_ = 1000000;
    std::uint32_t inProcessLedger = 0;
    mutable std::unordered_set<std::string> successors_;

    const char* const set_timeout = "SET statement_timeout TO 10000";

public:
    PostgresBackend(
        boost::asio::io_context& ioc,
        boost::json::object const& config);

    std::optional<std::uint32_t>
    fetchLatestLedgerSequence(boost::asio::yield_context& yield) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override;

    std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override;

    std::optional<Blob>
    doFetchLedgerObject(
        ripple::uint256 const& key,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override;

    // returns a transaction, metadata pair
    std::optional<TransactionAndMetadata>
    fetchTransaction(
        ripple::uint256 const& hash,
        boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::optional<NFT>
    fetchNFT(
        ripple::uint256 const& tokenID,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;
        
    std::optional<IssuerNFTs>
    fetchIssuerNFTs(
        ripple::AccountID const& issuer,
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t const limit,
        boost::asio::yield_context& yield) const override;

    TransactionsAndCursor
    fetchNFTTransactions(
        ripple::uint256 const& tokenID,
        std::uint32_t const limit,
        bool const forward,
        std::optional<TransactionsCursor> const& cursorIn,
        boost::asio::yield_context& yield) const override;

    std::vector<LedgerObject>
    fetchLedgerDiff(
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::optional<LedgerRange>
    hardFetchLedgerRange(boost::asio::yield_context& yield) const override;

    std::optional<ripple::uint256>
    doFetchSuccessorKey(
        ripple::uint256 key,
        std::uint32_t const ledgerSequence,
        boost::asio::yield_context& yield) const override;

    std::vector<TransactionAndMetadata>
    fetchTransactions(
        std::vector<ripple::uint256> const& hashes,
        boost::asio::yield_context& yield) const override;

    std::vector<Blob>
    doFetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        std::uint32_t const sequence,
        boost::asio::yield_context& yield) const override;

    TransactionsAndCursor
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t const limit,
        bool forward,
        std::optional<TransactionsCursor> const& cursor,
        boost::asio::yield_context& yield) const override;

    void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader) override;

    void
    doWriteLedgerObject(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& blob) override;

    void
    writeSuccessor(
        std::string&& key,
        std::uint32_t const seq,
        std::string&& successor) override;

    void
    writeTransaction(
        std::string&& hash,
        std::uint32_t const seq,
        std::uint32_t const date,
        std::string&& transaction,
        std::string&& metadata) override;

    void
    writeNFTs(std::vector<NFTsData>&& data) override;

    void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) override;

    void
    writeNFTTransactions(std::vector<NFTTransactionsData>&& data) override;

    void
    open(bool readOnly) override;

    void
    close() override;

    void
    startWrites() const override;

    bool
    doFinishWrites() override;

    bool
    doOnlineDelete(
        std::uint32_t const numLedgersToKeep,
        boost::asio::yield_context& yield) const override;
};
}  // namespace Backend
#endif
