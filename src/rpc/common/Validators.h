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
    auto hasError = false;
    if constexpr (std::is_same_v<Expected, bool>)
    {
        if (not value.is_bool())
            hasError = true;
    }
    else if constexpr (std::is_same_v<Expected, std::string>)
    {
        if (not value.is_string())
            hasError = true;
    }
    else if constexpr (
        std::is_same_v<Expected, double> or std::is_same_v<Expected, float>)
    {
        if (not value.is_double())
            hasError = true;
    }
    else if constexpr (std::is_same_v<Expected, boost::json::array>)
    {
        if (not value.is_array())
            hasError = true;
    }
    else if constexpr (std::is_same_v<Expected, boost::json::object>)
    {
        if (not value.is_object())
            hasError = true;
    }
    else if constexpr (
        std::is_convertible_v<Expected, uint64_t> or
        std::is_convertible_v<Expected, int64_t>)
    {
        if (not value.is_int64() && not value.is_uint64())
            hasError = true;
    }

    return not hasError;
}

/**
 * @brief A meta-validator that acts as a spec for a sub-object/section
 */
class Section final
{
    std::vector<FieldSpec> specs;

public:
    /**
     * @brief Construct new section validator from a list of specs
     *
     * @param specs List of specs @ref FieldSpec
     */
    explicit Section(std::initializer_list<FieldSpec> specs) : specs{specs}
    {
    }

    /**
     * @brief Verify that the JSON value representing the section is valid
     * according to the given specs
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the section from the outer object
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief A validator that simply requires a field to be present
 */
struct Required final
{
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief Validates that the type of the value is one of the given types
 */
template <typename... Types>
struct Type final
{
    /**
     * @brief Verify that the JSON value is (one) of specified type(s)
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer
     * object
     */
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
class Between final
{
    Type min_;
    Type max_;

public:
    /**
     * @brief Construct the validator storing min and max values
     *
     * @param min
     * @param max
     */
    explicit Between(Type min, Type max) : min_{min}, max_{max}
    {
    }

    /**
     * @brief Verify that the JSON value is within a certain range
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer
     * object
     */
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
class EqualTo final
{
    Type original_;

public:
    /**
     * @brief Construct the validator with stored original value
     *
     * @param original The original value to store
     */
    explicit EqualTo(Type original) : original_{original}
    {
    }

    /**
     * @brief Verify that the JSON value is equal to the stored original
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer
     * object
     */
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
 * @brief Deduction guide to help disambiguate what it means to EqualTo a
 * "string" without specifying the type.
 */
EqualTo(char const*)->EqualTo<std::string>;

/**
 * @brief Validates that the value is one of the values passed in
 */
template <typename Type>
class OneOf final
{
    std::vector<Type> options_;

public:
    /**
     * @brief Construct the validator with stored options
     *
     * @param options The list of allowed options
     */
    explicit OneOf(std::initializer_list<Type> options) : options_{options}
    {
    }

    /**
     * @brief Verify that the JSON value is one of the stored options
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer
     * object
     */
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
 * @brief Deduction guide to help disambiguate what it means to OneOf a
 * few "strings" without specifying the type.
 */
OneOf(std::initializer_list<char const*>)->OneOf<std::string>;

/**
 * @brief A meta-validator that specifies a list of specs to run against the
 * object at the given index in the array
 */
class ValidateArrayAt final
{
    std::size_t idx_;
    std::vector<FieldSpec> specs_;

public:
    /**
     * @brief Constructs a validator that validates the specified element of a
     * JSON array
     *
     * @param idx The index inside the array to validate
     * @param specs The specifications to validate against
     */
    ValidateArrayAt(std::size_t idx, std::initializer_list<FieldSpec> specs)
        : idx_{idx}, specs_{specs}
    {
    }

    /**
     * @brief Verify that the JSON array element at given index is valid
     * according the stored specs
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the array from the outer object
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief A meta-validator that specifies a list of requirements to run against
 * when the type matches the template parameter
 */
template <typename Type>
class IfType final
{
public:
    /**
     * @brief Constructs a validator that validates the specs if the type
     * matches
     * @param requirements The requirements to validate against
     */
    template <Requirement... Requirements>
    IfType(Requirements&&... requirements)
    {
        validator_ = [... r = std::forward<Requirements>(requirements)](
                         boost::json::value const& j,
                         std::string_view key) -> MaybeError {
            // clang-format off
            std::optional<RPC::Status> firstFailure = std::nullopt;

            // the check logic is the same as fieldspec
            ([&j, &key, &firstFailure, req = &r]() {
                if (firstFailure)
                    return;

                if (auto const res = req->verify(j, key); not res)
                    firstFailure = res.error();
            }(), ...);
            // clang-format on

            if (firstFailure)
                return Error{firstFailure.value()};

            return {};
        };
    }

    /**
     * @brief Verify that the element is valid
     * according the stored requirements when type matches
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the element from the outer object
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail
                        // instead

        if (not checkType<Type>(value.as_object().at(key.data())))
            return {};  // ignore if type does not match

        return validator_(value, key);
    }

private:
    std::function<MaybeError(boost::json::value const&, std::string_view)>
        validator_;
};

/**
 * @brief A meta-validator that wrapp other validator to send the customized
 * error
 */
template <typename Requirement>
class WithCustomError final
{
    Requirement requirement;
    RPC::Status error;

public:
    /**
     * @brief Constructs a validator that calls the given validator "req" and
     * return customized error "err"
     */
    WithCustomError(Requirement req, RPC::Status err)
        : requirement{std::move(req)}, error{err}
    {
    }

    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (auto const res = requirement.verify(value, key); not res)
            return Error{error};

        return {};
    }
};

/**
 * @brief A meta-validator that allows to specify a custom validation function
 */
class CustomValidator final
{
    std::function<MaybeError(boost::json::value const&, std::string_view)>
        validator_;

public:
    /**
     * @brief Constructs a custom validator from any supported callable
     *
     * @tparam Fn The type of callable
     * @param fn The callable/function object
     */
    template <typename Fn>
    explicit CustomValidator(Fn&& fn) : validator_{std::forward<Fn>(fn)}
    {
    }

    /**
     * @brief Verify that the JSON value is valid according to the custom
     * validation function stored
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer
     * object
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief Helper function to check if sv is an uint32 number or not
 */
[[nodiscard]] bool
checkIsU32Numeric(std::string_view sv);

/**
 * @brief Provide a common used validator for ledger index
 * LedgerIndex must be a string or int
 * If the specified LedgerIndex is a string, it's value must be either
 * "validated" or a valid integer value represented as a string.
 */
extern CustomValidator LedgerIndexValidator;

/**
 * @brief Provide a common used validator for account
 * Account must be a string and the converted public key is valid
 */
extern CustomValidator AccountValidator;

/**
 * @brief Provide a common used validator for account
 * Account must be a string and can convert to base58
 */
extern CustomValidator AccountBase58Validator;

/**
 * @brief Provide a common used validator for marker
 *  Marker is composed of a comma separated index and start hint. The
 *   former will be read as hex, and the latter can cast to uint64.
 */
extern CustomValidator MarkerValidator;

/**
 * @brief Provide a common used validator for uint256 hex string
 * It must be a string and hex
 * Transaction index, ledger hash all use this validator
 */
extern CustomValidator Uint256HexStringValidator;

/**
 * @brief Provide a common used validator for currency
 * including standard currency code and token code
 */
extern CustomValidator CurrencyValidator;

}  // namespace RPCng::validation
