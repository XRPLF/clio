#pragma once

#include "rpc/Errors.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <rpcspec/Errors.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief A process function that expects both some Input and a Context.
 */
template <typename T>
concept SomeContextProcessWithInput =
    requires(T a, T::Input const& in, T::Output out, Context const& ctx) {
        { a.process(in, ctx) } -> std::same_as<HandlerReturnType<decltype(out)>>;
    };

/**
 * @brief A process function that expects no Input but does take a Context.
 */
template <typename T>
concept SomeContextProcessWithoutInput = requires(T a, T::Output out, Context const& ctx) {
    { a.process(ctx) } -> std::same_as<HandlerReturnType<decltype(out)>>;
};

/**
 * @brief Specifies what a Handler validated by the shared consteval spec must provide.
 *
 * Such a handler inherits @c rpc::HandlerFor<Input>, which binds the shared spec base to
 * Clio JSON type and supplies a static @c parseInput (validate and deserialise in one pass)
 * and a static @c spec returning a type-erased view of the spec.
 */
template <typename T>
concept SomeHandlerWithTypedInput = requires(uint32_t version, boost::json::value jv) {
    typename T::Input;
    { T::parseInput(jv, version) } -> std::same_as<std::expected<typename T::Input, Status>>;
    { T::spec(version) } -> std::same_as<SpecView>;
} and SomeContextProcessWithInput<T>;

/**
 * @brief Specifies what a Handler without Input must provide.
 */
template <typename T>
concept SomeHandlerWithoutInput = SomeContextProcessWithoutInput<T>;

/**
 * @brief Specifies what a Handler type must provide.
 */
template <typename T>
concept SomeHandler = (SomeHandlerWithTypedInput<T> or SomeHandlerWithoutInput<T>) and
    boost::json::has_value_from<typename T::Output>::value;

}  // namespace rpc
