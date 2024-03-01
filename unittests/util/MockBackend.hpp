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
#include "util/config/Config.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace data;

struct MockBackend : public BackendInterface {
    MockBackend(util::Config)
    {
    }

    MOCK_METHOD(
        std::optional<ripple::LedgerInfo>,
        fetchLedgerBySequence,
        (std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<ripple::LedgerInfo>,
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
        std::optional<TransactionAndMetadata>,
        fetchTransaction,
        (ripple::uint256 const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<TransactionAndMetadata>,
        fetchTransactions,
        (std::vector<ripple::uint256> const&, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        TransactionsAndCursor,
        fetchAccountTransactions,
        (ripple::AccountID const&,
         std::uint32_t const,
         bool,
         std::optional<TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<TransactionAndMetadata>,
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
        std::optional<NFT>,
        fetchNFT,
        (ripple::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        TransactionsAndCursor,
        fetchNFTTransactions,
        (ripple::uint256 const&,
         std::uint32_t const,
         bool const,
         std::optional<TransactionsCursor> const&,
         boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        NFTsAndCursor,
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
        std::vector<Blob>,
        doFetchLedgerObjects,
        (std::vector<ripple::uint256> const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<Blob>,
        doFetchLedgerObject,
        (ripple::uint256 const&, std::uint32_t const, boost::asio::yield_context),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<LedgerObject>,
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

    MOCK_METHOD(std::optional<LedgerRange>, hardFetchLedgerRange, (boost::asio::yield_context), (const, override));

    MOCK_METHOD(void, writeLedger, (ripple::LedgerInfo const&, std::string&&), (override));

    MOCK_METHOD(void, writeLedgerObject, (std::string&&, std::uint32_t const, std::string&&), (override));

    MOCK_METHOD(
        void,
        writeTransaction,
        (std::string&&, std::uint32_t const, std::uint32_t const, std::string&&, std::string&&),
        (override)
    );

    MOCK_METHOD(void, writeNFTs, (std::vector<NFTsData> const&), (override));

    MOCK_METHOD(void, writeAccountTransactions, (std::vector<AccountTransactionsData>), (override));

    MOCK_METHOD(void, writeNFTTransactions, (std::vector<NFTTransactionsData> const&), (override));

    MOCK_METHOD(void, writeSuccessor, (std::string && key, std::uint32_t const, std::string&&), (override));

    MOCK_METHOD(void, startWrites, (), (const, override));

    MOCK_METHOD(bool, isTooBusy, (), (const, override));

    MOCK_METHOD(boost::json::object, stats, (), (const, override));

    MOCK_METHOD(void, doWriteLedgerObject, (std::string&&, std::uint32_t const, std::string&&), (override));

    MOCK_METHOD(bool, doFinishWrites, (), (override));

    MOCK_METHOD(
        void,
        writeMPTHolders,
        ((std::vector<std::pair<ripple::uint192, ripple::AccountID>> const&)),
        (override)
    );

    MOCK_METHOD(
        MPTHoldersAndCursor,
        fetchMPTHolders,
        (ripple::uint192 const& mptID,
         std::uint32_t const,
         (std::optional<ripple::AccountID> const&),
         std::uint32_t const,
         boost::asio::yield_context),
        (const, override)
    );
};
