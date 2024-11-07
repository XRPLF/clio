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

#include "migration/cassandra/FullTableScanner.hpp"
#include "util/LoggerFixtures.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>

namespace {

// Help to verify that the function is called
struct MockObject {
    MOCK_METHOD(void, call, (migration::cassandra::TokenRange const&, boost::asio::yield_context));
};

struct TestScannerAdaper {
    TestScannerAdaper(MockObject& obj) : objRef(obj) {};

    TestScannerAdaper(TestScannerAdaper const&) = default;
    TestScannerAdaper(TestScannerAdaper&&) = default;

    std::reference_wrapper<MockObject> objRef;

    void
    readByTokenRange(migration::cassandra::TokenRange const& range, boost::asio::yield_context yield) const
    {
        objRef.get().call(range, yield);
    }
};
}  // namespace

struct FullTableScannerTests : public NoLoggerFixture {};

TEST_F(FullTableScannerTests, workerNumZero)
{
    MockObject obj;
    EXPECT_DEATH(
        migration::cassandra::FullTableScanner<TestScannerAdaper>(1, 0, TestScannerAdaper(obj)),
        "workersNum for full table scanner must be greater than 0"
    );
}

TEST_F(FullTableScannerTests, SingleThreadCtx)
{
    MockObject obj;
    EXPECT_CALL(obj, call(testing::_, testing::_)).Times(100);
    auto scanner = migration::cassandra::FullTableScanner<TestScannerAdaper>(1, 1, TestScannerAdaper(obj));
    scanner.wait();
}

TEST_F(FullTableScannerTests, MultipleThreadCtx)
{
    MockObject obj;
    EXPECT_CALL(obj, call(testing::_, testing::_)).Times(200);
    auto scanner = migration::cassandra::FullTableScanner<TestScannerAdaper>(2, 2, TestScannerAdaper(obj));
    scanner.wait();
}
