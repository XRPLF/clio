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

#include "rpc/RPCHelpers.hpp"

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"
#include "web/Context.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/format/format_fwd.hpp>
#include <boost/format/free_funcs.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/bad_lexical_cast.hpp>
#include <fmt/core.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/XRPAmount.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/NFTSyntheticSerializer.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/Rate.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/nftPageMask.h>
#include <xrpl/protocol/tokens.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// local to compilation unit loggers
namespace {
util::Logger gLog{"RPC"};
}  // namespace

namespace rpc {

std::optional<AccountCursor>
parseAccountCursor(std::optional<std::string> jsonCursor)
{
    ripple::uint256 cursorIndex = beast::zero;
    std::uint64_t startHint = 0;

    if (!jsonCursor)
        return AccountCursor({cursorIndex, startHint});

    // Cursor is composed of a comma separated index and start hint. The
    // former will be read as hex, and the latter using boost lexical cast.
    std::stringstream cursor(*jsonCursor);
    std::string value;
    if (!std::getline(cursor, value, ','))
        return {};

    if (!cursorIndex.parseHex(value))
        return {};

    if (!std::getline(cursor, value, ','))
        return {};

    try {
        startHint = boost::lexical_cast<std::uint64_t>(value);
    } catch (boost::bad_lexical_cast&) {
        return {};
    }

    return AccountCursor({cursorIndex, startHint});
}

std::optional<ripple::STAmount>
getDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    std::uint32_t const ledgerSequence,
    uint32_t date
)
{
    if (meta->hasDeliveredAmount())
        return meta->getDeliveredAmount();
    if (txn->isFieldPresent(ripple::sfAmount)) {
        // Ledger 4594095 is the first ledger in which the DeliveredAmount field
        // was present when a partial payment was made and its absence indicates
        // that the amount delivered is listed in the Amount field.
        //
        // If the ledger closed long after the DeliveredAmount code was deployed
        // then its absence indicates that the amount delivered is listed in the
        // Amount field. DeliveredAmount went live January 24, 2014.
        // 446000000 is in Feb 2014, well after DeliveredAmount went live
        static std::uint32_t constexpr FIRST_LEDGER_WITH_DELIVERED_AMOUNT = 4594095;
        static std::uint32_t constexpr DELIVERED_AMOUNT_LIVE_DATE = 446000000;
        if (ledgerSequence >= FIRST_LEDGER_WITH_DELIVERED_AMOUNT || date > DELIVERED_AMOUNT_LIVE_DATE) {
            return txn->getFieldAmount(ripple::sfAmount);
        }
    }
    return {};
}

bool
canHaveDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta
)
{
    ripple::TxType const tt{txn->getTxnType()};
    if (tt != ripple::ttPAYMENT && tt != ripple::ttCHECK_CASH && tt != ripple::ttACCOUNT_DELETE)
        return false;

    if (meta->getResultTER() != ripple::tesSUCCESS)
        return false;

    return true;
}

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = ripple::strUnHex(account);

    std::optional<ripple::PublicKey> publicKey = {};
    if (blob && ripple::publicKeyType(ripple::makeSlice(*blob))) {
        publicKey = ripple::PublicKey(ripple::Slice{blob->data(), blob->size()});
    } else {
        publicKey = util::parseBase58Wrapper<ripple::PublicKey>(ripple::TokenType::AccountPublic, account);
    }

    std::optional<ripple::AccountID> result;
    if (publicKey) {
        result = ripple::calcAccountID(*publicKey);
    } else {
        result = util::parseBase58Wrapper<ripple::AccountID>(account);
    }

    return result;
}

std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs)
{
    try {
        std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::STObject const>> result;
        {
            ripple::SerialIter s{blobs.transaction.data(), blobs.transaction.size()};
            result.first = std::make_shared<ripple::STTx const>(s);
        }
        {
            ripple::SerialIter s{blobs.metadata.data(), blobs.metadata.size()};
            result.second = std::make_shared<ripple::STObject const>(s, ripple::sfMetadata);
        }
        return result;
    } catch (std::exception const& e) {
        std::stringstream txn;
        std::stringstream meta;
        std::copy(blobs.transaction.begin(), blobs.transaction.end(), std::ostream_iterator<unsigned char>(txn));
        std::copy(blobs.metadata.begin(), blobs.metadata.end(), std::ostream_iterator<unsigned char>(meta));
        LOG(gLog.error()) << "Failed to deserialize transaction. txn = " << txn.str() << " - meta = " << meta.str()
                          << " txn length = " << std::to_string(blobs.transaction.size())
                          << " meta length = " << std::to_string(blobs.metadata.size());
        throw e;
    }
}

std::pair<std::shared_ptr<ripple::STTx const>, std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs, std::uint32_t seq)
{
    auto [tx, meta] = deserializeTxPlusMeta(blobs);

    std::shared_ptr<ripple::TxMeta> const m = std::make_shared<ripple::TxMeta>(tx->getTransactionID(), seq, *meta);

    return {tx, m};
}

boost::json::object
toJson(ripple::STBase const& obj)
{
    boost::json::value value = boost::json::parse(obj.getJson(ripple::JsonOptions::none).toStyledString());

    return value.as_object();
}

std::pair<boost::json::object, boost::json::object>
toExpandedJson(
    data::TransactionAndMetadata const& blobs,
    std::uint32_t const apiVersion,
    NFTokenjson nftEnabled,
    std::optional<uint16_t> networkId
)
{
    auto [txn, meta] = deserializeTxPlusMeta(blobs, blobs.ledgerSequence);
    auto txnJson = toJson(*txn);
    auto metaJson = toJson(*meta);
    insertDeliveredAmount(metaJson, txn, meta, blobs.date);
    insertDeliverMaxAlias(txnJson, apiVersion);

    if (nftEnabled == NFTokenjson::ENABLE) {
        Json::Value nftJson;
        ripple::insertNFTSyntheticInJson(nftJson, txn, *meta);
        // if there is no nft fields, the nftJson will be {"meta":null}
        auto const nftBoostJson = toBoostJson(nftJson).as_object();
        if (nftBoostJson.contains(JS(meta)) and nftBoostJson.at(JS(meta)).is_object()) {
            for (auto const& [k, v] : nftBoostJson.at(JS(meta)).as_object())
                metaJson.insert_or_assign(k, v);
        }
    }

    if (networkId) {
        // networkId is available, insert ctid field to tx
        if (auto const ctid = rpc::encodeCTID(meta->getLgrSeq(), meta->getIndex(), *networkId)) {
            txnJson[JS(ctid)] = *ctid;
        }
    }

    return {txnJson, metaJson};
}

std::optional<std::string>
encodeCTID(uint32_t ledgerSeq, uint16_t txnIndex, uint16_t networkId) noexcept
{
    static uint32_t constexpr MAX_LEDGER_SEQ = 0x0FFF'FFFF;
    if (ledgerSeq > MAX_LEDGER_SEQ)
        return {};

    static uint64_t constexpr CTID_PREFIX = 0xC000'0000;
    uint64_t const ctidValue =
        ((CTID_PREFIX + static_cast<uint64_t>(ledgerSeq)) << 32) + (static_cast<uint64_t>(txnIndex) << 16) + networkId;

    return {fmt::format("{:016X}", ctidValue)};
}

bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    uint32_t date
)
{
    if (canHaveDeliveredAmount(txn, meta)) {
        if (auto amt = getDeliveredAmount(txn, meta, meta->getLgrSeq(), date)) {
            metaJson["delivered_amount"] = toBoostJson(amt->getJson(ripple::JsonOptions::include_date));
        } else {
            metaJson["delivered_amount"] = "unavailable";
        }
        return true;
    }
    return false;
}

void
insertDeliverMaxAlias(boost::json::object& txJson, std::uint32_t const apiVersion)
{
    if (txJson.contains(JS(TransactionType)) and txJson.at(JS(TransactionType)).is_string() and
        txJson.at(JS(TransactionType)).as_string() == JS(Payment) and txJson.contains(JS(Amount))) {
        txJson[JS(DeliverMax)] = txJson[JS(Amount)];
        if (apiVersion > 1)
            txJson.erase(JS(Amount));
    }
}

boost::json::object
toJson(ripple::TxMeta const& meta)
{
    boost::json::value value = boost::json::parse(meta.getJson(ripple::JsonOptions::none).toStyledString());

    return value.as_object();
}

boost::json::value
toBoostJson(Json::Value const& value)
{
    boost::json::value boostValue = boost::json::parse(value.toStyledString());

    return boostValue;
}

boost::json::object
toJson(ripple::SLE const& sle)
{
    boost::json::value value = boost::json::parse(sle.getJson(ripple::JsonOptions::none).toStyledString());
    if (sle.getType() == ripple::ltACCOUNT_ROOT) {
        if (sle.isFieldPresent(ripple::sfEmailHash)) {
            auto const& hash = sle.getFieldH128(ripple::sfEmailHash);
            std::string md5 = strHex(hash);
            boost::algorithm::to_lower(md5);
            value.as_object()["urlgravatar"] = str(boost::format("http://www.gravatar.com/avatar/%s") % md5);
        }
    }
    return value.as_object();
}

boost::json::object
toJson(ripple::LedgerHeader const& lgrInfo, bool const binary, std::uint32_t const apiVersion)
{
    boost::json::object header;
    if (binary) {
        header[JS(ledger_data)] = ripple::strHex(ledgerHeaderToBlob(lgrInfo));
    } else {
        header[JS(account_hash)] = ripple::strHex(lgrInfo.accountHash);
        header[JS(close_flags)] = lgrInfo.closeFlags;
        header[JS(close_time)] = lgrInfo.closeTime.time_since_epoch().count();
        header[JS(close_time_human)] = ripple::to_string(lgrInfo.closeTime);
        header[JS(close_time_resolution)] = lgrInfo.closeTimeResolution.count();
        header[JS(close_time_iso)] = ripple::to_string_iso(lgrInfo.closeTime);
        header[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
        header[JS(parent_close_time)] = lgrInfo.parentCloseTime.time_since_epoch().count();
        header[JS(parent_hash)] = ripple::strHex(lgrInfo.parentHash);
        header[JS(total_coins)] = ripple::to_string(lgrInfo.drops);
        header[JS(transaction_hash)] = ripple::strHex(lgrInfo.txHash);

        if (apiVersion < 2u) {
            header[JS(ledger_index)] = std::to_string(lgrInfo.seq);
        } else {
            header[JS(ledger_index)] = lgrInfo.seq;
        }
    }
    header[JS(closed)] = true;
    return header;
}

std::optional<std::uint32_t>
parseStringAsUInt(std::string const& value)
{
    std::optional<std::uint32_t> index = {};
    try {
        index = boost::lexical_cast<std::uint32_t>(value);
    } catch (boost::bad_lexical_cast const&) {
        index = std::nullopt;
    }

    return index;
}

std::variant<Status, ripple::LedgerHeader>
ledgerHeaderFromRequest(std::shared_ptr<data::BackendInterface const> const& backend, web::Context const& ctx)
{
    auto hashValue = ctx.params.contains("ledger_hash") ? ctx.params.at("ledger_hash") : nullptr;

    if (!hashValue.is_null()) {
        if (!hashValue.is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "ledgerHashNotString"};

        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(boost::json::value_to<std::string>(hashValue)))
            return Status{RippledError::rpcINVALID_PARAMS, "ledgerHashMalformed"};

        auto lgrInfo = backend->fetchLedgerByHash(ledgerHash, ctx.yield);

        if (!lgrInfo || lgrInfo->seq > ctx.range.maxSequence)
            return Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};

        return *lgrInfo;
    }

    auto indexValue = ctx.params.contains("ledger_index") ? ctx.params.at("ledger_index") : nullptr;

    std::optional<std::uint32_t> ledgerSequence = {};
    if (!indexValue.is_null()) {
        if (indexValue.is_string()) {
            auto const stringIndex = boost::json::value_to<std::string>(indexValue);
            if (stringIndex == "validated") {
                ledgerSequence = ctx.range.maxSequence;
            } else {
                ledgerSequence = parseStringAsUInt(stringIndex);
            }
        } else if (indexValue.is_int64())
            ledgerSequence = indexValue.as_int64();
    } else {
        ledgerSequence = ctx.range.maxSequence;
    }

    if (!ledgerSequence)
        return Status{RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"};

    auto lgrInfo = backend->fetchLedgerBySequence(*ledgerSequence, ctx.yield);

    if (!lgrInfo || lgrInfo->seq > ctx.range.maxSequence)
        return Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};

    return *lgrInfo;
}

// extract ledgerHeaderFromRequest's parameter from context
std::variant<Status, ripple::LedgerHeader>
getLedgerHeaderFromHashOrSeq(
    BackendInterface const& backend,
    boost::asio::yield_context yield,
    std::optional<std::string> ledgerHash,
    std::optional<uint32_t> ledgerIndex,
    uint32_t maxSeq
)
{
    std::optional<ripple::LedgerHeader> lgrInfo;
    auto err = Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};
    if (ledgerHash) {
        // invoke uint256's constructor to parse the hex string , instead of
        // copying buffer
        ripple::uint256 const ledgerHash256{std::string_view(*ledgerHash)};
        lgrInfo = backend.fetchLedgerByHash(ledgerHash256, yield);
        if (!lgrInfo || lgrInfo->seq > maxSeq)
            return err;

        return *lgrInfo;
    }
    auto const ledgerSequence = ledgerIndex.value_or(maxSeq);
    // return without check db
    if (ledgerSequence > maxSeq)
        return err;

    lgrInfo = backend.fetchLedgerBySequence(ledgerSequence, yield);
    if (!lgrInfo)
        return err;

    return *lgrInfo;
}

std::vector<unsigned char>
ledgerHeaderToBlob(ripple::LedgerHeader const& info, bool includeHash)
{
    ripple::Serializer s;
    s.add32(info.seq);
    s.add64(info.drops.drops());
    s.addBitString(info.parentHash);
    s.addBitString(info.txHash);
    s.addBitString(info.accountHash);
    s.add32(info.parentCloseTime.time_since_epoch().count());
    s.add32(info.closeTime.time_since_epoch().count());
    s.add8(info.closeTimeResolution.count());
    s.add8(info.closeFlags);
    if (includeHash)
        s.addBitString(info.hash);
    return s.peekData();
}

std::uint64_t
getStartHint(ripple::SLE const& sle, ripple::AccountID const& accountID)
{
    if (sle.getType() == ripple::ltRIPPLE_STATE) {
        if (sle.getFieldAmount(ripple::sfLowLimit).getIssuer() == accountID) {
            return sle.getFieldU64(ripple::sfLowNode);
        }
        if (sle.getFieldAmount(ripple::sfHighLimit).getIssuer() == accountID)
            return sle.getFieldU64(ripple::sfHighNode);
    }

    if (!sle.isFieldPresent(ripple::sfOwnerNode))
        return 0;

    return sle.getFieldU64(ripple::sfOwnerNode);
}

// traverse account's nfts
// return Status if error occurs
// return [nextpage, count of nft already found] if success
std::variant<Status, AccountCursor>
traverseNFTObjects(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& accountID,
    ripple::uint256 nextPage,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE)> atOwnedNode
)
{
    auto const firstNFTPage = ripple::keylet::nftpage_min(accountID);
    auto const lastNFTPage = ripple::keylet::nftpage_max(accountID);

    // check if nextPage is valid
    if (nextPage != beast::zero and firstNFTPage.key != (nextPage & ~ripple::nft::pageMask))
        return Status{RippledError::rpcINVALID_PARAMS, "Invalid marker."};

    // no marker, start from the last page
    ripple::uint256 const currentPage = nextPage == beast::zero ? lastNFTPage.key : nextPage;

    // read the current page
    auto page = backend.fetchLedgerObject(currentPage, sequence, yield);

    if (!page) {
        if (nextPage == beast::zero) {  // no nft objects in lastNFTPage
            return AccountCursor{beast::zero, 0};
        }
        // marker is in the right range, but still invalid
        return Status{RippledError::rpcINVALID_PARAMS, "Invalid marker."};
    }

    // the object exists and the key is in right range, must be nft page
    ripple::SLE pageSLE{ripple::SerialIter{page->data(), page->size()}, currentPage};

    auto count = 0u;
    // traverse the nft page linked list until the start of the list or reach the limit
    while (true) {
        auto const nftPreviousPage = pageSLE.getFieldH256(ripple::sfPreviousPageMin);
        atOwnedNode(std::move(pageSLE));
        count++;

        if (count == limit or nftPreviousPage == beast::zero)
            return AccountCursor{nftPreviousPage, count};

        page = backend.fetchLedgerObject(nftPreviousPage, sequence, yield);
        pageSLE = ripple::SLE{ripple::SerialIter{page->data(), page->size()}, nftPreviousPage};
    }

    return AccountCursor{beast::zero, 0};
}

std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    std::uint32_t limit,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE)> atOwnedNode,
    bool nftIncluded
)
{
    auto const maybeCursor = parseAccountCursor(jsonCursor);

    if (!maybeCursor)
        return Status{RippledError::rpcINVALID_PARAMS, "Malformed cursor."};

    // the format is checked in RPC framework level
    auto [hexCursor, startHint] = *maybeCursor;

    auto const isNftMarkerNonZero = startHint == std::numeric_limits<uint32_t>::max() and hexCursor != beast::zero;
    auto const isNftMarkerZero = startHint == std::numeric_limits<uint32_t>::max() and hexCursor == beast::zero;
    // if we need to traverse nft objects and this is the first request -> traverse nft objects
    // if we need to traverse nft objects and the marker is still in nft page -> traverse nft objects
    // if we need to traverse nft objects and the marker is still in nft page but next page is zero -> owned nodes
    // if we need to traverse nft objects and the marker is not in nft page -> traverse owned nodes
    if (nftIncluded and (!jsonCursor or isNftMarkerNonZero)) {
        auto const cursorMaybe = traverseNFTObjects(backend, sequence, accountID, hexCursor, limit, yield, atOwnedNode);

        if (auto const status = std::get_if<Status>(&cursorMaybe))
            return *status;

        auto const [nextNFTPage, nftsCount] = std::get<AccountCursor>(cursorMaybe);

        // if limit reach , we return the next page and max as marker
        if (nftsCount >= limit)
            return AccountCursor{nextNFTPage, std::numeric_limits<uint32_t>::max()};

        // adjust limit ,continue traversing owned nodes
        limit -= nftsCount;
        hexCursor = beast::zero;
        startHint = 0;
    } else if (nftIncluded and isNftMarkerZero) {
        // the last request happen to fetch all the nft, adjust marker to continue traversing owned nodes
        hexCursor = beast::zero;
        startHint = 0;
    }

    return traverseOwnedNodes(
        backend, ripple::keylet::ownerDir(accountID), hexCursor, startHint, sequence, limit, yield, atOwnedNode
    );
}

std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::Keylet const& owner,
    ripple::uint256 const& hexMarker,
    std::uint32_t const startHint,
    std::uint32_t sequence,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(ripple::SLE)> atOwnedNode
)
{
    auto cursor = AccountCursor({beast::zero, 0});

    auto const rootIndex = owner;
    auto currentIndex = rootIndex;
    // track the current page we are accessing, will return it as the next hint
    auto currentPage = startHint;

    std::vector<ripple::uint256> keys;
    // Only reserve 2048 nodes when fetching all owned ledger objects. If there
    // are more, then keys will allocate more memory, which is suboptimal, but
    // should only occur occasionally.
    static std::uint32_t constexpr MIN_NODES = 2048;
    keys.reserve(std::min(MIN_NODES, limit));

    auto start = std::chrono::system_clock::now();

    // If startAfter is not zero try jumping to that page using the hint
    if (hexMarker.isNonZero()) {
        auto const hintIndex = ripple::keylet::page(rootIndex, startHint);
        auto hintDir = backend.fetchLedgerObject(hintIndex.key, sequence, yield);

        if (!hintDir)
            return Status(ripple::rpcINVALID_PARAMS, "Invalid marker.");

        ripple::SerialIter hintDirIt{hintDir->data(), hintDir->size()};
        ripple::SLE const hintDirSle{hintDirIt, hintIndex.key};

        if (auto const& indexes = hintDirSle.getFieldV256(ripple::sfIndexes);
            std::find(std::begin(indexes), std::end(indexes), hexMarker) == std::end(indexes)) {
            // the index specified by marker is not in the page specified by marker
            return Status(ripple::rpcINVALID_PARAMS, "Invalid marker.");
        }

        currentIndex = hintIndex;
        bool found = false;
        for (;;) {
            auto const ownerDir = backend.fetchLedgerObject(currentIndex.key, sequence, yield);

            if (!ownerDir)
                return Status(ripple::rpcINVALID_PARAMS, "Owner directory not found.");

            ripple::SerialIter ownedDirIt{ownerDir->data(), ownerDir->size()};
            ripple::SLE const ownedDirSle{ownedDirIt, currentIndex.key};

            for (auto const& key : ownedDirSle.getFieldV256(ripple::sfIndexes)) {
                if (!found) {
                    if (key == hexMarker)
                        found = true;
                } else {
                    keys.push_back(key);

                    if (--limit == 0) {
                        break;
                    }
                }
            }

            if (limit == 0) {
                cursor = AccountCursor({keys.back(), currentPage});
                break;
            }
            // the next page
            auto const uNodeNext = ownedDirSle.getFieldU64(ripple::sfIndexNext);
            if (uNodeNext == 0)
                break;

            currentIndex = ripple::keylet::page(rootIndex, uNodeNext);
            currentPage = uNodeNext;
        }
    } else {
        for (;;) {
            auto const ownerDir = backend.fetchLedgerObject(currentIndex.key, sequence, yield);

            if (!ownerDir)
                break;

            ripple::SerialIter ownedDirIt{ownerDir->data(), ownerDir->size()};
            ripple::SLE const ownedDirSle{ownedDirIt, currentIndex.key};

            for (auto const& key : ownedDirSle.getFieldV256(ripple::sfIndexes)) {
                keys.push_back(key);

                if (--limit == 0)
                    break;
            }

            if (limit == 0) {
                cursor = AccountCursor({keys.back(), currentPage});
                break;
            }

            auto const uNodeNext = ownedDirSle.getFieldU64(ripple::sfIndexNext);
            if (uNodeNext == 0)
                break;

            currentIndex = ripple::keylet::page(rootIndex, uNodeNext);
            currentPage = uNodeNext;
        }
    }
    auto end = std::chrono::system_clock::now();

    LOG(gLog.debug()) << fmt::format(
        "Time loading owned directories: {} milliseconds, entries size: {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
        keys.size()
    );

    auto [objects, timeDiff] = util::timed([&]() { return backend.fetchLedgerObjects(keys, sequence, yield); });

    LOG(gLog.debug()) << "Time loading owned entries: " << timeDiff << " milliseconds";

    for (auto i = 0u; i < objects.size(); ++i) {
        ripple::SerialIter it{objects[i].data(), objects[i].size()};
        atOwnedNode(ripple::SLE{it, keys[i]});
    }

    if (limit == 0)
        return cursor;

    return AccountCursor({beast::zero, 0});
}

std::shared_ptr<ripple::SLE const>
read(
    std::shared_ptr<data::BackendInterface const> const& backend,
    ripple::Keylet const& keylet,
    ripple::LedgerHeader const& lgrInfo,
    web::Context const& context
)
{
    if (auto const blob = backend->fetchLedgerObject(keylet.key, lgrInfo.seq, context.yield); blob) {
        return std::make_shared<ripple::SLE const>(ripple::SerialIter{blob->data(), blob->size()}, keylet.key);
    }

    return nullptr;
}

std::optional<ripple::Seed>
parseRippleLibSeed(boost::json::value const& value)
{
    // ripple-lib encodes seed used to generate an Ed25519 wallet in a
    // non-standard way. While rippled never encode seeds that way, we
    // try to detect such keys to avoid user confusion.
    if (!value.is_string())
        return {};

    auto const result = ripple::decodeBase58Token(boost::json::value_to<std::string>(value), ripple::TokenType::None);

    static std::size_t constexpr SEED_SIZE = 18;
    static std::array<std::uint8_t, 2> constexpr SEED_PREFIX = {0xE1, 0x4B};
    if (result.size() == SEED_SIZE && static_cast<std::uint8_t>(result[0]) == SEED_PREFIX[0] &&
        static_cast<std::uint8_t>(result[1]) == SEED_PREFIX[1])
        return ripple::Seed(ripple::makeSlice(result.substr(2)));

    return {};
}

std::vector<ripple::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction)
{
    std::vector<ripple::AccountID> accounts = {};
    for (auto const& [key, value] : transaction) {
        if (value.is_object()) {
            auto inObject = getAccountsFromTransaction(value.as_object());
            accounts.insert(accounts.end(), inObject.begin(), inObject.end());
        } else if (value.is_string()) {
            auto const account = util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(value));
            if (account) {
                accounts.push_back(*account);
            }
        }
    }

    return accounts;
}

bool
isGlobalFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    if (ripple::isXRP(issuer))
        return false;

    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return false;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE const sle{it, key};

    return sle.isFlag(ripple::lsfGlobalFreeze);
}

bool
isFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    if (ripple::isXRP(currency))
        return false;

    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return false;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE const sle{it, key};

    if (sle.isFlag(ripple::lsfGlobalFreeze))
        return true;

    if (issuer != account) {
        key = ripple::keylet::line(account, issuer, currency).key;
        blob = backend.fetchLedgerObject(key, sequence, yield);

        if (!blob)
            return false;

        ripple::SerialIter issuerIt{blob->data(), blob->size()};
        ripple::SLE const issuerLine{issuerIt, key};

        auto frozen = (issuer > account) ? ripple::lsfHighFreeze : ripple::lsfLowFreeze;

        if (issuerLine.isFlag(frozen))
            return true;
    }

    return false;
}

ripple::XRPAmount
xrpLiquid(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& id,
    boost::asio::yield_context yield
)
{
    auto const key = ripple::keylet::account(id).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return beast::zero;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE const sle{it, key};

    std::uint32_t const ownerCount = sle.getFieldU32(ripple::sfOwnerCount);

    auto balance = sle.getFieldAmount(ripple::sfBalance);

    ripple::STAmount const amount = [&]() {
        // AMM doesn't require the reserves
        if ((sle.getFlags() & ripple::lsfAMMNode) != 0u)
            return balance;
        auto const reserve = backend.fetchFees(sequence, yield)->accountReserve(ownerCount);
        ripple::STAmount amount = balance - reserve;
        if (balance < reserve)
            amount.clear();
        return amount;
    }();

    return amount.xrp();
}

ripple::STAmount
accountFunds(
    BackendInterface const& backend,
    std::uint32_t const sequence,
    ripple::STAmount const& amount,
    ripple::AccountID const& id,
    boost::asio::yield_context yield
)
{
    if (!amount.native() && amount.getIssuer() == id) {
        return amount;
    }

    return accountHolds(backend, sequence, id, amount.getCurrency(), amount.getIssuer(), true, yield);
}

ripple::STAmount
accountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    bool const zeroIfFrozen,
    boost::asio::yield_context yield
)
{
    ripple::STAmount amount;
    if (ripple::isXRP(currency))
        return {xrpLiquid(backend, sequence, account, yield)};

    auto const key = ripple::keylet::line(account, issuer, currency).key;
    auto const blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob) {
        amount.clear({currency, issuer});
        return amount;
    }

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE const sle{it, key};

    if (zeroIfFrozen && isFrozen(backend, sequence, account, currency, issuer, yield)) {
        amount.clear(ripple::Issue(currency, issuer));
    } else {
        amount = sle.getFieldAmount(ripple::sfBalance);
        if (account > issuer) {
            // Put balance in account terms.
            amount.negate();
        }
        amount.setIssuer(issuer);
    }

    return amount;
}

ripple::Rate
transferRate(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (blob) {
        ripple::SerialIter it{blob->data(), blob->size()};
        ripple::SLE const sle{it, key};

        if (sle.isFieldPresent(ripple::sfTransferRate))
            return ripple::Rate{sle.getFieldU32(ripple::sfTransferRate)};
    }

    return ripple::parityRate;
}

boost::json::array
postProcessOrderBook(
    std::vector<data::LedgerObject> const& offers,
    ripple::Book const& book,
    ripple::AccountID const& takerID,
    data::BackendInterface const& backend,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context yield
)
{
    boost::json::array jsonOffers;

    std::map<ripple::AccountID, ripple::STAmount> umBalance;

    bool const globalFreeze = isGlobalFrozen(backend, ledgerSequence, book.out.account, yield) ||
        isGlobalFrozen(backend, ledgerSequence, book.in.account, yield);

    auto rate = transferRate(backend, ledgerSequence, book.out.account, yield);

    for (auto const& obj : offers) {
        try {
            ripple::SerialIter it{obj.blob.data(), obj.blob.size()};
            ripple::SLE const offer{it, obj.key};
            ripple::uint256 const bookDir = offer.getFieldH256(ripple::sfBookDirectory);

            auto const uOfferOwnerID = offer.getAccountID(ripple::sfAccount);
            auto const& saTakerGets = offer.getFieldAmount(ripple::sfTakerGets);
            auto const& saTakerPays = offer.getFieldAmount(ripple::sfTakerPays);
            ripple::STAmount saOwnerFunds;
            bool firstOwnerOffer = true;

            if (book.out.account == uOfferOwnerID) {
                // If an offer is selling issuer's own IOUs, it is fully
                // funded.
                saOwnerFunds = saTakerGets;
            } else if (globalFreeze) {
                // If either asset is globally frozen, consider all offers
                // that aren't ours to be totally unfunded
                saOwnerFunds.clear(book.out);
            } else {
                auto umBalanceEntry = umBalance.find(uOfferOwnerID);
                if (umBalanceEntry != umBalance.end()) {
                    // Found in running balance table.

                    saOwnerFunds = umBalanceEntry->second;
                    firstOwnerOffer = false;
                } else {
                    saOwnerFunds = accountHolds(
                        backend, ledgerSequence, uOfferOwnerID, book.out.currency, book.out.account, true, yield
                    );

                    if (saOwnerFunds < beast::zero)
                        saOwnerFunds.clear();
                }
            }

            boost::json::object offerJson = toJson(offer);

            ripple::STAmount saTakerGetsFunded;
            ripple::STAmount saOwnerFundsLimit = saOwnerFunds;
            ripple::Rate offerRate = ripple::parityRate;
            ripple::STAmount const dirRate = ripple::amountFromQuality(getQuality(bookDir));

            if (rate != ripple::parityRate
                // Have a tranfer fee.
                && takerID != book.out.account
                // Not taking offers of own IOUs.
                && book.out.account != uOfferOwnerID)
            // Offer owner not issuing ownfunds
            {
                // Need to charge a transfer fee to offer owner.
                offerRate = rate;
                saOwnerFundsLimit = ripple::divide(saOwnerFunds, offerRate);
            }

            if (saOwnerFundsLimit >= saTakerGets) {
                // Sufficient funds no shenanigans.
                saTakerGetsFunded = saTakerGets;
            } else {
                saTakerGetsFunded = saOwnerFundsLimit;
                offerJson["taker_gets_funded"] = toBoostJson(saTakerGetsFunded.getJson(ripple::JsonOptions::none));
                offerJson["taker_pays_funded"] =
                    toBoostJson(std::min(saTakerPays, ripple::multiply(saTakerGetsFunded, dirRate, saTakerPays.issue()))
                                    .getJson(ripple::JsonOptions::none));
            }

            ripple::STAmount const saOwnerPays = (ripple::parityRate == offerRate)
                ? saTakerGetsFunded
                : std::min(saOwnerFunds, ripple::multiply(saTakerGetsFunded, offerRate));

            umBalance[uOfferOwnerID] = saOwnerFunds - saOwnerPays;

            if (firstOwnerOffer)
                offerJson["owner_funds"] = saOwnerFunds.getText();

            offerJson["quality"] = dirRate.getText();

            jsonOffers.push_back(offerJson);
        } catch (std::exception const& e) {
            LOG(gLog.error()) << "caught exception: " << e.what();
        }
    }
    return jsonOffers;
}

// get book via currency type
std::variant<Status, ripple::Book>
parseBook(ripple::Currency pays, ripple::AccountID payIssuer, ripple::Currency gets, ripple::AccountID getIssuer)
{
    if (isXRP(pays) && !isXRP(payIssuer)) {
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED, "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        };
    }

    if (!isXRP(pays) && isXRP(payIssuer)) {
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED, "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        };
    }

    if (ripple::isXRP(gets) && !ripple::isXRP(getIssuer)) {
        return Status{
            RippledError::rpcDST_ISR_MALFORMED, "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        };
    }

    if (!ripple::isXRP(gets) && ripple::isXRP(getIssuer)) {
        return Status{
            RippledError::rpcDST_ISR_MALFORMED, "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        };
    }

    if (pays == gets && payIssuer == getIssuer)
        return Status{RippledError::rpcBAD_MARKET, "badMarket"};

    return ripple::Book{{pays, payIssuer}, {gets, getIssuer}};
}

std::variant<Status, ripple::Book>
parseBook(boost::json::object const& request)
{
    if (!request.contains("taker_pays"))
        return Status{RippledError::rpcINVALID_PARAMS, "Missing field 'taker_pays'"};

    if (!request.contains("taker_gets"))
        return Status{RippledError::rpcINVALID_PARAMS, "Missing field 'taker_gets'"};

    if (!request.at("taker_pays").is_object())
        return Status{RippledError::rpcINVALID_PARAMS, "Field 'taker_pays' is not an object"};

    if (!request.at("taker_gets").is_object())
        return Status{RippledError::rpcINVALID_PARAMS, "Field 'taker_gets' is not an object"};

    auto taker_pays = request.at("taker_pays").as_object();
    if (!taker_pays.contains("currency"))
        return Status{RippledError::rpcSRC_CUR_MALFORMED};

    if (!taker_pays.at("currency").is_string())
        return Status{RippledError::rpcSRC_CUR_MALFORMED};

    auto taker_gets = request.at("taker_gets").as_object();
    if (!taker_gets.contains("currency"))
        return Status{RippledError::rpcDST_AMT_MALFORMED};

    if (!taker_gets.at("currency").is_string()) {
        return Status{
            RippledError::rpcDST_AMT_MALFORMED,
        };
    }

    ripple::Currency pay_currency;
    if (!ripple::to_currency(pay_currency, boost::json::value_to<std::string>(taker_pays.at("currency"))))
        return Status{RippledError::rpcSRC_CUR_MALFORMED};

    ripple::Currency get_currency;
    if (!ripple::to_currency(get_currency, boost::json::value_to<std::string>(taker_gets["currency"])))
        return Status{RippledError::rpcDST_AMT_MALFORMED};

    ripple::AccountID pay_issuer;
    if (taker_pays.contains("issuer")) {
        if (!taker_pays.at("issuer").is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "takerPaysIssuerNotString"};

        if (!ripple::to_issuer(pay_issuer, boost::json::value_to<std::string>(taker_pays.at("issuer"))))
            return Status{RippledError::rpcSRC_ISR_MALFORMED};

        if (pay_issuer == ripple::noAccount())
            return Status{RippledError::rpcSRC_ISR_MALFORMED};
    } else {
        pay_issuer = ripple::xrpAccount();
    }

    if (isXRP(pay_currency) && !isXRP(pay_issuer)) {
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED, "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        };
    }

    if (!isXRP(pay_currency) && isXRP(pay_issuer)) {
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED, "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        };
    }

    if ((!isXRP(pay_currency)) && (!taker_pays.contains("issuer")))
        return Status{RippledError::rpcSRC_ISR_MALFORMED, "Missing non-XRP issuer."};

    ripple::AccountID get_issuer;

    if (taker_gets.contains("issuer")) {
        if (!taker_gets["issuer"].is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "taker_gets.issuer should be string"};

        if (!ripple::to_issuer(get_issuer, boost::json::value_to<std::string>(taker_gets.at("issuer"))))
            return Status{RippledError::rpcDST_ISR_MALFORMED, "Invalid field 'taker_gets.issuer', bad issuer."};

        if (get_issuer == ripple::noAccount()) {
            return Status{
                RippledError::rpcDST_ISR_MALFORMED, "Invalid field 'taker_gets.issuer', bad issuer account one."
            };
        }
    } else {
        get_issuer = ripple::xrpAccount();
    }

    if (ripple::isXRP(get_currency) && !ripple::isXRP(get_issuer)) {
        return Status{
            RippledError::rpcDST_ISR_MALFORMED, "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        };
    }

    if (!ripple::isXRP(get_currency) && ripple::isXRP(get_issuer)) {
        return Status{
            RippledError::rpcDST_ISR_MALFORMED, "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        };
    }

    if (pay_currency == get_currency && pay_issuer == get_issuer)
        return Status{RippledError::rpcBAD_MARKET, "badMarket"};

    return ripple::Book{{pay_currency, pay_issuer}, {get_currency, get_issuer}};
}

std::variant<Status, ripple::AccountID>
parseTaker(boost::json::value const& taker)
{
    std::optional<ripple::AccountID> takerID = {};
    if (!taker.is_string())
        return {Status{RippledError::rpcINVALID_PARAMS, "takerNotString"}};

    takerID = accountFromStringStrict(boost::json::value_to<std::string>(taker));

    if (!takerID)
        return Status{RippledError::rpcBAD_ISSUER, "invalidTakerAccount"};
    return *takerID;
}

ripple::Issue
parseIssue(boost::json::object const& issue)
{
    Json::Value jv;
    if (issue.contains(JS(issuer)) && issue.at(JS(issuer)).is_string())
        jv["issuer"] = boost::json::value_to<std::string>(issue.at(JS(issuer)));
    if (issue.contains(JS(currency)) && issue.at(JS(currency)).is_string())
        jv["currency"] = boost::json::value_to<std::string>(issue.at(JS(currency)));

    return ripple::issueFromJson(jv);
}

bool
specifiesCurrentOrClosedLedger(boost::json::object const& request)
{
    if (request.contains("ledger_index")) {
        auto indexValue = request.at("ledger_index");
        if (indexValue.is_string()) {
            auto const index = boost::json::value_to<std::string>(indexValue);
            return index == "current" || index == "closed";
        }
    }
    return false;
}

bool
isAdminCmd(std::string const& method, boost::json::object const& request)
{
    auto const isFieldSet = [&request](auto const field) {
        return request.contains(field) and request.at(field).is_bool() and request.at(field).as_bool();
    };

    if (method == JS(ledger)) {
        if (isFieldSet(JS(full)) or isFieldSet(JS(accounts)) or isFieldSet(JS(type)))
            return true;
    }

    if (method == JS(feature) and request.contains(JS(vetoed)))
        return true;
    return false;
}

std::variant<ripple::uint256, Status>
getNFTID(boost::json::object const& request)
{
    if (!request.contains(JS(nft_id)))
        return Status{RippledError::rpcINVALID_PARAMS, "missingTokenID"};

    if (!request.at(JS(nft_id)).is_string())
        return Status{RippledError::rpcINVALID_PARAMS, "tokenIDNotString"};

    ripple::uint256 tokenid;
    if (!tokenid.parseHex(boost::json::value_to<std::string>(request.at(JS(nft_id)))))
        return Status{RippledError::rpcINVALID_PARAMS, "malformedTokenID"};

    return tokenid;
}

boost::json::object
toJsonWithBinaryTx(data::TransactionAndMetadata const& txnPlusMeta, std::uint32_t const apiVersion)
{
    boost::json::object obj{};
    auto const metaKey = apiVersion > 1 ? JS(meta_blob) : JS(meta);
    obj[metaKey] = ripple::strHex(txnPlusMeta.metadata);
    obj[JS(tx_blob)] = ripple::strHex(txnPlusMeta.transaction);
    return obj;
}

}  // namespace rpc
