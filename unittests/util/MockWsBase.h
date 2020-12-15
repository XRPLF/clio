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

#include <webserver/WsBase.h>

struct MockSession : public WsBase
{
    std::string message;
    void
    send(std::shared_ptr<Message> msg_type) override
    {
        message += std::string(msg_type->data());
    }
    MockSession(util::TagDecoratorFactory const& factory) : WsBase(factory)
    {
    }
};

struct MockDeadSession : public WsBase
{
    void
    send(std::shared_ptr<Message> msg_type) override
    {
        // err happen, the session should remove from subscribers
        ec_.assign(2, boost::system::system_category());
    }
    MockDeadSession(util::TagDecoratorFactory const& factory) : WsBase(factory)
    {
    }
};
