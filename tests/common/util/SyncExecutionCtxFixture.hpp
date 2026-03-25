#pragma once

#include "util/LoggerFixtures.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/context/SyncExecutionContext.hpp"

#include <gmock/gmock.h>

/**
 * @brief Fixture with an embedded AnyExecutionContext wrapping a SyncExecutionContext
 *
 */
struct SyncExecutionCtxFixture : virtual public ::testing::Test {
protected:
    util::async::SyncExecutionContext syncCtx_;
    util::async::AnyExecutionContext ctx_{syncCtx_};
};
