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

#include <config/Config.h>
#include <log/Logger.h>
#include <rpc/common/APIVersion.h>
#include <util/Expected.h>

#include <algorithm>
#include <string>

namespace RPC::detail {

class ProductionAPIVersionParser : public APIVersionParser
{
    clio::Logger log_{"RPC"};

    uint32_t defaultVersion_;
    uint32_t minVersion_;
    uint32_t maxVersion_;

public:
    // Note: this constructor must remain in the header because of UNITTEST_BUILD definition below
    ProductionAPIVersionParser(
        uint32_t defaultVersion = API_VERSION_DEFAULT,
        uint32_t minVersion = API_VERSION_MIN,
        uint32_t maxVersion = API_VERSION_MAX)
        : defaultVersion_{defaultVersion}, minVersion_{minVersion}, maxVersion_{maxVersion}
    {
#ifndef UNITTEST_BUILD
        // in production, we don't want the ability to misconfigure clio with bogus versions
        // that are not actually supported by the code itself. for testing it is desired however.
        auto checkRange = [this](uint32_t version, std::string label) {
            if (std::clamp(version, API_VERSION_MIN, API_VERSION_MAX) != version)
            {
                log_.error() << "API version settings issue detected: " << label << " version with value " << version
                             << " is outside of supported range " << API_VERSION_MIN << "-" << API_VERSION_MAX
                             << "; Falling back to hardcoded values.";

                defaultVersion_ = API_VERSION_DEFAULT;
                minVersion_ = API_VERSION_MIN;
                maxVersion_ = API_VERSION_MAX;
            }
        };

        checkRange(defaultVersion, "default");
        checkRange(minVersion, "minimum");
        checkRange(maxVersion, "maximum");
#endif

        log_.info() << "API version settings: [min = " << minVersion_ << "; max = " << maxVersion_
                    << "; default = " << defaultVersion_ << "]";
    }

    ProductionAPIVersionParser(clio::Config const& config);

    util::Expected<uint32_t, std::string>
    parse(boost::json::object const& request) const override;

    inline uint32_t
    getDefaultVersion() const
    {
        return defaultVersion_;
    }

    inline uint32_t
    getMinVersion() const
    {
        return minVersion_;
    }

    inline uint32_t
    getMaxVersion() const
    {
        return maxVersion_;
    }
};

}  // namespace RPC::detail
