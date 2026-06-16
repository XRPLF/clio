#include "rpc/handlers/DepositAuthorized.hpp"

#include "rpc/CredentialHelpers.hpp"
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
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <string>
#include <utility>

namespace rpc {

DepositAuthorizedHandler::Result
DepositAuthorizedHandler::process(
    DepositAuthorizedHandler::Input const& input,
    Context const& ctx
) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "DepositAuthorized ledger range must be available");

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
    auto const sourceAccountID = accountFromStringStrict(input.sourceAccount);
    auto const destinationAccountID = accountFromStringStrict(input.destinationAccount);

    auto const srcAccountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        ripple::keylet::account(*sourceAccountID).key,
        lgrInfo.seq,
        ctx.yield
    );

    if (!srcAccountLedgerObject)
        return Error{Status{RippledError::rpcSRC_ACT_NOT_FOUND, "source_accountNotFound"}};

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto const dstKeylet = ripple::keylet::account(*destinationAccountID).key;
    auto const dstAccountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(dstKeylet, lgrInfo.seq, ctx.yield);

    if (!dstAccountLedgerObject)
        return Error{Status{RippledError::rpcDST_ACT_NOT_FOUND, "destination_accountNotFound"}};

    Output response;

    auto it = ripple::SerialIter{dstAccountLedgerObject->data(), dstAccountLedgerObject->size()};
    auto const sleDest = ripple::SLE{it, dstKeylet};
    bool const reqAuth =
        sleDest.isFlag(ripple::lsfDepositAuth) && (sourceAccountID != destinationAccountID);
    auto const& creds = input.credentials;
    bool const credentialsPresent = creds.has_value();

    ripple::STArray authCreds;
    if (credentialsPresent) {
        if (creds->empty()) {
            return Error{
                Status{RippledError::rpcINVALID_PARAMS, "credential array has no elements."}
            };
        }
        if (creds->size() > ripple::maxCredentialsArraySize) {
            return Error{Status{RippledError::rpcINVALID_PARAMS, "credential array too long."}};
        }
        auto const credArray = credentials::fetchCredentialArray(
            input.credentials,
            *sourceAccountID,  // NOLINT(bugprone-unchecked-optional-access)
            *sharedPtrBackend_,
            lgrInfo,
            ctx.yield
        );
        if (!credArray.has_value())
            return Error{std::move(credArray).error()};
        authCreds = *std::move(credArray);
    }

    // If the two accounts are the same OR if that flag is
    // not set, then the deposit should be fine.
    bool depositAuthorized = true;

    if (reqAuth) {
        ripple::uint256 hashKey;
        if (credentialsPresent) {
            auto const sortedAuthCreds = credentials::createAuthCredentials(authCreds);
            ASSERT(
                sortedAuthCreds.size() == authCreds.size(),
                "should already be checked above that there is no duplicate"
            );

            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            hashKey = ripple::keylet::depositPreauth(*destinationAccountID, sortedAuthCreds).key;
        } else {
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            hashKey = ripple::keylet::depositPreauth(*destinationAccountID, *sourceAccountID).key;
        }

        depositAuthorized =
            sharedPtrBackend_->fetchLedgerObject(hashKey, lgrInfo.seq, ctx.yield).has_value();
    }

    response.sourceAccount = input.sourceAccount;
    response.destinationAccount = input.destinationAccount;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.depositAuthorized = depositAuthorized;
    if (credentialsPresent)
        response.credentials = *input.credentials;

    return response;
}

DepositAuthorizedHandler::Input
tag_invoke(boost::json::value_to_tag<DepositAuthorizedHandler::Input>, boost::json::value const& jv)
{
    auto input = DepositAuthorizedHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.sourceAccount = boost::json::value_to<std::string>(jv.at(JS(source_account)));
    input.destinationAccount = boost::json::value_to<std::string>(jv.at(JS(destination_account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(credentials)))
        input.credentials = boost::json::value_to<boost::json::array>(jv.at(JS(credentials)));

    return input;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    DepositAuthorizedHandler::Output const& output
)
{
    jv = boost::json::object{
        {JS(deposit_authorized), output.depositAuthorized},
        {JS(source_account), output.sourceAccount},
        {JS(destination_account), output.destinationAccount},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated}
    };
    if (output.credentials)
        jv.as_object()[JS(credentials)] = *output.credentials;
}

}  // namespace rpc
