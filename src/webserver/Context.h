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

#include <backend/BackendInterface.h>
#include <log/Logger.h>
#include <util/Taggable.h>
#include <webserver/interface/ConnectionBase.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <memory>
#include <string>

namespace Web {

struct Context : util::Taggable
{
    std::reference_wrapper<boost::asio::yield_context> yield;
    std::string method;
    std::uint32_t apiVersion;
    boost::json::object params;
    std::shared_ptr<Server::ConnectionBase> session;
    Backend::LedgerRange range;
    std::string clientIp;

    Context(
        boost::asio::yield_context yield,
        std::string const& command,
        std::uint32_t apiVersion,
        boost::json::object params,
        std::shared_ptr<Server::ConnectionBase> const& session,
        util::TagDecoratorFactory const& tagFactory,
        Backend::LedgerRange const& range,
        std::string const& clientIp)
        : Taggable(tagFactory)
        , yield(std::ref(yield))
        , method(command)
        , apiVersion(apiVersion)
        , params(std::move(params))
        , session(session)
        , range(range)
        , clientIp(clientIp)
    {
        static clio::Logger perfLog{"Performance"};
        perfLog.debug() << tag() << "new Context created";
    }

    Context(Context&&) = default;
    Context&
    operator=(Context&&) = default;
};

}  // namespace Web
