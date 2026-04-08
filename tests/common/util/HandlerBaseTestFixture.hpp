#pragma once

#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockETLServiceTestFixture.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>

/**
 * @brief Fixture with an mock backend and an embedded boost::asio context.
 *
 * Use as a handler unittest base fixture thru either @see HandlerBaseTest, @see
 * HandlerBaseTestNaggy or @see HandlerBaseTestStrict.
 */
template <template <typename> typename MockType = ::testing::NiceMock>
struct HandlerBaseTestBase : util::prometheus::WithPrometheus,
                             MockBackendTestBase<MockType>,
                             SyncAsioContextTest,
                             MockETLServiceTestBase<MockType> {};

/**
 * @brief Fixture with a "nice" backend mock and an embedded boost::asio context.
 *
 * Use @see HandlerBaseTest during development to get unset call expectation warnings from the
 * backend mock. Once the test is ready and you are happy you can switch to this fixture to mute the
 * warnings.
 *
 * @see BackendBaseTest for more details on the injected backend mock.
 */
using HandlerBaseTest = HandlerBaseTestBase<::testing::NiceMock>;

/**
 * @brief Fixture with a "naggy" backend mock and an embedded boost::asio context.
 *
 * Use this during development to get unset call expectation warnings from the backend mock.
 */
using HandlerBaseTestNaggy = HandlerBaseTestBase<::testing::NaggyMock>;

/**
 * @brief Fixture with a "strict" backend mock and an embedded boost::asio context.
 */
using HandlerBaseTestStrict = HandlerBaseTestBase<::testing::StrictMock>;
