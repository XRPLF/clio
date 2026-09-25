#include "rpc/handlers/MPTHolders.hpp"

#include "data/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STInteger.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <vector>

using namespace xrpl;

namespace rpc {

namespace {

/**
 * @brief Serialize a single MPToken ledger object blob into the mpt_holders JSON.
 *
 * @param mptID The MPTokenIssuance ID the holder belongs to.
 * @param mpt The serialized MPToken ledger object.
 * @return The holder entry as a JSON object.
 */
boost::json::object
mpTokenToJson(xrpl::uint192 const& mptID, data::Blob const& mpt)
{
    xrpl::STLedgerEntry const sle{xrpl::SerialIter{mpt.data(), mpt.size()}, xrpl::uint256{}};
    auto const mptokenKey = keylet::mptoken(mptID, sle[xrpl::sfAccount]).key;
    boost::json::object mptJson;

    mptJson[JS(account)] = toBase58(sle[xrpl::sfAccount]);
    mptJson[JS(flags)] = sle.getFlags();
    mptJson[JS(mpt_amount)] = toBoostJson(
        xrpl::STUInt64{xrpl::sfMPTAmount, sle[xrpl::sfMPTAmount]}.getJson(JsonOptions::Values::None)
    );
    mptJson[JS(mptoken_index)] = xrpl::to_string(mptokenKey);

    if (sle.isFieldPresent(xrpl::sfLockedAmount)) {
        mptJson["locked_amount"] = toBoostJson(
            xrpl::STUInt64{xrpl::sfLockedAmount, sle[xrpl::sfLockedAmount]}.getJson(
                JsonOptions::Values::None
            )
        );
    }

    if (sle.isFieldPresent(xrpl::sfConfidentialBalanceInbox)) {
        mptJson[JS(confidential_balance_inbox)] =
            xrpl::strHex(sle.getFieldVL(xrpl::sfConfidentialBalanceInbox));
    }

    if (sle.isFieldPresent(xrpl::sfConfidentialBalanceSpending)) {
        mptJson[JS(confidential_balance_spending)] =
            xrpl::strHex(sle.getFieldVL(xrpl::sfConfidentialBalanceSpending));
    }

    if (sle.isFieldPresent(xrpl::sfConfidentialBalanceVersion))
        mptJson[JS(confidential_balance_version)] = sle[xrpl::sfConfidentialBalanceVersion];

    if (sle.isFieldPresent(xrpl::sfIssuerEncryptedBalance)) {
        mptJson[JS(issuer_encrypted_balance)] =
            xrpl::strHex(sle.getFieldVL(xrpl::sfIssuerEncryptedBalance));
    }

    if (sle.isFieldPresent(xrpl::sfAuditorEncryptedBalance)) {
        mptJson[JS(auditor_encrypted_balance)] =
            xrpl::strHex(sle.getFieldVL(xrpl::sfAuditorEncryptedBalance));
    }

    if (sle.isFieldPresent(xrpl::sfHolderEncryptionKey)) {
        mptJson[JS(holder_encryption_key)] =
            xrpl::strHex(sle.getFieldVL(xrpl::sfHolderEncryptionKey));
    }

    return mptJson;
}

}  // namespace

MPTHoldersHandler::Result
MPTHoldersHandler::process(MPTHoldersHandler::Input const& input, Context const& ctx) const
{
    // Account-list filter is a bounded, unpaginated lookup by key: marker/limit does not make sense
    // alongside it.
    if (input.accounts) {
        if (input.marker.has_value())
            return Error{Status{XrpldError::RpcInvalidParams, "accountsWithMarker"}};
        if (input.limit.has_value())
            return Error{Status{XrpldError::RpcInvalidParams, "accountsWithLimit"}};
    }

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "MPTHolder's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );
    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const limit = input.limit.value_or(MPTHoldersHandler::kLimitDefault);
    auto const& mptID = input.mptID;

    auto const issuanceLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::mptokenIssuance(mptID).key, lgrInfo.seq, ctx.yield
    );
    if (!issuanceLedgerObject)
        return Error{Status{XrpldError::RpcObjectNotFound, "objectNotFound"}};

    auto output = MPTHoldersHandler::Output{};
    output.mptID = to_string(mptID);
    output.ledgerIndex = lgrInfo.seq;

    // Account-list filter: bounded lookup by key. Duplicates are dropped in first-seen
    // order, non-holders are omitted, and no paging marker is produced.
    if (input.accounts) {
        std::vector<xrpl::uint256> keys;
        keys.reserve(input.accounts->size());
        for (auto const& accountID : *input.accounts) {
            auto const key = xrpl::keylet::mptoken(mptID, accountID).key;
            if (not std::ranges::contains(keys, key))
                keys.push_back(key);
        }

        auto const mptObjects = sharedPtrBackend_->fetchLedgerObjects(keys, lgrInfo.seq, ctx.yield);
        for (auto const& mpt : mptObjects) {
            if (not mpt.empty())
                output.mpts.push_back(mpTokenToJson(mptID, mpt));
        }

        return output;
    }

    output.limit = limit;

    auto const dbResponse =
        sharedPtrBackend_->fetchMPTHolders(mptID, limit, input.marker, lgrInfo.seq, ctx.yield);

    for (auto const& mpt : dbResponse.mptokens)
        output.mpts.push_back(mpTokenToJson(mptID, mpt));

    if (dbResponse.cursor.has_value())
        output.marker = strHex(*dbResponse.cursor);

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    MPTHoldersHandler::Output const& output
)
{
    jv = {
        {JS(mpt_issuance_id), output.mptID},
        {JS(ledger_index), output.ledgerIndex},
        {"mptokens", output.mpts},
        {JS(validated), output.validated},
    };

    if (output.limit.has_value())
        jv.as_object()[JS(limit)] = *(output.limit);

    if (output.marker.has_value())
        jv.as_object()[JS(marker)] = *(output.marker);
}

}  // namespace rpc
