#include "rpc/handlers/AccountObjects.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"
#include "util/LedgerUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <cstdint>
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

    if (!accountLedgerObject)
        return Error{Status{RippledError::RpcActNotFound}};

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

    Output response;
    auto const addToResponse = [&](xrpl::SLE&& sle) {
        if (not typeFilter or
            std::find(std::begin(*typeFilter), std::end(*typeFilter), sle.getType()) !=
                std::end(*typeFilter)) {
            response.accountObjects.push_back(std::move(sle));
        }
        return true;
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,  // NOLINT(bugprone-unchecked-optional-access)
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
    response.account = input.account;

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

AccountObjectsHandler::Input
tag_invoke(boost::json::value_to_tag<AccountObjectsHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountObjectsHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(type))) {
        input.type = util::LedgerTypes::getAccountOwnedLedgerTypeFromStr(
            boost::json::value_to<std::string>(jv.at(JS(type)))
        );
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(deletion_blockers_only)))
        input.deletionBlockersOnly = jsonObject.at(JS(deletion_blockers_only)).as_bool();

    return input;
}

}  // namespace rpc
