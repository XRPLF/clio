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
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace rpc;

struct ProductionHandlerProviderTest : util::prometheus::WithPrometheus, MockBackendTestStrict {
    util::config::ClioConfigDefinition config{
        {"api_version.default",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(
             rpc::kAPI_VERSION_DEFAULT
         )},
        {"api_version.min",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(
             rpc::kAPI_VERSION_MIN
         )},
        {"api_version.max",
         util::config::ConfigValue{util::config::ConfigType::Integer}.defaultValue(
             rpc::kAPI_VERSION_MAX
         )},
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
