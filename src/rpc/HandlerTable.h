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
#include <rpc/common/AnyHandler.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace RPC {

class HandlerTable
{
    struct Handler
    {
        RPCng::AnyHandler handler;
        bool isClioOnly = false;
    };

    std::unordered_map<std::string, Handler> handlerMap_;

public:
    HandlerTable(std::shared_ptr<BackendInterface> const& backend);

    bool
    contains(std::string const& method) const
    {
        return handlerMap_.contains(method);
    }

    std::optional<RPCng::AnyHandler>
    getHandler(std::string const& command) const
    {
        if (!handlerMap_.contains(command))
            return {};

        return handlerMap_.at(command).handler;
    }

    bool
    isClioOnly(std::string const& command) const
    {
        return handlerMap_.contains(command) &&
            handlerMap_.at(command).isClioOnly;
    }
};

}  // namespace RPC
