#include "rpc/handlers/MPTHolders.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
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

#include <cstdint>
#include <optional>
#include <string>

using namespace xrpl;

namespace rpc {

MPTHoldersHandler::Result
MPTHoldersHandler::process(MPTHoldersHandler::Input const& input, Context const& ctx) const
{
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

    std::optional<xrpl::AccountID> cursor;
    if (input.marker)
        cursor = xrpl::AccountID{input.marker->c_str()};

    auto const dbResponse =
        sharedPtrBackend_->fetchMPTHolders(mptID, limit, cursor, lgrInfo.seq, ctx.yield);
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

MPTHoldersHandler::Input
tag_invoke(boost::json::value_to_tag<MPTHoldersHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    MPTHoldersHandler::Input input;

    input.mptID = jsonObject.at(JS(mpt_issuance_id)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jsonObject.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = jsonObject.at(JS(marker)).as_string().c_str();

    return input;
}
}  // namespace rpc
