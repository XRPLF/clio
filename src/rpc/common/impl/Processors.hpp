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

#include "rpc/common/Concepts.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>

namespace rpc::detail {

template <typename>
static constexpr bool unsupported_handler_v = false;

template <SomeHandler HandlerType>
struct DefaultProcessor final {
    [[nodiscard]] ReturnType
    operator()(HandlerType const& handler, boost::json::value const& value, Context const& ctx) const
    {
        using boost::json::value_from;
        using boost::json::value_to;
        if constexpr (SomeHandlerWithInput<HandlerType>) {
            // first we run validation against specified API version
            auto const spec = handler.spec(ctx.apiVersion);
            auto input = value;  // copy here, spec require mutable data

            if (auto const ret = spec.process(input); not ret)
                return Error{ret.error()};  // forward Status

            auto const inData = value_to<typename HandlerType::Input>(input);
            auto const ret = handler.process(inData, ctx);

            // real handler is given expected Input, not json
            if (!ret) {
                return Error{ret.error()};  // forward Status
            }
            return value_from(ret.value());
        } else if constexpr (SomeHandlerWithoutInput<HandlerType>) {
            // no input to pass, ignore the value
            auto const ret = handler.process(ctx);
            if (not ret) {
                return Error{ret.error()};  // forward Status
            }
            return value_from(ret.value());
        } else {
            // when concept SomeHandlerWithInput and SomeHandlerWithoutInput not cover all Handler case
            static_assert(unsupported_handler_v<HandlerType>);
        }
    }
};

}  // namespace rpc::detail
