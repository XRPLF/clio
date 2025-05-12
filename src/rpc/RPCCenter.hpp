//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
