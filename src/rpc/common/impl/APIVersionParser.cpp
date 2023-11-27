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

#include "rpc/common/impl/APIVersionParser.h"
#include <boost/json/object.hpp>
#include "rpc/common/APIVersion.h"
#include "util/Expected.h"
#include "util/log/Logger.h"
#include <cstdint>
#include <string>

#include <fmt/core.h>

using namespace std;

namespace rpc::detail {

ProductionAPIVersionParser::ProductionAPIVersionParser(util::Config const& config)
    : ProductionAPIVersionParser(
          config.valueOr("default", API_VERSION_DEFAULT),
          config.valueOr("min", API_VERSION_MIN),
          config.valueOr("max", API_VERSION_MAX)
      )
{
}

util::Expected<uint32_t, std::string>
ProductionAPIVersionParser::parse(boost::json::object const& request) const
{
    using Error = util::Unexpected<std::string>;

    if (request.contains("api_version")) {
        if (!request.at("api_version").is_int64())
            return Error{"API version must be an integer"};

        auto const version = request.at("api_version").as_int64();

        if (version > maxVersion_)
            return Error{fmt::format("Requested API version is higher than maximum supported ({})", maxVersion_)};

        if (version < minVersion_)
            return Error{fmt::format("Requested API version is lower than minimum supported ({})", minVersion_)};

        return version;
    }

    return defaultVersion_;
}

}  // namespace rpc::detail
