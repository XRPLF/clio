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

#include "rpc/Errors.hpp"
#include "util/FakeFetchResponse.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

struct MockLoadBalancer {
    using RawLedgerObjectType = FakeLedgerObject;

    MOCK_METHOD(void, loadInitialLedger, (std::uint32_t, bool), ());
    MOCK_METHOD(std::optional<FakeFetchResponse>, fetchLedger, (uint32_t, bool, bool), ());
    MOCK_METHOD(boost::json::value, toJson, (), (const));

    using ForwardToRippledReturnType = std::expected<boost::json::object, rpc::ClioError>;
    MOCK_METHOD(
        ForwardToRippledReturnType,
        forwardToRippled,
        (boost::json::object const&, std::optional<std::string> const&, bool, boost::asio::yield_context),
        (const)
    );
};
