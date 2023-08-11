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

namespace rpc::detail {

template <typename>
static constexpr bool unsupported_v = false;

template <SomeProcessor... Processors>
[[nodiscard]] auto
makeFieldProcessor(std::string const& key, Processors&&... procs)
{
    return [key, ... proc = std::forward<Processors>(procs)](boost::json::value& j) -> MaybeError {
        std::optional<Status> firstFailure = std::nullopt;

        // This expands in order of Requirements and stops evaluating after first failure which is stored in
        // `firstFailure` and can be checked later on to see whether the verification failed as a whole or not.
        // clang-format off
        ([&j, &key, &firstFailure, req = &proc]() {
            if (firstFailure)
                return; // already failed earlier - skip

            if constexpr (SomeRequirement<decltype(*req)>) {
                if (auto const res = req->verify(j, key); not res)
                    firstFailure = res.error();
            } else if constexpr (SomeModifier<decltype(*req)>) {
                if (auto const res = req->modify(j, key); not res)
                    firstFailure = res.error();
            } else {
                static_assert(unsupported_v<decltype(*req)>);
            }

        }(), ...);
        // clang-format on

        if (firstFailure)
            return Error{firstFailure.value()};

        return {};
    };
}

}  // namespace rpc::detail
