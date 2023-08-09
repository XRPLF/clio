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

#include <rpc/Errors.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/APIVersion.h>
#include <rpc/common/Types.h>
#include <rpc/common/impl/APIVersionParser.h>

namespace RPC {

/**
 * @brief The version command returns the min,max and current api Version we are using
 *
 */
class VersionHandler
{
    RPC::detail::ProductionAPIVersionParser apiVersionParser_;

public:
    struct Output
    {
        uint32_t minVersion;
        uint32_t maxVersion;
        uint32_t currVersion;
    };

    explicit VersionHandler(clio::util::Config const& config)
        : apiVersionParser_(
              config.valueOr("default", API_VERSION_DEFAULT),
              config.valueOr("min", API_VERSION_MIN),
              config.valueOr("max", API_VERSION_MAX))
    {
    }

    using Result = HandlerReturnType<Output>;

    Result
    process([[maybe_unused]] Context const& ctx) const
    {
        using namespace RPC;

        auto output = Output{};
        output.currVersion = apiVersionParser_.getDefaultVersion();
        output.minVersion = apiVersionParser_.getMinVersion();
        output.maxVersion = apiVersionParser_.getMaxVersion();
        return output;
    }

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = {
            {"version",
             {
                 {"first", output.minVersion},
                 {"last", output.maxVersion},
                 {"good", output.currVersion},
             }},
        };
    }
};

}  // namespace RPC
