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
#include <rpc/common/Types.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace RPC {

class HandlerTable
{
    std::shared_ptr<HandlerProvider const> provider_;

public:
    HandlerTable(std::shared_ptr<HandlerProvider const> const& provider) : provider_{provider}
    {
    }

    bool
    contains(std::string const& method) const
    {
        return provider_->contains(method);
    }

    std::optional<AnyHandler>
    getHandler(std::string const& command) const
    {
        return provider_->getHandler(command);
    }

    bool
    isClioOnly(std::string const& command) const
    {
        return provider_->isClioOnly(command);
    }
};

}  // namespace RPC
