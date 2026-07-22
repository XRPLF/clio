#pragma once

#include "rpc/Errors.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc {

struct RpcSpec;

/**
 * @brief Specifies what a requirement used with @ref rpc::FieldSpec must provide.
 */
template <typename T>
concept SomeRequirement = requires(T a, boost::json::value lval) {
    { a.verify(lval, std::string{}) } -> std::same_as<MaybeError>;
};

/**
 * @brief Specifies what a modifier used with @ref rpc::FieldSpec must provide.
 */
template <typename T>
concept SomeModifier = requires(T a, boost::json::value lval) {
    { a.modify(lval, std::string{}) } -> std::same_as<MaybeError>;
};

/**
 * @brief Specifies what a check used with @ref rpc::FieldSpec must provide.
 */
template <typename T>
concept SomeCheck = requires(T a, boost::json::value lval) {
    { a.check(lval, std::string{}) } -> std::same_as<std::optional<check::Warning>>;
};

/**
 * @brief The requirements of a processor to be used with @ref rpc::FieldSpec.
 */
template <typename T>
concept SomeProcessor = (SomeRequirement<T> or SomeModifier<T>);

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
 * @brief Specifies what a Handler with Input must provide.
 */
template <typename T>
concept SomeHandlerWithInput = requires(T a, uint32_t version) {
    { a.spec(version) } -> std::same_as<RpcSpec const&>;
} and SomeContextProcessWithInput<T> and boost::json::has_value_to<typename T::Input>::value;

/**
 * @brief Specifies what a Handler without Input must provide.
 */
template <typename T>
concept SomeHandlerWithoutInput = SomeContextProcessWithoutInput<T>;

/**
 * @brief Specifies what a Handler type must provide.
 */
template <typename T>
concept SomeHandler = (SomeHandlerWithInput<T> or SomeHandlerWithoutInput<T>) and
    boost::json::has_value_from<typename T::Output>::value;

}  // namespace rpc
