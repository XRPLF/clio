#include "rpc/handlers/MPTHolders.hpp"

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
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
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace xrpl;

namespace rpc {

namespace {

/**
 * @brief Serialize a single MPToken ledger object blob into the mpt_holders JSON shape.
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
    if (input.accounts && input.marker)
        return Error{Status{RippledError::RpcInvalidParams, "accountsWithMarker"}};
    if (input.accounts && input.limit)
        return Error{Status{RippledError::RpcInvalidParams, "accountsWithLimit"}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "MPTHolder's ledger range must be available");

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
    auto const limit = input.limit.value_or(MPTHoldersHandler::kLimitDefault);
    auto const mptID = xrpl::uint192{input.mptID.c_str()};

    auto const issuanceLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::mptokenIssuance(mptID).key, lgrInfo.seq, ctx.yield
    );
    if (!issuanceLedgerObject)
        return Error{Status{RippledError::RpcObjectNotFound, "objectNotFound"}};

    auto output = MPTHoldersHandler::Output{};
    output.mptID = to_string(mptID);
    output.limit = limit;
    output.ledgerIndex = lgrInfo.seq;

    // Account-list filter: bounded lookup by key. Duplicates are dropped in first-seen
    // order, non-holders are omitted, and no paging marker is produced.
    if (input.accounts) {
        std::vector<xrpl::uint256> keys;
        keys.reserve(input.accounts->size());
        for (auto const& accountID : *input.accounts) {
            auto const key = xrpl::keylet::mptoken(mptID, accountID).key;
            if (std::ranges::find(keys, key) == keys.end())
                keys.push_back(key);
        }

        auto const mptObjects = sharedPtrBackend_->fetchLedgerObjects(keys, lgrInfo.seq, ctx.yield);
        for (auto const& mpt : mptObjects) {
            if (not mpt.empty())
                output.mpts.push_back(mpTokenToJson(mptID, mpt));
        }

        return output;
    }

    std::optional<xrpl::AccountID> cursor;
    if (input.marker)
        cursor = xrpl::AccountID{input.marker->c_str()};

    auto const dbResponse =
        sharedPtrBackend_->fetchMPTHolders(mptID, limit, cursor, lgrInfo.seq, ctx.yield);

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
        {JS(limit), output.limit},
        {JS(ledger_index), output.ledgerIndex},
        {"mptokens", output.mpts},
        {JS(validated), output.validated},
    };

    if (output.marker.has_value())
        jv.as_object()[JS(marker)] = *(output.marker);
}

MPTHoldersHandler::Input
tag_invoke(boost::json::value_to_tag<MPTHoldersHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    MPTHoldersHandler::Input input;

    input.mptID = boost::json::value_to<std::string>(jsonObject.at(JS(mpt_issuance_id)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jsonObject.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jsonObject.at(JS(marker)));

    if (jsonObject.contains(JS(accounts))) {
        auto const& accountsJson = jsonObject.at(JS(accounts)).as_array();
        auto& accounts = input.accounts.emplace();
        accounts.reserve(accountsJson.size());
        for (auto const& account : accountsJson) {
            // Spec already requires each entry to be a valid base58 account.
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            accounts.push_back(*util::parseBase58Wrapper<xrpl::AccountID>(
                boost::json::value_to<std::string>(account)
            ));
        }
    }

    return input;
}
}  // namespace rpc
