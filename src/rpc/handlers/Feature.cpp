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

#include "rpc/handlers/Feature.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <string>

namespace rpc {

FeatureHandler::Result
FeatureHandler::process([[maybe_unused]] FeatureHandler::Input input, [[maybe_unused]] Context const& ctx)
{
    // For now this handler only fires when "vetoed" is set in the request.
    // This always leads to a `notSupported` error as we don't want anyone to be able to
    ASSERT(false, "FeatureHandler::process is not implemented.");
    return Output{};
}

RpcSpecConstRef
FeatureHandler::spec([[maybe_unused]] uint32_t apiVersion)
{
    static RpcSpec const rpcSpec = {
        {JS(feature), validation::Type<std::string>{}},
        {JS(vetoed),
         meta::WithCustomError{
             validation::NotSupported{},
             Status(RippledError::rpcNO_PERMISSION, "The admin portion of feature API is not available through Clio.")
         }},
    };
    return rpcSpec;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, FeatureHandler::Output const& output)
{
    using boost::json::value_from;

    jv = {
        {JS(validated), output.validated},
    };
}

FeatureHandler::Input
tag_invoke(boost::json::value_to_tag<FeatureHandler::Input>, boost::json::value const& jv)
{
    auto input = FeatureHandler::Input{};
    auto const jsonObject = jv.as_object();

    if (jsonObject.contains(JS(feature)))
        input.feature = jv.at(JS(feature)).as_string();

    return input;
}

}  // namespace rpc
