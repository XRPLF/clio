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
#include <gmock/gmock.h>

using namespace Backend;

class MockBackend : public BackendInterface
{
public:
    MockBackend(clio::Config)
    {
    }
    MOCK_METHOD(
        std::optional<ripple::LedgerInfo>,
        fetchLedgerBySequence,
        (std::uint32_t const sequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::optional<ripple::LedgerInfo>,
        fetchLedgerByHash,
        (ripple::uint256 const& hash, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::optional<std::uint32_t>,
        fetchLatestLedgerSequence,
        (boost::asio::yield_context & yield),
        (const, override));

    MOCK_METHOD(
        std::optional<TransactionAndMetadata>,
        fetchTransaction,
        (ripple::uint256 const& hash, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::vector<TransactionAndMetadata>,
        fetchTransactions,
        (std::vector<ripple::uint256> const& hashes, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        TransactionsAndCursor,
        fetchAccountTransactions,
        (ripple::AccountID const& account,
         std::uint32_t const limit,
         bool forward,
         std::optional<TransactionsCursor> const& cursor,
         boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::vector<TransactionAndMetadata>,
        fetchAllTransactionsInLedger,
        (std::uint32_t const ledgerSequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::vector<ripple::uint256>,
        fetchAllTransactionHashesInLedger,
        (std::uint32_t const ledgerSequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::optional<NFT>,
        fetchNFT,
        (ripple::uint256 const& tokenID, std::uint32_t const ledgerSequence, boost::asio::yield_context& yieldd),
        (const, override));

    MOCK_METHOD(
        TransactionsAndCursor,
        fetchNFTTransactions,
        (ripple::uint256 const& tokenID,
         std::uint32_t const limit,
         bool const forward,
         std::optional<TransactionsCursor> const& cursorIn,
         boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::vector<Blob>,
        doFetchLedgerObjects,
        (std::vector<ripple::uint256> const& key, std::uint32_t const sequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::optional<Blob>,
        doFetchLedgerObject,
        (ripple::uint256 const& key, std::uint32_t const sequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::vector<LedgerObject>,
        fetchLedgerDiff,
        (std::uint32_t const ledgerSequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::optional<ripple::uint256>,
        doFetchSuccessorKey,
        (ripple::uint256 key, std::uint32_t const ledgerSequence, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(
        std::optional<LedgerRange>,
        hardFetchLedgerRange,
        (boost::asio::yield_context & yield),
        (const, override));

    MOCK_METHOD(void, writeLedger, (ripple::LedgerInfo const& ledgerInfo, std::string&& ledgerHeader), (override));

    MOCK_METHOD(void, writeLedgerObject, (std::string && key, std::uint32_t const seq, std::string&& blob), (override));

    MOCK_METHOD(
        void,
        writeTransaction,
        (std::string && hash,
         std::uint32_t const seq,
         std::uint32_t const date,
         std::string&& transaction,
         std::string&& metadata),
        (override));

    MOCK_METHOD(void, writeNFTs, (std::vector<NFTsData> && blob), (override));

    MOCK_METHOD(void, writeAccountTransactions, (std::vector<AccountTransactionsData> && blob), (override));

    MOCK_METHOD(void, writeNFTTransactions, (std::vector<NFTTransactionsData> && blob), (override));

    MOCK_METHOD(
        void,
        writeSuccessor,
        (std::string && key, std::uint32_t const seq, std::string&& successor),
        (override));

    MOCK_METHOD(void, startWrites, (), (const, override));

    MOCK_METHOD(
        bool,
        doOnlineDelete,
        (std::uint32_t numLedgersToKeep, boost::asio::yield_context& yield),
        (const, override));

    MOCK_METHOD(bool, isTooBusy, (), (const, override));

    MOCK_METHOD(void, open, (bool), (override));

    MOCK_METHOD(void, close, (), (override));

    MOCK_METHOD(
        void,
        doWriteLedgerObject,
        (std::string && key, std::uint32_t const seq, std::string&& blob),
        (override));

    MOCK_METHOD(bool, doFinishWrites, (), (override));
};
