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
#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/digest.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

template <typename T>
concept SomeAmendmentProvider = requires(T a) {
    { a() } -> std::same_as<std::vector<Amendment>>;
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
class AmendmentCenter {
    std::shared_ptr<data::BackendInterface> backend_;

    std::unordered_map<std::string, Amendment> supported_;
    std::vector<Amendment> all_;

public:
    /**
     * @brief Construct a new AmendmentCenter instance
     *
     * @param backend The backend
     * @param provider Provides the full list of amendments to consider
     * @param amendments A list of features and fixes supported by Clio
     */
    AmendmentCenter(
        std::shared_ptr<data::BackendInterface> const& backend,
        SomeAmendmentProvider auto provider,
        std::vector<std::string> amendments
    )
        : backend_{backend}
    {
        namespace rg = std::ranges;
        namespace vs = std::views;

        rg::copy(
            provider() | vs::transform([&](auto const& a) {
                auto const supported = rg::find(amendments, a.name) != rg::end(amendments);
                return Amendment{a.name, supported};
            }),
            std::back_inserter(all_)
        );

        for (auto const& am : all_ | vs::filter([](auto const& am) { return am.supportedByClio; }))
            supported_.insert_or_assign(am.name, am);
    }

    /**
     * @brief Check whether an amendment is supported by Clio
     *
     * @param name The name of the amendment to check
     * @return true if supported; false otherwise
     */
    bool
    isSupported(std::string name) const;

    /**
     * @brief Get all supported amendments as a map
     *
     * @return The amendments supported by Clio
     */
    std::unordered_map<std::string, Amendment> const&
    getSupported() const;

    /**
     * @brief Get all known amendments
     *
     * @return All known amendments as a vector
     */
    std::vector<Amendment> const&
    getAll() const;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param name The name of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    bool
    isEnabled(std::string name, uint32_t seq) const;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param name The name of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    bool
    isEnabled(boost::asio::yield_context yield, std::string name, uint32_t seq) const;
};

}  // namespace data
