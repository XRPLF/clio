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

#include <boost/json/object.hpp>

#include <cstddef>

namespace web::dosguard {

/**
 * @brief Interface for determining request weights in DOS protection.
 *
 * This interface defines the contract for classes that calculate weights for incoming
 * requests, which is used for DOS protection mechanisms.
 */
class WeightsInterface {
public:
    virtual ~WeightsInterface() = default;

    /**
     * @brief Calculate the weight of a request.
     *
     * @param request The JSON object representing the request
     * @return The calculated weight of the request
     */
    virtual size_t
    requestWeight(boost::json::object const& request) const = 0;
};

}  // namespace web::dosguard
