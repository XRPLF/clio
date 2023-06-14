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

#include <rpc/common/Types.h>

#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <string>

namespace RPC {

struct RpcSpec;

/**
 * @brief A concept that specifies what a requirement used with @ref FieldSpec
 * must provide
 */
// clang-format off
template <typename T>
concept Requirement = requires(T a) {
    { a.verify(boost::json::value{}, std::string{}) } -> std::same_as<MaybeError>;
};
// clang-format on

/**
 * @brief A concept that specifies what a Handler type must provide
 *
 * Note that value_from and value_to should be implemented using tag_invoke
 * as per boost::json documentation for these functions.
 */
// clang-format off
template <typename T>
concept ContextProcessWithInput = requires(T a, typename T::Input in, typename T::Output out, Context const& ctx) {
    { a.process(in, ctx) } -> std::same_as<HandlerReturnType<decltype(out)>>; 
};

template <typename T>
concept ContextProcessWithoutInput = requires(T a, typename T::Output out, Context const& ctx) {
    { a.process(ctx) } -> std::same_as<HandlerReturnType<decltype(out)>>; 
};

template <typename T>
concept HandlerWithInput = requires(T a, uint32_t version) {
    { a.spec(version) } -> std::same_as<RpcSpecConstRef>; 
}
and ContextProcessWithInput<T>
and boost::json::has_value_to<typename T::Input>::value;

template <typename T>
concept HandlerWithoutInput = ContextProcessWithoutInput<T>;

template <typename T>
concept Handler = 
(
    HandlerWithInput<T> or
    HandlerWithoutInput<T>
) 
and boost::json::has_value_from<typename T::Output>::value;
// clang-format on

}  // namespace RPC
