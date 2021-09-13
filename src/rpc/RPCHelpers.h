
#ifndef XRPL_REPORTING_RPCHELPERS_H_INCLUDED
#define XRPL_REPORTING_RPCHELPERS_H_INCLUDED
/*
 * This file contains a variety of utility functions used when executing
 * the handlers
 */

#include <ripple/app/ledger/Ledger.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/jss.h>
#include <backend/BackendInterface.h>
#include <rpc/RPC.h>

namespace RPC {
std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account);
std::optional<ripple::AccountID>
accountFromSeed(std::string const& account);

// TODO this function should probably be in a different file and namespace
std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(Backend::TransactionAndMetadata const& blobs);

// TODO this function should probably be in a different file and namespace
std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(
    Backend::TransactionAndMetadata const& blobs,
    std::uint32_t seq);

std::pair<boost::json::object, boost::json::object>
toExpandedJson(Backend::TransactionAndMetadata const& blobs);

bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta);

boost::json::object
toJson(ripple::STBase const& obj);

boost::json::object
toJson(ripple::SLE const& sle);

boost::json::object
toJson(ripple::LedgerInfo const& info);

boost::json::object
toJson(ripple::TxMeta const& meta);

using RippledJson = Json::Value;
boost::json::value
toBoostJson(RippledJson const& value);

boost::json::object
generatePubLedgerMessage(
    ripple::LedgerInfo const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    uint32_t txnCount);

std::variant<Status, ripple::LedgerInfo>
ledgerInfoFromRequest(Context const& ctx);

std::optional<ripple::uint256>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    ripple::uint256 const& cursor,
    std::function<bool(ripple::SLE)> atOwnedNode);

std::variant<Status, std::pair<ripple::PublicKey, ripple::SecretKey>>
keypairFromRequst(boost::json::object const& request);

std::vector<ripple::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction);

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerInfo const& info);

bool
isGlobalFrozen(
    BackendInterface const& backend,
    std::uint32_t seq,
    ripple::AccountID const& issuer);

bool
isFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer);

ripple::STAmount
accountFunds(
    BackendInterface const& backend,
    uint32_t sequence,
    ripple::STAmount const& amount,
    ripple::AccountID const& id);

ripple::STAmount
accountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    bool zeroIfFrozen = false);

ripple::Rate
transferRate(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer);

ripple::XRPAmount
xrpLiquid(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& id);
boost::json::array
postProcessOrderBook(
    std::vector<Backend::LedgerObject> const& offers,
    ripple::Book const& book,
    ripple::AccountID const& takerID,
    Backend::BackendInterface const& backend,
    uint32_t ledgerSequence);

std::variant<Status, ripple::Book>
parseBook(boost::json::object const& request);

std::variant<Status, ripple::AccountID>
parseTaker(boost::json::value const& request);

std::optional<uint32_t>
getUInt(boost::json::object const& request, std::string const& field);

uint32_t
getUInt(
    boost::json::object const& request,
    std::string const& field,
    uint32_t dfault);

uint32_t
getRequiredUInt(boost::json::object const& request, std::string const& field);

std::optional<bool>
getBool(boost::json::object const& request, std::string const& field);

bool
getBool(
    boost::json::object const& request,
    std::string const& field,
    bool dfault);

bool
getRequiredBool(boost::json::object const& request, std::string const& field);

std::optional<std::string>
getString(boost::json::object const& request, std::string const& field);

std::string
getRequiredString(boost::json::object const& request, std::string const& field);

std::string
getString(
    boost::json::object const& request,
    std::string const& field,
    std::string dfault);
}  // namespace RPC
#endif
