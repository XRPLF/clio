#pragma once

#include <string_view>

namespace web::dosguard {

/**
 * @brief Interface for a whitelist handler
 */
class WhitelistHandlerInterface {
public:
    /** @brief Virtual destructor */
    virtual ~WhitelistHandlerInterface() = default;

    /**
     * @brief Checks to see if the given IP is whitelisted
     *
     * @param ip The IP to check
     * @return true if the given IP is whitelisted; false otherwise
     */
    [[nodiscard]] virtual bool
    isWhiteListed(std::string_view ip) const = 0;
};

}  // namespace web::dosguard
