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

#include <boost/json.hpp>
#include <gmock/gmock.h>

#include <chrono>

struct MockCounters {
    MOCK_METHOD(void, rpcFailed, (std::string const&), ());
    MOCK_METHOD(void, rpcErrored, (std::string const&), ());
    MOCK_METHOD(void, rpcComplete, (std::string const&, std::chrono::microseconds const&), ());
    MOCK_METHOD(void, rpcForwarded, (std::string const&), ());
    MOCK_METHOD(void, rpcFailedToForward, (std::string const&), ());
    MOCK_METHOD(void, onTooBusy, (), ());
    MOCK_METHOD(void, onNotReady, (), ());
    MOCK_METHOD(void, onBadSyntax, (), ());
    MOCK_METHOD(void, onUnknownCommand, (), ());
    MOCK_METHOD(void, onInternalError, (), ());
    MOCK_METHOD(boost::json::object, report, (), (const));
    MOCK_METHOD(std::chrono::seconds, uptime, (), (const));
};
