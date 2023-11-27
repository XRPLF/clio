//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <ripple/basics/base_uint.h>
#include "data/Types.h"
#include "etl/impl/CacheLoader.h"
#include "util/Fixtures.h"
#include "util/MockBackend.h"
#include "util/MockCache.h"
#include "util/config/Config.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <gmock/gmock.h>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>
#include <vector>

namespace json = boost::json;
using namespace etl::detail;
using namespace util;
using namespace data;
using namespace testing;

constexpr static auto SEQ = 30;
constexpr static auto INDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

struct CacheLoaderTest : public MockBackendTest {
    void
    SetUp() override
    {
        MockBackendTest::SetUp();
        work.emplace(ctx);
        for (auto i = 0; i < 2; ++i)
            optThreads.emplace_back([&] { ctx.run(); });
    }

    void
    TearDown() override
    {
        work.reset();
        for (auto& optThread : optThreads) {
            if (optThread.joinable())
                optThread.join();
        }
        ctx.stop();
        MockBackendTest::TearDown();
    }

protected:
    MockCache cache;
    Config cfg{json::parse("{}")};
    std::optional<boost::asio::io_service::work> work;
    boost::asio::io_context ctx;
    std::vector<std::thread> optThreads;
};

namespace {

std::vector<LedgerObject>
getLatestDiff()
{
    return std::vector<LedgerObject>{
        {.key = ripple::uint256{"05E1EAC2574BE082B00B16F907CE32E6058DEB8F9E81CF34A00E80A5D71FA4FE"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"110872C7196EE6EF7032952F1852B11BB461A96FF2D7E06A8003B4BB30FD130B"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"3B3A84E850C724E914293271785A31D0BFC8B9DD1B6332E527B149AD72E80E18"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"4EC98C5C3F34C44409BC058998CBD64F6AED3FF6C0CAAEC15F7F42DF14EE9F04"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"58CEC9F17733EA7BA68C88E6179B8F207D001EE04D4E0366F958CC04FF6AB834"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"64FB1712146BA604C274CC335C5DE7ADFE52D1F8C3E904A9F9765FE8158A3E01"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"700BE23B1D9EE3E6BF52543D05843D5345B85D9EDB3D33BBD6B4C3A13C54B38E"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"82C297FCBCD634C4424F263D17480AA2F13975DF5846A5BB57246022CEEBE441"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"A2AA4C212DC2CA2C49BF58805F7C63363BC981018A01AC9609A7CBAB2A02CEDF"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"BC0DAE09C0BFBC4A49AA94B849266588BFD6E1F554B184B5788AC55D6E07EB95"}, .blob = Blob{'s'}},
        {.key = ripple::uint256{"DCC8759A35CB946511763AA5553A82AA25F20B901C98C9BB74D423BCFAFF5F9D"}, .blob = Blob{'s'}},
    };
}

};  // namespace

TEST_F(CacheLoaderTest, FromCache)
{
    MockBackend* rawBackendPtr = dynamic_cast<MockBackend*>(mockBackendPtr.get());
    CacheLoader loader{cfg, ctx, mockBackendPtr, cache};

    auto const diffs = getLatestDiff();
    ON_CALL(*rawBackendPtr, fetchLedgerDiff(_, _)).WillByDefault(Return(diffs));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerDiff(_, _)).Times(32);

    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;
    std::mutex keysMutex;

    std::map<std::thread::id, uint32_t> threadKeysMap;
    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(_, SEQ, _))
        .WillByDefault(Invoke([&]() -> std::optional<ripple::uint256> {
            // mock the result from doFetchSuccessorKey, be aware this function will be called from multiple threads
            // for each thread, the last 2 items must be end flag and nullopt, otherwise it will loop forever
            std::lock_guard<std::mutex> const guard(keysMutex);
            threadKeysMap[std::this_thread::get_id()]++;

            if (threadKeysMap[std::this_thread::get_id()] == keysSize - 1) {
                return lastKey;
            }
            if (threadKeysMap[std::this_thread::get_id()] == keysSize) {
                threadKeysMap[std::this_thread::get_id()] = 0;
                return std::nullopt;
            }
            return ripple::uint256{INDEX1};
        }));
    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(keysSize * loops);

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects(_, SEQ, _))
        .WillByDefault(Return(std::vector<Blob>{keysSize - 1, Blob{'s'}}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(loops);

    EXPECT_CALL(cache, updateImp).Times(loops);
    EXPECT_CALL(cache, isFull).Times(1);

    std::mutex m;
    std::condition_variable cv;
    bool cacheReady = false;
    ON_CALL(cache, setFull).WillByDefault(Invoke([&]() {
        {
            std::lock_guard const lk(m);
            cacheReady = true;
        }
        cv.notify_one();
    }));
    // cache is successfully loaded
    EXPECT_CALL(cache, setFull).Times(1);

    loader.load(SEQ);

    {
        std::unique_lock lk(m);
        cv.wait_for(lk, std::chrono::milliseconds(300), [&] { return cacheReady; });
    }
}
