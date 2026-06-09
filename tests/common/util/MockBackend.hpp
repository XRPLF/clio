#pragma once

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "data/LedgerCache.hpp"
#include "data/Types.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct MockBackend : public BackendInterface {
    MockBackend(util::config::ClioConfigDefinition) : BackendInterface(cache_)
    {
    }

    MOCK_METHOD(
        std::optional<ripple::LedgerHeader>,
        fetchLedgerBySequence,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<ripple::LedgerHeader>,
        fetchLedgerByHash,
        (ripple::uint256 const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<std::uint32_t>,
        fetchLatestLedgerSequence,
        (boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::TransactionAndMetadata>,
        fetchTransaction,
        (ripple::uint256 const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::TransactionAndMetadata>,
        fetchTransactions,
        (std::vector<ripple::uint256> const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchAccountTransactions,
        (ripple::AccountID const&,
         std::uint32_t const,
         bool,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::TransactionAndMetadata>,
        fetchAllTransactionsInLedger,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<ripple::uint256>,
        fetchAllTransactionHashesInLedger,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::NFT>,
        fetchNFT,
        (ripple::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchNFTTransactions,
        (ripple::uint256 const&,
         std::uint32_t const,
         bool const,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchMPTokenIssuanceTransactions,
        (ripple::uint192 const&,
         std::uint32_t,
         bool,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchAccountMPTokenIssuanceTransactions,
        (ripple::uint192 const&,
         ripple::AccountID const&,
         std::uint32_t,
         bool,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::NFTsAndCursor,
        fetchNFTsByIssuer,
        (ripple::AccountID const& issuer,
         std::optional<std::uint32_t> const& taxon,
         std::uint32_t const ledgerSequence,
         std::uint32_t const limit,
         std::optional<ripple::uint256> const& cursorIn,
         boost::asio::yield_context yield),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::Blob>,
        doFetchLedgerObjects,
        (std::vector<ripple::uint256> const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<ripple::uint256>,
        fetchAccountRoots,
        (std::uint32_t, std::uint32_t, std::uint32_t, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::Blob>,
        doFetchLedgerObject,
        (ripple::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<std::uint32_t>,
        doFetchLedgerObjectSeq,
        (ripple::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::LedgerObject>,
        fetchLedgerDiff,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<ripple::uint256>,
        doFetchSuccessorKey,
        (ripple::uint256, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<std::string>,
        fetchMigratorStatus,
        (std::string const&, boost::asio::yield_context),
        (const, override)
    );

    using FetchClioNodeReturnType =
        std::expected<std::vector<std::pair<boost::uuids::uuid, std::string>>, std::string>;
    MOCK_METHOD(
        FetchClioNodeReturnType,
        fetchClioNodesData,
        (boost::asio::yield_context yield),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::LedgerRange>,
        hardFetchLedgerRange,
        (boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(void, writeLedger, (ripple::LedgerHeader const&, std::string&&), (override));

    MOCK_METHOD(
        void,
        writeLedgerObject,
        (std::string&&, std::uint32_t const, std::string&&),
        (override)
    );

    MOCK_METHOD(
        void,
        writeTransaction,
        (std::string&&, std::uint32_t const, std::uint32_t const, std::string&&, std::string&&),
        (override)
    );

    MOCK_METHOD(void, writeNFTs, (std::vector<NFTsData> const&), (override));

    MOCK_METHOD(void, writeAccountTransactions, (std::vector<AccountTransactionsData>), (override));

    MOCK_METHOD(void, writeAccountTransaction, (AccountTransactionsData), (override));

    MOCK_METHOD(void, writeNFTTransactions, (std::vector<NFTTransactionsData> const&), (override));

    MOCK_METHOD(
        void,
        writeMPTokenIssuanceTransactions,
        (std::vector<MPTokenIssuanceTransactionsData> const&),
        (override)
    );

    MOCK_METHOD(
        void,
        writeAccountMPTokenIssuanceTransactions,
        (std::vector<MPTokenIssuanceTransactionsData> const&),
        (override)
    );

    MOCK_METHOD(
        void,
        writeSuccessor,
        (std::string && key, std::uint32_t const, std::string&&),
        (override)
    );

    MOCK_METHOD(
        void,
        writeNodeMessage,
        (boost::uuids::uuid const& uuid, std::string message),
        (override)
    );

    MOCK_METHOD(void, startWrites, (), (const, override));

    MOCK_METHOD(bool, isTooBusy, (), (const, override));

    MOCK_METHOD(boost::json::object, stats, (), (const, override));

    MOCK_METHOD(
        void,
        doWriteLedgerObject,
        (std::string&&, std::uint32_t const, std::string&&),
        (override)
    );

    MOCK_METHOD(void, waitForWritesToFinish, (), (override));

    MOCK_METHOD(bool, doFinishWrites, (), (override));

    MOCK_METHOD(void, writeMPTHolders, (std::vector<MPTHolderData> const&), (override));

    MOCK_METHOD(
        data::MPTHoldersAndCursor,
        fetchMPTHolders,
        (ripple::uint192 const& mptID,
         std::uint32_t const,
         (std::optional<ripple::AccountID> const&),
         std::uint32_t const,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(void, writeMigratorStatus, (std::string const&, std::string const&), (override));

protected:
    data::LedgerCache cache_;  // TODO: this should probably be injected and MockLedgerCache instead
};
