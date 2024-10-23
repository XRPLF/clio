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

#include "rpc/common/impl/APIVersionParser.hpp"

#include "util/newconfig/ObjectView.hpp"

#include <boost/json/object.hpp>
#include <fmt/core.h>

#include <cstdint>
#include <expected>
#include <string>

using namespace std;

namespace rpc::impl {

ProductionAPIVersionParser::ProductionAPIVersionParser(
    uint32_t defaultVersion,
    uint32_t minVersion,
    uint32_t maxVersion
)
    : defaultVersion_{defaultVersion}, minVersion_{minVersion}, maxVersion_{maxVersion}
{
    LOG(log_.info()) << "API version settings: [min = " << minVersion_ << "; max = " << maxVersion_
                     << "; default = " << defaultVersion_ << "]";
}

ProductionAPIVersionParser::ProductionAPIVersionParser(util::config::ObjectView const& config)
    : ProductionAPIVersionParser(
          config.getValue<uint32_t>("default"),
          config.getValue<uint32_t>("min"),
          config.getValue<uint32_t>("max")
      )
{
}

std::expected<uint32_t, std::string>
ProductionAPIVersionParser::parse(boost::json::object const& request) const
{
    using Error = std::unexpected<std::string>;

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

}  // namespace rpc::impl
