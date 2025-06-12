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

#include "data/Types.hpp"
#include "etlng/impl/AmendmentBlockHandler.hpp"
#include "etlng/impl/Monitor.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <semaphore>

using namespace etlng::impl;
using namespace data;

namespace {
constexpr auto kSTART_SEQ = 123u;
constexpr auto kNO_NEW_LEDGER_REPORT_DELAY = std::chrono::milliseconds(1u);
}  // namespace

struct MonitorTests : util::prometheus::WithPrometheus, MockBackendTest {
protected:
    util::async::CoroExecutionContext ctx_;
    StrictMockNetworkValidatedLedgersPtr ledgers_;
    testing::StrictMock<testing::MockFunction<void(uint32_t)>> actionMock_;
    testing::StrictMock<testing::MockFunction<void()>> dbStalledMock_;

    etlng::impl::Monitor monitor_ =
        etlng::impl::Monitor(ctx_, backend_, ledgers_, kSTART_SEQ, kNO_NEW_LEDGER_REPORT_DELAY);
};

TEST_F(MonitorTests, ConsumesAndNotifiesForAllOutstandingSequencesAtOnce)
{
    uint8_t count = 3;
    LedgerRange const range(kSTART_SEQ, kSTART_SEQ + count - 1);

    std::binary_semaphore unblock(0);

    EXPECT_CALL(*ledgers_, subscribe(testing::_));
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(range));
    EXPECT_CALL(actionMock_, Call).Times(count).WillRepeatedly([&] {
        if (--count == 0u)
            unblock.release();
    });

    auto subscription = monitor_.subscribeToNewSequence(actionMock_.AsStdFunction());
    monitor_.run(std::chrono::milliseconds{10});
    unblock.acquire();
}

TEST_F(MonitorTests, NotifiesForEachSequence)
{
    uint8_t count = 3;
    LedgerRange range(kSTART_SEQ, kSTART_SEQ);

    std::binary_semaphore unblock(0);

    EXPECT_CALL(*ledgers_, subscribe(testing::_));
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).Times(count).WillRepeatedly([&] {
        auto tmp = range;
        ++range.maxSequence;
        return tmp;
    });
    EXPECT_CALL(actionMock_, Call).Times(count).WillRepeatedly([&] {
        if (--count == 0u)
            unblock.release();
    });

    auto subscription = monitor_.subscribeToNewSequence(actionMock_.AsStdFunction());
    monitor_.run(std::chrono::milliseconds{1});
    unblock.acquire();
}

TEST_F(MonitorTests, NotifiesWhenForcedByNewSequenceAvailableFromNetwork)
{
    LedgerRange const range(kSTART_SEQ, kSTART_SEQ);
    std::binary_semaphore unblock(0);
    std::function<void(uint32_t)> pusher;

    EXPECT_CALL(*ledgers_, subscribe(testing::_)).WillOnce([&](auto&& subscriber) {
        pusher = subscriber;
        return boost::signals2::scoped_connection();  // to keep the compiler happy
    });
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(range));
    EXPECT_CALL(actionMock_, Call).WillOnce([&] { unblock.release(); });

    auto subscription = monitor_.subscribeToNewSequence(actionMock_.AsStdFunction());
    monitor_.run(std::chrono::seconds{10});  // expected to be force-invoked sooner than in 10 sec
    pusher(kSTART_SEQ);                      // pretend network validated a new ledger
    unblock.acquire();
}

TEST_F(MonitorTests, NotifiesWhenForcedByLedgerLoaded)
{
    LedgerRange const range(kSTART_SEQ, kSTART_SEQ);
    std::binary_semaphore unblock(0);

    EXPECT_CALL(*ledgers_, subscribe(testing::_));
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(range));
    EXPECT_CALL(actionMock_, Call).WillOnce([&] { unblock.release(); });

    auto subscription = monitor_.subscribeToNewSequence(actionMock_.AsStdFunction());
    monitor_.run(std::chrono::seconds{10});     // expected to be force-invoked sooner than in 10 sec
    monitor_.notifySequenceLoaded(kSTART_SEQ);  // notify about newly committed ledger
    unblock.acquire();
}

TEST_F(MonitorTests, ResumesMonitoringFromNextSequenceAfterWriteConflict)
{
    constexpr uint32_t kCONFLICT_SEQ = 456u;
    constexpr uint32_t kEXPECTED_NEXT_SEQ = kCONFLICT_SEQ + 1;

    LedgerRange const rangeBeforeConflict(kSTART_SEQ, kSTART_SEQ);
    LedgerRange const rangeAfterConflict(kEXPECTED_NEXT_SEQ, kEXPECTED_NEXT_SEQ);
    std::binary_semaphore unblock(0);

    EXPECT_CALL(*ledgers_, subscribe(testing::_));

    {
        testing::InSequence const seq;  // second call will produce conflict
        EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(rangeBeforeConflict));
        EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillRepeatedly(testing::Return(rangeAfterConflict));
    }

    EXPECT_CALL(actionMock_, Call(kEXPECTED_NEXT_SEQ)).WillOnce([&](uint32_t seq) {
        EXPECT_EQ(seq, kEXPECTED_NEXT_SEQ);
        unblock.release();
    });

    auto subscription = monitor_.subscribeToNewSequence(actionMock_.AsStdFunction());
    monitor_.run(std::chrono::nanoseconds{100});
    monitor_.notifyWriteConflict(kCONFLICT_SEQ);
    unblock.acquire();
}

TEST_F(MonitorTests, DbStalledChannelTriggeredWhenTimeoutExceeded)
{
    std::binary_semaphore unblock(0);

    EXPECT_CALL(*ledgers_, subscribe(testing::_));
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(dbStalledMock_, Call()).WillOnce([&]() { unblock.release(); });

    auto subscription = monitor_.subscribeToDbStalled(dbStalledMock_.AsStdFunction());
    monitor_.run(std::chrono::nanoseconds{100});
    unblock.acquire();
}
