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
#include <rpc/common/Types.h>
#include <rpc/common/impl/Factories.h>

#include <string>
#include <vector>

namespace RPCng {

/**
 * @brief Represents a Specification for one field of an RPC command
 */
struct FieldSpec final
{
    /**
     * @brief Construct a field specification out of a set of requirements
     *
     * @tparam Requirements The types of requirements @ref Requirement
     * @param key The key in a JSON object that the field validates
     * @param requirements The requirements, each of them have to fulfil
     * the @ref Requirement concept
     */
    template <Requirement... Requirements>
    FieldSpec(std::string const& key, Requirements&&... requirements)
        : validator_{detail::makeFieldValidator<Requirements...>(
              key,
              std::forward<Requirements>(requirements)...)}
    {
    }

    /**
     * @brief Validates the passed JSON value using the stored requirements
     *
     * @param value The JSON value to validate
     * @return Nothing on success; @ref RPC::Status on error
     */
    [[nodiscard]] MaybeError
    validate(boost::json::value const& value) const;

private:
    std::function<MaybeError(boost::json::value const&)> validator_;
};

/**
 * @brief Represents a Specification of an entire RPC command
 *
 * Note: this should really be all constexpr and handlers would expose
 * static constexpr RpcSpec spec instead. Maybe some day in the future.
 */
struct RpcSpec final
{
    /**
     * @brief Construct a full RPC request specification
     *
     * @param fields The fields of the RPC specification @ref FieldSpec
     */
    RpcSpec(std::initializer_list<FieldSpec> fields) : fields_{fields}
    {
    }

    /**
     * @brief Validates the passed JSON value using the stored field specs
     *
     * @param value The JSON value to validate
     * @return Nothing on success; @ref RPC::Status on error
     */
    [[nodiscard]] MaybeError
    validate(boost::json::value const& value) const;

private:
    std::vector<FieldSpec> fields_;
};

}  // namespace RPCng
