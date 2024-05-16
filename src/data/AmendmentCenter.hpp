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

#include "data/BackendInterface.hpp"

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

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#define REGISTER_AMENDMENT_PROXY(r, data, name) \
    impl::AmendmentProxy<std::to_array(BOOST_PP_STRINGIZE(name)), AmendmentCenter> const name{*this};
#define STRINGIZE(r, data, name) BOOST_PP_STRINGIZE(name),
#define REGISTER_AMENDMENTS(...)                                                             \
    BOOST_PP_SEQ_FOR_EACH(REGISTER_AMENDMENT_PROXY, , BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    constexpr static auto const SUPPORTED_AMENDMENTS = std::array                            \
    {                                                                                        \
        BOOST_PP_SEQ_FOR_EACH(STRINGIZE, , BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))            \
    }

namespace data {

/**
 * @brief Represents an amendment in the XRPL
 */
struct Amendment {
    std::string name;
    ripple::uint256 feature;
    bool supportedByClio;

    /**
     * @brief Construct a new Amendment
     *
     * @param amendmentName The name of the amendment
     * @param supported Whether Clio supports this amendment; defaults to false
     */
    Amendment(std::string amendmentName, bool supported = false)
        : name{std::move(amendmentName)}, feature{GetAmendmentId(name)}, supportedByClio{supported}
    {
    }

    /**
     * @brief Get the amendment Id from its name
     *
     * @param name The name of the amendment
     * @return The amendment Id as uint256
     */
    static ripple::uint256
    GetAmendmentId(std::string_view const name)
    {
        return ripple::sha512Half(ripple::Slice(name.data(), name.size()));
    }
};

namespace impl {

template <std::array Name, typename AmendmentCenterType>
class AmendmentProxy {
    std::reference_wrapper<AmendmentCenterType const> center_;

public:
    explicit AmendmentProxy(AmendmentCenterType const& center) : center_{std::cref(center)}
    {
    }

    Amendment const&
    get() const
    {
        static auto const& am = center_.get().getAmendment(std::string{Name.begin(), Name.end()});
        return am;
    }

    operator std::string() const
    {
        return get().name;
    }

    operator ripple::uint256() const
    {
        return get().feature;
    }
};

}  // namespace impl

/**
 * @brief Provides all amendments supported by libxrpl
 *
 * @return A vector of amendments
 */
std::vector<Amendment>
xrplAmendments();

/**
 * @brief The interface of an amendment center
 */
class AmentmentCenterInterface {
public:
    virtual ~AmentmentCenterInterface() = default;

    /**
     * @brief Check whether an amendment is supported by Clio
     *
     * @param name The name of the amendment to check
     * @return true if supported; false otherwise
     */
    virtual bool
    isSupported(std::string name) const = 0;

    /**
     * @brief Get all supported amendments as a map
     *
     * @return The amendments supported by Clio
     */
    virtual std::unordered_map<std::string, Amendment> const&
    getSupported() const = 0;

    /**
     * @brief Get all known amendments
     *
     * @return All known amendments as a vector
     */
    virtual std::vector<Amendment> const&
    getAll() const = 0;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param name The name of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    virtual bool
    isEnabled(std::string name, uint32_t seq) const = 0;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param name The name of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    virtual bool
    isEnabled(boost::asio::yield_context yield, std::string name, uint32_t seq) const = 0;

    /**
     * @brief Get an amendment
     *
     * @param name The name of the amendment to get
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    virtual Amendment const&
    getAmendment(std::string name) const = 0;
};

/**
 * @brief Knowledge center for amendments within XRPL
 */
class AmendmentCenter : public AmentmentCenterInterface {
    std::shared_ptr<data::BackendInterface> backend_;

    std::unordered_map<std::string, Amendment> supported_;
    std::vector<Amendment> all_;

public:
    // NOTE: everytime Clio adds support for an Amendment it should also be listed here
    /** @cond */
    REGISTER_AMENDMENTS(
        OwnerPaysFee,
        Flow,
        FlowCross,
        fix1513,
        DepositAuth,
        Checks,
        fix1571,
        fix1543,
        fix1623,
        DepositPreauth,
        fix1515,
        fix1578,
        MultiSignReserve,
        fixTakerDryOfferRemoval,
        fixMasterKeyAsRegularKey,
        fixCheckThreading,
        fixPayChanRecipientOwnerDir,
        DeletableAccounts,
        fixQualityUpperBound,
        RequireFullyCanonicalSig,
        fix1781,
        HardenedValidations,
        fixAmendmentMajorityCalc,
        NegativeUNL,
        TicketBatch,
        FlowSortStrands,
        fixSTAmountCanonicalize,
        fixRmSmallIncreasedQOffers,
        CheckCashMakesTrustLine,
        ExpandedSignerList,
        NonFungibleTokensV1_1,
        fixTrustLinesToSelf,
        fixRemoveNFTokenAutoTrustLine,
        ImmediateOfferKilled,
        DisallowIncoming,
        XRPFees,
        fixUniversalNumber,
        fixNonFungibleTokensV1_2,
        fixNFTokenRemint,
        fixReducedOffersV1,
        Clawback,
        AMM,
        XChainBridge,
        fixDisallowIncomingV1,
        DID,
        fixFillOrKill,
        fixNFTokenReserve,
        fixInnerObjTemplate,
        fixAMMOverflowOffer,
        PriceOracle,
        fixEmptyDID,
        fixXChainRewardRounding,
        fixPreviousTxnID,
        fixAMMRounding
    );
    /** @endcond */

public:
    /**
     * @brief Construct a new AmendmentCenter instance
     *
     * @param backend The backend
     */
    explicit AmendmentCenter(std::shared_ptr<data::BackendInterface> const& backend) : backend_{backend}
    {
        namespace rg = std::ranges;
        namespace vs = std::views;

        rg::copy(
            xrplAmendments() | vs::transform([&](auto const& a) {
                auto const supported = rg::find(SUPPORTED_AMENDMENTS, a.name) != rg::end(SUPPORTED_AMENDMENTS);
                return Amendment{a.name, supported};
            }),
            std::back_inserter(all_)
        );

        for (auto const& am : all_ | vs::filter([](auto const& am) { return am.supportedByClio; }))
            supported_.insert_or_assign(am.name, am);
    }

    bool
    isSupported(std::string name) const override;

    std::unordered_map<std::string, Amendment> const&
    getSupported() const override;

    std::vector<Amendment> const&
    getAll() const override;

    bool
    isEnabled(std::string name, uint32_t seq) const override;

    bool
    isEnabled(boost::asio::yield_context yield, std::string name, uint32_t seq) const override;

    Amendment const&
    getAmendment(std::string name) const override;
};

}  // namespace data
