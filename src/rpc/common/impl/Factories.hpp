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

#include "rpc/Errors.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/Concepts.hpp"
#include "rpc/common/Types.hpp"
#include "util/UnsupportedType.hpp"

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc::impl {

using FieldSpecProcessor = std::function<MaybeError(boost::json::value&)>;

static FieldSpecProcessor const EMPTY_FIELD_PROCESSOR = [](boost::json::value&) -> MaybeError { return {}; };

template <SomeProcessor... Processors>
[[nodiscard]] FieldSpecProcessor
makeFieldProcessor(std::string const& key, Processors&&... procs)
{
    return [key, ... proc = std::forward<Processors>(procs)](boost::json::value& j) -> MaybeError {
        std::optional<Status> firstFailure = std::nullopt;

        // This expands in order of Requirements and stops evaluating after first failure which is stored in
        // `firstFailure` and can be checked later on to see whether the verification failed as a whole or not.
        (
            [&j, &key, &firstFailure, req = &proc]() {
                if (firstFailure)
                    return;  // already failed earlier - skip

                if constexpr (SomeRequirement<decltype(*req)>) {
                    if (auto const res = req->verify(j, key); not res)
                        firstFailure = res.error();
                } else if constexpr (SomeModifier<decltype(*req)>) {
                    if (auto const res = req->modify(j, key); not res)
                        firstFailure = res.error();
                } else {
                    static_assert(util::unsupportedType<decltype(*req)>);
                }
            }(),
            ...
        );

        if (firstFailure)
            return std::unexpected{std::move(firstFailure).value()};

        return {};
    };
}

using FieldChecker = std::function<check::Warnings(boost::json::value const&)>;

static FieldChecker const EMPTY_FIELD_CHECKER = [](boost::json::value const&) -> check::Warnings { return {}; };

template <SomeCheck... Checks>
[[nodiscard]] FieldChecker
makeFieldChecker(std::string const& key, Checks&&... checks)
{
    return [key, ... checks = std::forward<Checks>(checks)](boost::json::value const& j) -> check::Warnings {
        check::Warnings warnings;
        // This expands in order of Checks and collects all warnings into a WarningsCollection
        (
            [&j, &key, &warnings, req = &checks]() {
                if (auto res = req->check(j, key); res)
                    warnings.push_back(std::move(res).value());
            }(),
            ...
        );
        return warnings;
    };
}

}  // namespace rpc::impl
