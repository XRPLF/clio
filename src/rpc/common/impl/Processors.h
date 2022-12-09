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

#include <rpc/common/Concepts.h>
#include <rpc/common/Types.h>

namespace RPCng::detail {

template <Handler HandlerType>
struct DefaultProcessor final
{
    [[nodiscard]] ReturnType
    operator()(HandlerType const& handler, boost::json::value const& value)
        const
    {
        using boost::json::value_from;
        using boost::json::value_to;

        // first we run validation
        auto const spec = handler.spec();
        if (auto const ret = spec.validate(value); not ret)
            return Error{ret.error()};  // forward Status

        auto const inData = value_to<typename HandlerType::Input>(value);

        // real handler is given expected Input, not json
        if (auto const ret = handler.process(inData); not ret)
            return Error{ret.error()};  // forward Status
        else
            return value_from(ret.value());
    }
};

}  // namespace RPCng::detail
