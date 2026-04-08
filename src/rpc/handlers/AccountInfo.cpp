#include "rpc/handlers/AccountInfo.hpp"

#include "data/AmendmentCenter.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/JsonBool.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/ledger/View.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc {
AccountInfoHandler::Result
AccountInfoHandler::process(AccountInfoHandler::Input const& input, Context const& ctx) const
{
    using namespace data;

    if (!input.account && !input.ident) {
        return Error{
            Status{RippledError::rpcINVALID_PARAMS, ripple::RPC::missing_field_message(JS(account))}
        };
    }

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountInfo's ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = expectedLgrInfo.value();
    auto const accountStr = input.account.value_or(input.ident.value_or(""));
    auto const accountID = accountFromStringStrict(accountStr);
    auto const accountKeylet = ripple::keylet::account(*accountID);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND}};

    ripple::STLedgerEntry const sle{
        ripple::SerialIter{accountLedgerObject->data(), accountLedgerObject->size()},
        accountKeylet.key
    };

    if (!accountKeylet.check(sle))
        return Error{Status{RippledError::rpcDB_DESERIALIZATION}};

    auto isEnabled = [this, &ctx, seq = lgrInfo.seq](auto key) {
        return amendmentCenter_->isEnabled(ctx.yield, key, seq);
    };

    auto const isDisallowIncomingEnabled = isEnabled(Amendments::DisallowIncoming);
    auto const isClawbackEnabled = isEnabled(Amendments::Clawback);
    auto const isTokenEscrowEnabled = isEnabled(Amendments::TokenEscrow);

    Output out{
        .ledgerIndex = lgrInfo.seq,
        .ledgerHash = ripple::strHex(lgrInfo.hash),
        .accountData = sle,
        .isDisallowIncomingEnabled = isDisallowIncomingEnabled,
        .isClawbackEnabled = isClawbackEnabled,
        .isTokenEscrowEnabled = isTokenEscrowEnabled,
        .apiVersion = ctx.apiVersion,
        .signerLists = std::nullopt
    };

    // Return SignerList(s) if that is requested.
    if (input.signerLists) {
        // We put the SignerList in an array because of an anticipated
        // future when we support multiple signer lists on one account.
        auto const signersKey = ripple::keylet::signers(*accountID);

        // This code will need to be revisited if in the future we
        // support multiple SignerLists on one account.
        auto const signers =
            sharedPtrBackend_->fetchLedgerObject(signersKey.key, lgrInfo.seq, ctx.yield);
        out.signerLists = std::vector<ripple::STLedgerEntry>();

        if (signers) {
            ripple::STLedgerEntry const sleSigners{
                ripple::SerialIter{signers->data(), signers->size()}, signersKey.key
            };

            if (!signersKey.check(sleSigners))
                return Error{Status{RippledError::rpcDB_DESERIALIZATION}};

            out.signerLists->push_back(sleSigners);
        }
    }

    return out;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountInfoHandler::Output const& output
)
{
    jv = boost::json::object{
        {JS(account_data), toJson(output.accountData)},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
    };

    std::vector<std::pair<std::string_view, ripple::LedgerSpecificFlags>> lsFlags{{
        {"defaultRipple", ripple::lsfDefaultRipple},
        {"depositAuth", ripple::lsfDepositAuth},
        {"disableMasterKey", ripple::lsfDisableMaster},
        {"disallowIncomingXRP", ripple::lsfDisallowXRP},
        {"globalFreeze", ripple::lsfGlobalFreeze},
        {"noFreeze", ripple::lsfNoFreeze},
        {"passwordSpent", ripple::lsfPasswordSpent},
        {"requireAuthorization", ripple::lsfRequireAuth},
        {"requireDestinationTag", ripple::lsfRequireDestTag},
    }};

    if (output.isDisallowIncomingEnabled) {
        std::vector<std::pair<std::string_view, ripple::LedgerSpecificFlags>> const
            disallowIncomingFlags = {
                {"disallowIncomingNFTokenOffer", ripple::lsfDisallowIncomingNFTokenOffer},
                {"disallowIncomingCheck", ripple::lsfDisallowIncomingCheck},
                {"disallowIncomingPayChan", ripple::lsfDisallowIncomingPayChan},
                {"disallowIncomingTrustline", ripple::lsfDisallowIncomingTrustline},
            };
        lsFlags.insert(lsFlags.end(), disallowIncomingFlags.begin(), disallowIncomingFlags.end());
    }

    if (output.isClawbackEnabled)
        lsFlags.emplace_back("allowTrustLineClawback", ripple::lsfAllowTrustLineClawback);

    if (output.isTokenEscrowEnabled)
        lsFlags.emplace_back("allowTrustLineLocking", ripple::lsfAllowTrustLineLocking);

    boost::json::object acctFlags;
    for (auto const& lsf : lsFlags)
        acctFlags[lsf.first] = output.accountData.isFlag(lsf.second);

    jv.as_object()[JS(account_flags)] = std::move(acctFlags);

    auto const pseudoFields = ripple::getPseudoAccountFields();
    for (auto const& pseudoField : pseudoFields) {
        if (output.accountData.isFieldPresent(*pseudoField)) {
            std::string_view name = pseudoField->fieldName;
            if (name.ends_with("ID")) {
                // Remove the ID suffix from the field name.
                name = name.substr(0, name.size() - 2);
                ASSERT(!name.empty(), "Field name is empty after stripping 'ID'");
            }
            // ValidPseudoAccounts invariant guarantees that only one field can be set
            jv.as_object()[JS(pseudo_account)].as_object()[JS(type)] = name;
            break;
        }
    }

    if (output.signerLists) {
        auto signers = boost::json::array();
        std::transform(
            std::cbegin(output.signerLists.value()),
            std::cend(output.signerLists.value()),
            std::back_inserter(signers),
            [](auto const& signerList) { return toJson(signerList); }
        );
        if (output.apiVersion == 1) {
            jv.as_object()[JS(account_data)].as_object()[JS(signer_lists)] = std::move(signers);
        } else {
            jv.as_object()[JS(signer_lists)] = signers;
        }
    }
}

AccountInfoHandler::Input
tag_invoke(boost::json::value_to_tag<AccountInfoHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountInfoHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ident)))
        input.ident = boost::json::value_to<std::string>(jsonObject.at(JS(ident)));

    if (jsonObject.contains(JS(account)))
        input.account = boost::json::value_to<std::string>(jsonObject.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(signer_lists)))
        input.signerLists = boost::json::value_to<JsonBool>(jsonObject.at(JS(signer_lists)));

    return input;
}

}  // namespace rpc
