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

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"
#include "util/JsonUtils.hpp"
#include "util/log/Logger.hpp"
#include "web/Context.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/regex.hpp>
#include <boost/regex/v5/regex_fwd.hpp>
#include <boost/regex/v5/regex_match.hpp>
#include <fmt/core.h>
#include <xrpl/basics/XRPAmount.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/Rate.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/TxMeta.h>
#include <xrpl/protocol/UintTypes.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {

/** @brief Enum for NFT json manipulation */
enum class NFTokenjson { ENABLE, DISABLE };

/**
 * @brief Get a ripple::AccountID from its string representation
 *
 * @param account The string representation of the account
 * @return The account ID or std::nullopt if the string is not a valid account
 */
std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account);

/**
 * @brief Check whether the SLE is owned by the account
 *
 * @param sle The ledger entry
 * @param accountID The account ID
 * @return true if the SLE is owned by the account
 */
bool
isOwnedByAccount(ripple::SLE const& sle, ripple::AccountID const& accountID);

/**
 * @brief Get the start hint for the account
 *
 * @param sle The ledger entry
 * @param accountID The account ID
 * @return The start hint
 */
std::uint64_t
getStartHint(ripple::SLE const& sle, ripple::AccountID const& accountID);

/**
 * @brief Parse the account cursor from the JSON
 *
 * @param jsonCursor The JSON cursor
 * @return The parsed account cursor
 */
std::optional<AccountCursor>
parseAccountCursor(std::optional<std::string> jsonCursor);

// TODO this function should probably be in a different file and namespace
/**
 * @brief Deserialize a TransactionAndMetadata into a pair of STTx and STObject
 *
 * @param blobs The TransactionAndMetadata to deserialize
 * @return The deserialized objects
 */
std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs);

// TODO this function should probably be in a different file and namespace
/**
 * @brief Deserialize a TransactionAndMetadata into a pair of STTx and TxMeta
 *
 * @param blobs The TransactionAndMetadata to deserialize
 * @param seq The sequence number to set
 * @return The deserialized objects
 */
std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs, std::uint32_t seq);

/**
 * @brief Convert a TransactionAndMetadata to two JSON objects
 *
 * @param blobs The TransactionAndMetadata to convert
 * @param apiVersion The api version to generate the JSON for
 * @param nftEnabled Whether to include NFT information in the JSON
 * @param networkId The network ID to use for ctid, not include ctid if nullopt
 * @return The JSON objects
 */
std::pair<boost::json::object, boost::json::object>
toExpandedJson(
    data::TransactionAndMetadata const& blobs,
    std::uint32_t apiVersion,
    NFTokenjson nftEnabled = NFTokenjson::DISABLE,
    std::optional<uint16_t> networkId = std::nullopt
);

/**
 * @brief Convert a TransactionAndMetadata to JSON object containing tx and metadata data in hex format. According to
 * the apiVersion, the key is "tx_blob" and "meta" or "meta_blob".
 * @param txnPlusMeta The TransactionAndMetadata to convert.
 * @param apiVersion The api version
 * @return The JSON object containing tx and metadata data in hex format.
 */
boost::json::object
toJsonWithBinaryTx(data::TransactionAndMetadata const& txnPlusMeta, std::uint32_t apiVersion);

/**
 * @brief Add "DeliverMax" which is the alias of "Amount" for "Payment" transaction to transaction json. Remove the
 * "Amount" field when version is greater than 1
 * @param txJson The transaction json object
 * @param apiVersion The api version
 */
void
insertDeliverMaxAlias(boost::json::object& txJson, std::uint32_t apiVersion);

/**
 * @brief Add "DeliveredAmount" to the transaction json object
 *
 * @param metaJson The metadata json object to add "DeliveredAmount"
 * @param txn The transaction object
 * @param meta The metadata object
 * @param date The date of the ledger
 * @return true if the "DeliveredAmount" is added to the metadata json object
 */
bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    uint32_t date
);

/**
 * @brief Convert STBase object to JSON
 *
 * @param obj The object to convert
 * @return The JSON object
 */
boost::json::object
toJson(ripple::STBase const& obj);

/**
 * @brief Convert SLE to JSON
 *
 * @param sle The ledger entry to convert
 * @return The JSON object
 */
boost::json::object
toJson(ripple::SLE const& sle);

/**
 * @brief Convert a LedgerHeader to JSON object.
 *
 * @param info The LedgerHeader to convert.
 * @param binary Whether to convert in hex format.
 * @param apiVersion The api version
 * @return The JSON object.
 */
boost::json::object
toJson(ripple::LedgerHeader const& info, bool binary, std::uint32_t apiVersion);

/**
 * @brief Convert a TxMeta to JSON object.
 *
 * @param meta The TxMeta to convert.
 * @return The JSON object.
 */
boost::json::object
toJson(ripple::TxMeta const& meta);

using RippledJson = Json::Value;

/**
 * @brief Convert a RippledJson to boost::json::value
 *
 * @param value The RippledJson to convert
 * @return The JSON value
 */
boost::json::value
toBoostJson(RippledJson const& value);

/**
 * @brief Generate a JSON object to publish ledger message
 *
 * @param lgrInfo The ledger header
 * @param fees The fees
 * @param ledgerRange The ledger range
 * @param txnCount The transaction count
 * @return The JSON object
 */
boost::json::object
generatePubLedgerMessage(
    ripple::LedgerHeader const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t txnCount
);

/**
 * @brief Get ledger info from the request
 *
 * @param backend The backend to use
 * @param ctx The context of the request
 * @return The ledger info or an error status
 */
std::variant<Status, ripple::LedgerHeader>
ledgerHeaderFromRequest(std::shared_ptr<data::BackendInterface const> const& backend, web::Context const& ctx);

/**
 * @brief Get ledger info from hash or sequence
 *
 * @param backend The backend to use
 * @param yield The coroutine context
 * @param ledgerHash The optional ledger hash
 * @param ledgerIndex The optional ledger index
 * @param maxSeq The maximum sequence to search
 * @return The ledger info or an error status
 */
std::variant<Status, ripple::LedgerHeader>
getLedgerHeaderFromHashOrSeq(
    BackendInterface const& backend,
    boost::asio::yield_context yield,
    std::optional<std::string> ledgerHash,
    std::optional<uint32_t> ledgerIndex,
    uint32_t maxSeq
);

/**
 * @brief Traverse nodes owned by an account
 *
 * @param backend The backend to use
 * @param owner The keylet of the owner
 * @param hexMarker The marker
 * @param startHint The start hint
 * @param sequence The sequence
 * @param limit The limit of nodes to traverse
 * @param yield The coroutine context
 * @param atOwnedNode The function to call for each owned node
 * @return The status or the account cursor
 */
std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::Keylet const& owner,
    ripple::uint256 const& hexMarker,
    std::uint32_t startHint,
    std::uint32_t sequence,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE)> atOwnedNode
);

/**
 * @brief Traverse nodes owned by an account
 *
 * @note Like the other one but removes the account check
 *
 * @param backend The backend to use
 * @param accountID The account ID
 * @param sequence The sequence
 * @param limit The limit of nodes to traverse
 * @param jsonCursor The optional JSON cursor
 * @param yield The coroutine context
 * @param atOwnedNode The function to call for each owned node
 * @param nftIncluded Whether to include NFTs
 * @return The status or the account cursor
 */
std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    std::uint32_t limit,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE)> atOwnedNode,
    bool nftIncluded = false
);

/**
 * @brief Read SLE from the backend
 *
 * @param backend The backend to use
 * @param keylet The keylet to read
 * @param lgrInfo The ledger header
 * @param context The context of the request
 * @return The SLE or nullptr if not found
 */
std::shared_ptr<ripple::SLE const>
read(
    std::shared_ptr<data::BackendInterface const> const& backend,
    ripple::Keylet const& keylet,
    ripple::LedgerHeader const& lgrInfo,
    web::Context const& context
);

/**
 * @brief Get the account associated with a transaction
 *
 * @param transaction The transaction
 * @return A vector of accounts associated with the transaction
 */
std::vector<ripple::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction);

/**
 * @brief Convert a ledger header to a blob
 *
 * @param info The ledger header
 * @param includeHash Whether to include the hash
 * @return The blob
 */
std::vector<unsigned char>
ledgerHeaderToBlob(ripple::LedgerHeader const& info, bool includeHash = false);

/**
 * @brief Whether global frozen is set
 *
 * @param backend The backend to use
 * @param seq The ledger sequence
 * @param issuer The issuer
 * @param yield The coroutine context
 * @return true if the global frozen is set; false otherwise
 */
bool
isGlobalFrozen(
    BackendInterface const& backend,
    std::uint32_t seq,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
);

/**
 * @brief Whether the account is frozen
 *
 * @param backend The backend to use
 * @param sequence The sequence
 * @param account The account
 * @param currency The currency
 * @param issuer The issuer
 * @param yield The coroutine context
 * @return true if the account is frozen; false otherwise
 */
bool
isFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
);

/**
 * @brief Get the account funds
 *
 * @param backend The backend to use
 * @param sequence The sequence
 * @param amount The amount
 * @param id The account ID
 * @param yield The coroutine context
 * @return The account funds
 */
ripple::STAmount
accountFunds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::STAmount const& amount,
    ripple::AccountID const& id,
    boost::asio::yield_context yield
);

/**
 * @brief Get the amount that an account holds
 *
 * @param backend The backend to use
 * @param sequence The sequence
 * @param account The account
 * @param currency The currency
 * @param issuer The issuer
 * @param zeroIfFrozen Whether to return zero if frozen
 * @param yield The coroutine context
 * @return The amount account holds
 */
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

/**
 * @brief Get the transfer rate
 *
 * @param backend The backend to use
 * @param sequence The sequence
 * @param issuer The issuer
 * @param yield The coroutine context
 * @return The transfer rate
 */
ripple::Rate
transferRate(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
);

/**
 * @brief Get the XRP liquidity
 *
 * @param backend The backend to use
 * @param sequence The sequence
 * @param id The account ID
 * @param yield The coroutine context
 * @return The XRP liquidity
 */
ripple::XRPAmount
xrpLiquid(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& id,
    boost::asio::yield_context yield
);

/**
 * @brief Post process an order book
 *
 * @param offers The offers
 * @param book The book
 * @param takerID The taker ID
 * @param backend The backend to use
 * @param ledgerSequence The ledger sequence
 * @param yield The coroutine context
 * @return The post processed order book
 */
boost::json::array
postProcessOrderBook(
    std::vector<data::LedgerObject> const& offers,
    ripple::Book const& book,
    ripple::AccountID const& takerID,
    data::BackendInterface const& backend,
    std::uint32_t ledgerSequence,
    boost::asio::yield_context yield
);

/**
 * @brief Parse the book from the request
 *
 * @param pays The currency to pay
 * @param payIssuer The issuer of the currency to pay
 * @param gets The currency to get
 * @param getIssuer The issuer of the currency to get
 * @return The book or an error status
 */
std::variant<Status, ripple::Book>
parseBook(ripple::Currency pays, ripple::AccountID payIssuer, ripple::Currency gets, ripple::AccountID getIssuer);

/**
 * @brief Parse the book from the request
 *
 * @param request The request
 * @return The book or an error status
 */
std::variant<Status, ripple::Book>
parseBook(boost::json::object const& request);

/**
 * @brief Parse the taker from the request
 *
 * @param taker The taker as json
 * @return The taker account or an error status
 */
std::variant<Status, ripple::AccountID>
parseTaker(boost::json::value const& taker);

/**
 * @brief Parse the json object into a ripple::Issue object.
 * @param issue The json object to parse. The accepted format is { "currency" : "USD", "issuer" :
 * "r9cZA1mLK5R5Am25ArfXFmqgNwjZgnfk59" } or {"currency" : "XRP"}
 * @return The ripple::Issue object.
 * @exception raise Json::error exception if the json object is not in the accepted format.
 */
ripple::Issue
parseIssue(boost::json::object const& issue);

/**
 * @brief Check whethe the request specifies the `current` or `closed` ledger
 * @param request The request to check
 * @return true if the request specifies the `current` or `closed` ledger
 */
bool
specifiesCurrentOrClosedLedger(boost::json::object const& request);

/**
 * @brief Check whether a request requires administrative privileges on rippled side.
 *
 * @param method The method name to check
 * @param request The request to check
 * @return true if the request requires ADMIN role
 */
bool
isAdminCmd(std::string const& method, boost::json::object const& request);

/**
 * @brief Get the NFTID from the request
 *
 * @param request The request
 * @return The NFTID or an error status
 */
std::variant<ripple::uint256, Status>
getNFTID(boost::json::object const& request);

/**
 * @brief Encode CTID as string
 *
 * @param ledgerSeq The ledger sequence
 * @param txnIndex The transaction index
 * @param networkId The network ID
 * @return The encoded CTID or std::nullopt if the input is invalid
 */
std::optional<std::string>
encodeCTID(uint32_t ledgerSeq, uint16_t txnIndex, uint16_t networkId) noexcept;

/**
 * @brief Decode the CTID from a string or a uint64_t
 *
 * @tparam T The type of the CTID
 * @param ctid The CTID to decode
 * @return The decoded CTID
 */
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

/**
 * @brief Log the duration of the request processing
 *
 * @tparam T The type of the duration
 * @param ctx The context of the request
 * @param dur The duration to log
 */
template <typename T>
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

/**
 * @brief Parse a ripple-lib seed
 *
 * @param value JSON value to parse from
 * @return The parsed seed if successful; std::nullopt otherwise
 */
std::optional<ripple::Seed>
parseRippleLibSeed(boost::json::value const& value);

/**
 * @brief Traverse NFT objects and call the callback for each owned node
 *
 * @param backend The backend to use
 * @param sequence The sequence
 * @param accountID The account ID
 * @param nextPage The next page
 * @param limit The limit
 * @param yield The coroutine context
 * @param atOwnedNode The function to call for each owned node
 * @return The account cursor or an error status
 */
std::variant<Status, AccountCursor>
traverseNFTObjects(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& accountID,
    ripple::uint256 nextPage,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE)> atOwnedNode
);

/**
 * @brief Parse the string as a uint32_t
 *
 * @param value The string to parse
 * @return The parsed value or std::nullopt if the string is not a valid uint32_t
 */
std::optional<std::uint32_t>
parseStringAsUInt(std::string const& value);  // TODO: move to string utils or something?

/**
 * @brief Whether the transaction can have a delivered amount
 *
 * @param txn The transaction
 * @param meta The metadata
 * @return true if the transaction can have a delivered amount
 */
bool
canHaveDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta
);

/**
 * @brief Get the delivered amount
 *
 * @param txn The transaction
 * @param meta The metadata
 * @param ledgerSequence The sequence
 * @param date The date of the ledger
 * @return The delivered amount or std::nullopt if not available
 */
std::optional<ripple::STAmount>
getDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    std::uint32_t ledgerSequence,
    uint32_t date
);

}  // namespace rpc
