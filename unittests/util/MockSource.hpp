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

#include "data/BackendInterface.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/Source.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/config/Config.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct MockSource : etl::SourceBase {
    MOCK_METHOD(void, run, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(void, setForwarding, (bool), (override));
    MOCK_METHOD(boost::json::object, toJson, (), (const, override));
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
};
template <template <typename> typename MockType>
struct MockSourceData {
    MockSourceData(
        etl::SourceBase::OnDisconnectHook onDisconnect,
        etl::SourceBase::OnConnectHook onConnect,
        etl::SourceBase::OnLedgerClosedHook onLedgerClosed
    )
        : onDisconnect(std::move(onDisconnect))
        , onConnect(std::move(onConnect))
        , onLedgerClosed(std::move(onLedgerClosed))
    {
    }

    MockType<MockSource> mockSource;
    etl::SourceBase::OnDisconnectHook onDisconnect;
    etl::SourceBase::OnConnectHook onConnect;
    etl::SourceBase::OnLedgerClosedHook onLedgerClosed;
};

template <template <typename> typename MockType>
using MockSourceDataPtr = std::shared_ptr<MockSourceData<MockType>>;

template <template <typename> typename MockType>
class MockSourceWrapper : etl::SourceBase {
    MockSourceDataPtr<MockType> mockData;

public:
    MockSourceWrapper(MockSourceDataPtr<MockType> mockData) : mockData(std::move(mockData))
    {
    }

    void
    run() override
    {
        mockData->run();
    }

    bool
    isConnected() const override
    {
        return mockData->isConnected();
    }

    void
    setForwarding(bool isForwarding) override
    {
        mockData->setForwarding(isForwarding);
    }

    boost::json::object
    toJson() const override
    {
        return mockData->toJson();
    }

    std::string
    toString() const override
    {
        return mockData->toString();
    }

    bool
    hasLedger(uint32_t sequence) const override
    {
        return mockData->hasLedger(sequence);
    }

    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t sequence, bool getObjects, bool getObjectNeighbors) override
    {
        return mockData->fetchLedger(sequence, getObjects, getObjectNeighbors);
    }

    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(uint32_t sequence, uint32_t maxLedger, bool getObjects) override
    {
        return mockData->loadInitialLedger(sequence, maxLedger, getObjects);
    }

    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        boost::asio::yield_context yield
    ) const override
    {
        return mockData->forwardToRippled(request, forwardToRippledClientIp, yield);
    }
};

template <template <typename> typename MockType>
struct MockSourceFactory {
    std::vector<MockSourceDataPtr<MockType>> mockData_;

    etl::SourcePtr
    makeSourceMock(
        util::Config const&,
        boost::asio::io_context&,
        std::shared_ptr<BackendInterface>,
        std::shared_ptr<feed::SubscriptionManagerInterface>,
        std::shared_ptr<etl::NetworkValidatedLedgersInterface>,
        etl::SourceBase::OnDisconnectHook onDisconnect,
        etl::SourceBase::OnConnectHook onConnect,
        etl::SourceBase::OnLedgerClosedHook onLedgerClosed
    )
    {
        auto mockSourceData = std::make_shared<MockSourceData<MockType>>(
            std::move(onDisconnect), std::move(onConnect), std::move(onLedgerClosed)
        );
        mockData_.push_back(mockSourceData);

        return std::make_unique<MockSourceWrapper<MockType>>(mockSourceData);
    }
};
