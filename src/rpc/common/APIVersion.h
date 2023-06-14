//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <rpc/common/Types.h>
#include <util/Expected.h>

#include <boost/json.hpp>

#include <string>

namespace RPC {

/**
 * @brief Default API version to use if no version is specified by clients
 */
static constexpr uint32_t API_VERSION_DEFAULT = 2u;

/**
 * @brief Minimum API version supported by this build
 *
 * Note: Clio does not support v1 and only supports v2 and newer.
 */
static constexpr uint32_t API_VERSION_MIN = 2u;

/**
 * @brief Maximum API version supported by this build
 */
static constexpr uint32_t API_VERSION_MAX = 2u;

/**
 * @brief A baseclass for API version helper
 */
class APIVersionParser
{
public:
    virtual ~APIVersionParser() = default;

    util::Expected<uint32_t, std::string> virtual parse(boost::json::object const& request) const = 0;
};

}  // namespace RPC
