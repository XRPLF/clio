//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

/** @file */
#pragma once

/*
 * This file contains a variety of utility functions used when executing the handlers.
 */

#include <data/BackendInterface.h>
#include <rpc/Amendments.h>
#include <rpc/JS.h>
#include <rpc/common/Types.h>
#include <util/JsonUtils.h>
#include <web/Context.h>

#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Rate.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STTx.h>

#include <boost/regex.hpp>
#include <fmt/core.h>

namespace rpc {

enum class NFTokenjson { ENABLE, DISABLE };

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account);

bool
isOwnedByAccount(ripple::SLE const& sle, ripple::AccountID const& accountID);

std::uint64_t
getStartHint(ripple::SLE const& sle, ripple::AccountID const& accountID);

std::optional<AccountCursor>
parseAccountCursor(std::optional<std::string> jsonCursor);

// TODO this function should probably be in a different file and namespace
std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs);

// TODO this function should probably be in a different file and namespace
std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs, std::uint32_t seq);

/**
 * @brief Convert a TransactionAndMetadata to two JSON objects.
 *
 * @param blobs The TransactionAndMetadata to convert.
 * @param nftEnabled Whether to include NFT information in the JSON.
 * @param networkId The network ID to use for ctid, not include ctid if nullopt.
 */
std::pair<boost::json::object, boost::json::object>
toExpandedJson(
    data::TransactionAndMetadata const& blobs,
    NFTokenjson nftEnabled = NFTokenjson::DISABLE,
    std::optional<uint16_t> networkId = std::nullopt
);

bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    uint32_t date
);

boost::json::object
toJson(ripple::STBase const& obj);

boost::json::object
toJson(ripple::SLE const& sle);

boost::json::object
toJson(ripple::LedgerHeader const& info);

boost::json::object
toJson(ripple::TxMeta const& meta);

using RippledJson = Json::Value;
boost::json::value
toBoostJson(RippledJson const& value);

boost::json::object
generatePubLedgerMessage(
    ripple::LedgerHeader const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t txnCount
);

std::variant<Status, ripple::LedgerHeader>
ledgerInfoFromRequest(std::shared_ptr<data::BackendInterface const> const& backend, web::Context const& ctx);

std::variant<Status, ripple::LedgerHeader>
getLedgerInfoFromHashOrSeq(
    BackendInterface const& backend,
    boost::asio::yield_context yield,
    std::optional<std::string> ledgerHash,
    std::optional<uint32_t> ledgerIndex,
    uint32_t maxSeq
);

std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::Keylet const& owner,
    ripple::uint256 const& hexMarker,
    std::uint32_t startHint,
    std::uint32_t sequence,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE&&)> atOwnedNode
);

// Remove the account check from traverseOwnedNodes
// Account check has been done by framework,remove it from internal function
std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    std::uint32_t limit,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE&&)> atOwnedNode,
    bool nftIncluded = false
);

std::shared_ptr<ripple::SLE const>
read(
    std::shared_ptr<data::BackendInterface const> const& backend,
    ripple::Keylet const& keylet,
    ripple::LedgerHeader const& lgrInfo,
    web::Context const& context
);

std::variant<Status, std::pair<ripple::PublicKey, ripple::SecretKey>>
keypairFromRequst(boost::json::object const& request);

std::vector<ripple::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction);

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerHeader const& info, bool includeHash = false);

bool
isGlobalFrozen(
    BackendInterface const& backend,
    std::uint32_t seq,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
);

bool
isFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
);

ripple::STAmount
accountFunds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::STAmount const& amount,
    ripple::AccountID const& id,
    boost::asio::yield_context yield
);

ripple::STAmount
accountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    bool zeroIfFrozen,
    boost::asio::yield_context yield
);

ripple::Rate
transferRate(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
);

ripple::XRPAmount
xrpLiquid(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& id,
    boost::asio::yield_context yield
);

boost::json::array
postProcessOrderBook(
    std::vector<data::LedgerObject> const& offers,
    ripple::Book const& book,
    ripple::AccountID const& takerID,
    data::BackendInterface const& backend,
    std::uint32_t ledgerSequence,
    boost::asio::yield_context yield
);

std::variant<Status, ripple::Book>
parseBook(ripple::Currency pays, ripple::AccountID payIssuer, ripple::Currency gets, ripple::AccountID getIssuer);

std::variant<Status, ripple::Book>
parseBook(boost::json::object const& request);

std::variant<Status, ripple::AccountID>
parseTaker(boost::json::value const& taker);

bool
specifiesCurrentOrClosedLedger(boost::json::object const& request);

std::variant<ripple::uint256, Status>
getNFTID(boost::json::object const& request);

bool
isAmendmentEnabled(
    std::shared_ptr<data::BackendInterface const> const& backend,
    boost::asio::yield_context yield,
    uint32_t seq,
    ripple::uint256 amendmentId
);

std::optional<std::string>
encodeCTID(uint32_t ledgerSeq, uint16_t txnIndex, uint16_t networkId) noexcept;

template <typename T>
inline std::optional<std::tuple<uint32_t, uint16_t, uint16_t>>
decodeCTID(T const ctid) noexcept
{
    auto const getCTID64 = [](T const ctid) noexcept -> std::optional<uint64_t> {
        if constexpr (std::is_convertible_v<T, std::string>) {
            std::string const ctidString(ctid);
            static std::size_t constexpr CTID_STRING_LENGTH = 16;
            if (ctidString.length() != CTID_STRING_LENGTH)
                return {};

            if (!boost::regex_match(ctidString, boost::regex("^[0-9A-F]+$")))
                return {};

            return std::stoull(ctidString, nullptr, 16);
        }

        if constexpr (std::is_same_v<T, uint64_t>)
            return ctid;

        return {};
    };

    auto const ctidValue = getCTID64(ctid).value_or(0);

    static uint64_t constexpr CTID_PREFIX = 0xC000'0000'0000'0000ULL;
    static uint64_t constexpr CTID_PREFIX_MASK = 0xF000'0000'0000'0000ULL;

    if ((ctidValue & CTID_PREFIX_MASK) != CTID_PREFIX)
        return {};

    uint32_t const ledgerSeq = (ctidValue >> 32) & 0xFFFF'FFFUL;
    uint16_t const txnIndex = (ctidValue >> 16) & 0xFFFFU;
    uint16_t const networkId = ctidValue & 0xFFFFU;
    return {{ledgerSeq, txnIndex, networkId}};
}

template <class T>
void
logDuration(web::Context const& ctx, T const& dur)
{
    using boost::json::serialize;

    static util::Logger const log{"RPC"};
    static std::int64_t constexpr DURATION_ERROR_THRESHOLD_SECONDS = 10;

    auto const millis = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    auto const seconds = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    auto const msg = fmt::format(
        "Request processing duration = {} milliseconds. request = {}", millis, serialize(util::removeSecret(ctx.params))
    );

    if (seconds > DURATION_ERROR_THRESHOLD_SECONDS) {
        LOG(log.error()) << ctx.tag() << msg;
    } else if (seconds > 1) {
        LOG(log.warn()) << ctx.tag() << msg;
    } else
        LOG(log.info()) << ctx.tag() << msg;
}

}  // namespace rpc
