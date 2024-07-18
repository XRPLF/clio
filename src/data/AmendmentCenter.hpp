//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "data/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/digest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#define REGISTER(name)                                   \
    inline static impl::WritingAmendmentKey const name = \
        impl::WritingAmendmentKey(std::string(BOOST_PP_STRINGIZE(name)))

namespace data {
namespace impl {

struct WritingAmendmentKey : AmendmentKey {
    explicit WritingAmendmentKey(std::string amendmentName);
};

}  // namespace impl

/**
 * @brief List of supported amendments
 */
struct Amendments {
    // NOTE: if Clio wants to report it supports an Amendment it should be listed here.
    // Whether an amendment is obsolete and/or supported by libxrpl is extracted directly from libxrpl.
    // If an amendment is in the list below it just means Clio did whatever changes needed to support it.
    // Most of the time it's going to be no changes at all.

    /** @cond */
    REGISTER(OwnerPaysFee);
    REGISTER(Flow);
    REGISTER(FlowCross);
    REGISTER(fix1513);
    REGISTER(DepositAuth);
    REGISTER(Checks);
    REGISTER(fix1571);
    REGISTER(fix1543);
    REGISTER(fix1623);
    REGISTER(DepositPreauth);
    REGISTER(fix1515);
    REGISTER(fix1578);
    REGISTER(MultiSignReserve);
    REGISTER(fixTakerDryOfferRemoval);
    REGISTER(fixMasterKeyAsRegularKey);
    REGISTER(fixCheckThreading);
    REGISTER(fixPayChanRecipientOwnerDir);
    REGISTER(DeletableAccounts);
    REGISTER(fixQualityUpperBound);
    REGISTER(RequireFullyCanonicalSig);
    REGISTER(fix1781);
    REGISTER(HardenedValidations);
    REGISTER(fixAmendmentMajorityCalc);
    REGISTER(NegativeUNL);
    REGISTER(TicketBatch);
    REGISTER(FlowSortStrands);
    REGISTER(fixSTAmountCanonicalize);
    REGISTER(fixRmSmallIncreasedQOffers);
    REGISTER(CheckCashMakesTrustLine);
    REGISTER(ExpandedSignerList);
    REGISTER(NonFungibleTokensV1_1);
    REGISTER(fixTrustLinesToSelf);
    REGISTER(fixRemoveNFTokenAutoTrustLine);
    REGISTER(ImmediateOfferKilled);
    REGISTER(DisallowIncoming);
    REGISTER(XRPFees);
    REGISTER(fixUniversalNumber);
    REGISTER(fixNonFungibleTokensV1_2);
    REGISTER(fixNFTokenRemint);
    REGISTER(fixReducedOffersV1);
    REGISTER(Clawback);
    REGISTER(AMM);
    REGISTER(XChainBridge);
    REGISTER(fixDisallowIncomingV1);
    REGISTER(DID);
    REGISTER(fixFillOrKill);
    REGISTER(fixNFTokenReserve);
    REGISTER(fixInnerObjTemplate);
    REGISTER(fixAMMOverflowOffer);
    REGISTER(PriceOracle);
    REGISTER(fixEmptyDID);
    REGISTER(fixXChainRewardRounding);
    REGISTER(fixPreviousTxnID);
    REGISTER(fixAMMv1_1);
    REGISTER(NFTokenMintOffer);
    REGISTER(fixReducedOffersV2);
    REGISTER(fixEnforceNFTokenTrustline);
    REGISTER(MPTokensV1);

    // Obsolete but supported by libxrpl
    REGISTER(CryptoConditionsSuite);
    REGISTER(NonFungibleTokensV1);
    REGISTER(fixNFTokenDirV1);
    REGISTER(fixNFTokenNegOffer);

    // Retired amendments
    REGISTER(MultiSign);
    REGISTER(TrustSetAuth);
    REGISTER(FeeEscalation);
    REGISTER(PayChan);
    REGISTER(fix1368);
    REGISTER(CryptoConditions);
    REGISTER(Escrow);
    REGISTER(TickSize);
    REGISTER(fix1373);
    REGISTER(EnforceInvariants);
    REGISTER(SortedDirectories);
    REGISTER(fix1201);
    REGISTER(fix1512);
    REGISTER(fix1523);
    REGISTER(fix1528);
    /** @endcond */
};

#undef REGISTER

/**
 * @brief Knowledge center for amendments within XRPL
 */
class AmendmentCenter : public AmendmentCenterInterface {
    std::shared_ptr<data::BackendInterface> backend_;

    std::map<std::string, Amendment> supported_;
    std::vector<Amendment> all_;

public:
    /**
     * @brief Construct a new AmendmentCenter instance
     *
     * @param backend The backend
     */
    explicit AmendmentCenter(std::shared_ptr<data::BackendInterface> const& backend);

    /**
     * @brief Check whether an amendment is supported by Clio
     *
     * @param key The key of the amendment to check
     * @return true if supported; false otherwise
     */
    [[nodiscard]] bool
    isSupported(AmendmentKey const& key) const final;

    /**
     * @brief Get all supported amendments as a map
     *
     * @return The amendments supported by Clio
     */
    [[nodiscard]] std::map<std::string, Amendment> const&
    getSupported() const final;

    /**
     * @brief Get all known amendments
     *
     * @return All known amendments as a vector
     */
    [[nodiscard]] std::vector<Amendment> const&
    getAll() const final;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param key The key of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    [[nodiscard]] bool
    isEnabled(AmendmentKey const& key, uint32_t seq) const final;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param key The key of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    [[nodiscard]] bool
    isEnabled(boost::asio::yield_context yield, AmendmentKey const& key, uint32_t seq) const final;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param keys The keys of the amendments to check
     * @param seq The sequence to check for
     * @return A vector of bools representing enabled state for each of the given keys
     */
    [[nodiscard]] std::vector<bool>
    isEnabled(boost::asio::yield_context yield, std::vector<AmendmentKey> const& keys, uint32_t seq) const final;

    /**
     * @brief Get an amendment
     *
     * @param key The key of the amendment to get
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    [[nodiscard]] Amendment const&
    getAmendment(AmendmentKey const& key) const final;

    /**
     * @brief Get an amendment by its key

     * @param key The amendment key from @see Amendments
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    [[nodiscard]] Amendment const&
    operator[](AmendmentKey const& key) const final;

private:
    [[nodiscard]] std::optional<std::vector<ripple::uint256>>
    fetchAmendmentsList(boost::asio::yield_context yield, uint32_t seq) const;
};

}  // namespace data
