#include "rpc/handlers/DepositAuthorized.hpp"

#include "rpc/CredentialHelpers.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <memory>
#include <optional>
#include <ranges>
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

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const& sourceAccountID = input.sourceAccount;
    auto const& destinationAccountID = input.destinationAccount;

    auto const srcAccountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::account(sourceAccountID).key, lgrInfo.seq, ctx.yield
    );

    if (!srcAccountLedgerObject)
        return Error{Status{RippledError::RpcSrcActNotFound, "source_accountNotFound"}};

    auto const dstKeylet = xrpl::keylet::account(destinationAccountID).key;
    auto const dstAccountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(dstKeylet, lgrInfo.seq, ctx.yield);

    if (!dstAccountLedgerObject)
        return Error{Status{RippledError::RpcDstActNotFound, "destination_accountNotFound"}};

    Output response;

    auto it = xrpl::SerialIter{dstAccountLedgerObject->data(), dstAccountLedgerObject->size()};
    auto const sleDest = xrpl::SLE{it, dstKeylet};
    bool const reqAuth =
        sleDest.isFlag(xrpl::lsfDepositAuth) && (sourceAccountID != destinationAccountID);
    auto const& creds = input.credentials;
    bool const credentialsPresent = creds.has_value();

    xrpl::STArray authCreds;
    if (credentialsPresent) {
        if (creds->empty()) {
            return Error{
                Status{RippledError::RpcInvalidParams, "credential array has no elements."}
            };
        }
        if (creds->size() > xrpl::kMaxCredentialsArraySize) {
            return Error{Status{RippledError::RpcInvalidParams, "credential array too long."}};
        }
        auto const credArray = credentials::fetchCredentialArray(
            *creds, sourceAccountID, *sharedPtrBackend_, lgrInfo, ctx.yield
        );
        if (!credArray.has_value())
            return Error{std::move(credArray).error()};
        authCreds = *std::move(credArray);
    }

    // If the two accounts are the same OR if that flag is
    // not set, then the deposit should be fine.
    bool depositAuthorized = true;

    if (reqAuth) {
        xrpl::uint256 hashKey;
        if (credentialsPresent) {
            auto const sortedAuthCreds = credentials::createAuthCredentials(authCreds);
            ASSERT(
                sortedAuthCreds.size() == authCreds.size(),
                "should already be checked above that there is no duplicate"
            );

            hashKey = xrpl::keylet::depositPreauth(destinationAccountID, sortedAuthCreds).key;
        } else {
            hashKey = xrpl::keylet::depositPreauth(destinationAccountID, sourceAccountID).key;
        }

        depositAuthorized =
            sharedPtrBackend_->fetchLedgerObject(hashKey, lgrInfo.seq, ctx.yield).has_value();
    }

    response.sourceAccount = xrpl::to_string(sourceAccountID);
    response.destinationAccount = xrpl::to_string(destinationAccountID);
    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.depositAuthorized = depositAuthorized;
    if (credentialsPresent)
        response.credentials = creds;

    return response;
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
    if (output.credentials.has_value()) {
        jv.as_object()[JS(credentials)] = *output.credentials                             //
            | std::views::transform([](auto const& cred) { return xrpl::strHex(cred); })  //
            | std::ranges::to<boost::json::array>();
    }
}

}  // namespace rpc
