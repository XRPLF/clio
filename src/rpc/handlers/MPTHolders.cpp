#include "rpc/handlers/MPTHolders.hpp"

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

#include <optional>
#include <string>

using namespace xrpl;

namespace rpc {

MPTHoldersHandler::Result
MPTHoldersHandler::process(MPTHoldersHandler::Input const& input, Context const& ctx) const
{
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
    auto const limit = input.limit;
    auto const& mptID = input.mptID;

    auto const issuanceLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::mptokenIssuance(mptID).key, lgrInfo.seq, ctx.yield
    );
    if (!issuanceLedgerObject)
        return Error{Status{RippledError::RpcObjectNotFound, "objectNotFound"}};

    auto const dbResponse =
        sharedPtrBackend_->fetchMPTHolders(mptID, limit, input.marker, lgrInfo.seq, ctx.yield);
    auto output = MPTHoldersHandler::Output{};
    output.mptID = to_string(mptID);
    output.limit = limit;
    output.ledgerIndex = lgrInfo.seq;

    boost::json::array const mpts;
    for (auto const& mpt : dbResponse.mptokens) {
        xrpl::STLedgerEntry const sle{
            xrpl::SerialIter{mpt.data(), mpt.size()}, keylet::mptokenIssuance(mptID).key
        };
        boost::json::object mptJson;

        mptJson[JS(account)] = toBase58(sle[xrpl::sfAccount]);
        mptJson[JS(flags)] = sle.getFlags();
        mptJson[JS(mpt_amount)] = toBoostJson(
            xrpl::STUInt64{xrpl::sfMPTAmount, sle[xrpl::sfMPTAmount]}.getJson(
                JsonOptions::Values::None
            )
        );
        mptJson[JS(mptoken_index)] =
            xrpl::to_string(xrpl::keylet::mptoken(mptID, sle[xrpl::sfAccount]).key);

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

        output.mpts.push_back(mptJson);
    }

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

}  // namespace rpc
