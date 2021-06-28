
#ifndef XRPL_REPORTING_RPCHELPERS_H_INCLUDED
#define XRPL_REPORTING_RPCHELPERS_H_INCLUDED

#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STTx.h>
#include <boost/json.hpp>
#include <rpc/Status.h>
#include <rpc/Context.h>
#include <reporting/BackendInterface.h>

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account);

std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(Backend::TransactionAndMetadata const& blobs);

std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(Backend::TransactionAndMetadata const& blobs, std::uint32_t seq);

boost::json::object
getJson(ripple::STBase const& obj);

boost::json::object
getJson(ripple::SLE const& sle);

boost::json::object
getJson(ripple::TxMeta const& meta);

boost::json::value
getJson(Json::Value const& value);

std::optional<uint32_t>
ledgerSequenceFromRequest(
    boost::json::object const& request,
    BackendInterface const& backend);

std::variant<RPC::Status, ripple::LedgerInfo>
ledgerInfoFromRequest(RPC::Context const& ctx);

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerInfo const& info);

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

#endif
