#include "rpc/handlers/LedgerEntry.hpp"

#include "rpc/CredentialHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STXChainBridge.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace rpc {

LedgerEntryHandler::Result
LedgerEntryHandler::process(LedgerEntryHandler::Input const& input, Context const& ctx) const
{
    xrpl::uint256 key;

    if (input.index) {
        key = xrpl::uint256{std::string_view(*(input.index))};
        if (key.isZero())
            return Error{Status{RippledError::RpcEntryNotFound}};
    } else if (input.accountRoot) {
        key = xrpl::keylet::account(
                  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                  *util::parseBase58Wrapper<xrpl::AccountID>(*(input.accountRoot))
        )
                  .key;
    } else if (input.did) {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::did(*util::parseBase58Wrapper<xrpl::AccountID>(*(input.did))).key;
    } else if (input.directory) {
        auto const expectedkey = composeKeyFromDirectory(*input.directory);
        if (!expectedkey.has_value())
            return Error{expectedkey.error()};

        key = expectedkey.value();  // std::expected, not optional
    } else if (input.offer) {
        auto const id = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.offer->at(JS(account)))
        );

        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        key =
            xrpl::keylet::offer(*id, boost::json::value_to<std::uint32_t>(input.offer->at(JS(seq))))
                .key;
        // NOLINTEND(bugprone-unchecked-optional-access)
    } else if (input.rippleStateAccount) {
        auto const id1 =
            util::parseBase58Wrapper<xrpl::AccountID>(boost::json::value_to<std::string>(
                input.rippleStateAccount->at(JS(accounts)).as_array().at(0)
            ));
        auto const id2 =
            util::parseBase58Wrapper<xrpl::AccountID>(boost::json::value_to<std::string>(
                input.rippleStateAccount->at(JS(accounts)).as_array().at(1)
            ));
        auto const currency = xrpl::toCurrency(
            boost::json::value_to<std::string>(input.rippleStateAccount->at(JS(currency)))
        );

        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::line(*id1, *id2, currency).key;
    } else if (input.escrow) {
        auto const id = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.escrow->at(JS(owner)))
        );
        key =
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            xrpl::keylet::escrow(*id, util::integralValueAs<uint32_t>(input.escrow->at(JS(seq))))
                .key;
    } else if (input.depositPreauth) {
        auto const owner = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.depositPreauth->at(JS(owner)))
        );
        // Only one of authorize or authorized_credentials MUST exist;
        if (input.depositPreauth->contains(JS(authorized)) ==
            input.depositPreauth->contains(JS(authorized_credentials))) {
            return Error{Status{
                ClioError::RpcMalformedRequest,
                "Must have one of authorized or authorized_credentials."
            }};
        }

        if (input.depositPreauth->contains(JS(authorized))) {
            auto const authorized = util::parseBase58Wrapper<xrpl::AccountID>(
                boost::json::value_to<std::string>(input.depositPreauth->at(JS(authorized)))
            );
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            key = xrpl::keylet::depositPreauth(*owner, *authorized).key;
        } else {
            auto const authorizedCredentials = rpc::credentials::parseAuthorizeCredentials(
                input.depositPreauth->at(JS(authorized_credentials)).as_array()
            );

            auto const authCreds = credentials::createAuthCredentials(authorizedCredentials);
            if (authCreds.size() != authorizedCredentials.size()) {
                return Error{Status{
                    ClioError::RpcMalformedAuthorizedCredentials, "duplicates in credentials."
                }};
            }

            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            key = xrpl::keylet::depositPreauth(*owner, authCreds).key;
        }
    } else if (input.ticket) {
        auto const id = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.ticket->at(JS(account)))
        );

        key = xrpl::getTicketIndex(
            *id,  // NOLINT(bugprone-unchecked-optional-access)
            util::integralValueAs<uint32_t>(input.ticket->at(JS(ticket_seq)))
        );
    } else if (input.amm) {
        auto const getIssuerFromJson = [](auto const& assetJson) {
            // the field check has been done in validator
            auto const currency =
                xrpl::toCurrency(boost::json::value_to<std::string>(assetJson.at(JS(currency))));
            if (xrpl::isXRP(currency)) {
                return xrpl::xrpIssue();
            }
            auto const issuer = util::parseBase58Wrapper<xrpl::AccountID>(
                boost::json::value_to<std::string>(assetJson.at(JS(issuer)))
            );
            return xrpl::Issue{currency, *issuer};
        };

        key = xrpl::keylet::amm(
                  getIssuerFromJson(input.amm->at(JS(asset))),
                  getIssuerFromJson(input.amm->at(JS(asset2)))
        )
                  .key;
    } else if (input.bridge) {
        if (!input.bridgeAccount && !input.chainClaimId && !input.createAccountClaimId)
            return Error{Status{ClioError::RpcMalformedRequest}};

        if (input.bridgeAccount) {
            auto const bridgeAccount =
                util::parseBase58Wrapper<xrpl::AccountID>(*(input.bridgeAccount));
            auto const chainType =
                xrpl::STXChainBridge::srcChain(bridgeAccount == input.bridge->lockingChainDoor());

            if (bridgeAccount != input.bridge->door(chainType))
                return Error{Status{ClioError::RpcMalformedRequest}};

            key = xrpl::keylet::bridge(input.bridge->value(), chainType).key;
        } else if (input.chainClaimId) {
            key = xrpl::keylet::xChainClaimID(input.bridge->value(), *input.chainClaimId).key;
        } else {
            key = xrpl::keylet::xChainCreateAccountClaimID(
                      input.bridge->value(), *input.createAccountClaimId
            )
                      .key;
        }
    } else if (input.oracleNode) {
        key = *input.oracleNode;
    } else if (input.credential) {
        key = *input.credential;
    } else if (input.mptIssuance) {
        auto const mptIssuanceID = xrpl::uint192{std::string_view(*(input.mptIssuance))};
        key = xrpl::keylet::mptIssuance(mptIssuanceID).key;
    } else if (input.mptoken) {
        auto const holder = xrpl::parseBase58<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.mptoken->at(JS(account)))
        );
        auto const mptIssuanceID = xrpl::uint192{std::string_view(
            boost::json::value_to<std::string>(input.mptoken->at(JS(mpt_issuance_id)))
        )};
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::mptoken(mptIssuanceID, *holder).key;
    } else if (input.permissionedDomain) {
        auto const account = xrpl::parseBase58<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.permissionedDomain->at(JS(account)))
        );
        auto const seq = util::integralValueAs<uint32_t>(input.permissionedDomain->at(JS(seq)));
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::permissionedDomain(*account, seq).key;
    } else if (input.vault) {
        auto const account = xrpl::parseBase58<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.vault->at(JS(owner)))
        );
        auto const seq = util::integralValueAs<uint32_t>(input.vault->at(JS(seq)));
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::vault(*account, seq).key;
    } else if (input.loanBroker) {
        auto const account = xrpl::parseBase58<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.loanBroker->at(JS(owner)))
        );
        auto const seq = util::integralValueAs<uint32_t>(input.loanBroker->at(JS(seq)));
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::loanbroker(*account, seq).key;
    } else if (input.loan) {
        auto const id = xrpl::uint256{
            boost::json::value_to<std::string>(input.loan->at(JS(loan_broker_id))).data()
        };
        auto const seq = util::integralValueAs<uint32_t>(input.loan->at(JS(loan_seq)));
        key = xrpl::keylet::loan(id, seq).key;
    } else if (input.delegate) {
        auto const account = xrpl::parseBase58<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.delegate->at(JS(account)))
        );
        auto const authorize = xrpl::parseBase58<xrpl::AccountID>(
            boost::json::value_to<std::string>(input.delegate->at(JS(authorize)))
        );
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        key = xrpl::keylet::delegate(*account, *authorize).key;
    } else {
        // Must specify 1 of the following fields to indicate what type
        if (ctx.apiVersion == 1)
            return Error{Status{ClioError::RpcUnknownOption}};
        return Error{Status{RippledError::RpcInvalidParams, "No ledger_entry params provided."}};
    }

    // check ledger exists
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "LedgerEntry's ledger range must be available");
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
    auto output = LedgerEntryHandler::Output{};
    auto ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, lgrInfo.seq, ctx.yield);

    if (!ledgerObject || ledgerObject->empty()) {
        if (not input.includeDeleted)
            return Error{Status{RippledError::RpcEntryNotFound}};
        auto const deletedSeq =
            sharedPtrBackend_->fetchLedgerObjectSeq(key, lgrInfo.seq, ctx.yield);
        if (!deletedSeq)
            return Error{Status{RippledError::RpcEntryNotFound}};
        ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, *deletedSeq - 1, ctx.yield);
        if (!ledgerObject || ledgerObject->empty())
            return Error{Status{RippledError::RpcEntryNotFound}};
        output.deletedLedgerIndex = deletedSeq;
    }

    xrpl::STLedgerEntry const sle{
        xrpl::SerialIter{ledgerObject->data(), ledgerObject->size()}, key
    };

    if (input.expectedType != xrpl::ltANY && sle.getType() != input.expectedType)
        return Error{Status{RippledError::RpcUnexpectedLedgerType}};

    output.index = xrpl::strHex(key);
    output.ledgerIndex = lgrInfo.seq;
    output.ledgerHash = xrpl::strHex(lgrInfo.hash);

    if (input.binary) {
        output.nodeBinary = xrpl::strHex(*ledgerObject);
    } else {
        output.node = toJson(sle);
    }

    return output;
}

std::expected<xrpl::uint256, Status>
LedgerEntryHandler::composeKeyFromDirectory(boost::json::object const& directory) noexcept
{
    // can not specify both dir_root and owner.
    if (directory.contains(JS(dir_root)) && directory.contains(JS(owner))) {
        return std::unexpected{
            Status{RippledError::RpcInvalidParams, "mayNotSpecifyBothDirRootAndOwner"}
        };
    }

    // at least one should available
    if (!(directory.contains(JS(dir_root)) || directory.contains(JS(owner))))
        return std::unexpected{Status{RippledError::RpcInvalidParams, "missingOwnerOrDirRoot"}};

    uint64_t const subIndex = directory.contains(JS(sub_index))
        ? boost::json::value_to<uint64_t>(directory.at(JS(sub_index)))
        : 0;

    if (directory.contains(JS(dir_root))) {
        xrpl::uint256 const uDirRoot{
            boost::json::value_to<std::string>(directory.at(JS(dir_root))).data()
        };
        return xrpl::keylet::page(uDirRoot, subIndex).key;
    }

    auto const ownerID = util::parseBase58Wrapper<xrpl::AccountID>(
        boost::json::value_to<std::string>(directory.at(JS(owner)))
    );
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return xrpl::keylet::page(xrpl::keylet::ownerDir(*ownerID), subIndex).key;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    LedgerEntryHandler::Output const& output
)
{
    auto object = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(index), output.index},
    };

    if (output.deletedLedgerIndex)
        object["deleted_ledger_index"] = *(output.deletedLedgerIndex);

    if (output.nodeBinary) {
        object[JS(node_binary)] = *(output.nodeBinary);
    } else {
        object[JS(node)] = *(output.node);  // NOLINT(bugprone-unchecked-optional-access)
    }

    jv = std::move(object);
}

LedgerEntryHandler::Input
tag_invoke(boost::json::value_to_tag<LedgerEntryHandler::Input>, boost::json::value const& jv)
{
    auto input = LedgerEntryHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(binary)))
        input.binary = jv.at(JS(binary)).as_bool();

    // check all the potential index
    static auto const kIndexFieldTypeMap = std::unordered_map<std::string, xrpl::LedgerEntryType>{
        {JS(index), xrpl::ltANY},
        {JS(directory), xrpl::ltDIR_NODE},
        {JS(offer), xrpl::ltOFFER},
        {JS(check), xrpl::ltCHECK},
        {JS(escrow), xrpl::ltESCROW},
        {JS(payment_channel), xrpl::ltPAYCHAN},
        {JS(deposit_preauth), xrpl::ltDEPOSIT_PREAUTH},
        {JS(ticket), xrpl::ltTICKET},
        {JS(nft_page), xrpl::ltNFTOKEN_PAGE},
        {JS(amm), xrpl::ltAMM},
        {JS(xchain_owned_create_account_claim_id), xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
        {JS(xchain_owned_claim_id), xrpl::ltXCHAIN_OWNED_CLAIM_ID},
        {JS(oracle), xrpl::ltORACLE},
        {JS(credential), xrpl::ltCREDENTIAL},
        {JS(mptoken), xrpl::ltMPTOKEN},
        {JS(permissioned_domain), xrpl::ltPERMISSIONED_DOMAIN},
        {JS(vault), xrpl::ltVAULT},
        {JS(loan_broker), xrpl::ltLOAN_BROKER},
        {JS(loan), xrpl::ltLOAN},
        {JS(delegate), xrpl::ltDELEGATE},
        {JS(amendments), xrpl::ltAMENDMENTS},
        {JS(fee), xrpl::ltFEE_SETTINGS},
        {JS(hashes), xrpl::ltLEDGER_HASHES},
        {JS(nft_offer), xrpl::ltNFTOKEN_OFFER},
        {JS(nunl), xrpl::ltNEGATIVE_UNL},
        {JS(signer_list), xrpl::ltSIGNER_LIST},
    };

    auto const parseBridgeFromJson = [](boost::json::value const& bridgeJson) {
        auto const lockingDoor =
            *util::parseBase58Wrapper<xrpl::AccountID>(boost::json::value_to<std::string>(
                bridgeJson.at(xrpl::sfLockingChainDoor.getJsonName().cStr())
            ));
        auto const issuingDoor =
            *util::parseBase58Wrapper<xrpl::AccountID>(boost::json::value_to<std::string>(
                bridgeJson.at(xrpl::sfIssuingChainDoor.getJsonName().cStr())
            ));
        auto const lockingIssue =
            parseIssue(bridgeJson.at(xrpl::sfLockingChainIssue.getJsonName().cStr()).as_object());
        auto const issuingIssue =
            parseIssue(bridgeJson.at(xrpl::sfIssuingChainIssue.getJsonName().cStr()).as_object());

        return xrpl::STXChainBridge{lockingDoor, lockingIssue, issuingDoor, issuingIssue};
    };

    auto const parseOracleFromJson = [](boost::json::value const& json) {
        auto const account = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(json.at(JS(account)))
        );
        auto const documentId = boost::json::value_to<uint32_t>(json.at(JS(oracle_document_id)));

        return xrpl::keylet::oracle(*account, documentId).key;
    };

    auto const parseCredentialFromJson = [](boost::json::value const& json) {
        auto const subject = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(json.at(JS(subject)))
        );
        auto const issuer = util::parseBase58Wrapper<xrpl::AccountID>(
            boost::json::value_to<std::string>(json.at(JS(issuer)))
        );
        auto const credType =
            xrpl::strUnHex(boost::json::value_to<std::string>(json.at(JS(credential_type))));

        return xrpl::keylet::credential(
                   *subject, *issuer, xrpl::Slice(credType->data(), credType->size())
        )
            .key;
    };

    auto const indexFieldType =
        std::ranges::find_if(kIndexFieldTypeMap, [&jsonObject](auto const& pair) {
            auto const& [field, _] = pair;
            return jsonObject.contains(field) && jsonObject.at(field).is_string();
        });

    if (indexFieldType != kIndexFieldTypeMap.end()) {
        input.index = boost::json::value_to<std::string>(jv.at(indexFieldType->first));
        input.expectedType = indexFieldType->second;
    }
    // check if request for account root
    else if (jsonObject.contains(JS(account_root))) {
        input.accountRoot = boost::json::value_to<std::string>(jv.at(JS(account_root)));
    } else if (jsonObject.contains(JS(did))) {
        input.did = boost::json::value_to<std::string>(jv.at(JS(did)));
    } else if (jsonObject.contains(JS(mpt_issuance))) {
        input.mptIssuance = boost::json::value_to<std::string>(jv.at(JS(mpt_issuance)));
    }
    // no need to check if_object again, validator only allows string or object
    else if (jsonObject.contains(JS(directory))) {
        input.directory = jv.at(JS(directory)).as_object();
    } else if (jsonObject.contains(JS(offer))) {
        input.offer = jv.at(JS(offer)).as_object();
    } else if (jsonObject.contains(JS(ripple_state))) {
        input.rippleStateAccount = jv.at(JS(ripple_state)).as_object();
    } else if (jsonObject.contains(JS(escrow))) {
        input.escrow = jv.at(JS(escrow)).as_object();
    } else if (jsonObject.contains(JS(deposit_preauth))) {
        input.depositPreauth = jv.at(JS(deposit_preauth)).as_object();
    } else if (jsonObject.contains(JS(ticket))) {
        input.ticket = jv.at(JS(ticket)).as_object();
    } else if (jsonObject.contains(JS(amm))) {
        input.amm = jv.at(JS(amm)).as_object();
    } else if (jsonObject.contains(JS(bridge))) {
        input.bridge.emplace(parseBridgeFromJson(jv.at(JS(bridge))));
        if (jsonObject.contains(JS(bridge_account)))
            input.bridgeAccount = boost::json::value_to<std::string>(jv.at(JS(bridge_account)));
    } else if (jsonObject.contains(JS(xchain_owned_claim_id))) {
        input.bridge.emplace(parseBridgeFromJson(jv.at(JS(xchain_owned_claim_id))));
        input.chainClaimId = boost::json::value_to<std::int32_t>(
            jv.at(JS(xchain_owned_claim_id)).at(JS(xchain_owned_claim_id))
        );
    } else if (jsonObject.contains(JS(xchain_owned_create_account_claim_id))) {
        input.bridge.emplace(parseBridgeFromJson(jv.at(JS(xchain_owned_create_account_claim_id))));
        input.createAccountClaimId =
            boost::json::value_to<std::int32_t>(jv.at(JS(xchain_owned_create_account_claim_id))
                                                    .at(JS(xchain_owned_create_account_claim_id)));
    } else if (jsonObject.contains(JS(oracle))) {
        input.oracleNode = parseOracleFromJson(jv.at(JS(oracle)));
    } else if (jsonObject.contains(JS(credential))) {
        input.credential = parseCredentialFromJson(jv.at(JS(credential)));
    } else if (jsonObject.contains(JS(mptoken))) {
        input.mptoken = jv.at(JS(mptoken)).as_object();
    } else if (jsonObject.contains(JS(permissioned_domain))) {
        input.permissionedDomain = jv.at(JS(permissioned_domain)).as_object();
    } else if (jsonObject.contains(JS(vault))) {
        input.vault = jv.at(JS(vault)).as_object();
    } else if (jsonObject.contains(JS(loan_broker))) {
        input.loanBroker = jv.at(JS(loan_broker)).as_object();
    } else if (jsonObject.contains(JS(loan))) {
        input.loan = jv.at(JS(loan)).as_object();
    } else if (jsonObject.contains(JS(delegate))) {
        input.delegate = jv.at(JS(delegate)).as_object();
    }

    if (jsonObject.contains("include_deleted"))
        input.includeDeleted = jv.at("include_deleted").as_bool();

    return input;
}

}  // namespace rpc
