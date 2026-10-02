#include "rpc/handlers/AccountObjects.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/ledger/helpers/SponsorHelpers.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rpc {

AccountObjectsHandler::Result
AccountObjectsHandler::process(AccountObjectsHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountObject's ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const& accountID = input.account;
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::account(accountID).key, lgrInfo.seq, ctx.yield
    );

    if (!accountLedgerObject)
        return Error{Status{XrpldError::RpcActNotFound}};

    auto typeFilter = std::optional<std::vector<xrpl::LedgerEntryType>>{};

    if (input.deletionBlockersOnly) {
        typeFilter.emplace();
        auto const& deletionBlockers = util::LedgerTypes::getDeletionBlockerLedgerTypes();
        typeFilter->reserve(deletionBlockers.size());

        for (auto type : deletionBlockers) {
            if (input.type && input.type != type)
                continue;

            typeFilter->push_back(type);
        }
    } else {
        if (input.type && input.type != xrpl::ltANY)
            typeFilter = {*input.type};
    }

    auto const sponsorField = [](xrpl::SLE const& sle,
                                 xrpl::SField const& field) -> std::optional<xrpl::AccountID> {
        if (sle.isFieldPresent(field))
            return sle.getAccountID(field);
        return std::nullopt;
    };

    auto const sponsorOf = [&sponsorField](xrpl::SLE const& sle) {
        // A trust line records a sponsor per side, so either one makes it sponsored.
        if (sle.getType() == xrpl::ltRIPPLE_STATE) {
            if (auto const high = sponsorField(sle, xrpl::sfHighSponsor); high.has_value())
                return high;

            return sponsorField(sle, xrpl::sfLowSponsor);
        }

        // NFTokenPage is not in isLedgerEntrySupportedBySponsorship's allowlist, so that
        // predicate alone would report every page as unsponsored; xrpld reads pages on a
        // separate path for the same reason. No page can actually carry sfSponsor today --
        // SponsorshipTransfer::preclaim rejects unsupported types with tecNO_PERMISSION -- so
        // this only starts to matter if NFTokenPage is added to that allowlist upstream.
        if (sle.getType() == xrpl::ltNFTOKEN_PAGE or xrpl::isLedgerEntrySupportedBySponsorship(sle))
            return sponsorField(sle, xrpl::sfSponsor);

        return std::optional<xrpl::AccountID>{};
    };

    Output response;
    auto const addToResponse = [&](xrpl::SLE&& sle) {
        if (input.sponsored.has_value() and
            sponsorOf(sle).has_value() != static_cast<bool>(*input.sponsored)) {
            return true;
        }

        if (not typeFilter or
            std::find(std::begin(*typeFilter), std::end(*typeFilter), sle.getType()) !=
                std::end(*typeFilter)) {
            response.accountObjects.push_back(std::move(sle));
        }
        return true;
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_,
        accountID,
        lgrInfo.seq,
        input.limit,
        input.marker,
        ctx.yield,
        addToResponse,
        true
    );

    if (not expectedNext.has_value())
        return Error{expectedNext.error()};

    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.limit = input.limit;
    response.account = xrpl::to_string(accountID);

    auto const& nextMarker = *expectedNext;

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountObjectsHandler::Output const& output
)
{
    auto objects = boost::json::array{};
    std::ranges::transform(
        output.accountObjects,

        std::back_inserter(objects),
        [](auto const& sle) { return toJson(sle); }
    );

    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {JS(account), output.account},
        {JS(account_objects), objects},
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = *(output.marker);
}

}  // namespace rpc
