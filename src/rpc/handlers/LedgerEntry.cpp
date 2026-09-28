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

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {

namespace {

namespace le = rpc::spec::handlers::ledger_entry;
using Input = LedgerEntryHandler::Input;

/**
 * @brief The ledger key a request resolves to, plus the entry type it must have.
 *
 * @c expectedType is @c ltANY when the key was computed from a precise keylet, since the
 * type is then implied by construction and needs no verification after the read.
 */
struct Locator {
    xrpl::uint256 key;
    xrpl::LedgerEntryType expectedType = xrpl::ltANY;
};

/**
 * @brief A resolved Locator, or the Status explaining why the request could not name one.
 */
using LocatorOrStatus = std::expected<Locator, Status>;

xrpl::STXChainBridge
makeBridge(le::BridgeSpec const& spec)
{
    return xrpl::STXChainBridge{
        spec.lockingChainDoor, spec.lockingChainIssue, spec.issuingChainDoor, spec.issuingChainIssue
    };
}

/**
 * @brief Resolve a locator field that accepts either a hex key or an entry object.
 *
 * A raw hex key only names the entry, so @p type is recorded and verified after the read.
 * The object form computes a precise keylet via @p fromEntry and needs no verification.
 *
 * @param field The variant field to resolve
 * @param type The entry type implied by the hex form
 * @param fromEntry Computes the locator from the object form
 * @return The resolved locator, or a Status describing why it could not be
 */
template <typename Entry, typename FromEntry>
LocatorOrStatus
locatorFrom(
    std::variant<xrpl::uint256, Entry> const& field,
    xrpl::LedgerEntryType type,
    FromEntry&& fromEntry
)
{
    if (auto const* hash = std::get_if<xrpl::uint256>(&field))
        return Locator{.key = *hash, .expectedType = type};

    return std::forward<FromEntry>(fromEntry)(std::get<Entry>(field));
}

/**
 * @brief One kHexLocators entry: the Input field carrying the key, and the type it implies.
 */
struct HexLocator {
    std::optional<xrpl::uint256> Input::* field;
    xrpl::LedgerEntryType type;
};

/**
 * @brief Locator fields that carry a ledger key directly, each implying its entry type.
 *
 * These are pure data: the field is the key, and naming the field names the type. Kept in
 * request-field order so the search order matches the rest of resolveLocator().
 */
constexpr auto kHexLocators = std::to_array<HexLocator>({
    {.field = &Input::check, .type = xrpl::ltCHECK},
    {.field = &Input::paymentChannel, .type = xrpl::ltPAYCHAN},
    {.field = &Input::nftPage, .type = xrpl::ltNFTOKEN_PAGE},
    {.field = &Input::nftOffer, .type = xrpl::ltNFTOKEN_OFFER},
    {.field = &Input::signerList, .type = xrpl::ltSIGNER_LIST},
    {.field = &Input::amendments, .type = xrpl::ltAMENDMENTS},
    {.field = &Input::fee, .type = xrpl::ltFEE_SETTINGS},
    {.field = &Input::hashes, .type = xrpl::ltLEDGER_HASHES},
    {.field = &Input::nunl, .type = xrpl::ltNEGATIVE_UNL},
});

LocatorOrStatus
directoryLocator(le::DirectoryEntry const& entry)
{
    // xrpld folds these into a single "exactly one of" check; Clio reports them separately.
    // This should be unified after xrpld is migrated to rpc-spec.
    if (entry.dirRoot.has_value() and entry.owner.has_value()) {
        return std::unexpected{
            Status{XrpldError::RpcInvalidParams, "mayNotSpecifyBothDirRootAndOwner"}
        };
    }

    if (not entry.dirRoot.has_value() and not entry.owner.has_value())
        return std::unexpected{Status{XrpldError::RpcInvalidParams, "missingOwnerOrDirRoot"}};

    auto const subIndex = entry.subIndex.value_or(0);
    if (entry.dirRoot.has_value())
        return Locator{.key = xrpl::keylet::page(*entry.dirRoot, subIndex).key};

    return Locator{.key = xrpl::keylet::page(xrpl::keylet::ownerDir(*entry.owner), subIndex).key};
}

LocatorOrStatus
depositPreauthLocator(le::DepositPreauthEntry const& entry)
{
    // Exactly one of authorized or authorized_credentials MUST exist.
    if (entry.authorized.has_value() == entry.authorizedCredentials.has_value()) {
        return std::unexpected{Status{
            ClioError::RpcMalformedRequest, "Must have one of authorized or authorized_credentials."
        }};
    }

    if (entry.authorized.has_value())
        return Locator{.key = xrpl::keylet::depositPreauth(entry.owner, *entry.authorized).key};

    std::set<std::pair<xrpl::AccountID, xrpl::Slice>> authCreds;

    // Keep the decoded credential-type bytes alive while the Slices that reference them
    // are used to build the keylet.
    std::vector<xrpl::Blob> buffers;
    buffers.reserve(entry.authorizedCredentials->size());

    for (auto const& cred : *entry.authorizedCredentials) {
        auto const decoded = xrpl::strUnHex(cred.credentialType);
        ASSERT(decoded.has_value(), "credential_type is hex-validated by the spec");
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        buffers.push_back(*decoded);
        authCreds.emplace(cred.issuer, xrpl::Slice(buffers.back().data(), buffers.back().size()));
    }

    if (authCreds.size() != entry.authorizedCredentials->size()) {
        return std::unexpected{
            Status{ClioError::RpcMalformedAuthorizedCredentials, "duplicates in credentials."}
        };
    }

    return Locator{.key = xrpl::keylet::depositPreauth(entry.owner, authCreds).key};
}

LocatorOrStatus
credentialLocator(le::CredentialEntry const& entry)
{
    auto const credType = xrpl::strUnHex(entry.credentialType);
    ASSERT(credType.has_value(), "credential_type is hex-validated by the spec");
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto const credSlice = xrpl::Slice(credType->data(), credType->size());

    return Locator{.key = xrpl::keylet::credential(entry.subject, entry.issuer, credSlice).key};
}

/**
 * @brief Resolve the `bridge` locator, the only one needing a second request field.
 *
 * @param bridge The `bridge` field
 * @param bridgeAccount The `bridge_account` field, which is required alongside it
 * @return The resolved locator, or a Status describing why it could not be
 */
LocatorOrStatus
bridgeLocator(le::BridgeSpec const& bridge, std::optional<xrpl::AccountID> const& bridgeAccount)
{
    if (not bridgeAccount.has_value())
        return std::unexpected{Status{ClioError::RpcMalformedRequest}};

    auto const stBridge = makeBridge(bridge);
    auto const chainType =
        xrpl::STXChainBridge::srcChain(*bridgeAccount == bridge.lockingChainDoor);

    if (*bridgeAccount != stBridge.door(chainType))
        return std::unexpected{Status{ClioError::RpcMalformedRequest}};

    return Locator{.key = xrpl::keylet::bridge(stBridge, chainType).key};
}

/**
 * @brief Work out which ledger key the request asks for.
 *
 * The spec admits one locator field per request; fields are searched in request-field order
 * so that a malformed multi-locator request resolves deterministically.
 *
 * @param input The request input
 * @param apiVersion The API version, which selects the error for a request with no locator
 * @return The resolved locator, or a Status describing why it could not be
 */
LocatorOrStatus
resolveLocator(Input const& input, uint32_t apiVersion)
{
    if (input.index.has_value()) {
        // A raw key names no entry type, so nothing is verified after the read.
        if (input.index->isZero())
            return std::unexpected{Status{XrpldError::RpcEntryNotFound}};

        return Locator{.key = *input.index};
    }

    if (input.accountRoot.has_value())
        return Locator{.key = xrpl::keylet::account(*input.accountRoot).key};

    if (input.did.has_value())
        return Locator{.key = xrpl::keylet::did(*input.did).key};

    for (auto const& [field, type] : kHexLocators) {
        if (auto const& hexKey = input.*field; hexKey.has_value())
            return Locator{.key = *hexKey, .expectedType = type};
    }

    if (input.mptIssuance.has_value())
        return Locator{.key = xrpl::keylet::mptokenIssuance(*input.mptIssuance).key};

    if (input.directory.has_value())
        return locatorFrom(*input.directory, xrpl::ltDIR_NODE, directoryLocator);

    if (input.offer.has_value()) {
        return locatorFrom(*input.offer, xrpl::ltOFFER, [](le::OfferEntry const& entry) {
            return LocatorOrStatus{Locator{
                .key =
                    xrpl::keylet::offer(entry.account, xrpl::SeqProxy::rawSequence(entry.seq)).key
            }};
        });
    }

    if (input.rippleStateAccount.has_value()) {
        auto const& state = *input.rippleStateAccount;
        return Locator{
            .key = xrpl::keylet::trustLine(state.accounts[0], state.accounts[1], state.currency).key
        };
    }

    if (input.escrow.has_value()) {
        return locatorFrom(*input.escrow, xrpl::ltESCROW, [](le::EscrowEntry const& entry) {
            return LocatorOrStatus{Locator{
                .key = xrpl::keylet::escrow(entry.owner, xrpl::SeqProxy::rawSequence(entry.seq)).key
            }};
        });
    }

    if (input.depositPreauth.has_value())
        return locatorFrom(*input.depositPreauth, xrpl::ltDEPOSIT_PREAUTH, depositPreauthLocator);

    if (input.ticket.has_value()) {
        return locatorFrom(*input.ticket, xrpl::ltTICKET, [](le::TicketEntry const& entry) {
            return LocatorOrStatus{Locator{
                .key =
                    xrpl::keylet::ticket(entry.account, xrpl::SeqProxy::rawTicket(entry.ticketSeq))
                        .key
            }};
        });
    }

    if (input.amm.has_value()) {
        return locatorFrom(*input.amm, xrpl::ltAMM, [](le::AmmEntry const& entry) {
            return LocatorOrStatus{
                Locator{.key = xrpl::keylet::amm(entry.asset, entry.asset2).key}
            };
        });
    }

    if (input.bridge.has_value())
        return bridgeLocator(*input.bridge, input.bridgeAccount);

    if (input.xchainOwnedClaimId.has_value()) {
        return locatorFrom(
            *input.xchainOwnedClaimId,
            xrpl::ltXCHAIN_OWNED_CLAIM_ID,
            [](le::XChainClaimIdEntry const& entry) {
                return LocatorOrStatus{Locator{
                    .key = xrpl::keylet::xChainClaimID(makeBridge(entry.bridge), entry.claimId).key
                }};
            }
        );
    }

    if (input.xchainOwnedCreateAccountClaimId.has_value()) {
        return locatorFrom(
            *input.xchainOwnedCreateAccountClaimId,
            xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID,
            [](le::XChainClaimIdEntry const& entry) {
                return LocatorOrStatus{Locator{
                    .key = xrpl::keylet::xChainCreateAccountClaimID(
                               makeBridge(entry.bridge), entry.claimId
                    )
                               .key
                }};
            }
        );
    }

    if (input.oracle.has_value()) {
        return locatorFrom(*input.oracle, xrpl::ltORACLE, [](le::OracleEntry const& entry) {
            return LocatorOrStatus{
                Locator{.key = xrpl::keylet::oracle(entry.account, entry.oracleDocumentId).key}
            };
        });
    }

    if (input.credential.has_value())
        return locatorFrom(*input.credential, xrpl::ltCREDENTIAL, credentialLocator);

    if (input.mptoken.has_value()) {
        return locatorFrom(*input.mptoken, xrpl::ltMPTOKEN, [](le::MptokenEntry const& entry) {
            return LocatorOrStatus{
                Locator{.key = xrpl::keylet::mptoken(entry.mptIssuanceId, entry.account).key}
            };
        });
    }

    if (input.permissionedDomain.has_value()) {
        return locatorFrom(
            *input.permissionedDomain,
            xrpl::ltPERMISSIONED_DOMAIN,
            [](le::PermissionedDomainEntry const& entry) {
                return LocatorOrStatus{Locator{
                    .key = xrpl::keylet::permissionedDomain(
                               entry.account, xrpl::SeqProxy::rawSequence(entry.seq)
                    )
                               .key
                }};
            }
        );
    }

    if (input.vault.has_value()) {
        return locatorFrom(*input.vault, xrpl::ltVAULT, [](le::VaultEntry const& entry) {
            return LocatorOrStatus{Locator{
                .key = xrpl::keylet::vault(entry.owner, xrpl::SeqProxy::rawSequence(entry.seq)).key
            }};
        });
    }

    if (input.loanBroker.has_value()) {
        return locatorFrom(
            *input.loanBroker, xrpl::ltLOAN_BROKER, [](le::LoanBrokerEntry const& entry) {
                return LocatorOrStatus{Locator{
                    .key = xrpl::keylet::loanBroker(
                               entry.owner, xrpl::SeqProxy::rawSequence(entry.seq)
                    )
                               .key
                }};
            }
        );
    }

    if (input.loan.has_value()) {
        return locatorFrom(*input.loan, xrpl::ltLOAN, [](le::LoanEntry const& entry) {
            return LocatorOrStatus{Locator{
                .key = xrpl::keylet::loan(
                           entry.loanBrokerId, xrpl::SeqProxy::rawSequence(entry.loanSeq)
                )
                           .key
            }};
        });
    }

    if (input.delegate.has_value()) {
        return locatorFrom(*input.delegate, xrpl::ltDELEGATE, [](le::DelegateEntry const& entry) {
            return LocatorOrStatus{
                Locator{.key = xrpl::keylet::delegate(entry.account, entry.authorize).key}
            };
        });
    }

    if (input.sponsorship.has_value()) {
        return locatorFrom(
            *input.sponsorship, xrpl::ltSPONSORSHIP, [](le::SponsorshipEntry const& entry) {
                return LocatorOrStatus{
                    Locator{.key = xrpl::keylet::sponsorship(entry.sponsor, entry.sponsee).key}
                };
            }
        );
    }

    if (apiVersion == 1u)
        return std::unexpected{Status{ClioError::RpcUnknownOption}};

    return std::unexpected{
        Status{XrpldError::RpcInvalidParams, "No ledger_entry params provided."}
    };
}

}  // namespace

LedgerEntryHandler::Result
LedgerEntryHandler::process(LedgerEntryHandler::Input const& input, Context const& ctx) const
{
    auto const locator = resolveLocator(input, ctx.apiVersion);
    if (not locator.has_value())
        return Error{locator.error()};

    auto const key = locator->key;
    auto const expectedType = locator->expectedType;

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
            return Error{Status{XrpldError::RpcEntryNotFound}};
        auto const deletedSeq =
            sharedPtrBackend_->fetchLedgerObjectSeq(key, lgrInfo.seq, ctx.yield);
        if (not deletedSeq.has_value())
            return Error{Status{XrpldError::RpcEntryNotFound}};
        ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, *deletedSeq - 1, ctx.yield);
        if (not ledgerObject.has_value() or ledgerObject->empty())
            return Error{Status{XrpldError::RpcEntryNotFound}};
        output.deletedLedgerIndex = deletedSeq;
    }

    xrpl::STLedgerEntry const sle{
        xrpl::SerialIter{ledgerObject->data(), ledgerObject->size()}, key
    };

    if (expectedType != xrpl::ltANY && sle.getType() != expectedType)
        return Error{Status{XrpldError::RpcUnexpectedLedgerType}};

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
