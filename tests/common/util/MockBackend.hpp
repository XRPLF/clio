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
        std::optional<xrpl::LedgerHeader>,
        fetchLedgerBySequence,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<xrpl::LedgerHeader>,
        fetchLedgerByHash,
        (xrpl::uint256 const&, boost::asio::yield_context),
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
        (xrpl::uint256 const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::TransactionAndMetadata>,
        fetchTransactions,
        (std::vector<xrpl::uint256> const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchAccountTransactions,
        (xrpl::AccountID const&,
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
        std::vector<xrpl::uint256>,
        fetchAllTransactionHashesInLedger,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::NFT>,
        fetchNFT,
        (xrpl::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchNFTTransactions,
        (xrpl::uint256 const&,
         std::uint32_t const,
         bool const,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchMPTokenIssuanceTransactions,
        (xrpl::uint192 const&,
         std::uint32_t,
         bool,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::TransactionsAndCursor,
        fetchAccountMPTokenIssuanceTransactions,
        (xrpl::uint192 const&,
         xrpl::AccountID const&,
         std::uint32_t,
         bool,
         std::optional<data::TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        data::NFTsAndCursor,
        fetchNFTsByIssuer,
        (xrpl::AccountID const& issuer,
         std::optional<std::uint32_t> const& taxon,
         std::uint32_t const ledgerSequence,
         std::uint32_t const limit,
         std::optional<xrpl::uint256> const& cursorIn,
         boost::asio::yield_context yield),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::Blob>,
        doFetchLedgerObjects,
        (std::vector<xrpl::uint256> const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<xrpl::uint256>,
        fetchAccountRoots,
        (std::uint32_t, std::uint32_t, std::uint32_t, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::Blob>,
        doFetchLedgerObject,
        (xrpl::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<std::uint32_t>,
        doFetchLedgerObjectSeq,
        (xrpl::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<data::LedgerObject>,
        fetchLedgerDiff,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<xrpl::uint256>,
        doFetchSuccessorKey,
        (xrpl::uint256, std::uint32_t const, boost::asio::yield_context),
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

    MOCK_METHOD(void, writeLedger, (xrpl::LedgerHeader const&, std::string&&), (override));

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
        (xrpl::uint192 const& mptID,
         std::uint32_t const,
         (std::optional<xrpl::AccountID> const&),
         std::uint32_t const,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(void, writeMigratorStatus, (std::string const&, std::string const&), (override));

protected:
    data::LedgerCache cache_;  // TODO: this should probably be injected and MockLedgerCache instead
};
