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

struct Context : public util::Taggable
{
    clio::Logger perfLog_{"Performance"};
    boost::asio::yield_context& yield;
    std::string method;
    std::uint32_t version;
    boost::json::object const& params;
    std::shared_ptr<Server::ConnectionBase> session;
    Backend::LedgerRange const& range;
    std::string clientIp;

    Context(
        boost::asio::yield_context& yield_,
        std::string const& command_,
        std::uint32_t version_,
        boost::json::object const& params_,
        std::shared_ptr<Server::ConnectionBase> const& session_,
        util::TagDecoratorFactory const& tagFactory_,
        Backend::LedgerRange const& range_,
        std::string const& clientIp_)
        : Taggable(tagFactory_)
        , yield(yield_)
        , method(command_)
        , version(version_)
        , params(params_)
        , session(session_)
        , range(range_)
        , clientIp(clientIp_)
    {
        perfLog_.debug() << tag() << "new Context created";
    }
};

}  // namespace Web
