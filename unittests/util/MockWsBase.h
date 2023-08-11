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

#include <web/interface/ConnectionBase.h>

struct MockSession : public web::ConnectionBase
{
    std::string message;
    void
    send(std::shared_ptr<std::string> msg_type) override
    {
        message += std::string(msg_type->data());
    }

    void
    send(std::string&& msg, boost::beast::http::status status = boost::beast::http::status::ok) override
    {
        message += msg;
    }

    MockSession(util::TagDecoratorFactory const& factory) : web::ConnectionBase(factory, "")
    {
    }
};

struct MockDeadSession : public web::ConnectionBase
{
    void
    send(std::shared_ptr<std::string> _) override
    {
        // err happen, the session should remove from subscribers
        ec_.assign(2, boost::system::system_category());
    }

    void
    send(std::string&& _, boost::beast::http::status __ = boost::beast::http::status::ok) override
    {
    }

    MockDeadSession(util::TagDecoratorFactory const& factory) : web::ConnectionBase(factory, "")
    {
    }
};
