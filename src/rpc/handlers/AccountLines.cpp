#include "rpc/handlers/AccountLines.hpp"

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
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

void
AccountLinesHandler::addLine(
    std::vector<LineResponse>& lines,
    xrpl::SLE const& lineSle,
    xrpl::AccountID const& account,
    std::optional<xrpl::AccountID> const& peerAccount
)
{
    auto const flags = lineSle.getFieldU32(xrpl::sfFlags);
    auto const lowLimit = lineSle.getFieldAmount(xrpl::sfLowLimit);
    auto const highLimit = lineSle.getFieldAmount(xrpl::sfHighLimit);
    auto const lowID = lowLimit.getIssuer();
    auto const highID = highLimit.getIssuer();
    auto const lowQualityIn = lineSle.getFieldU32(xrpl::sfLowQualityIn);
    auto const lowQualityOut = lineSle.getFieldU32(xrpl::sfLowQualityOut);
    auto const highQualityIn = lineSle.getFieldU32(xrpl::sfHighQualityIn);
    auto const highQualityOut = lineSle.getFieldU32(xrpl::sfHighQualityOut);
    auto balance = lineSle.getFieldAmount(xrpl::sfBalance);

    auto const viewLowest = (lowID == account);
    auto const lineLimit = viewLowest ? lowLimit : highLimit;
    auto const lineLimitPeer = not viewLowest ? lowLimit : highLimit;
    auto const lineAccountIDPeer = not viewLowest ? lowID : highID;
    auto const lineQualityIn = viewLowest ? lowQualityIn : highQualityIn;
    auto const lineQualityOut = viewLowest ? lowQualityOut : highQualityOut;

    if (peerAccount && peerAccount != lineAccountIDPeer)
        return;

    if (not viewLowest)
        balance.negate();

    bool const lineAuth = (flags & (viewLowest ? xrpl::lsfLowAuth : xrpl::lsfHighAuth)) != 0u;
    bool const lineAuthPeer =
        (flags & (not viewLowest ? xrpl::lsfLowAuth : xrpl::lsfHighAuth)) != 0u;
    bool const lineNoRipple =
        (flags & (viewLowest ? xrpl::lsfLowNoRipple : xrpl::lsfHighNoRipple)) != 0u;
    bool const lineNoRipplePeer =
        (flags & (not viewLowest ? xrpl::lsfLowNoRipple : xrpl::lsfHighNoRipple)) != 0u;
    bool const lineFreeze = (flags & (viewLowest ? xrpl::lsfLowFreeze : xrpl::lsfHighFreeze)) != 0u;
    bool const lineFreezePeer =
        (flags & (not viewLowest ? xrpl::lsfLowFreeze : xrpl::lsfHighFreeze)) != 0u;
    bool const lineDeepFreeze =
        (flags & (viewLowest ? xrpl::lsfLowDeepFreeze : xrpl::lsfHighDeepFreeze)) != 0u;
    bool const lineDeepFreezePeer =
        (flags & (not viewLowest ? xrpl::lsfLowDeepFreeze : xrpl::lsfHighDeepFreeze)) != 0u;

    xrpl::STAmount const& saBalance = balance;
    xrpl::STAmount const& saLimit = lineLimit;
    xrpl::STAmount const& saLimitPeer = lineLimitPeer;

    LineResponse line;
    line.account = xrpl::to_string(lineAccountIDPeer);
    line.balance = saBalance.getText();
    line.currency = xrpl::to_string(saBalance.get<xrpl::Issue>().currency);
    line.limit = saLimit.getText();
    line.limitPeer = saLimitPeer.getText();
    line.qualityIn = lineQualityIn;
    line.qualityOut = lineQualityOut;

    if (lineNoRipple)
        line.noRipple = true;

    if (lineNoRipplePeer)
        line.noRipplePeer = true;

    if (lineAuth)
        line.authorized = true;

    if (lineAuthPeer)
        line.peerAuthorized = true;

    if (lineFreeze)
        line.freeze = true;

    if (lineFreezePeer)
        line.freezePeer = true;

    if (lineDeepFreeze)
        line.deepFreeze = true;

    if (lineDeepFreezePeer)
        line.deepFreezePeer = true;

    lines.push_back(line);
}

AccountLinesHandler::Result
AccountLinesHandler::process(AccountLinesHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountLines' ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        xrpl::keylet::account(*accountID).key,
        lgrInfo.seq,
        ctx.yield
    );

    if (not accountLedgerObject)
        return Error{Status{RippledError::RpcActNotFound}};

    auto const peerAccountID =
        input.peer ? accountFromStringStrict(*(input.peer)) : std::optional<xrpl::AccountID>{};

    Output response;
    response.lines.reserve(input.limit);

    auto const addToResponse = [&](xrpl::SLE const sle) {
        if (sle.getType() == xrpl::ltRIPPLE_STATE) {
            auto ignore = false;
            if (input.ignoreDefault) {
                if (sle.getFieldAmount(xrpl::sfLowLimit).getIssuer() == accountID) {
                    ignore = ((sle.getFieldU32(xrpl::sfFlags) & xrpl::lsfLowReserve) == 0u);
                } else {
                    ignore = ((sle.getFieldU32(xrpl::sfFlags) & xrpl::lsfHighReserve) == 0u);
                }
            }

            if (not ignore)
                addLine(response.lines, sle, *accountID, peerAccountID);
        }
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,  // NOLINT(bugprone-unchecked-optional-access)
        lgrInfo.seq,
        input.limit,
        input.marker,
        ctx.yield,
        addToResponse
    );

    if (not expectedNext.has_value())
        return Error{expectedNext.error()};

    auto const nextMarker = *expectedNext;

    response.account = input.account;
    response.limit = input.limit;  // not documented,
                                   // https://github.com/XRPLF/xrpl-dev-portal/issues/1838
    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountLinesHandler::Input
tag_invoke(boost::json::value_to_tag<AccountLinesHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountLinesHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));
    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(peer)))
        input.peer = boost::json::value_to<std::string>(jv.at(JS(peer)));

    if (jsonObject.contains(JS(ignore_default)))
        input.ignoreDefault = jv.at(JS(ignore_default)).as_bool();

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountLinesHandler::Output const& output
)
{
    using boost::json::value_from;

    auto obj = boost::json::object{
        {JS(account), output.account},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {JS(lines), value_from(output.lines)},
    };

    if (output.marker)
        obj[JS(marker)] = *output.marker;

    jv = std::move(obj);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    [[maybe_unused]] AccountLinesHandler::LineResponse const& line
)
{
    auto obj = boost::json::object{
        {JS(account), line.account},
        {JS(balance), line.balance},
        {JS(currency), line.currency},
        {JS(limit), line.limit},
        {JS(limit_peer), line.limitPeer},
        {JS(quality_in), line.qualityIn},
        {JS(quality_out), line.qualityOut},
    };

    if (line.noRipple)
        obj[JS(no_ripple)] = *(line.noRipple);

    if (line.noRipplePeer)
        obj[JS(no_ripple_peer)] = *(line.noRipplePeer);

    if (line.authorized)
        obj[JS(authorized)] = *(line.authorized);

    if (line.peerAuthorized)
        obj[JS(peer_authorized)] = *(line.peerAuthorized);

    if (line.freeze)
        obj[JS(freeze)] = *(line.freeze);

    if (line.freezePeer)
        obj[JS(freeze_peer)] = *(line.freezePeer);

    if (line.deepFreeze)
        obj[JS(deep_freeze)] = *(line.deepFreeze);

    if (line.deepFreezePeer)
        obj[JS(deep_freeze_peer)] = *(line.deepFreezePeer);

    jv = std::move(obj);
}

}  // namespace rpc
