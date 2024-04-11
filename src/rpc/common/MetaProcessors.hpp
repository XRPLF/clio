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
#include "rpc/common/Concepts.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/value.hpp>
#include <fmt/core.h>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc::meta {

/**
 * @brief A meta-processor that acts as a spec for a sub-object/section.
 */
class Section final {
    std::vector<FieldSpec> specs;

public:
    /**
     * @brief Construct new section validator from a list of specs.
     *
     * @param specs List of specs @ref FieldSpec
     */
    explicit Section(std::initializer_list<FieldSpec> specs) : specs{specs}
    {
    }

    /**
     * @brief Verify that the JSON value representing the section is valid according to the given specs.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the section from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value& value, std::string_view key) const;
};

/**
 * @brief A meta-processor that specifies a list of specs to run against the object at the given index in the array.
 */
class ValidateArrayAt final {
    std::size_t idx_;
    std::vector<FieldSpec> specs_;

public:
    /**
     * @brief Constructs a processor that validates the specified element of a JSON array.
     *
     * @param idx The index inside the array to validate
     * @param specs The specifications to validate against
     */
    ValidateArrayAt(std::size_t idx, std::initializer_list<FieldSpec> specs) : idx_{idx}, specs_{specs}
    {
    }

    /**
     * @brief Verify that the JSON array element at given index is valid according the stored specs.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the array from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value& value, std::string_view key) const;
};

/**
 * @brief A meta-processor that specifies a list of requirements to run against when the type matches the template
 * parameter.
 */
template <typename Type>
class IfType final {
public:
    /**
     * @brief Constructs a validator that validates the specs if the type matches.
     * @param requirements The requirements to validate against
     */
    template <SomeRequirement... Requirements>
    explicit IfType(Requirements&&... requirements)
        : processor_(
              [... r = std::forward<Requirements>(requirements
               )](boost::json::value& j, std::string_view key) -> MaybeError {
                  std::optional<Status> firstFailure = std::nullopt;

                  // the check logic is the same as fieldspec
                  (
                      [&j, &key, &firstFailure, req = &r]() {
                          if (firstFailure)
                              return;

                          if (auto const res = req->verify(j, key); not res)
                              firstFailure = res.error();
                      }(),
                      ...
                  );

                  if (firstFailure)
                      return Error{firstFailure.value()};

                  return {};
              }
          )
    {
    }

    IfType(IfType const&) = default;
    IfType(IfType&&) = default;

    /**
     * @brief Verify that the element is valid according to the stored requirements when type matches.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the element from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key.data()))
            return {};  // ignore. field does not exist, let 'required' fail instead

        if (not rpc::validation::checkType<Type>(value.as_object().at(key.data())))
            return {};  // ignore if type does not match

        return processor_(value, key);
    }

private:
    std::function<MaybeError(boost::json::value&, std::string_view)> processor_;
};

/**
 * @brief A meta-processor that wraps a validator and produces a custom error in case the wrapped validator fails.
 */
template <typename SomeRequirement>
class WithCustomError final {
    SomeRequirement requirement;
    Status error;

public:
    /**
     * @brief Constructs a validator that calls the given validator `req` and returns a custom error `err` in case `req`
     * fails.
     *
     * @param req The requirement to validate against
     * @param err The custom error to return in case `req` fails
     */
    WithCustomError(SomeRequirement req, Status err) : requirement{std::move(req)}, error{std::move(err)}
    {
    }

    /**
     * @brief Runs the stored validator and produces a custom error if the wrapped validator fails.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the element from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (auto const res = requirement.verify(value, key); not res)
            return Error{error};

        return {};
    }

    /**
     * @brief Runs the stored validator and produces a custom error if the wrapped validator fails. This is an overload
     * for the requirement which can modify the value. Such as IfType.
     *
     * @param value The JSON value representing the outer object, this value can be modified by the requirement inside
     * @param key The key used to retrieve the element from the outer object
     * @return Possibly an error
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value& value, std::string_view key) const
    {
        if (auto const res = requirement.verify(value, key); not res)
            return Error{error};

        return {};
    }
};

}  // namespace rpc::meta
