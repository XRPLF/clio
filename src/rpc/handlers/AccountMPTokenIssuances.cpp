#include "rpc/handlers/AccountMPTokenIssuances.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

void
AccountMPTokenIssuancesHandler::addMPTokenIssuance(
    std::vector<MPTokenIssuanceResponse>& issuances,
    xrpl::SLE const& sle,
    xrpl::AccountID const& account
)
{
    MPTokenIssuanceResponse issuance;

    issuance.mpTokenIssuanceId = xrpl::strHex(sle.key());
    issuance.issuer = xrpl::to_string(account);
    issuance.sequence = sle.getFieldU32(xrpl::sfSequence);
    auto const flags = sle.getFieldU32(xrpl::sfFlags);

    auto const setFlag = [&](std::optional<bool>& field, std::uint32_t mask) {
        if ((flags & mask) != 0u)
            field = true;
    };

    setFlag(issuance.mptLocked, xrpl::lsfMPTLocked);
    setFlag(issuance.mptCanLock, xrpl::lsfMPTCanLock);
    setFlag(issuance.mptRequireAuth, xrpl::lsfMPTRequireAuth);
    setFlag(issuance.mptCanEscrow, xrpl::lsfMPTCanEscrow);
    setFlag(issuance.mptCanTrade, xrpl::lsfMPTCanTrade);
    setFlag(issuance.mptCanTransfer, xrpl::lsfMPTCanTransfer);
    setFlag(issuance.mptCanClawback, xrpl::lsfMPTCanClawback);

    if (sle.isFieldPresent(xrpl::sfMutableFlags)) {
        auto const mutableFlags = sle.getFieldU32(xrpl::sfMutableFlags);

        auto const setMutableFlag = [&](std::optional<bool>& field, std::uint32_t mask) {
            if ((mutableFlags & mask) != 0u)
                field = true;
        };

        setMutableFlag(issuance.mptCanMutateCanLock, xrpl::lsmfMPTCanMutateCanLock);
        setMutableFlag(issuance.mptCanMutateRequireAuth, xrpl::lsmfMPTCanMutateRequireAuth);
        setMutableFlag(issuance.mptCanMutateCanEscrow, xrpl::lsmfMPTCanMutateCanEscrow);
        setMutableFlag(issuance.mptCanMutateCanTrade, xrpl::lsmfMPTCanMutateCanTrade);
        setMutableFlag(issuance.mptCanMutateCanTransfer, xrpl::lsmfMPTCanMutateCanTransfer);
        setMutableFlag(issuance.mptCanMutateCanClawback, xrpl::lsmfMPTCanMutateCanClawback);
        setMutableFlag(issuance.mptCanMutateMetadata, xrpl::lsmfMPTCanMutateMetadata);
        setMutableFlag(issuance.mptCanMutateTransferFee, xrpl::lsmfMPTCanMutateTransferFee);
    }

    if (sle.isFieldPresent(xrpl::sfTransferFee))
        issuance.transferFee = sle.getFieldU16(xrpl::sfTransferFee);

    if (sle.isFieldPresent(xrpl::sfAssetScale))
        issuance.assetScale = sle.getFieldU8(xrpl::sfAssetScale);

    if (sle.isFieldPresent(xrpl::sfMaximumAmount))
        issuance.maximumAmount = sle.getFieldU64(xrpl::sfMaximumAmount);

    if (sle.isFieldPresent(xrpl::sfOutstandingAmount))
        issuance.outstandingAmount = sle.getFieldU64(xrpl::sfOutstandingAmount);

    if (sle.isFieldPresent(xrpl::sfLockedAmount))
        issuance.lockedAmount = sle.getFieldU64(xrpl::sfLockedAmount);

    if (sle.isFieldPresent(xrpl::sfMPTokenMetadata))
        issuance.mptokenMetadata = xrpl::strHex(sle.getFieldVL(xrpl::sfMPTokenMetadata));

    if (sle.isFieldPresent(xrpl::sfDomainID))
        issuance.domainID = xrpl::strHex(sle.getFieldH256(xrpl::sfDomainID));

    issuances.push_back(issuance);
}

AccountMPTokenIssuancesHandler::Result
AccountMPTokenIssuancesHandler::process(
    AccountMPTokenIssuancesHandler::Input const& input,
    Context const& ctx
) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountMPTokenIssuances' ledger range must be available");
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
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        xrpl::keylet::account(*accountID).key,
        lgrInfo.seq,
        ctx.yield
    );

    if (not accountLedgerObject.has_value())
        return Error{Status{RippledError::RpcActNotFound}};

    Output response;
    response.issuances.reserve(input.limit);

    auto const addToResponse = [&](xrpl::SLE const& sle) {
        if (sle.getType() == xrpl::ltMPTOKEN_ISSUANCE) {
            addMPTokenIssuance(response.issuances, sle, *accountID);
        }
    };

    auto const expectedNext = traverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,  // NOLINT(bugprone-unchecked-optional-access)
        lgrInfo.seq,
        input.limit,
        input.marker,
        ctx.yield,
        addToResponse
    );

    if (not expectedNext.has_value())
        return Error{expectedNext.error()};

    auto const nextMarker = *expectedNext;

    response.account = input.account;
    response.limit = input.limit;

    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountMPTokenIssuancesHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountMPTokenIssuancesHandler::Input>,
    boost::json::value const& jv
)
{
    auto input = AccountMPTokenIssuancesHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jv.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jv.at(JS(marker)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jv.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountMPTokenIssuancesHandler::Output const& output
)
{
    using boost::json::value_from;

    auto obj = boost::json::object{
        {JS(account), output.account},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {"mpt_issuances", value_from(output.issuances)},
    };

    if (output.marker.has_value())
        obj[JS(marker)] = *output.marker;

    jv = std::move(obj);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountMPTokenIssuancesHandler::MPTokenIssuanceResponse const& issuance
)
{
    auto obj = boost::json::object{
        {JS(mpt_issuance_id), issuance.mpTokenIssuanceId},
        {JS(issuer), issuance.issuer},
        {JS(sequence), issuance.sequence},
    };

    auto const setIfPresent = [&](boost::json::string_view field, auto const& value) {
        if (value.has_value()) {
            obj[field] = *value;
        }
    };

    setIfPresent("transfer_fee", issuance.transferFee);
    setIfPresent("asset_scale", issuance.assetScale);
    setIfPresent("maximum_amount", issuance.maximumAmount);
    setIfPresent("outstanding_amount", issuance.outstandingAmount);
    setIfPresent("locked_amount", issuance.lockedAmount);
    setIfPresent("mptoken_metadata", issuance.mptokenMetadata);
    setIfPresent("domain_id", issuance.domainID);

    setIfPresent("mpt_locked", issuance.mptLocked);
    setIfPresent("mpt_can_lock", issuance.mptCanLock);
    setIfPresent("mpt_require_auth", issuance.mptRequireAuth);
    setIfPresent("mpt_can_escrow", issuance.mptCanEscrow);
    setIfPresent("mpt_can_trade", issuance.mptCanTrade);
    setIfPresent("mpt_can_transfer", issuance.mptCanTransfer);
    setIfPresent("mpt_can_clawback", issuance.mptCanClawback);

    setIfPresent("mpt_can_mutate_can_lock", issuance.mptCanMutateCanLock);
    setIfPresent("mpt_can_mutate_require_auth", issuance.mptCanMutateRequireAuth);
    setIfPresent("mpt_can_mutate_can_escrow", issuance.mptCanMutateCanEscrow);
    setIfPresent("mpt_can_mutate_can_trade", issuance.mptCanMutateCanTrade);
    setIfPresent("mpt_can_mutate_can_transfer", issuance.mptCanMutateCanTransfer);
    setIfPresent("mpt_can_mutate_can_clawback", issuance.mptCanMutateCanClawback);
    setIfPresent("mpt_can_mutate_metadata", issuance.mptCanMutateMetadata);
    setIfPresent("mpt_can_mutate_transfer_fee", issuance.mptCanMutateTransferFee);

    jv = std::move(obj);
}

}  // namespace rpc
