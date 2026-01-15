//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/Assert.hpp"

#include "util/log/Logger.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

namespace util::impl {

OnAssert::ActionType OnAssert::action;

void
OnAssert::call(std::string_view message)
{
    if (not OnAssert::action) {
        resetAction();
    }
    OnAssert::action(message);
}

void
OnAssert::setAction(ActionType newAction)
{
    OnAssert::action = std::move(newAction);
}

void
OnAssert::resetAction()
{
    OnAssert::action = [](std::string_view m) { OnAssert::defaultAction(m); };
}

void
OnAssert::defaultAction(std::string_view message)
{
    if (LogServiceState::initialized() and LogServiceState::hasSinks()) {
        LOG(LogService::fatal()) << message;
    } else {
        std::cerr << message << std::endl;
    }
    std::exit(EXIT_FAILURE);  // std::abort does not flush gcovr output and causes uncovered lines
}

}  // namespace util::impl
