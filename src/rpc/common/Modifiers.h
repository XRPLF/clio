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
#include <rpc/common/Specs.h>
#include <rpc/common/Types.h>
#include <util/JsonUtils.h>

#include <string_view>

namespace rpc::modifiers {

/**
 * @brief Clamp value between min and max.
 */
template <typename Type>
class Clamp final
{
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
struct ToLower final
{
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

        value.as_object()[key.data()] = util::toLower(value.as_object().at(key.data()).as_string().c_str());
        return {};
    }
};

}  // namespace rpc::modifiers
