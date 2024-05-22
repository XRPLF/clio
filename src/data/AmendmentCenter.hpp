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
#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/digest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#define REGISTER(name)                                       \
    inline static AmendmentKey const name = std::string_view \
    {                                                        \
        BOOST_PP_STRINGIZE(name)                             \
    }

namespace data {

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
    REGISTER(fixAMMRounding);

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

/**
 * @brief Provides all amendments supported by libxrpl
 *
 * @return A vector of amendments
 */
std::vector<Amendment>
xrplAmendments();

/**
 * @brief Knowledge center for amendments within XRPL
 */
class AmendmentCenter : public AmendmentCenterInterface {
    std::shared_ptr<data::BackendInterface> backend_;

    std::unordered_map<std::string, Amendment> supported_;
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
     * @param name The name of the amendment to check
     * @return true if supported; false otherwise
     */
    bool
    isSupported(std::string name) const final;

    /**
     * @brief Get all supported amendments as a map
     *
     * @return The amendments supported by Clio
     */
    std::unordered_map<std::string, Amendment> const&
    getSupported() const final;

    /**
     * @brief Get all known amendments
     *
     * @return All known amendments as a vector
     */
    std::vector<Amendment> const&
    getAll() const final;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param name The name of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    bool
    isEnabled(std::string name, uint32_t seq) const final;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param key The key of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    bool
    isEnabled(boost::asio::yield_context yield, AmendmentKey const& key, uint32_t seq) const final;

    /**
     * @brief Get an amendment
     *
     * @param name The name of the amendment to get
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    Amendment const&
    getAmendment(std::string name) const final;

    /**
     * @brief Get an amendment by its key

     * @param key The amendment key from @see Amendments
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    Amendment const&
    operator[](AmendmentKey const& key) const final;
};

}  // namespace data
