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

#include "rpc/common/Checkers.hpp"
#include "rpc/common/Concepts.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/impl/Factories.hpp"

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief Represents a Specification for one field of an RPC command.
 */
struct FieldSpec final {
    /**
     * @brief Construct a field specification out of a set of processors.
     *
     * @tparam Processors The types of processors
     * @param key The key in a JSON object that the field validates
     * @param processors The processors, each of them have to fulfil the @ref rpc::SomeProcessor concept
     */
    template <SomeProcessor... Processors>
    FieldSpec(std::string const& key, Processors&&... processors)
        : processor_{impl::makeFieldProcessor<Processors...>(key, std::forward<Processors>(processors)...)}
        , checker_{impl::EMPTY_FIELD_CHECKER}
    {
    }

    /**
     * @brief Construct a field specification out of a set of checkers.
     *
     * @tparam Checks The types of checkers
     * @param key The key in a JSON object that the field validates
     * @param checks The checks, each of them have to fulfil the @ref rpc::SomeCheck concept
     */
    template <SomeCheck... Checks>
    FieldSpec(std::string const& key, Checks&&... checks)
        : processor_{impl::EMPTY_FIELD_PROCESSOR}
        , checker_{impl::makeFieldChecker<Checks...>(key, std::forward<Checks>(checks)...)}
    {
    }

    /**
     * @brief Processes the passed JSON value using the stored processors.
     *
     * @param value The JSON value to validate and/or modify
     * @return Nothing on success; @ref Status on error
     */
    [[nodiscard]] MaybeError
    process(boost::json::value& value) const;

    /**
     * @brief Checks the passed JSON value using the stored checkers.
     *
     * @param value The JSON value to validate
     * @return A vector of warnings (empty if no warnings)
     */
    [[nodiscard]] check::Warnings
    check(boost::json::value const& value) const;

private:
    impl::FieldSpecProcessor processor_;
    impl::FieldChecker checker_;
};

/**
 * @brief Represents a Specification of an entire RPC command.
 *
 * Note: this should really be all constexpr and handlers would expose
 * static constexpr RpcSpec spec instead. Maybe some day in the future.
 */
struct RpcSpec final {
    /**
     * @brief Construct a full RPC request specification.
     *
     * @param fields The fields of the RPC specification @ref FieldSpec
     */
    RpcSpec(std::initializer_list<FieldSpec> fields) : fields_{fields}
    {
    }

    /**
     * @brief Construct a full RPC request specification from another spec and additional fields.
     *
     * @param other The other spec to copy fields from
     * @param additionalFields The additional fields to add to the spec
     */
    RpcSpec(RpcSpec const& other, std::initializer_list<FieldSpec> additionalFields) : fields_{other.fields_}
    {
        for (auto& f : additionalFields)
            fields_.push_back(f);
    }

    /**
     * @brief Processes the passed JSON value using the stored field specs.
     *
     * @param value The JSON value to validate and/or modify
     * @return Nothing on success; @ref Status on error
     */
    [[nodiscard]] MaybeError
    process(boost::json::value& value) const;

    /**
     * @brief Checks the passed JSON value using the stored field specs.
     *
     * @param value The JSON value to validate
     * @return JSON array of warnings (empty if no warnings)
     */
    [[nodiscard]] boost::json::array
    check(boost::json::value const& value) const;

private:
    std::vector<FieldSpec> fields_;
};

}  // namespace rpc
