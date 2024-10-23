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

#include "rpc/common/APIVersion.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <boost/json/object.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace rpc::impl {

class ProductionAPIVersionParser : public APIVersionParser {
    util::Logger log_{"RPC"};

    uint32_t defaultVersion_;
    uint32_t minVersion_;
    uint32_t maxVersion_;

public:
    ProductionAPIVersionParser(
        uint32_t defaultVersion = API_VERSION_DEFAULT,
        uint32_t minVersion = API_VERSION_MIN,
        uint32_t maxVersion = API_VERSION_MAX
    );

    ProductionAPIVersionParser(util::config::ObjectView const& config);

    std::expected<uint32_t, std::string>
    parse(boost::json::object const& request) const override;

    uint32_t
    getDefaultVersion() const
    {
        return defaultVersion_;
    }

    uint32_t
    getMinVersion() const
    {
        return minVersion_;
    }

    uint32_t
    getMaxVersion() const
    {
        return maxVersion_;
    }
};

}  // namespace rpc::impl
