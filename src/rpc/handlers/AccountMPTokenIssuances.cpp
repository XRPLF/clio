//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "rpc/handlers/AccountMPTokenIssuances.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

void
AccountMPTokenIssuancesHandler::addMPTokenIssuance(
    std::vector<MPTokenIssuanceResponse>& issuances,
    ripple::SLE const& sle,
    ripple::AccountID const& account
)
{
    MPTokenIssuanceResponse issuance;

    issuance.issuer = ripple::to_string(account);
    issuance.sequence = sle.getFieldU32(ripple::sfSequence);
    auto const flags = sle.getFieldU32(ripple::sfFlags);

    auto const setFlag = [&](std::optional<bool>& field, std::uint32_t mask) {
        if ((flags & mask) != 0u)
            field = true;
    };

    setFlag(issuance.mptLocked, ripple::lsfMPTLocked);
    setFlag(issuance.mptCanLock, ripple::lsfMPTCanLock);
    setFlag(issuance.mptRequireAuth, ripple::lsfMPTRequireAuth);
    setFlag(issuance.mptCanEscrow, ripple::lsfMPTCanEscrow);
    setFlag(issuance.mptCanTrade, ripple::lsfMPTCanTrade);
    setFlag(issuance.mptCanTransfer, ripple::lsfMPTCanTransfer);
    setFlag(issuance.mptCanClawback, ripple::lsfMPTCanClawback);

    if (sle.isFieldPresent(ripple::sfTransferFee))
        issuance.transferFee = sle.getFieldU16(ripple::sfTransferFee);

    if (sle.isFieldPresent(ripple::sfAssetScale))
        issuance.assetScale = sle.getFieldU8(ripple::sfAssetScale);

    if (sle.isFieldPresent(ripple::sfMaximumAmount))
        issuance.maximumAmount = sle.getFieldU64(ripple::sfMaximumAmount);

    if (sle.isFieldPresent(ripple::sfOutstandingAmount))
        issuance.outstandingAmount = sle.getFieldU64(ripple::sfOutstandingAmount);

    if (sle.isFieldPresent(ripple::sfLockedAmount))
        issuance.lockedAmount = sle.getFieldU64(ripple::sfLockedAmount);

    if (sle.isFieldPresent(ripple::sfMPTokenMetadata))
        issuance.mptokenMetadata = ripple::strHex(sle.getFieldVL(ripple::sfMPTokenMetadata));

    if (sle.isFieldPresent(ripple::sfDomainID))
        issuance.domainID = ripple::strHex(sle.getFieldH256(ripple::sfDomainID));

    issuances.push_back(issuance);
}

AccountMPTokenIssuancesHandler::Result
AccountMPTokenIssuancesHandler::process(AccountMPTokenIssuancesHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountMPTokenIssuances' ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (!expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = expectedLgrInfo.value();
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*accountID).key, lgrInfo.seq, ctx.yield);

    if (not accountLedgerObject.has_value())
        return Error{Status{RippledError::rpcACT_NOT_FOUND}};

    Output response;
    response.issuances.reserve(input.limit);

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltMPTOKEN_ISSUANCE) {
            addMPTokenIssuance(response.issuances, sle, *accountID);
        }
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_, *accountID, lgrInfo.seq, input.limit, input.marker, ctx.yield, addToResponse
    );

    if (!expectedNext.has_value())
        return Error{expectedNext.error()};

    auto const nextMarker = expectedNext.value();

    response.account = input.account;
    response.limit = input.limit;

    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountMPTokenIssuancesHandler::Input
tag_invoke(boost::json::value_to_tag<AccountMPTokenIssuancesHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountMPTokenIssuancesHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = util::integralValueAs<uint32_t>(jv.at(JS(ledger_index)));
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountMPTokenIssuancesHandler::Output const& output)
{
    using boost::json::value_from;

    auto obj = boost::json::object{
        {JS(account), output.account},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {"mpt_issuances", value_from(output.issuances)},
    };

    if (output.marker.has_value())
        obj[JS(marker)] = *output.marker;

    jv = std::move(obj);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountMPTokenIssuancesHandler::MPTokenIssuanceResponse const& issuance
)
{
    auto obj = boost::json::object{
        {JS(issuer), issuance.issuer},
        {JS(sequence), issuance.sequence},
    };

    auto const setIfPresent = [&](boost::json::string_view field, auto const& value) {
        if (value.has_value()) {
            obj[field] = *value;
        }
    };

    setIfPresent("transfer_fee", issuance.transferFee);
    setIfPresent("asset_scale", issuance.assetScale);
    setIfPresent("maximum_amount", issuance.maximumAmount);
    setIfPresent("outstanding_amount", issuance.outstandingAmount);
    setIfPresent("locked_amount", issuance.lockedAmount);
    setIfPresent("mptoken_metadata", issuance.mptokenMetadata);
    setIfPresent("domain_id", issuance.domainID);

    setIfPresent("mpt_locked", issuance.mptLocked);
    setIfPresent("mpt_can_lock", issuance.mptCanLock);
    setIfPresent("mpt_require_auth", issuance.mptRequireAuth);
    setIfPresent("mpt_can_escrow", issuance.mptCanEscrow);
    setIfPresent("mpt_can_trade", issuance.mptCanTrade);
    setIfPresent("mpt_can_transfer", issuance.mptCanTransfer);
    setIfPresent("mpt_can_clawback", issuance.mptCanClawback);

    jv = std::move(obj);
}

}  // namespace rpc
