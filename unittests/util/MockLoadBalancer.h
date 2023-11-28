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

#include "etl/Source.h"
#include "util/FakeFetchResponse.h"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <gmock/gmock.h>

#include <optional>

struct MockLoadBalancer {
    using RawLedgerObjectType = FakeLedgerObject;

    MOCK_METHOD(void, loadInitialLedger, (std::uint32_t, bool), ());
    MOCK_METHOD(std::optional<FakeFetchResponse>, fetchLedger, (uint32_t, bool, bool), ());
    MOCK_METHOD(bool, shouldPropagateTxnStream, (etl::Source*), (const));
    MOCK_METHOD(boost::json::value, toJson, (), (const));
    MOCK_METHOD(
        std::optional<boost::json::object>,
        forwardToRippled,
        (boost::json::object const&, std::optional<std::string> const&, boost::asio::yield_context),
        (const)
    );
};
