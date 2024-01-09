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

#include "rpc/handlers/LedgerEntry.h"

#include "rpc/Errors.h"
#include "rpc/JS.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/Types.h"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/tokens.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace rpc {

LedgerEntryHandler::Result
LedgerEntryHandler::process(LedgerEntryHandler::Input input, Context const& ctx) const
{
    ripple::uint256 key;

    if (input.index) {
        key = ripple::uint256{std::string_view(*(input.index))};
    } else if (input.accountRoot) {
        key = ripple::keylet::account(*ripple::parseBase58<ripple::AccountID>(*(input.accountRoot))).key;
    } else if (input.did) {
        key = ripple::keylet::did(*ripple::parseBase58<ripple::AccountID>(*(input.did))).key;
    } 
    else if (input.mptIssuanceID){
        auto const mptIssuanceID = ripple::uint192{std::string_view(*(input.mptIssuanceID))};
        key = ripple::keylet::mptIssuance(mptIssuanceID).key;
    }
    else if (input.directory) {
        auto const keyOrStatus = composeKeyFromDirectory(*input.directory);
        if (auto const status = std::get_if<Status>(&keyOrStatus))
            return Error{*status};

        key = std::get<ripple::uint256>(keyOrStatus);
    } else if (input.offer) {
        auto const id = ripple::parseBase58<ripple::AccountID>(input.offer->at(JS(account)).as_string().c_str());
        key = ripple::keylet::offer(*id, boost::json::value_to<std::uint32_t>(input.offer->at(JS(seq)))).key;
    } else if (input.rippleStateAccount) {
        auto const id1 = ripple::parseBase58<ripple::AccountID>(
            input.rippleStateAccount->at(JS(accounts)).as_array().at(0).as_string().c_str()
        );
        auto const id2 = ripple::parseBase58<ripple::AccountID>(
            input.rippleStateAccount->at(JS(accounts)).as_array().at(1).as_string().c_str()
        );
        auto const currency = ripple::to_currency(input.rippleStateAccount->at(JS(currency)).as_string().c_str());

        key = ripple::keylet::line(*id1, *id2, currency).key;
    } else if (input.escrow) {
        auto const id = ripple::parseBase58<ripple::AccountID>(input.escrow->at(JS(owner)).as_string().c_str());
        key = ripple::keylet::escrow(*id, input.escrow->at(JS(seq)).as_int64()).key;
    } else if (input.depositPreauth) {
        auto const owner =
            ripple::parseBase58<ripple::AccountID>(input.depositPreauth->at(JS(owner)).as_string().c_str());
        auto const authorized =
            ripple::parseBase58<ripple::AccountID>(input.depositPreauth->at(JS(authorized)).as_string().c_str());

        key = ripple::keylet::depositPreauth(*owner, *authorized).key;
    } else if (input.ticket) {
        auto const id = ripple::parseBase58<ripple::AccountID>(input.ticket->at(JS(account)).as_string().c_str());

        key = ripple::getTicketIndex(*id, input.ticket->at(JS(ticket_seq)).as_int64());
    } else if (input.amm) {
        auto const getIssuerFromJson = [](auto const& assetJson) {
            // the field check has been done in validator
            auto const currency = ripple::to_currency(assetJson.at(JS(currency)).as_string().c_str());
            if (ripple::isXRP(currency)) {
                return ripple::xrpIssue();
            }
            auto const issuer = ripple::parseBase58<ripple::AccountID>(assetJson.at(JS(issuer)).as_string().c_str());
            return ripple::Issue{currency, *issuer};
        };

        key = ripple::keylet::amm(
                  getIssuerFromJson(input.amm->at(JS(asset))), getIssuerFromJson(input.amm->at(JS(asset2)))
        )
                  .key;
    } else {
        // Must specify 1 of the following fields to indicate what type
        if (ctx.apiVersion == 1)
            return Error{Status{ClioError::rpcUNKNOWN_OPTION}};
        return Error{Status{RippledError::rpcINVALID_PARAMS}};
    }

    // check ledger exists
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, lgrInfo.seq, ctx.yield);

    if (!ledgerObject || ledgerObject->empty())
        return Error{Status{"entryNotFound"}};

    ripple::STLedgerEntry const sle{ripple::SerialIter{ledgerObject->data(), ledgerObject->size()}, key};

    if (input.expectedType != ripple::ltANY && sle.getType() != input.expectedType)
        return Error{Status{"unexpectedLedgerType"}};

    auto output = LedgerEntryHandler::Output{};
    output.index = ripple::strHex(key);
    output.ledgerIndex = lgrInfo.seq;
    output.ledgerHash = ripple::strHex(lgrInfo.hash);

    if (input.binary) {
        output.nodeBinary = ripple::strHex(*ledgerObject);
    } else {
        output.node = toJson(sle);
    }

    return output;
}

std::variant<ripple::uint256, Status>
LedgerEntryHandler::composeKeyFromDirectory(boost::json::object const& directory) noexcept
{
    // can not specify both dir_root and owner.
    if (directory.contains(JS(dir_root)) && directory.contains(JS(owner)))
        return Status{RippledError::rpcINVALID_PARAMS, "mayNotSpecifyBothDirRootAndOwner"};

    // at least one should availiable
    if (!(directory.contains(JS(dir_root)) || directory.contains(JS(owner))))
        return Status{RippledError::rpcINVALID_PARAMS, "missingOwnerOrDirRoot"};

    uint64_t const subIndex =
        directory.contains(JS(sub_index)) ? boost::json::value_to<uint64_t>(directory.at(JS(sub_index))) : 0;

    if (directory.contains(JS(dir_root))) {
        ripple::uint256 const uDirRoot{directory.at(JS(dir_root)).as_string().c_str()};
        return ripple::keylet::page(uDirRoot, subIndex).key;
    }

    auto const ownerID = ripple::parseBase58<ripple::AccountID>(directory.at(JS(owner)).as_string().c_str());
    return ripple::keylet::page(ripple::keylet::ownerDir(*ownerID), subIndex).key;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LedgerEntryHandler::Output const& output)
{
    auto object = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(index), output.index},
    };

    if (output.nodeBinary) {
        object[JS(node_binary)] = *(output.nodeBinary);
    } else {
        object[JS(node)] = *(output.node);
    }

    jv = std::move(object);
}

LedgerEntryHandler::Input
tag_invoke(boost::json::value_to_tag<LedgerEntryHandler::Input>, boost::json::value const& jv)
{
    auto input = LedgerEntryHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
        }
    }

    if (jsonObject.contains(JS(binary)))
        input.binary = jv.at(JS(binary)).as_bool();

    // check all the protential index
    static auto const indexFieldTypeMap = std::unordered_map<std::string, ripple::LedgerEntryType>{
        {JS(index), ripple::ltANY},
        {JS(directory), ripple::ltDIR_NODE},
        {JS(offer), ripple::ltOFFER},
        {JS(check), ripple::ltCHECK},
        {JS(escrow), ripple::ltESCROW},
        {JS(payment_channel), ripple::ltPAYCHAN},
        {JS(deposit_preauth), ripple::ltDEPOSIT_PREAUTH},
        {JS(ticket), ripple::ltTICKET},
        {JS(nft_page), ripple::ltNFTOKEN_PAGE},
        {JS(amm), ripple::ltAMM}
    };

    auto const indexFieldType =
        std::find_if(indexFieldTypeMap.begin(), indexFieldTypeMap.end(), [&jsonObject](auto const& pair) {
            auto const& [field, _] = pair;
            return jsonObject.contains(field) && jsonObject.at(field).is_string();
        });

    if (indexFieldType != indexFieldTypeMap.end()) {
        input.index = jv.at(indexFieldType->first).as_string().c_str();
        input.expectedType = indexFieldType->second;
    }
    // check if request for account root
    else if (jsonObject.contains(JS(account_root))) {
        input.accountRoot = jv.at(JS(account_root)).as_string().c_str();
    } else if (jsonObject.contains(JS(did))) {
        input.did = jv.at(JS(did)).as_string().c_str();
    } else if (jsonObject.contains(JS(mpt_issuance_id))) {
        input.mptIssuanceID = jv.at(JS(mpt_issuance_id)).as_string().c_str();
    }
    // no need to check if_object again, validator only allows string or object
    else if (jsonObject.contains(JS(directory))) {
        input.directory = jv.at(JS(directory)).as_object();
    } else if (jsonObject.contains(JS(offer))) {
        input.offer = jv.at(JS(offer)).as_object();
    } else if (jsonObject.contains(JS(ripple_state))) {
        input.rippleStateAccount = jv.at(JS(ripple_state)).as_object();
    } else if (jsonObject.contains(JS(escrow))) {
        input.escrow = jv.at(JS(escrow)).as_object();
    } else if (jsonObject.contains(JS(deposit_preauth))) {
        input.depositPreauth = jv.at(JS(deposit_preauth)).as_object();
    } else if (jsonObject.contains(JS(ticket))) {
        input.ticket = jv.at(JS(ticket)).as_object();
    } else if (jsonObject.contains(JS(amm))) {
        input.amm = jv.at(JS(amm)).as_object();
    }

    return input;
}

}  // namespace rpc
