#include "rpc/handlers/LedgerEntry.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STXChainBridge.h>
#include <xrpl/protocol/SeqProxy.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <expected>
#include <set>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {

LedgerEntryHandler::Result
LedgerEntryHandler::process(LedgerEntryHandler::Input const& input, Context const& ctx) const
{
    using namespace rpc::spec::handlers::ledger_entry;

    auto const makeBridge = [](BridgeSpec const& spec) {
        return xrpl::STXChainBridge{
            spec.lockingChainDoor,
            spec.lockingChainIssue,
            spec.issuingChainDoor,
            spec.issuingChainIssue
        };
    };

    xrpl::uint256 key;
    // For locators supplied as a raw ledger-entry hex key, the type is implied and
    // enforced below; a precisely-computed keylet leaves this as ltANY (no check).
    xrpl::LedgerEntryType expectedType = xrpl::ltANY;

    if (input.index.has_value()) {
        key = *input.index;
        if (key.isZero())
            return Error{Status{RippledError::RpcEntryNotFound}};
    } else if (input.accountRoot.has_value()) {
        key = xrpl::keylet::account(*input.accountRoot).key;
    } else if (input.did.has_value()) {
        key = xrpl::keylet::did(*input.did).key;
    } else if (input.check.has_value()) {
        key = *input.check;
        expectedType = xrpl::ltCHECK;
    } else if (input.paymentChannel.has_value()) {
        key = *input.paymentChannel;
        expectedType = xrpl::ltPAYCHAN;
    } else if (input.nftPage.has_value()) {
        key = *input.nftPage;
        expectedType = xrpl::ltNFTOKEN_PAGE;
    } else if (input.nftOffer.has_value()) {
        key = *input.nftOffer;
        expectedType = xrpl::ltNFTOKEN_OFFER;
    } else if (input.signerList.has_value()) {
        key = *input.signerList;
        expectedType = xrpl::ltSIGNER_LIST;
    } else if (input.amendments.has_value()) {
        key = *input.amendments;
        expectedType = xrpl::ltAMENDMENTS;
    } else if (input.fee.has_value()) {
        key = *input.fee;
        expectedType = xrpl::ltFEE_SETTINGS;
    } else if (input.hashes.has_value()) {
        key = *input.hashes;
        expectedType = xrpl::ltLEDGER_HASHES;
    } else if (input.nunl.has_value()) {
        key = *input.nunl;
        expectedType = xrpl::ltNEGATIVE_UNL;
    } else if (input.mptIssuance.has_value()) {
        key = xrpl::keylet::mptokenIssuance(*input.mptIssuance).key;
    } else if (input.directory.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.directory)) {
            key = *hash;
            expectedType = xrpl::ltDIR_NODE;
        } else {
            auto const& dirEntry = std::get<DirectoryEntry>(*input.directory);
            if (dirEntry.dirRoot.has_value() && dirEntry.owner.has_value()) {
                return Error{
                    Status{RippledError::RpcInvalidParams, "mayNotSpecifyBothDirRootAndOwner"}
                };
            }
            if (not dirEntry.dirRoot.has_value() and not dirEntry.owner.has_value())
                return Error{Status{RippledError::RpcInvalidParams, "missingOwnerOrDirRoot"}};

            uint64_t const subIndex = dirEntry.subIndex.value_or(0);
            if (dirEntry.dirRoot.has_value()) {
                key = xrpl::keylet::page(*dirEntry.dirRoot, subIndex).key;
            } else {
                key = xrpl::keylet::page(xrpl::keylet::ownerDir(*dirEntry.owner), subIndex).key;
            }
        }
    } else if (input.offer.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.offer)) {
            key = *hash;
            expectedType = xrpl::ltOFFER;
        } else {
            auto const& entry = std::get<OfferEntry>(*input.offer);
            key = xrpl::keylet::offer(entry.account, xrpl::SeqProxy::rawSequence(entry.seq)).key;
        }
    } else if (input.rippleStateAccount.has_value()) {
        auto const& rippleState = *input.rippleStateAccount;
        key = xrpl::keylet::trustLine(
                  rippleState.accounts[0], rippleState.accounts[1], rippleState.currency
        )
                  .key;
    } else if (input.escrow.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.escrow)) {
            key = *hash;
            expectedType = xrpl::ltESCROW;
        } else {
            auto const& entry = std::get<EscrowEntry>(*input.escrow);
            key = xrpl::keylet::escrow(entry.owner, xrpl::SeqProxy::rawSequence(entry.seq)).key;
        }
    } else if (input.depositPreauth.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.depositPreauth)) {
            key = *hash;
            expectedType = xrpl::ltDEPOSIT_PREAUTH;
        } else {
            auto const& preauthEntry = std::get<DepositPreauthEntry>(*input.depositPreauth);
            // Exactly one of authorized or authorized_credentials MUST exist.
            if (preauthEntry.authorized.has_value() ==
                preauthEntry.authorizedCredentials.has_value()) {
                return Error{Status{
                    ClioError::RpcMalformedRequest,
                    "Must have one of authorized or authorized_credentials."
                }};
            }

            if (preauthEntry.authorized.has_value()) {
                key =
                    xrpl::keylet::depositPreauth(preauthEntry.owner, *preauthEntry.authorized).key;
            } else {
                std::set<std::pair<xrpl::AccountID, xrpl::Slice>> authCreds;
                // Keep the decoded credential-type bytes alive while the Slices
                // that reference them are used to build the keylet.
                std::vector<xrpl::Blob> buffers;
                buffers.reserve(preauthEntry.authorizedCredentials->size());
                for (auto const& cred : *preauthEntry.authorizedCredentials) {
                    auto const decoded = xrpl::strUnHex(cred.credentialType);
                    ASSERT(decoded.has_value(), "credential_type is hex-validated by the spec");
                    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                    buffers.push_back(*decoded);
                    authCreds.emplace(
                        cred.issuer, xrpl::Slice(buffers.back().data(), buffers.back().size())
                    );
                }

                if (authCreds.size() != preauthEntry.authorizedCredentials->size()) {
                    return Error{Status{
                        ClioError::RpcMalformedAuthorizedCredentials, "duplicates in credentials."
                    }};
                }

                key = xrpl::keylet::depositPreauth(preauthEntry.owner, authCreds).key;
            }
        }
    } else if (input.ticket.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.ticket)) {
            key = *hash;
            expectedType = xrpl::ltTICKET;
        } else {
            auto const& entry = std::get<TicketEntry>(*input.ticket);
            key =
                xrpl::keylet::ticket(entry.account, xrpl::SeqProxy::rawTicket(entry.ticketSeq)).key;
        }
    } else if (input.amm.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.amm)) {
            key = *hash;
            expectedType = xrpl::ltAMM;
        } else {
            auto const& entry = std::get<AmmEntry>(*input.amm);
            key = xrpl::keylet::amm(entry.asset, entry.asset2).key;
        }
    } else if (input.bridge.has_value()) {
        if (not input.bridgeAccount.has_value())
            return Error{Status{ClioError::RpcMalformedRequest}};

        auto const stBridge = makeBridge(*input.bridge);
        auto const& bridgeAccount = *input.bridgeAccount;
        auto const chainType =
            xrpl::STXChainBridge::srcChain(bridgeAccount == input.bridge->lockingChainDoor);

        if (bridgeAccount != stBridge.door(chainType))
            return Error{Status{ClioError::RpcMalformedRequest}};

        key = xrpl::keylet::bridge(stBridge, chainType).key;
    } else if (input.xchainOwnedClaimId.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.xchainOwnedClaimId)) {
            key = *hash;
            expectedType = xrpl::ltXCHAIN_OWNED_CLAIM_ID;
        } else {
            auto const& entry = std::get<XChainClaimIdEntry>(*input.xchainOwnedClaimId);
            key = xrpl::keylet::xChainClaimID(makeBridge(entry.bridge), entry.claimId).key;
        }
    } else if (input.xchainOwnedCreateAccountClaimId.has_value()) {
        if (auto const* hash =
                std::get_if<xrpl::uint256>(&*input.xchainOwnedCreateAccountClaimId)) {
            key = *hash;
            expectedType = xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID;
        } else {
            auto const& entry =
                std::get<XChainClaimIdEntry>(*input.xchainOwnedCreateAccountClaimId);
            key = xrpl::keylet::xChainCreateAccountClaimID(makeBridge(entry.bridge), entry.claimId)
                      .key;
        }
    } else if (input.oracle.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.oracle)) {
            key = *hash;
            expectedType = xrpl::ltORACLE;
        } else {
            auto const& entry = std::get<OracleEntry>(*input.oracle);
            key = xrpl::keylet::oracle(entry.account, entry.oracleDocumentId).key;
        }
    } else if (input.credential.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.credential)) {
            key = *hash;
            expectedType = xrpl::ltCREDENTIAL;
        } else {
            auto const& entry = std::get<CredentialEntry>(*input.credential);
            auto const credType = xrpl::strUnHex(entry.credentialType);
            ASSERT(credType.has_value(), "credential_type is not a hex");
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            auto const credSlice = xrpl::Slice(credType->data(), credType->size());
            key = xrpl::keylet::credential(entry.subject, entry.issuer, credSlice).key;
        }
    } else if (input.mptoken.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.mptoken)) {
            key = *hash;
            expectedType = xrpl::ltMPTOKEN;
        } else {
            auto const& entry = std::get<MptokenEntry>(*input.mptoken);
            key = xrpl::keylet::mptoken(entry.mptIssuanceId, entry.account).key;
        }
    } else if (input.permissionedDomain.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.permissionedDomain)) {
            key = *hash;
            expectedType = xrpl::ltPERMISSIONED_DOMAIN;
        } else {
            auto const& entry = std::get<PermissionedDomainEntry>(*input.permissionedDomain);
            key = xrpl::keylet::permissionedDomain(
                      entry.account, xrpl::SeqProxy::rawSequence(entry.seq)
            )
                      .key;
        }
    } else if (input.vault.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.vault)) {
            key = *hash;
            expectedType = xrpl::ltVAULT;
        } else {
            auto const& entry = std::get<VaultEntry>(*input.vault);
            key = xrpl::keylet::vault(entry.owner, xrpl::SeqProxy::rawSequence(entry.seq)).key;
        }
    } else if (input.loanBroker.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.loanBroker)) {
            key = *hash;
            expectedType = xrpl::ltLOAN_BROKER;
        } else {
            auto const& entry = std::get<LoanBrokerEntry>(*input.loanBroker);
            key = xrpl::keylet::loanBroker(entry.owner, xrpl::SeqProxy::rawSequence(entry.seq)).key;
        }
    } else if (input.loan.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.loan)) {
            key = *hash;
            expectedType = xrpl::ltLOAN;
        } else {
            auto const& entry = std::get<LoanEntry>(*input.loan);
            key = xrpl::keylet::loan(entry.loanBrokerId, xrpl::SeqProxy::rawSequence(entry.loanSeq))
                      .key;
        }
    } else if (input.delegate.has_value()) {
        if (auto const* hash = std::get_if<xrpl::uint256>(&*input.delegate)) {
            key = *hash;
            expectedType = xrpl::ltDELEGATE;
        } else {
            auto const& entry = std::get<DelegateEntry>(*input.delegate);
            key = xrpl::keylet::delegate(entry.account, entry.authorize).key;
        }
    } else {
        if (ctx.apiVersion == 1)
            return Error{Status{ClioError::RpcUnknownOption}};
        return Error{Status{RippledError::RpcInvalidParams, "No ledger_entry params provided."}};
    }

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "LedgerEntry's ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto output = LedgerEntryHandler::Output{};
    auto ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, lgrInfo.seq, ctx.yield);

    if (not ledgerObject.has_value() or ledgerObject->empty()) {
        if (not input.includeDeleted)
            return Error{Status{RippledError::RpcEntryNotFound}};
        auto const deletedSeq =
            sharedPtrBackend_->fetchLedgerObjectSeq(key, lgrInfo.seq, ctx.yield);
        if (not deletedSeq.has_value())
            return Error{Status{RippledError::RpcEntryNotFound}};
        ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, *deletedSeq - 1, ctx.yield);
        if (not ledgerObject.has_value() or ledgerObject->empty())
            return Error{Status{RippledError::RpcEntryNotFound}};
        output.deletedLedgerIndex = deletedSeq;
    }

    xrpl::STLedgerEntry const sle{
        xrpl::SerialIter{ledgerObject->data(), ledgerObject->size()}, key
    };

    if (expectedType != xrpl::ltANY && sle.getType() != expectedType)
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

    if (output.deletedLedgerIndex.has_value())
        object["deleted_ledger_index"] = *(output.deletedLedgerIndex);

    if (output.nodeBinary.has_value()) {
        object[JS(node_binary)] = *(output.nodeBinary);
    } else {
        object[JS(node)] = *(output.node);  // NOLINT(bugprone-unchecked-optional-access)
    }

    jv = std::move(object);
}

}  // namespace rpc
