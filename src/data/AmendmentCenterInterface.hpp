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
    isEnabled(
        boost::asio::yield_context yield,
        std::vector<AmendmentKey> const& keys,
        uint32_t seq
    ) const = 0;

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
