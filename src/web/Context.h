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

#include <data/BackendInterface.h>
#include <util/Taggable.h>
#include <util/log/Logger.h>
#include <web/interface/ConnectionBase.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <memory>
#include <string>
#include <utility>

namespace web {

/**
 * @brief Context that is used by the Webserver to pass around information about an incoming request.
 */
struct Context : util::Taggable
{
    boost::asio::yield_context yield;
    std::string method;
    std::uint32_t apiVersion;
    boost::json::object params;
    std::shared_ptr<web::ConnectionBase> session;
    data::LedgerRange range;
    std::string clientIp;
    bool isAdmin;

    /**
     * @brief Create a new Context instance.
     *
     * @param yield The coroutine context
     * @param command The method/command requested
     * @param apiVersion The api_version parsed from the request
     * @param params Request's parameters/data as a JSON object
     * @param session The connection to the peer
     * @param tagFactory A factory that is used to generate tags to track requests and connections
     * @param range The ledger range that is available at the time of the request
     * @param clientIp IP of the peer
     * @param isAdmin Whether the peer has admin privileges
     */
    Context(
        boost::asio::yield_context yield,
        std::string command,
        std::uint32_t apiVersion,
        boost::json::object params,
        std::shared_ptr<web::ConnectionBase> const& session,
        util::TagDecoratorFactory const& tagFactory,
        data::LedgerRange const& range,
        std::string clientIp,
        bool isAdmin
    )
        : Taggable(tagFactory)
        , yield(std::move(yield))
        , method(std::move(command))
        , apiVersion(apiVersion)
        , params(std::move(params))
        , session(session)
        , range(range)
        , clientIp(std::move(clientIp))
        , isAdmin(isAdmin)
    {
        static util::Logger const perfLog{"Performance"};
        LOG(perfLog.debug()) << tag() << "new Context created";
    }

    Context(Context&&) = default;
    Context&
    operator=(Context&&) = default;
};

}  // namespace web
