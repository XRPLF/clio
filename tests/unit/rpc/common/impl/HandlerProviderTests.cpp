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

#include "rpc/Counters.hpp"
#include "rpc/RPCCenter.hpp"
#include "rpc/WorkQueue.hpp"
#include "rpc/common/APIVersion.hpp"
#include "rpc/common/impl/HandlerProvider.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockETLService.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace rpc;

struct ProductionHandlerProviderTest : util::prometheus::WithPrometheus, MockBackendTestStrict {
    util::config::ClioConfigDefinition config{
        {"api_version.default",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(rpc::kAPI_VERSION_DEFAULT)},
        {"api_version.min",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(rpc::kAPI_VERSION_MIN)},
        {"api_version.max",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(rpc::kAPI_VERSION_MAX)},
    };
    StrictMockSubscriptionManagerSharedPtr subscriptionManagerMock;
    std::shared_ptr<testing::StrictMock<MockLoadBalancer>> loadBalancerMock;
    std::shared_ptr<testing::StrictMock<MockETLService>> etlServiceMock;
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr;
    WorkQueue workQueue{1};
    Counters counters{workQueue};

    impl::ProductionHandlerProvider handlerProvider{
        config,
        backend_,
        subscriptionManagerMock,
        loadBalancerMock,
        etlServiceMock,
        mockAmendmentCenterPtr,
        counters
    };
};

TEST_F(ProductionHandlerProviderTest, HandlersListIsComplete)
{
    auto const handlerNames = handlerProvider.handlerNames();
    for (auto const& name : handlerNames)
        EXPECT_TRUE(RPCCenter::isHandled(name));
}
