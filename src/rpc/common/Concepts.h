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

namespace rpc {

struct RpcSpec;

/**
 * @brief Specifies what a requirement used with @ref rpc::FieldSpec must provide.
 */
// clang-format off
template <typename T>
concept SomeRequirement = requires(T a, boost::json::value lval) {
    { a.verify(lval, std::string{}) } -> std::same_as<MaybeError>;
};
// clang-format on

/**
 * @brief Specifies what a modifier used with @ref rpc::FieldSpec must provide.
 */
// clang-format off
template <typename T>
concept SomeModifier = requires(T a, boost::json::value lval) {
    { a.modify(lval, std::string{}) } -> std::same_as<MaybeError>;
};
// clang-format on

/**
 * @brief The requirements of a processor to be used with @ref rpc::FieldSpec.
 */
template <typename T>
concept SomeProcessor = (SomeRequirement<T> or SomeModifier<T>);

/**
 * @brief A process function that expects both some Input and a Context.
 */
// clang-format off
template <typename T>
concept SomeContextProcessWithInput = requires(T a, typename T::Input in, typename T::Output out, Context const& ctx) {
    { a.process(in, ctx) } -> std::same_as<HandlerReturnType<decltype(out)>>; 
};
// clang-format on

/**
 * @brief A process function that expects no Input but does take a Context.
 */
// clang-format off
template <typename T>
concept SomeContextProcessWithoutInput = requires(T a, typename T::Output out, Context const& ctx) {
    { a.process(ctx) } -> std::same_as<HandlerReturnType<decltype(out)>>; 
};
// clang-format on

/**
 * @brief Specifies what a Handler with Input must provide.
 */
// clang-format off
template <typename T>
concept SomeHandlerWithInput = requires(T a, uint32_t version) {
    { a.spec(version) } -> std::same_as<RpcSpecConstRef>; 
}
and SomeContextProcessWithInput<T>
and boost::json::has_value_to<typename T::Input>::value;
// clang-format on

/**
 * @brief Specifies what a Handler without Input must provide.
 */
// clang-format off
template <typename T>
concept SomeHandlerWithoutInput = SomeContextProcessWithoutInput<T>;
// clang-format on

/**
 * @brief Specifies what a Handler type must provide.
 */
// clang-format off
template <typename T>
concept SomeHandler = 
(
    SomeHandlerWithInput<T> or
    SomeHandlerWithoutInput<T>
) 
and boost::json::has_value_from<typename T::Output>::value;
// clang-format on

}  // namespace rpc
