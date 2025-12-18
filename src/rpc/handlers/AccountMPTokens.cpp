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

#include "rpc/handlers/AccountMPTokens.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
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
AccountMPTokensHandler::addMPToken(std::vector<MPTokenResponse>& mpts, ripple::SLE const& sle)
{
    MPTokenResponse token{};
    auto const flags = sle.getFieldU32(ripple::sfFlags);

    token.MPTokenID = ripple::strHex(sle.key());
    token.account = ripple::to_string(sle.getAccountID(ripple::sfAccount));
    token.MPTokenIssuanceID = ripple::strHex(sle.getFieldH192(ripple::sfMPTokenIssuanceID));
    token.MPTAmount = sle.getFieldU64(ripple::sfMPTAmount);

    if (sle.isFieldPresent(ripple::sfLockedAmount))
        token.lockedAmount = sle.getFieldU64(ripple::sfLockedAmount);

    auto const setFlag = [&](std::optional<bool>& field, std::uint32_t mask) {
        if ((flags & mask) != 0u)
            field = true;
    };

    setFlag(token.mptLocked, ripple::lsfMPTLocked);
    setFlag(token.mptAuthorized, ripple::lsfMPTAuthorized);

    mpts.push_back(token);
}

AccountMPTokensHandler::Result
AccountMPTokensHandler::process(AccountMPTokensHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountMPTokens' ledger range must be available");
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
    response.mpts.reserve(input.limit);

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltMPTOKEN) {
            addMPToken(response.mpts, sle);
        }
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_, *accountID, lgrInfo.seq, input.limit, input.marker, ctx.yield, addToResponse
    );

    if (!expectedNext.has_value())
        return Error{expectedNext.error()};

    auto const& nextMarker = expectedNext.value();

    response.account = input.account;
    response.limit = input.limit;

    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountMPTokensHandler::Input
tag_invoke(boost::json::value_to_tag<AccountMPTokensHandler::Input>, boost::json::value const& jv)
{
    AccountMPTokensHandler::Input input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountMPTokensHandler::Output const& output)
{
    auto obj = boost::json::object{
        {JS(account), output.account},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {"mptokens", boost::json::value_from(output.mpts)},
    };

    if (output.marker.has_value())
        obj[JS(marker)] = *output.marker;

    jv = std::move(obj);
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountMPTokensHandler::MPTokenResponse const& mptoken)
{
    auto obj = boost::json::object{
        {"mpt_id", mptoken.MPTokenID},
        {JS(account), mptoken.account},
        {JS(mpt_issuance_id), mptoken.MPTokenIssuanceID},
        {JS(mpt_amount), mptoken.MPTAmount},
    };

    auto const setIfPresent = [&](boost::json::string_view field, auto const& value) {
        if (value.has_value()) {
            obj[field] = *value;
        }
    };

    setIfPresent("locked_amount", mptoken.lockedAmount);
    setIfPresent("mpt_locked", mptoken.mptLocked);
    setIfPresent("mpt_authorized", mptoken.mptAuthorized);

    jv = std::move(obj);
}

}  // namespace rpc
