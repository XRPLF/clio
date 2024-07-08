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
#include "rpc/common/Types.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/ErrorCodes.h>

#include <exception>
#include <functional>
#include <string>
#include <string_view>

namespace rpc::modifiers {

/**
 * @brief Clamp value between min and max.
 */
template <typename Type>
class Clamp final {
    Type min_;
    Type max_;

public:
    /**
     * @brief Construct the modifier storing min and max values.
     *
     * @param min
     * @param max
     */
    explicit Clamp(Type min, Type max) : min_{min}, max_{max}
    {
    }

    /**
     * @brief Clamp the value to stored min and max values.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the modified value from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] MaybeError
    modify(boost::json::value& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail instead

        // clamp to min_ and max_
        auto const oldValue = value_to<Type>(value.as_object().at(key.data()));
        value.as_object()[key.data()] = std::clamp<Type>(oldValue, min_, max_);

        return {};
    }
};

/**
 * @brief Convert input string to lower case.
 *
 * Note: the conversion is only performed if the input value is a string.
 */
struct ToLower final {
    /**
     * @brief Update the input string to lower case.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the modified value from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] static MaybeError
    modify(boost::json::value& value, std::string_view key)
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail instead

        if (not value.as_object().at(key.data()).is_string())
            return {};  // ignore for non-string types

        value.as_object()[key.data()] =
            util::toLower(boost::json::value_to<std::string>(value.as_object().at(key.data())));
        return {};
    }
};

/**
 * @brief Convert input string to integer.
 *
 * Note: the conversion is only performed if the input value is a string.
 */
struct ToNumber final {
    /**
     * @brief Update the input string to integer if it can be converted to integer by stoi.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the modified value from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] static MaybeError
    modify(boost::json::value& value, std::string_view key)
    {
        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        if (not value.as_object().at(key).is_string())
            return {};  // ignore for non-string types

        auto const strInt = boost::json::value_to<std::string>(value.as_object().at(key));
        if (strInt.find('.') != std::string::npos)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};  // maybe a float

        try {
            value.as_object()[key.data()] = std::stoi(strInt);
        } catch (std::exception& e) {
            return Error{Status{RippledError::rpcINVALID_PARAMS}};
        }
        return {};
    }
};

/**
 * @brief Customised modifier allowing user define how to modify input in provided callable.
 */
class CustomModifier final {
    std::function<MaybeError(boost::json::value&, std::string_view)> modifier_;

public:
    /**
     * @brief Constructs a custom modifier from any supported callable.
     *
     * @tparam Fn The type of callable
     * @param fn The callable/function object
     */
    template <typename Fn>
        requires std::invocable<Fn, boost::json::value&, std::string_view>
    explicit CustomModifier(Fn&& fn) : modifier_{std::forward<Fn>(fn)}
    {
    }

    /**
     * @brief Modify the JSON value according to the custom modifier function stored.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return Any compatible user-provided error if modify/verify failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    modify(boost::json::value& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        return modifier_(value.as_object().at(key.data()), key);
    };
};

}  // namespace rpc::modifiers
