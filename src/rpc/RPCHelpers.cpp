#include "rpc/RPCHelpers.hpp"

#include "data/AmendmentCenter.hpp"
#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"
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
#include <boost/json/serialize.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/bad_lexical_cast.hpp>
#include <fmt/format.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/json/json_reader.h>
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
#include <xrpl/protocol/XRPAmount.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/nftPageMask.h>
#include <xrpl/protocol/tokens.h>

#include <algorithm>
#include <array>
#include <cassert>
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
#include <vector>

namespace rpc {

std::optional<AccountCursor>
parseAccountCursor(std::optional<std::string> jsonCursor)
{
    xrpl::uint256 cursorIndex = beast::kZero;
    std::uint64_t startHint = 0;

    if (!jsonCursor)
        return AccountCursor({.index = cursorIndex, .hint = startHint});

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

    return AccountCursor({.index = cursorIndex, .hint = startHint});
}

std::optional<xrpl::STAmount>
getDeliveredAmount(
    std::shared_ptr<xrpl::STTx const> const& txn,
    std::shared_ptr<xrpl::TxMeta const> const& meta,
    std::uint32_t const ledgerSequence,
    uint32_t date
)
{
    if (auto const delivered = meta->getDeliveredAmount(); delivered.has_value())
        return delivered;
    if (txn->isFieldPresent(xrpl::sfAmount)) {
        // Ledger 4594095 is the first ledger in which the DeliveredAmount field
        // was present when a partial payment was made and its absence indicates
        // that the amount delivered is listed in the Amount field.
        //
        // If the ledger closed long after the DeliveredAmount code was deployed
        // then its absence indicates that the amount delivered is listed in the
        // Amount field. DeliveredAmount went live January 24, 2014.
        // 446000000 is in Feb 2014, well after DeliveredAmount went live
        static constexpr std::uint32_t kFirstLedgerWithDeliveredAmount = 4594095;
        static constexpr std::uint32_t kDeliveredAmountLiveDate = 446000000;
        if (ledgerSequence >= kFirstLedgerWithDeliveredAmount || date > kDeliveredAmountLiveDate) {
            return txn->getFieldAmount(xrpl::sfAmount);
        }
    }
    return {};
}

bool
canHaveDeliveredAmount(
    std::shared_ptr<xrpl::STTx const> const& txn,
    std::shared_ptr<xrpl::TxMeta const> const& meta
)
{
    xrpl::TxType const tt{txn->getTxnType()};
    if (tt != xrpl::ttPAYMENT && tt != xrpl::ttCHECK_CASH && tt != xrpl::ttACCOUNT_DELETE)
        return false;

    return meta->getResultTER() == xrpl::tesSUCCESS;
}

std::optional<xrpl::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = xrpl::strUnHex(account);

    std::optional<xrpl::PublicKey> publicKey = {};
    if (blob && xrpl::publicKeyType(xrpl::makeSlice(*blob))) {
        publicKey = xrpl::PublicKey(xrpl::Slice{blob->data(), blob->size()});
    } else {
        publicKey =
            util::parseBase58Wrapper<xrpl::PublicKey>(xrpl::TokenType::AccountPublic, account);
    }

    std::optional<xrpl::AccountID> result;
    if (publicKey) {
        result = xrpl::calcAccountID(*publicKey);
    } else {
        result = util::parseBase58Wrapper<xrpl::AccountID>(account);
    }

    return result;
}

std::pair<std::shared_ptr<xrpl::STTx const>, std::shared_ptr<xrpl::STObject const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs)
{
    static util::Logger const log{"RPC"};  // NOLINT(readability-identifier-naming)

    try {
        std::pair<std::shared_ptr<xrpl::STTx const>, std::shared_ptr<xrpl::STObject const>> result;
        {
            xrpl::SerialIter s{blobs.transaction.data(), blobs.transaction.size()};
            result.first = std::make_shared<xrpl::STTx const>(s);
        }
        {
            xrpl::SerialIter s{blobs.metadata.data(), blobs.metadata.size()};
            result.second = std::make_shared<xrpl::STObject const>(s, xrpl::sfMetadata);
        }
        return result;
    } catch (std::exception const& e) {
        std::stringstream txn;
        std::stringstream meta;
        std::ranges::copy(blobs.transaction, std::ostream_iterator<unsigned char>(txn));
        std::ranges::copy(blobs.metadata, std::ostream_iterator<unsigned char>(meta));
        LOG(log.error()) << "Failed to deserialize transaction. txn = " << txn.str()
                         << " - meta = " << meta.str()
                         << " txn length = " << std::to_string(blobs.transaction.size())
                         << " meta length = " << std::to_string(blobs.metadata.size());
        throw e;
    }
}

std::pair<std::shared_ptr<xrpl::STTx const>, std::shared_ptr<xrpl::TxMeta const>>
deserializeTxPlusMeta(data::TransactionAndMetadata const& blobs, std::uint32_t seq)
{
    auto [tx, meta] = deserializeTxPlusMeta(blobs);

    std::shared_ptr<xrpl::TxMeta> const m =
        std::make_shared<xrpl::TxMeta>(tx->getTransactionID(), seq, *meta);

    return {tx, m};
}

boost::json::object
toJson(xrpl::STBase const& obj)
{
    boost::json::value value =
        boost::json::parse(obj.getJson(xrpl::JsonOptions::Values::None).toStyledString());

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
    auto txnJson = rpc::toJson(*txn);
    auto metaJson = rpc::toJson(*meta);
    insertDeliveredAmount(metaJson, txn, meta, blobs.date);
    insertDeliverMaxAlias(txnJson, apiVersion);
    insertMPTIssuanceID(txnJson, txn, metaJson, meta);

    if (nftEnabled == NFTokenjson::ENABLE) {
        json::Value nftJson;
        xrpl::RPC::insertNFTSyntheticInJson(nftJson, txn, *meta);
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
    static constexpr uint32_t kMaxLedgerSeq = 0x0FFF'FFFF;
    static constexpr uint32_t kMaxTxnIndex = 0xFFFF;
    static constexpr uint32_t kMaxNetworkId = 0xFFFF;

    if (ledgerSeq > kMaxLedgerSeq || txnIndex > kMaxTxnIndex || networkId > kMaxNetworkId)
        return {};

    static constexpr uint64_t kCtidPrefix = 0xC000'0000;
    uint64_t const ctidValue = ((kCtidPrefix + static_cast<uint64_t>(ledgerSeq)) << 32) +
        (static_cast<uint64_t>(txnIndex) << 16) + networkId;

    return {fmt::format("{:016X}", ctidValue)};
}

bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<xrpl::STTx const> const& txn,
    std::shared_ptr<xrpl::TxMeta const> const& meta,
    uint32_t date
)
{
    if (canHaveDeliveredAmount(txn, meta)) {
        if (auto amt = getDeliveredAmount(txn, meta, meta->getLgrSeq(), date)) {
            metaJson["delivered_amount"] =
                toBoostJson(amt->getJson(xrpl::JsonOptions::Values::IncludeDate));
        } else {
            metaJson["delivered_amount"] = "unavailable";
        }
        return true;
    }
    return false;
}

/**
 * @brief Get the delivered amount
 *
 * @param meta The metadata
 * @return The mpt_issuance_id or std::nullopt if not available
 */
static std::optional<xrpl::uint192>
getMPTIssuanceID(std::shared_ptr<xrpl::TxMeta const> const& meta)
{
    xrpl::TxMeta const& transactionMeta = *meta;

    for (xrpl::STObject const& node : transactionMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltMPTOKEN_ISSUANCE ||
            node.getFName() != xrpl::sfCreatedNode)
            continue;

        auto const& mptNode = node.peekAtField(xrpl::sfNewFields).downcast<xrpl::STObject>();
        return xrpl::makeMptID(mptNode[xrpl::sfSequence], mptNode[xrpl::sfIssuer]);
    }

    return {};
}

/**
 * @brief Check if transaction has a new MPToken created
 *
 * @param txn The transaction object
 * @param meta The metadata object
 * @return true if the transaction can have a mpt_issuance_id
 */
static bool
canHaveMPTIssuanceID(
    std::shared_ptr<xrpl::STTx const> const& txn,
    std::shared_ptr<xrpl::TxMeta const> const& meta
)
{
    if (txn->getTxnType() != xrpl::ttMPTOKEN_ISSUANCE_CREATE)
        return false;

    return (meta->getResultTER() == xrpl::tesSUCCESS);
}

bool
insertMPTIssuanceID(
    boost::json::object& txnJson,
    std::shared_ptr<xrpl::STTx const> const& txn,
    boost::json::object& metaJson,
    std::shared_ptr<xrpl::TxMeta const> const& meta
)
{
    if (!canHaveMPTIssuanceID(txn, meta))
        return false;

    auto const id = getMPTIssuanceID(meta);
    ASSERT(id.has_value(), "MPTIssuanceID must have value");
    if (!id)
        return false;

    // For mpttokenissuance create, add mpt_issuance_id to metajson
    // Otherwise, add it to txn json
    if (txnJson.contains(JS(TransactionType)) && txnJson.at(JS(TransactionType)).is_string() and
        txnJson.at(JS(TransactionType)).as_string() == JS(MPTokenIssuanceCreate)) {
        metaJson[JS(mpt_issuance_id)] = xrpl::to_string(*id);
    } else {
        txnJson[JS(mpt_issuance_id)] = xrpl::to_string(*id);
    }

    return true;
}

void
insertDeliverMaxAlias(boost::json::object& txJson, std::uint32_t const apiVersion)
{
    if (txJson.contains(JS(TransactionType)) and txJson.at(JS(TransactionType)).is_string() and
        txJson.at(JS(TransactionType)).as_string() == JS(Payment) and txJson.contains(JS(Amount))) {
        txJson.insert_or_assign(JS(DeliverMax), txJson[JS(Amount)]);
        if (apiVersion > 1)
            txJson.erase(JS(Amount));
    }
}

boost::json::object
toJson(xrpl::TxMeta const& meta)
{
    boost::json::value value =
        boost::json::parse(meta.getJson(xrpl::JsonOptions::Values::None).toStyledString());

    return value.as_object();
}

boost::json::value
toBoostJson(json::Value const& value)
{
    boost::json::value boostValue = boost::json::parse(value.toStyledString());

    return boostValue;
}

boost::json::object
toJson(xrpl::SLE const& sle)
{
    boost::json::value value =
        boost::json::parse(sle.getJson(xrpl::JsonOptions::Values::None).toStyledString());
    if (sle.getType() == xrpl::ltACCOUNT_ROOT) {
        if (sle.isFieldPresent(xrpl::sfEmailHash)) {
            auto const& hash = sle.getFieldH128(xrpl::sfEmailHash);
            std::string md5 = strHex(hash);
            boost::algorithm::to_lower(md5);
            value.as_object()["urlgravatar"] =
                str(boost::format("http://www.gravatar.com/avatar/%s") % md5);
        }
    }
    return value.as_object();
}

boost::json::object
toJson(xrpl::LedgerHeader const& lgrInfo, bool const binary, std::uint32_t const apiVersion)
{
    boost::json::object header;
    if (binary) {
        header[JS(ledger_data)] = xrpl::strHex(ledgerHeaderToBlob(lgrInfo));
    } else {
        header[JS(account_hash)] = xrpl::strHex(lgrInfo.accountHash);
        header[JS(close_flags)] = lgrInfo.closeFlags;
        header[JS(close_time)] = lgrInfo.closeTime.time_since_epoch().count();
        header[JS(close_time_human)] = xrpl::to_string(lgrInfo.closeTime);
        header[JS(close_time_resolution)] = lgrInfo.closeTimeResolution.count();
        header[JS(close_time_iso)] = xrpl::toStringIso(lgrInfo.closeTime);
        header[JS(ledger_hash)] = xrpl::strHex(lgrInfo.hash);
        header[JS(parent_close_time)] = lgrInfo.parentCloseTime.time_since_epoch().count();
        header[JS(parent_hash)] = xrpl::strHex(lgrInfo.parentHash);
        header[JS(total_coins)] = xrpl::to_string(lgrInfo.drops);
        header[JS(transaction_hash)] = xrpl::strHex(lgrInfo.txHash);

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

std::expected<xrpl::LedgerHeader, Status>
ledgerHeaderFromRequest(
    std::shared_ptr<data::BackendInterface const> const& backend,
    web::Context const& ctx
)
{
    auto hashValue = ctx.params.contains("ledger_hash") ? ctx.params.at("ledger_hash") : nullptr;

    if (!hashValue.is_null()) {
        if (!hashValue.is_string())
            return std::unexpected{Status{RippledError::RpcInvalidParams, "ledgerHashNotString"}};

        xrpl::uint256 ledgerHash;
        if (!ledgerHash.parseHex(boost::json::value_to<std::string>(hashValue)))
            return std::unexpected{Status{RippledError::RpcInvalidParams, "ledgerHashMalformed"}};

        auto lgrInfo = backend->fetchLedgerByHash(ledgerHash, ctx.yield);

        if (!lgrInfo || lgrInfo->seq > ctx.range.maxSequence)
            return std::unexpected{Status{RippledError::RpcLgrNotFound, "ledgerNotFound"}};

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
        } else if (indexValue.is_int64() or indexValue.is_uint64()) {
            ledgerSequence = util::integralValueAs<uint32_t>(indexValue);
        }
    } else {
        ledgerSequence = ctx.range.maxSequence;
    }

    if (!ledgerSequence)
        return std::unexpected{Status{RippledError::RpcInvalidParams, "ledgerIndexMalformed"}};

    auto lgrInfo = backend->fetchLedgerBySequence(*ledgerSequence, ctx.yield);

    if (!lgrInfo || lgrInfo->seq > ctx.range.maxSequence)
        return std::unexpected{Status{RippledError::RpcLgrNotFound, "ledgerNotFound"}};

    return *lgrInfo;
}

// extract ledgerHeaderFromRequest's parameter from context
std::expected<xrpl::LedgerHeader, Status>
getLedgerHeaderFromHashOrSeq(
    BackendInterface const& backend,
    boost::asio::yield_context yield,
    std::optional<std::string> ledgerHash,
    std::optional<uint32_t> ledgerIndex,
    uint32_t maxSeq
)
{
    std::optional<xrpl::LedgerHeader> lgrInfo;
    auto const err = std::unexpected{Status{RippledError::RpcLgrNotFound, "ledgerNotFound"}};
    if (ledgerHash) {
        // invoke uint256's constructor to parse the hex string , instead of
        // copying buffer
        xrpl::uint256 const ledgerHash256{std::string_view(*ledgerHash)};
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
ledgerHeaderToBlob(xrpl::LedgerHeader const& info, bool includeHash)
{
    xrpl::Serializer s;
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
getStartHint(xrpl::SLE const& sle, xrpl::AccountID const& accountID)
{
    if (sle.getType() == xrpl::ltRIPPLE_STATE) {
        if (sle.getFieldAmount(xrpl::sfLowLimit).getIssuer() == accountID) {
            return sle.getFieldU64(xrpl::sfLowNode);
        }
        if (sle.getFieldAmount(xrpl::sfHighLimit).getIssuer() == accountID)
            return sle.getFieldU64(xrpl::sfHighNode);
    }

    if (!sle.isFieldPresent(xrpl::sfOwnerNode))
        return 0;

    return sle.getFieldU64(xrpl::sfOwnerNode);
}

// traverse account's nfts
// return Status if error occurs
// return [nextpage, count of nft already found] if success
std::expected<AccountCursor, Status>
traverseNFTObjects(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& accountID,
    xrpl::uint256 nextPage,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(xrpl::SLE)> atOwnedNode
)
{
    auto const firstNFTPage = xrpl::keylet::nftpageMin(accountID);
    auto const lastNFTPage = xrpl::keylet::nftpageMax(accountID);

    // check if nextPage is valid
    if (nextPage != beast::kZero and firstNFTPage.key != (nextPage & ~xrpl::nft::kPageMask))
        return std::unexpected{Status{RippledError::RpcInvalidParams, "Invalid marker."}};

    // no marker, start from the last page
    xrpl::uint256 const currentPage = nextPage == beast::kZero ? lastNFTPage.key : nextPage;

    // read the current page
    auto page = backend.fetchLedgerObject(currentPage, sequence, yield);

    if (!page) {
        if (nextPage == beast::kZero) {  // no nft objects in lastNFTPage
            return AccountCursor{.index = beast::kZero, .hint = 0};
        }
        // marker is in the right range, but still invalid
        return std::unexpected{Status{RippledError::RpcInvalidParams, "Invalid marker."}};
    }

    // the object exists and the key is in right range, must be nft page
    xrpl::SLE pageSLE{xrpl::SerialIter{page->data(), page->size()}, currentPage};

    auto count = 0u;
    // traverse the nft page linked list until the start of the list or reach the limit
    while (true) {
        auto const nftPreviousPage = pageSLE.getFieldH256(xrpl::sfPreviousPageMin);
        atOwnedNode(std::move(pageSLE));
        count++;

        if (count == limit or nftPreviousPage == beast::kZero)
            return AccountCursor{.index = nftPreviousPage, .hint = count};

        page = backend.fetchLedgerObject(nftPreviousPage, sequence, yield);
        if (!page)
            break;
        pageSLE = xrpl::SLE{xrpl::SerialIter{page->data(), page->size()}, nftPreviousPage};
    }

    return AccountCursor{.index = beast::kZero, .hint = 0};
}

std::expected<AccountCursor, Status>
traverseOwnedNodes(
    BackendInterface const& backend,
    xrpl::AccountID const& accountID,
    std::uint32_t sequence,
    std::uint32_t limit,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context yield,
    std::function<void(xrpl::SLE)> atOwnedNode,
    bool nftIncluded
)
{
    auto const maybeCursor = parseAccountCursor(jsonCursor);

    if (!maybeCursor)
        return std::unexpected{Status{RippledError::RpcInvalidParams, "Malformed cursor."}};

    // the format is checked in RPC framework level
    auto [hexCursor, startHint] = *maybeCursor;

    auto const isNftMarkerNonZero =
        startHint == std::numeric_limits<uint32_t>::max() and hexCursor != beast::kZero;
    auto const isNftMarkerZero =
        startHint == std::numeric_limits<uint32_t>::max() and hexCursor == beast::kZero;
    // if we need to traverse nft objects and this is the first request -> traverse nft objects
    // if we need to traverse nft objects and the marker is still in nft page -> traverse nft
    // objects if we need to traverse nft objects and the marker is still in nft page but next page
    // is zero -> owned nodes if we need to traverse nft objects and the marker is not in nft page
    // -> traverse owned nodes
    if (nftIncluded and (!jsonCursor or isNftMarkerNonZero)) {
        auto const cursorMaybe =
            traverseNFTObjects(backend, sequence, accountID, hexCursor, limit, yield, atOwnedNode);

        if (!cursorMaybe.has_value())
            return cursorMaybe;

        auto const [nextNFTPage, nftsCount] = cursorMaybe.value();

        // if limit reach , we return the next page and max as marker
        if (nftsCount >= limit) {
            return AccountCursor{
                .index = nextNFTPage, .hint = std::numeric_limits<uint32_t>::max()
            };
        }

        // adjust limit ,continue traversing owned nodes
        limit -= nftsCount;
        hexCursor = beast::kZero;
        startHint = 0;
    } else if (nftIncluded and isNftMarkerZero) {
        // the last request happen to fetch all the nft, adjust marker to continue traversing owned
        // nodes
        hexCursor = beast::kZero;
        startHint = 0;
    }

    return traverseOwnedNodes(
        backend,
        xrpl::keylet::ownerDir(accountID),
        hexCursor,
        startHint,
        sequence,
        limit,
        yield,
        atOwnedNode
    );
}

std::expected<AccountCursor, Status>
traverseOwnedNodes(
    BackendInterface const& backend,
    xrpl::Keylet const& owner,
    xrpl::uint256 const& hexMarker,
    std::uint32_t const startHint,
    std::uint32_t sequence,
    std::uint32_t limit,
    boost::asio::yield_context yield,
    std::function<void(xrpl::SLE)> atOwnedNode
)
{
    auto cursor = AccountCursor({.index = beast::kZero, .hint = 0});

    auto const rootIndex = owner;
    auto currentIndex = rootIndex;
    // track the current page we are accessing, will return it as the next hint
    auto currentPage = startHint;

    std::vector<xrpl::uint256> keys;
    // Only reserve 2048 nodes when fetching all owned ledger objects. If there
    // are more, then keys will allocate more memory, which is suboptimal, but
    // should only occur occasionally.
    static constexpr std::uint32_t kMinNodes = 2048;
    keys.reserve(std::min(kMinNodes, limit));

    auto start = std::chrono::system_clock::now();

    // If startAfter is not zero try jumping to that page using the hint
    if (hexMarker.isNonZero()) {
        auto const hintIndex = xrpl::keylet::page(rootIndex, startHint);
        auto hintDir = backend.fetchLedgerObject(hintIndex.key, sequence, yield);

        if (!hintDir)
            return std::unexpected{Status(xrpl::RpcInvalidParams, "Invalid marker.")};

        xrpl::SerialIter hintDirIt{hintDir->data(), hintDir->size()};
        xrpl::SLE const hintDirSle{hintDirIt, hintIndex.key};

        if (auto const& indexes = hintDirSle.getFieldV256(xrpl::sfIndexes);
            std::ranges::find(indexes, hexMarker) == std::end(indexes)) {
            // the index specified by marker is not in the page specified by marker
            return std::unexpected{Status(xrpl::RpcInvalidParams, "Invalid marker.")};
        }

        currentIndex = hintIndex;
        bool found = false;
        for (;;) {
            auto const ownerDir = backend.fetchLedgerObject(currentIndex.key, sequence, yield);

            if (!ownerDir) {
                return std::unexpected{
                    Status(xrpl::RpcInvalidParams, "Owner directory not found.")
                };
            }

            xrpl::SerialIter ownedDirIt{ownerDir->data(), ownerDir->size()};
            xrpl::SLE const ownedDirSle{ownedDirIt, currentIndex.key};

            for (auto const& key : ownedDirSle.getFieldV256(xrpl::sfIndexes)) {
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
                cursor = AccountCursor({.index = keys.back(), .hint = currentPage});
                break;
            }
            // the next page
            auto const uNodeNext = ownedDirSle.getFieldU64(xrpl::sfIndexNext);
            if (uNodeNext == 0)
                break;

            currentIndex = xrpl::keylet::page(rootIndex, uNodeNext);
            currentPage = uNodeNext;
        }
    } else {
        for (;;) {
            auto const ownerDir = backend.fetchLedgerObject(currentIndex.key, sequence, yield);

            if (!ownerDir)
                break;

            xrpl::SerialIter ownedDirIt{ownerDir->data(), ownerDir->size()};
            xrpl::SLE const ownedDirSle{ownedDirIt, currentIndex.key};

            for (auto const& key : ownedDirSle.getFieldV256(xrpl::sfIndexes)) {
                keys.push_back(key);

                if (--limit == 0)
                    break;
            }

            if (limit == 0) {
                cursor = AccountCursor({.index = keys.back(), .hint = currentPage});
                break;
            }

            auto const uNodeNext = ownedDirSle.getFieldU64(xrpl::sfIndexNext);
            if (uNodeNext == 0)
                break;

            currentIndex = xrpl::keylet::page(rootIndex, uNodeNext);
            currentPage = uNodeNext;
        }
    }
    auto end = std::chrono::system_clock::now();

    static util::Logger const log{"RPC"};  // NOLINT(readability-identifier-naming)

    LOG(log.debug()) << fmt::format(
        "Time loading owned directories: {} milliseconds, entries size: {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
        keys.size()
    );

    auto [objects, timeDiff] =
        util::timed([&]() { return backend.fetchLedgerObjects(keys, sequence, yield); });

    LOG(log.debug()) << "Time loading owned entries: " << timeDiff << " milliseconds";

    for (auto i = 0u; i < objects.size(); ++i) {
        xrpl::SerialIter it{objects[i].data(), objects[i].size()};
        atOwnedNode(xrpl::SLE{it, keys[i]});
    }

    if (limit == 0)
        return cursor;

    return AccountCursor({.index = beast::kZero, .hint = 0});
}

std::shared_ptr<xrpl::SLE const>
read(
    std::shared_ptr<data::BackendInterface const> const& backend,
    xrpl::Keylet const& keylet,
    xrpl::LedgerHeader const& lgrInfo,
    web::Context const& context
)
{
    if (auto const blob = backend->fetchLedgerObject(keylet.key, lgrInfo.seq, context.yield);
        blob) {
        return std::make_shared<xrpl::SLE const>(
            xrpl::SerialIter{blob->data(), blob->size()}, keylet.key
        );
    }

    return nullptr;
}

std::optional<xrpl::Seed>
parseRippleLibSeed(boost::json::value const& value)
{
    // ripple-lib encodes seed used to generate an Ed25519 wallet in a
    // non-standard way. While rippled never encode seeds that way, we
    // try to detect such keys to avoid user confusion.
    if (!value.is_string())
        return {};

    auto const result =
        xrpl::decodeBase58Token(boost::json::value_to<std::string>(value), xrpl::TokenType::None);

    static constexpr std::size_t kSeedSize = 18;
    static constexpr std::array<std::uint8_t, 2> kSeedPrefix = {0xE1, 0x4B};
    if (result.size() == kSeedSize && static_cast<std::uint8_t>(result[0]) == kSeedPrefix[0] &&
        static_cast<std::uint8_t>(result[1]) == kSeedPrefix[1])
        return xrpl::Seed(xrpl::makeSlice(result.substr(2)));

    return {};
}

std::vector<xrpl::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction)
{
    std::vector<xrpl::AccountID> accounts = {};
    for (auto const& [key, value] : transaction) {
        if (value.is_object()) {
            auto inObject = getAccountsFromTransaction(value.as_object());
            accounts.insert(accounts.end(), inObject.begin(), inObject.end());
        } else if (value.is_string()) {
            auto const account = util::parseBase58Wrapper<xrpl::AccountID>(
                boost::json::value_to<std::string>(value)
            );
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
    xrpl::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    if (xrpl::isXRP(issuer))
        return false;

    auto key = xrpl::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return false;

    xrpl::SerialIter it{blob->data(), blob->size()};
    xrpl::SLE const sle{it, key};

    return sle.isFlag(xrpl::lsfGlobalFreeze);
}

bool
fetchAndCheckAnyFlagsExists(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::Keylet const& keylet,
    std::vector<std::uint32_t> const& flags,
    boost::asio::yield_context yield
)
{
    auto const blob = backend.fetchLedgerObject(keylet.key, sequence, yield);

    if (!blob)
        return false;

    xrpl::SerialIter it{blob->data(), blob->size()};
    xrpl::SLE const sle{it, keylet.key};

    return std::ranges::any_of(flags, [sle](std::uint32_t flag) { return sle.isFlag(flag); });
}

bool
isFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& account,
    xrpl::Currency const& currency,
    xrpl::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    if (xrpl::isXRP(currency))
        return false;

    if (fetchAndCheckAnyFlagsExists(
            backend, sequence, xrpl::keylet::account(issuer), {xrpl::lsfGlobalFreeze}, yield
        ))
        return true;

    auto const trustLineKeylet = xrpl::keylet::line(account, issuer, currency);
    return issuer != account &&
        fetchAndCheckAnyFlagsExists(
               backend,
               sequence,
               trustLineKeylet,
               {(issuer > account) ? xrpl::lsfHighFreeze : xrpl::lsfLowFreeze},
               yield
        );
}

bool
isDeepFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& account,
    xrpl::Currency const& currency,
    xrpl::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    if (xrpl::isXRP(currency))
        return false;

    if (issuer == account)
        return false;

    auto const trustLineKeylet = xrpl::keylet::line(account, issuer, currency);

    return fetchAndCheckAnyFlagsExists(
        backend, sequence, trustLineKeylet, {xrpl::lsfHighDeepFreeze, xrpl::lsfLowDeepFreeze}, yield
    );
}

bool
isLPTokenFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& account,
    xrpl::Issue const& asset,
    xrpl::Issue const& asset2,
    boost::asio::yield_context yield
)
{
    return isFrozen(backend, sequence, account, asset.currency, asset.account, yield) ||
        isFrozen(backend, sequence, account, asset2.currency, asset2.account, yield);
}

xrpl::XRPAmount
xrpLiquid(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& id,
    boost::asio::yield_context yield
)
{
    auto const key = xrpl::keylet::account(id).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return beast::kZero;

    xrpl::SerialIter it{blob->data(), blob->size()};
    xrpl::SLE const sle{it, key};

    std::uint32_t const ownerCount = sle.getFieldU32(xrpl::sfOwnerCount);

    auto balance = sle.getFieldAmount(xrpl::sfBalance);

    xrpl::STAmount const amount = [&]() {
        // AMM doesn't require the reserves
        if ((sle.getFlags() & xrpl::lsfAMMNode) != 0u)
            return balance;
        auto const reserve = backend.fetchFees(sequence, yield)->accountReserve(ownerCount);
        xrpl::STAmount amount = balance - reserve;
        if (balance < reserve)
            amount.clear();
        return amount;
    }();

    return amount.xrp();
}

xrpl::STAmount
accountFunds(
    BackendInterface const& backend,
    data::AmendmentCenterInterface const& amendmentCenter,
    std::uint32_t const sequence,
    xrpl::STAmount const& amount,
    xrpl::AccountID const& id,
    boost::asio::yield_context yield
)
{
    if (!amount.native() && amount.getIssuer() == id) {
        return amount;
    }

    return accountHolds(
        backend,
        amendmentCenter,
        sequence,
        id,
        amount.get<xrpl::Issue>().currency,
        amount.getIssuer(),
        true,
        yield
    );
}

xrpl::STAmount
ammAccountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& account,
    xrpl::Currency const& currency,
    xrpl::AccountID const& issuer,
    bool const zeroIfFrozen,
    boost::asio::yield_context yield
)
{
    xrpl::STAmount amount;
    ASSERT(!xrpl::isXRP(currency), "LPToken currency can never be XRP");
    if (xrpl::isXRP(currency))
        return {xrpLiquid(backend, sequence, account, yield)};

    auto const key = xrpl::keylet::line(account, issuer, currency).key;
    auto const blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob) {
        amount.setIssue(xrpl::Issue(currency, issuer));
        amount.clear();
        return amount;
    }

    xrpl::SerialIter it{blob->data(), blob->size()};
    xrpl::SLE const sle{it, key};

    if (zeroIfFrozen &&
        (isFrozen(backend, sequence, account, currency, issuer, yield) ||
         isDeepFrozen(backend, sequence, account, currency, issuer, yield))) {
        amount.setIssue(xrpl::Issue(currency, issuer));
        amount.clear();
    } else {
        amount = sle.getFieldAmount(xrpl::sfBalance);
        if (account > issuer) {
            // Put balance in account terms.
            amount.negate();
        }
        amount.setIssue(xrpl::Issue{amount.get<xrpl::Issue>().currency, issuer});
    }

    return amount;
}

xrpl::STAmount
accountHolds(
    BackendInterface const& backend,
    data::AmendmentCenterInterface const& amendmentCenter,
    std::uint32_t sequence,
    xrpl::AccountID const& account,
    xrpl::Currency const& currency,
    xrpl::AccountID const& issuer,
    bool const zeroIfFrozen,
    boost::asio::yield_context yield
)
{
    xrpl::STAmount amount;
    if (xrpl::isXRP(currency))
        return {xrpLiquid(backend, sequence, account, yield)};

    auto const key = xrpl::keylet::line(account, issuer, currency).key;
    auto const blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob) {
        amount.setIssue(xrpl::Issue(currency, issuer));
        amount.clear();
        return amount;
    }

    auto const allowBalance = [&]() {
        if (!zeroIfFrozen)
            return true;

        if (isFrozen(backend, sequence, account, currency, issuer, yield))
            return false;

        if (amendmentCenter.isEnabled(
                yield, data::Amendments::fixFrozenLPTokenTransfer, sequence
            )) {
            auto const issuerBlob =
                backend.fetchLedgerObject(xrpl::keylet::account(issuer).key, sequence, yield);

            if (!issuerBlob)
                return false;

            xrpl::SLE const issuerSle{
                xrpl::SerialIter{issuerBlob->data(), issuerBlob->size()},
                xrpl::keylet::account(issuer).key
            };

            // if the issuer is an amm account, then currency is lptoken, so we will need to check
            // if the assets in the pool are frozen as well
            if (issuerSle.isFieldPresent(xrpl::sfAMMID)) {
                auto const ammKeylet = xrpl::keylet::amm(issuerSle[xrpl::sfAMMID]);
                auto const ammBlob = backend.fetchLedgerObject(ammKeylet.key, sequence, yield);

                if (!ammBlob)
                    return false;

                xrpl::SLE const ammSle{
                    xrpl::SerialIter{ammBlob->data(), ammBlob->size()}, ammKeylet.key
                };

                return !isLPTokenFrozen(
                    backend,
                    sequence,
                    account,
                    ammSle[xrpl::sfAsset].get<xrpl::Issue>(),
                    ammSle[xrpl::sfAsset2].get<xrpl::Issue>(),
                    yield
                );
            }
        }

        return true;
    }();

    if (allowBalance) {
        xrpl::SerialIter it{blob->data(), blob->size()};
        xrpl::SLE const sle{it, key};

        amount = sle.getFieldAmount(xrpl::sfBalance);
        if (account > issuer) {
            // Put balance in account terms.
            amount.negate();
        }
        amount.setIssue(xrpl::Issue{amount.get<xrpl::Issue>().currency, issuer});
    } else {
        amount.setIssue(xrpl::Issue(currency, issuer));
        amount.clear();
    }

    return amount;
}

xrpl::Rate
transferRate(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::AccountID const& issuer,
    boost::asio::yield_context yield
)
{
    auto key = xrpl::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (blob) {
        xrpl::SerialIter it{blob->data(), blob->size()};
        xrpl::SLE const sle{it, key};

        if (sle.isFieldPresent(xrpl::sfTransferRate))
            return xrpl::Rate{sle.getFieldU32(xrpl::sfTransferRate)};
    }

    return xrpl::kParityRate;
}

boost::json::array
postProcessOrderBook(
    std::vector<data::LedgerObject> const& offers,
    xrpl::Book const& book,
    xrpl::AccountID const& takerID,
    data::BackendInterface const& backend,
    data::AmendmentCenterInterface const& amendmentCenter,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context yield
)
{
    boost::json::array jsonOffers;

    std::map<xrpl::AccountID, xrpl::STAmount> umBalance;

    bool const globalFreeze =
        isGlobalFrozen(backend, ledgerSequence, book.out.getIssuer(), yield) ||
        isGlobalFrozen(backend, ledgerSequence, book.in.getIssuer(), yield);

    auto rate = transferRate(backend, ledgerSequence, book.out.getIssuer(), yield);

    for (auto const& obj : offers) {
        try {
            xrpl::SerialIter it{obj.blob.data(), obj.blob.size()};
            xrpl::SLE const offer{it, obj.key};
            xrpl::uint256 const bookDir = offer.getFieldH256(xrpl::sfBookDirectory);

            auto const uOfferOwnerID = offer.getAccountID(xrpl::sfAccount);
            auto const& saTakerGets = offer.getFieldAmount(xrpl::sfTakerGets);
            auto const& saTakerPays = offer.getFieldAmount(xrpl::sfTakerPays);
            xrpl::STAmount saOwnerFunds;
            bool firstOwnerOffer = true;

            if (book.out.getIssuer() == uOfferOwnerID) {
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
                        backend,
                        amendmentCenter,
                        ledgerSequence,
                        uOfferOwnerID,
                        book.out.get<xrpl::Issue>().currency,
                        book.out.getIssuer(),
                        true,
                        yield
                    );

                    if (saOwnerFunds < beast::kZero)
                        saOwnerFunds.clear();
                }
            }

            boost::json::object offerJson = toJson(offer);

            xrpl::STAmount saTakerGetsFunded;
            xrpl::STAmount saOwnerFundsLimit = saOwnerFunds;
            xrpl::Rate offerRate = xrpl::kParityRate;
            xrpl::STAmount const dirRate = xrpl::amountFromQuality(getQuality(bookDir));

            if (rate != xrpl::kParityRate
                // Have a transfer fee.
                && takerID != book.out.getIssuer()
                // Not taking offers of own IOUs.
                && book.out.getIssuer() != uOfferOwnerID)
            // Offer owner not issuing ownfunds
            {
                // Need to charge a transfer fee to offer owner.
                offerRate = rate;
                saOwnerFundsLimit = xrpl::divide(saOwnerFunds, offerRate);
            }

            if (saOwnerFundsLimit >= saTakerGets) {
                // Sufficient funds no shenanigans.
                saTakerGetsFunded = saTakerGets;
            } else {
                saTakerGetsFunded = saOwnerFundsLimit;
                offerJson["taker_gets_funded"] =
                    toBoostJson(saTakerGetsFunded.getJson(xrpl::JsonOptions::Values::None));
                offerJson["taker_pays_funded"] = toBoostJson(
                    std::min(
                        saTakerPays,
                        xrpl::multiply(saTakerGetsFunded, dirRate, saTakerPays.get<xrpl::Issue>())
                    )
                        .getJson(xrpl::JsonOptions::Values::None)
                );
            }

            xrpl::STAmount const saOwnerPays = (xrpl::kParityRate == offerRate)
                ? saTakerGetsFunded
                : std::min(saOwnerFunds, xrpl::multiply(saTakerGetsFunded, offerRate));

            umBalance[uOfferOwnerID] = saOwnerFunds - saOwnerPays;

            if (firstOwnerOffer)
                offerJson["owner_funds"] = saOwnerFunds.getText();

            offerJson["quality"] = dirRate.getText();

            jsonOffers.push_back(offerJson);
        } catch (std::exception const& e) {
            util::Logger const log{"RPC"};
            LOG(log.error()) << "caught exception: " << e.what();
        }
    }
    return jsonOffers;
}

// get book via currency type
std::expected<xrpl::Book, Status>
parseBook(
    xrpl::Currency pays,
    xrpl::AccountID payIssuer,
    xrpl::Currency gets,
    xrpl::AccountID getIssuer,
    std::optional<std::string> const& domain
)
{
    if (isXRP(pays) && !isXRP(payIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcSrcIsrMalformed,
            "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        }};
    }

    if (!isXRP(pays) && isXRP(payIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcSrcIsrMalformed,
            "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        }};
    }

    if (xrpl::isXRP(gets) && !xrpl::isXRP(getIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcDstIsrMalformed,
            "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        }};
    }

    if (!xrpl::isXRP(gets) && xrpl::isXRP(getIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcDstIsrMalformed,
            "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        }};
    }

    if (pays == gets && payIssuer == getIssuer)
        return std::unexpected{Status{RippledError::RpcBadMarket, "badMarket"}};

    std::optional<xrpl::uint256> domainID = std::nullopt;
    if (domain.has_value()) {
        xrpl::uint256 dom;
        if (!dom.parseHex(*domain))
            return std::unexpected{Status{RippledError::RpcDomainMalformed}};
        domainID = dom;
    }

    return xrpl::Book{xrpl::Issue{pays, payIssuer}, xrpl::Issue{gets, getIssuer}, domainID};
}

std::expected<xrpl::Book, Status>
parseBook(boost::json::object const& request)
{
    if (!request.contains("taker_pays")) {
        return std::unexpected{
            Status{RippledError::RpcInvalidParams, "Missing field 'taker_pays'"}
        };
    }

    if (!request.contains("taker_gets")) {
        return std::unexpected{
            Status{RippledError::RpcInvalidParams, "Missing field 'taker_gets'"}
        };
    }

    if (!request.at("taker_pays").is_object()) {
        return std::unexpected{
            Status{RippledError::RpcInvalidParams, "Field 'taker_pays' is not an object"}
        };
    }

    if (!request.at("taker_gets").is_object()) {
        return std::unexpected{
            Status{RippledError::RpcInvalidParams, "Field 'taker_gets' is not an object"}
        };
    }

    auto takerPays = request.at("taker_pays").as_object();
    if (!takerPays.contains("currency"))
        return std::unexpected{Status{RippledError::RpcSrcCurMalformed}};

    if (!takerPays.at("currency").is_string())
        return std::unexpected{Status{RippledError::RpcSrcCurMalformed}};

    auto takerGets = request.at("taker_gets").as_object();
    if (!takerGets.contains("currency"))
        return std::unexpected{Status{RippledError::RpcDstAmtMalformed}};

    if (!takerGets.at("currency").is_string()) {
        return std::unexpected{Status{
            RippledError::RpcDstAmtMalformed,
        }};
    }

    if (request.contains("domain") && !request.at("domain").is_string())
        return std::unexpected{Status{RippledError::RpcDomainMalformed}};

    xrpl::Currency payCurrency;
    if (!xrpl::toCurrency(
            payCurrency, boost::json::value_to<std::string>(takerPays.at("currency"))
        ))
        return std::unexpected{Status{RippledError::RpcSrcCurMalformed}};

    xrpl::Currency getCurrency;
    if (!xrpl::toCurrency(getCurrency, boost::json::value_to<std::string>(takerGets["currency"])))
        return std::unexpected{Status{RippledError::RpcDstAmtMalformed}};

    xrpl::AccountID payIssuer;
    if (takerPays.contains("issuer")) {
        if (!takerPays.at("issuer").is_string()) {
            return std::unexpected{
                Status{RippledError::RpcInvalidParams, "takerPaysIssuerNotString"}
            };
        }

        if (!xrpl::toIssuer(payIssuer, boost::json::value_to<std::string>(takerPays.at("issuer"))))
            return std::unexpected{Status{RippledError::RpcSrcIsrMalformed}};

        if (payIssuer == xrpl::noAccount())
            return std::unexpected{Status{RippledError::RpcSrcIsrMalformed}};
    } else {
        payIssuer = xrpl::xrpAccount();
    }

    if (isXRP(payCurrency) && !isXRP(payIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcSrcIsrMalformed,
            "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        }};
    }

    if (!isXRP(payCurrency) && isXRP(payIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcSrcIsrMalformed,
            "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        }};
    }

    if ((!isXRP(payCurrency)) && (!takerPays.contains("issuer"))) {
        return std::unexpected{Status{RippledError::RpcSrcIsrMalformed, "Missing non-XRP issuer."}};
    }

    xrpl::AccountID getIssuer;

    if (takerGets.contains("issuer")) {
        if (!takerGets["issuer"].is_string()) {
            return std::unexpected{
                Status{RippledError::RpcInvalidParams, "taker_gets.issuer should be string"}
            };
        }

        if (!xrpl::toIssuer(
                getIssuer, boost::json::value_to<std::string>(takerGets.at("issuer"))
            )) {
            return std::unexpected{Status{
                RippledError::RpcDstIsrMalformed, "Invalid field 'taker_gets.issuer', bad issuer."
            }};
        }

        if (getIssuer == xrpl::noAccount()) {
            return std::unexpected{Status{
                RippledError::RpcDstIsrMalformed,
                "Invalid field 'taker_gets.issuer', bad issuer account one."
            }};
        }
    } else {
        getIssuer = xrpl::xrpAccount();
    }

    if (xrpl::isXRP(getCurrency) && !xrpl::isXRP(getIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcDstIsrMalformed,
            "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        }};
    }

    if (!xrpl::isXRP(getCurrency) && xrpl::isXRP(getIssuer)) {
        return std::unexpected{Status{
            RippledError::RpcDstIsrMalformed,
            "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        }};
    }

    if (payCurrency == getCurrency && payIssuer == getIssuer)
        return std::unexpected{Status{RippledError::RpcBadMarket, "badMarket"}};

    std::optional<xrpl::uint256> domainID;
    if (request.contains("domain")) {
        xrpl::uint256 dom;
        if (!dom.parseHex(boost::json::value_to<std::string>(request.at("domain"))))
            return std::unexpected{Status{RippledError::RpcDomainMalformed}};
        domainID = dom;
    }

    return xrpl::Book{
        xrpl::Issue{payCurrency, payIssuer}, xrpl::Issue{getCurrency, getIssuer}, domainID
    };
}

std::expected<xrpl::AccountID, Status>
parseTaker(boost::json::value const& taker)
{
    std::optional<xrpl::AccountID> takerID = {};
    if (!taker.is_string())
        return std::unexpected{Status{RippledError::RpcInvalidParams, "takerNotString"}};

    takerID = accountFromStringStrict(boost::json::value_to<std::string>(taker));

    if (!takerID)
        return std::unexpected{Status{RippledError::RpcBadIssuer, "invalidTakerAccount"}};
    return *takerID;
}

xrpl::Issue
parseIssue(boost::json::object const& issue)
{
    json::Value jv;
    if (issue.contains(JS(issuer)) && issue.at(JS(issuer)).is_string())
        jv["issuer"] = boost::json::value_to<std::string>(issue.at(JS(issuer)));
    if (issue.contains(JS(currency)) && issue.at(JS(currency)).is_string())
        jv["currency"] = boost::json::value_to<std::string>(issue.at(JS(currency)));

    return xrpl::issueFromJson(jv);
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
    if (method == JS(ledger)) {
        auto const requestStr = boost::json::serialize(request);
        json::Value jv;
        json::Reader{}.parse(requestStr, jv);
        // rippled considers string/non-zero int/non-empty array/ non-empty json as true.
        // Use rippled's API asBool to get the same result.
        // https://github.com/XRPLF/rippled/issues/5119
        auto const isFieldSet = [&jv](auto const field) {
            return jv.isMember(field) and jv[field].asBool();
        };

        // According to doc
        // https://xrpl.org/docs/references/http-websocket-apis/public-api-methods/ledger-methods/ledger,
        // full/accounts/type are admin only, but type only works when full/accounts are set, so we
        // don't need to check type.
        if (isFieldSet(JS(full)) or isFieldSet(JS(accounts)))
            return true;
    }

    if (method == JS(feature) and request.contains(JS(vetoed)))
        return true;
    return false;
}

std::expected<xrpl::uint256, Status>
getNFTID(boost::json::object const& request)
{
    if (!request.contains(JS(nft_id)))
        return std::unexpected{Status{RippledError::RpcInvalidParams, "missingTokenID"}};

    if (!request.at(JS(nft_id)).is_string())
        return std::unexpected{Status{RippledError::RpcInvalidParams, "tokenIDNotString"}};

    xrpl::uint256 tokenid;
    if (!tokenid.parseHex(boost::json::value_to<std::string>(request.at(JS(nft_id)))))
        return std::unexpected{Status{RippledError::RpcInvalidParams, "malformedTokenID"}};

    return tokenid;
}

boost::json::object
toJsonWithBinaryTx(data::TransactionAndMetadata const& txnPlusMeta, std::uint32_t const apiVersion)
{
    boost::json::object obj{};
    auto const metaKey = apiVersion > 1 ? JS(meta_blob) : JS(meta);
    obj[metaKey] = xrpl::strHex(txnPlusMeta.metadata);
    obj[JS(tx_blob)] = xrpl::strHex(txnPlusMeta.transaction);
    return obj;
}

}  // namespace rpc
