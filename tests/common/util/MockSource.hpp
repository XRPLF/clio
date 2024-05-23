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

#include <algorithm>
#include <cstddef>
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
        (boost::json::object const&, std::optional<std::string> const&, bool, boost::asio::yield_context),
        (const, override)
    );
};

template <template <typename> typename MockType>
using MockSourcePtr = std::shared_ptr<MockType<MockSource>>;

template <template <typename> typename MockType>
class MockSourceWrapper : public etl::SourceBase {
    MockSourcePtr<MockType> mock_;

public:
    MockSourceWrapper(MockSourcePtr<MockType> mockData) : mock_(std::move(mockData))
    {
    }

    void
    run() override
    {
        mock_->run();
    }

    bool
    isConnected() const override
    {
        return mock_->isConnected();
    }

    void
    setForwarding(bool isForwarding) override
    {
        mock_->setForwarding(isForwarding);
    }

    boost::json::object
    toJson() const override
    {
        return mock_->toJson();
    }

    std::string
    toString() const override
    {
        return mock_->toString();
    }

    bool
    hasLedger(uint32_t sequence) const override
    {
        return mock_->hasLedger(sequence);
    }

    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t sequence, bool getObjects, bool getObjectNeighbors) override
    {
        return mock_->fetchLedger(sequence, getObjects, getObjectNeighbors);
    }

    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(uint32_t sequence, uint32_t maxLedger, bool getObjects) override
    {
        return mock_->loadInitialLedger(sequence, maxLedger, getObjects);
    }

    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        bool isAdmin,
        boost::asio::yield_context yield
    ) const override
    {
        return mock_->forwardToRippled(request, forwardToRippledClientIp, isAdmin, yield);
    }
};

struct MockSourceCallbacks {
    etl::SourceBase::OnDisconnectHook onDisconnect;
    etl::SourceBase::OnConnectHook onConnect;
    etl::SourceBase::OnLedgerClosedHook onLedgerClosed;
};

template <template <typename> typename MockType>
struct MockSourceData {
    MockSourcePtr<MockType> source = std::make_shared<MockType<MockSource>>();
    std::optional<MockSourceCallbacks> callbacks;
};

template <template <typename> typename MockType = testing::NiceMock>
class MockSourceFactoryImpl {
    std::vector<MockSourceData<MockType>> mockData_;

public:
    MockSourceFactoryImpl(size_t numSources)
    {
        mockData_.reserve(numSources);
        std::ranges::generate_n(std::back_inserter(mockData_), numSources, [] { return MockSourceData<MockType>{}; });
    }

    etl::SourcePtr
    makeSourceMock(
        util::Config const&,
        boost::asio::io_context&,
        std::shared_ptr<BackendInterface>,
        std::shared_ptr<feed::SubscriptionManagerInterface>,
        std::shared_ptr<etl::NetworkValidatedLedgersInterface>,
        etl::SourceBase::OnConnectHook onConnect,
        etl::SourceBase::OnDisconnectHook onDisconnect,
        etl::SourceBase::OnLedgerClosedHook onLedgerClosed
    )
    {
        auto it = std::ranges::find_if(mockData_, [](auto const& d) { return not d.callbacks.has_value(); });
        [&]() { ASSERT_NE(it, mockData_.end()) << "Make source called more than expected"; }();
        it->callbacks = MockSourceCallbacks{std::move(onDisconnect), std::move(onConnect), std::move(onLedgerClosed)};

        return std::make_unique<MockSourceWrapper<MockType>>(it->source);
    }

    MockType<MockSource>&
    sourceAt(size_t index)
    {
        return *mockData_.at(index).source;
    }

    MockSourceCallbacks&
    callbacksAt(size_t index)
    {
        auto& callbacks = mockData_.at(index).callbacks;
        [&]() { ASSERT_TRUE(callbacks.has_value()) << "Callbacks not set"; }();
        return *callbacks;
    }
};

using MockSourceFactory = MockSourceFactoryImpl<>;
using StrictMockSourceFactory = MockSourceFactoryImpl<testing::StrictMock>;
