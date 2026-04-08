#pragma once

#include "util/LoggerFixtures.hpp"
#include "util/MockMigrationBackend.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <gmock/gmock.h>

#include <memory>

template <template <typename> typename MockType = ::testing::NiceMock>
struct MockMigrationBackendTestBase : virtual public ::testing::Test {
    class BackendProxy {
        std::shared_ptr<MockType<MockMigrationBackend>> backend_ =
            std::make_shared<MockType<MockMigrationBackend>>(util::config::ClioConfigDefinition{});

    public:
        auto
        operator->()
        {
            return backend_.get();
        }

        operator std::shared_ptr<MockMigrationBackend>()
        {
            return backend_;
        }

        operator std::shared_ptr<MockMigrationBackend const>() const
        {
            return backend_;
        }

        MockType<MockMigrationBackend>&
        operator*()
        {
            return *backend_;
        }
    };

protected:
    BackendProxy backend_;
};

/**
 * @brief Fixture with a "nice" mock backend.
 *
 * Use @see MockBackendTestNaggy during development to get unset call expectation warnings.
 * Once the test is ready and you are happy you can switch to this fixture to mute the warnings.
 *
 * A fixture that is based off of this MockBackendTest or MockBackendTestNaggy get a `backend`
 * member that is a `BackendProxy` that can be used to access the mock backend. It can be used
 * wherever a `std::shared_ptr<BackendInterface>` is expected as well as `*backend` can be used with
 * EXPECT_CALL and ON_CALL.
 */
using MockMigrationBackendTest = MockMigrationBackendTestBase<::testing::NiceMock>;

/**
 * @brief Fixture with a "naggy" mock backend.
 *
 * Use this during development to get unset call expectation warnings.
 */
using MockMigrationBackendTestNaggy = MockMigrationBackendTestBase<::testing::NaggyMock>;

/**
 * @brief Fixture with a "strict" mock backend.
 */
using MockMigrationBackendTestStrict = MockMigrationBackendTestBase<::testing::StrictMock>;
