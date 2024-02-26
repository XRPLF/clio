//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "etl/Source.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value_to.hpp>
#include <gmock/gmock.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace etl;

using testing::Return;
using testing::StrictMock;

struct GrpcSourceMock {
    using FetchLedgerReturnType = std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>;
    MOCK_METHOD(FetchLedgerReturnType, fetchLedger, (uint32_t, bool, bool));

    using LoadLedgerReturnType = std::pair<std::vector<std::string>, bool>;
    MOCK_METHOD(LoadLedgerReturnType, loadInitialLedger, (uint32_t, uint32_t, bool));
};

struct SubscriptionSourceMock {
    MOCK_METHOD(void, run, ());
    MOCK_METHOD(bool, hasLedger, (uint32_t), (const));
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(void, setForwarding, (bool));
    MOCK_METHOD(std::chrono::steady_clock::time_point, lastMessageTime, (), (const));
    MOCK_METHOD(std::string, validatedRange, (), (const));
    MOCK_METHOD(void, stop, ());
};

struct ForwardingSourceMock {
    MOCK_METHOD(void, constructor, (std::string const&, std::string const&, std::chrono::steady_clock::duration));

    using ForwardToRippledReturnType = std::optional<boost::json::object>;
    using ClientIpOpt = std::optional<std::string>;
    MOCK_METHOD(
        ForwardToRippledReturnType,
        forwardToRippled,
        (boost::json::object const&, ClientIpOpt const&, boost::asio::yield_context)
    );
};

struct SourceTest : public ::testing::Test {
    boost::asio::io_context ioc_;

    StrictMock<GrpcSourceMock> grpcSourceMock_;
    std::shared_ptr<StrictMock<SubscriptionSourceMock>> subscriptionSourceMock_ =
        std::make_shared<StrictMock<SubscriptionSourceMock>>();
    std::shared_ptr<StrictMock<ForwardingSourceMock>> forwardingSourceMock_ =
        std::make_shared<StrictMock<ForwardingSourceMock>>();

    SourceImpl<
        StrictMock<GrpcSourceMock>&,
        std::shared_ptr<StrictMock<SubscriptionSourceMock>>,
        std::shared_ptr<StrictMock<ForwardingSourceMock>>>
        source_{
            "some_ip",
            "some_ws_port",
            "some_grpc_port",
            grpcSourceMock_,
            subscriptionSourceMock_,
            forwardingSourceMock_
        };
};

TEST_F(SourceTest, run)
{
    EXPECT_CALL(*subscriptionSourceMock_, run());
    source_.run();
}

TEST_F(SourceTest, isConnected)
{
    EXPECT_CALL(*subscriptionSourceMock_, isConnected()).WillOnce(testing::Return(true));
    EXPECT_TRUE(source_.isConnected());
}

TEST_F(SourceTest, setForwarding)
{
    EXPECT_CALL(*subscriptionSourceMock_, setForwarding(true));
    source_.setForwarding(true);
}

TEST_F(SourceTest, toJson)
{
    EXPECT_CALL(*subscriptionSourceMock_, validatedRange()).WillOnce(Return(std::string("some_validated_range")));
    EXPECT_CALL(*subscriptionSourceMock_, isConnected()).WillOnce(Return(true));
    auto const lastMessageTime = std::chrono::steady_clock::now();
    EXPECT_CALL(*subscriptionSourceMock_, lastMessageTime()).WillOnce(Return(lastMessageTime));

    auto const json = source_.toJson();

    EXPECT_EQ(boost::json::value_to<std::string>(json.at("validated_range")), "some_validated_range");
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("is_connected")), "1");
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("ip")), "some_ip");
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("ws_port")), "some_ws_port");
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("grpc_port")), "some_grpc_port");
    auto lastMessageAgeStr = boost::json::value_to<std::string>(json.at("last_msg_age_seconds"));
    EXPECT_GE(std::stoi(lastMessageAgeStr), 0);
}

TEST_F(SourceTest, toString)
{
    EXPECT_CALL(*subscriptionSourceMock_, validatedRange()).WillOnce(Return(std::string("some_validated_range")));

    auto const str = source_.toString();
    EXPECT_EQ(
        str,
        "{validated range: some_validated_range, ip: some_ip, web socket port: some_ws_port, grpc port: some_grpc_port}"
    );
}

TEST_F(SourceTest, hasLedger)
{
    uint32_t const ledgerSeq = 123;
    EXPECT_CALL(*subscriptionSourceMock_, hasLedger(ledgerSeq)).WillOnce(Return(true));
    EXPECT_TRUE(source_.hasLedger(ledgerSeq));
}

TEST_F(SourceTest, fetchLedger)
{
    uint32_t const ledgerSeq = 123;

    EXPECT_CALL(grpcSourceMock_, fetchLedger(ledgerSeq, true, false));
    auto const [actualStatus, actualResponse] = source_.fetchLedger(ledgerSeq);

    EXPECT_EQ(actualStatus.error_code(), grpc::StatusCode::OK);
}

TEST_F(SourceTest, loadInitialLedger)
{
    uint32_t const ledgerSeq = 123;
    uint32_t const numMarkers = 3;

    EXPECT_CALL(grpcSourceMock_, loadInitialLedger(ledgerSeq, numMarkers, false))
        .WillOnce(Return(std::make_pair(std::vector<std::string>{}, true)));
    auto const [actualLedgers, actualSuccess] = source_.loadInitialLedger(ledgerSeq, numMarkers);

    EXPECT_TRUE(actualLedgers.empty());
    EXPECT_TRUE(actualSuccess);
}

TEST_F(SourceTest, forwardToRippled)
{
    boost::json::object const request = {{"some_key", "some_value"}};
    std::optional<std::string> const clientIp = "some_client_ip";

    EXPECT_CALL(*forwardingSourceMock_, forwardToRippled(request, clientIp, testing::_)).WillOnce(Return(request));

    boost::asio::io_context ioContext;
    boost::asio::spawn(ioContext, [&](boost::asio::yield_context yield) {
        auto const response = source_.forwardToRippled(request, clientIp, yield);
        EXPECT_EQ(response, request);
    });
    ioContext.run();
}
