#include "util/AsioContextTestFixture.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/dosguard/DOSGuardMock.hpp"
#include "web/dosguard/IntervalSweepHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

using namespace web::dosguard;
using namespace util::config;

struct IntervalSweepHandlerTest : SyncAsioContextTest {
protected:
    static constexpr auto kJSON_DATA = R"JSON(
        {
            "dos_guard": {
                "sweep_interval": 0
            }
        }
    )JSON";

    DOSGuardStrictMock guardMock_;

    ClioConfigDefinition cfg_{
        {"dos_guard.sweep_interval", ConfigValue{ConfigType::Integer}.defaultValue(0)}
    };
    IntervalSweepHandler sweepHandler_{cfg_, ctx_, guardMock_};
};

TEST_F(IntervalSweepHandlerTest, SweepAfterInterval)
{
    EXPECT_CALL(guardMock_, clear()).Times(testing::AtLeast(10));
    runContextFor(std::chrono::milliseconds{20});
}
