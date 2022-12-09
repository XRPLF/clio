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

namespace RPCng::validation {

/**
 * @brief Check that the type is the same as what was expected
 *
 * @tparam Expected The expected type that value should be convertible to
 * @param value The json value to check the type of
 * @return true if convertible; false otherwise
 */
template <typename Expected>
[[nodiscard]] bool static checkType(boost::json::value const& value)
{
    auto has_error = false;
    if constexpr (std::is_same_v<Expected, bool>)
    {
        if (not value.is_bool())
            has_error = true;
    }
    else if constexpr (std::is_same_v<Expected, std::string>)
    {
        if (not value.is_string())
            has_error = true;
    }
    else if constexpr (
        std::is_same_v<Expected, double> or std::is_same_v<Expected, float>)
    {
        if (not value.is_double())
            has_error = true;
    }
    else if constexpr (
        std::is_convertible_v<Expected, uint64_t> or
        std::is_convertible_v<Expected, int64_t>)
    {
        if (not value.is_int64() && not value.is_uint64())
            has_error = true;
    }
    else if constexpr (std::is_same_v<Expected, boost::json::array>)
    {
        if (not value.is_array())
            has_error = true;
    }

    return not has_error;
}

/**
 * @brief A meta-validator that acts as a spec for a sub-object/section
 */
class section final
{
    std::vector<FieldSpec> specs;

public:
    explicit section(std::initializer_list<FieldSpec> specs) : specs{specs}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief A validator that simply requires a field to be present
 */
struct required final
{
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief Validates that the type of the value is one of the given types
 */
template <typename... Types>
struct type final
{
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail
                        // instead

        auto const& res = value.as_object().at(key.data());
        auto const convertible = (checkType<Types>(res) || ...);

        if (not convertible)
            return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is between specified min and max
 */
template <typename Type>
class between final
{
    Type min_;
    Type max_;

public:
    explicit between(Type min, Type max) : min_{min}, max_{max}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail
                        // instead

        using boost::json::value_to;
        auto const res = value_to<Type>(value.as_object().at(key.data()));
        // todo: may want a way to make this code more generic (e.g. use a free
        // function that can be overridden for this comparison)
        if (res < min_ || res > max_)
            return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validates that the value is equal to the one passed in
 */
template <typename Type>
class equalTo final
{
    Type original_;

public:
    explicit equalTo(Type original) : original_{original}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail
                        // instead

        using boost::json::value_to;
        auto const res = value_to<Type>(value.as_object().at(key.data()));
        if (res != original_)
            return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validates that the value is one of the values passed in
 */
template <typename Type>
class oneOf final
{
    std::vector<Type> options_;

public:
    explicit oneOf(std::initializer_list<Type> options) : options_{options}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail
                        // instead

        using boost::json::value_to;
        auto const res = value_to<Type>(value.as_object().at(key.data()));
        if (std::find(std::begin(options_), std::end(options_), res) ==
            std::end(options_))
            return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief A meta-validator that specifies a list of specs to run against the
 * object at the given index in the array
 */
class validateArrayAt final
{
    std::size_t idx_;
    std::vector<FieldSpec> specs_;

public:
    validateArrayAt(std::size_t idx, std::initializer_list<FieldSpec> specs)
        : idx_{idx}, specs_{specs}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief A meta-validator that allows to specify a custom validation function
 */
class customValidator final
{
    std::function<MaybeError(boost::json::value const&, std::string_view)>
        validator_;

public:
    template <typename Fn>
    explicit customValidator(Fn&& fn) : validator_{std::forward<Fn>(fn)}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

}  // namespace RPCng::validation
