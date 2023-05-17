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

#include <rpc/Errors.h>
#include <util/Expected.h>

#include <ripple/basics/base_uint.h>

#include <boost/asio/spawn.hpp>
#include <boost/json/value.hpp>

namespace Server {
struct ConnectionBase;
}
class SubscriptionManager;

namespace RPC {

/**
 * @brief Return type used for Validators that can return error but don't have
 * specific value to return
 */
using MaybeError = util::Expected<void, Status>;

/**
 * @brief The type that represents just the error part of @ref MaybeError
 */
using Error = util::Unexpected<Status>;

/**
 * @brief Return type for each individual handler
 */
template <typename OutputType>
using HandlerReturnType = util::Expected<OutputType, Status>;

/**
 * @brief The final return type out of RPC engine
 */
using ReturnType = util::Expected<boost::json::value, Status>;

struct RpcSpec;
struct FieldSpec;

using RpcSpecConstRef = RpcSpec const&;

struct VoidOutput
{
};

struct Context
{
    // TODO: we shall change yield_context to const yield_context after we
    // update backend interfaces to use const& yield
    std::reference_wrapper<boost::asio::yield_context> yield;
    std::shared_ptr<Server::ConnectionBase> session;
    bool isAdmin = false;
    std::string clientIp;
};

using Result = std::variant<Status, boost::json::object>;

struct AccountCursor
{
    ripple::uint256 index;
    std::uint32_t hint;

    std::string
    toString() const
    {
        return ripple::strHex(index) + "," + std::to_string(hint);
    }

    bool
    isNonZero() const
    {
        return index.isNonZero() || hint != 0;
    }
};

class AnyHandler;

class HandlerProvider
{
public:
    virtual ~HandlerProvider() = default;

    virtual bool
    contains(std::string const& method) const = 0;

    virtual std::optional<AnyHandler>
    getHandler(std::string const& command) const = 0;

    virtual bool
    isClioOnly(std::string const& command) const = 0;
};

inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, VoidOutput const&)
{
    jv = boost::json::object{};
}

}  // namespace RPC
