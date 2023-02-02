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

#include <boost/json/value.hpp>

#include <optional>

namespace RPCng::detail {

template <Requirement... Requirements>
[[nodiscard]] auto
makeFieldValidator(std::string const& key, Requirements&&... requirements)
{
    return [key, ... r = std::forward<Requirements>(requirements)](
               boost::json::value const& j) -> MaybeError {
        // clang-format off
        std::optional<RPC::Status> firstFailure = std::nullopt;

        // This expands in order of Requirements and stops evaluating after 
        // first failure which is stored in `firstFailure` and can be checked 
        // later on to see whether the verification failed as a whole or not.
        ([&j, &key, &firstFailure, req = &r]() {
            if (firstFailure)
                return; // already failed earlier - skip

            if (auto const res = req->verify(j, key); not res)
                firstFailure = res.error();
        }(), ...);
        // clang-format on

        if (firstFailure)
            return Error{firstFailure.value()};

        return {};
    };
}

}  // namespace RPCng::detail
