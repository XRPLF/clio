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

#pragma once

#include "util/Assert.hpp"  // IWYU pragma: keep

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace common::util {
class WithMockAssert : virtual public testing::Test {
public:
    struct MockAssertException {
        std::string message;
    };

    WithMockAssert();
    ~WithMockAssert() override;

private:
    static void
    throwOnAssert(std::string_view m);
};

class WithMockAssertNoThrow : virtual public testing::Test {
public:
    ~WithMockAssertNoThrow() override;
};

namespace impl {
template <typename T>
struct MockGuard {
    T mock;
    ~MockGuard()
    {
        ::util::impl::OnAssert::resetAction();
    }
};
}  // namespace impl

}  // namespace common::util

#define EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(statement, message_regex)                          \
    if (dynamic_cast<common::util::WithMockAssert*>(this) != nullptr) {                         \
        EXPECT_THROW(                                                                           \
            {                                                                                   \
                try {                                                                           \
                    statement;                                                                  \
                } catch (common::util::WithMockAssert::MockAssertException const& e) {          \
                    EXPECT_THAT(e.message, testing::ContainsRegex(message_regex));              \
                    throw;                                                                      \
                }                                                                               \
            },                                                                                  \
            common::util::WithMockAssert::MockAssertException                                   \
        );                                                                                      \
    } else if (dynamic_cast<common::util::WithMockAssertNoThrow*>(this) != nullptr) {           \
        using MockGuardType = common::util::impl::MockGuard<                                    \
            testing::StrictMock<testing::MockFunction<void(std::string_view)>>>;                \
        auto mockGuard = std::make_shared<MockGuardType>();                                     \
        ::util::impl::OnAssert::setAction([mockGuard](std::string_view m) {                     \
            mockGuard->mock.Call(m);                                                            \
        });                                                                                     \
        EXPECT_CALL(mockGuard->mock, Call(testing::ContainsRegex(message_regex)));              \
        statement;                                                                              \
    } else {                                                                                    \
        std::cerr << "EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE() can be used only inside test body" \
                  << std::endl;                                                                 \
        std::terminate();                                                                       \
    }

#define EXPECT_CLIO_ASSERT_FAIL(statement) EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(statement, ".*")
