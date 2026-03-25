#pragma once

namespace util {

/**
 * @brief Overload set for lambdas
 *
 * @tparam Ts Types of lambdas
 */
template <typename... Ts>
struct OverloadSet : Ts... {
    using Ts::operator()...;
};

/**
 * @brief Deduction guide for OverloadSet
 *
 * @tparam Ts Types of lambdas
 */
template <class... Ts>
OverloadSet(Ts...) -> OverloadSet<Ts...>;

}  // namespace util
