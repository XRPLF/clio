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
