#pragma once

#include <string_view>

namespace rpc {

/**
 * @brief Registry of RPC commands supported by Clio
 *
 * The RPCCenter maintains lists of RPC commands that can be handled locally
 * and those that need to be forwarded to rippled.
 */
struct RPCCenter {
    /**
     * @brief Checks if a string is a valid RPC command name
     *
     * @param s The string to check
     * @return true if the string is a recognized RPC name, false otherwise
     */
    static bool
    isRpcName(std::string_view s);

    /**
     * @brief Checks if a string is a RPC command handled by Clio without forwarding to rippled
     *
     * @param s The string to check
     * @return true if the string is a handled RPC command, false otherwise
     */
    static bool
    isHandled(std::string_view s);

    /**
     * @brief Checks if a string is a RPC command that will be forwarded to rippled
     *
     * @param s The string to check
     * @return true if the string is a forwarded RPC command, false otherwise
     */
    static bool
    isForwarded(std::string_view s);
};

}  // namespace rpc
