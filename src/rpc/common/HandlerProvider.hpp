#pragma once

#include "rpc/common/AnyHandler.hpp"

#include <optional>
#include <string>

namespace rpc {

/**
 * @brief Interface for the provider of RPC handlers.
 */
class HandlerProvider {
public:
    virtual ~HandlerProvider() = default;

    /**
     * @brief Check if the provider contains a handler for a given method
     *
     * @param command The method to check for
     * @return true if the provider contains a handler for the method, false otherwise
     */
    virtual bool
    contains(std::string const& command) const = 0;

    /**
     * @brief Get the handler for a given method
     *
     * @param command The method to get the handler for
     * @return The handler for the method, or std::nullopt if the method is not found
     */
    virtual std::optional<AnyHandler>
    getHandler(std::string const& command) const = 0;

    /**
     * @brief Check if a given method is Clio-only
     *
     * @param command The method to check
     * @return true if the method is Clio-only, false otherwise
     */
    virtual bool
    isClioOnly(std::string const& command) const = 0;
};

}  // namespace rpc
