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

#include "data/Types.hpp"

#include <boost/asio/spawn.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace data {

/**
 * @brief The interface of an amendment center
 */
class AmendmentCenterInterface {
public:
    virtual ~AmendmentCenterInterface() = default;

    /**
     * @brief Check whether an amendment is supported by Clio
     *
     * @param key The key of the amendment to check
     * @return true if supported; false otherwise
     */
    [[nodiscard]] virtual bool
    isSupported(AmendmentKey const& key) const = 0;

    /**
     * @brief Get all supported amendments as a map
     *
     * @return The amendments supported by Clio
     */
    [[nodiscard]] virtual std::map<std::string, Amendment> const&
    getSupported() const = 0;

    /**
     * @brief Get all known amendments
     *
     * @return All known amendments as a vector
     */
    [[nodiscard]] virtual std::vector<Amendment> const&
    getAll() const = 0;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param key The key of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    [[nodiscard]] virtual bool
    isEnabled(AmendmentKey const& key, uint32_t seq) const = 0;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param key The key of the amendment to check
     * @param seq The sequence to check for
     * @return true if enabled; false otherwise
     */
    [[nodiscard]] virtual bool
    isEnabled(boost::asio::yield_context yield, AmendmentKey const& key, uint32_t seq) const = 0;

    /**
     * @brief Check whether an amendment was/is enabled for a given sequence
     *
     * @param yield The coroutine context to use
     * @param keys The keys of the amendments to check
     * @param seq The sequence to check for
     * @return A vector of bools representing enabled state for each of the given keys
     */
    [[nodiscard]] virtual std::vector<bool>
    isEnabled(boost::asio::yield_context yield, std::vector<AmendmentKey> const& keys, uint32_t seq) const = 0;

    /**
     * @brief Get an amendment
     *
     * @param key The key of the amendment to get
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    [[nodiscard]] virtual Amendment const&
    getAmendment(AmendmentKey const& key) const = 0;

    /**
     * @brief Get an amendment by its key
     *
     * @param key The amendment key from @see Amendments
     * @return The amendment as a const ref; asserts if the amendment is unknown
     */
    [[nodiscard]] virtual Amendment const&
    operator[](AmendmentKey const& key) const = 0;
};

}  // namespace data
