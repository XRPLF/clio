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

#pragma once

#include "util/LoggerFixtures.hpp"
#include "util/MockMigrationBackend.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <gmock/gmock.h>

#include <memory>

template <template <typename> typename MockType = ::testing::NiceMock>
struct MockMigrationBackendTestBase : virtual public NoLoggerFixture {
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
 * A fixture that is based off of this MockBackendTest or MockBackendTestNaggy get a `backend` member
 * that is a `BackendProxy` that can be used to access the mock backend. It can be used wherever a
 * `std::shared_ptr<BackendInterface>` is expected as well as `*backend` can be used with EXPECT_CALL and ON_CALL.
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
