
#ifndef XRPL_REPORTING_RPCHELPERS_H_INCLUDED
#define XRPL_REPORTING_RPCHELPERS_H_INCLUDED

#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STTx.h>
#include <boost/json.hpp>
#include <handlers/Status.h>
#include <handlers/Context.h>
#include <backend/BackendInterface.h>

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

std::pair<ripple::PublicKey, ripple::SecretKey>
keypairFromRequst(
    boost::json::object const& request,
    boost::json::value& error);

std::vector<ripple::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction);

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerInfo const& info);

#endif
