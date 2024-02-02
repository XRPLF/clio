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

#include "etl/Source.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class MockSource : public etl::Source {
public:
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(boost::json::object, toJson, (), (const, override));
    MOCK_METHOD(void, run, (), (override));
    MOCK_METHOD(void, pause, (), (override));
    MOCK_METHOD(void, resume, (), (override));
    MOCK_METHOD(std::string, toString, (), (const, override));
    MOCK_METHOD(bool, hasLedger, (uint32_t), (const, override));
    MOCK_METHOD(
        (std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>),
        fetchLedger,
        (uint32_t, bool, bool),
        (override)
    );
    MOCK_METHOD((std::pair<std::vector<std::string>, bool>), loadInitialLedger, (uint32_t, uint32_t, bool), (override));
    MOCK_METHOD(
        std::optional<boost::json::object>,
        forwardToRippled,
        (boost::json::object const&, std::optional<std::string> const&, boost::asio::yield_context),
        (const, override)
    );
    MOCK_METHOD(
        std::optional<boost::json::object>,
        requestFromRippled,
        (boost::json::object const&, std::optional<std::string> const&, boost::asio::yield_context),
        (const, override)
    );
    MOCK_METHOD(boost::uuids::uuid, token, (), (const, override));
};
