
#ifndef XRPL_REPORTING_RPCHELPERS_H_INCLUDED
#define XRPL_REPORTING_RPCHELPERS_H_INCLUDED

#include <ripple/app/ledger/Ledger.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <backend/BackendInterface.h>
#include <handlers/Context.h>
#include <handlers/Status.h>

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account);

std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(Backend::TransactionAndMetadata const& blobs);

std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(
    Backend::TransactionAndMetadata const& blobs,
    std::uint32_t seq);

std::pair<boost::json::object, boost::json::object>
toExpandedJson(Backend::TransactionAndMetadata const& blobs);

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

std::variant<RPC::Status, ripple::LedgerInfo>
ledgerInfoFromRequest(RPC::Context const& ctx);

std::optional<ripple::uint256>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    ripple::uint256 const& cursor,
    std::function<bool(ripple::SLE)> atOwnedNode);

std::variant<RPC::Status, std::pair<ripple::PublicKey, ripple::SecretKey>>
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
accountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer);

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

#endif
