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

#include "util/Fixtures.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#include "etl/impl/ExtractionDataPipe.h"

#include <gtest/gtest.h>

constexpr static auto STRIDE = 4;
constexpr static auto START_SEQ = 1234;

class ETLExtractionDataPipeTest : public NoLoggerFixture {
protected:
    etl::detail::ExtractionDataPipe<uint32_t> pipe_{STRIDE, START_SEQ};
};

TEST_F(ETLExtractionDataPipeTest, StrideMatchesInput)
{
    EXPECT_EQ(pipe_.getStride(), STRIDE);
}

TEST_F(ETLExtractionDataPipeTest, PushedDataCanBeRetrievedAndMatchesOriginal)
{
    for (std::size_t i = 0; i < 8; ++i)
        pipe_.push(START_SEQ + i, START_SEQ + i);

    for (std::size_t i = 0; i < 8; ++i) {
        auto const data = pipe_.popNext(START_SEQ + i);
        EXPECT_EQ(data.value(), START_SEQ + i);
    }
}

TEST_F(ETLExtractionDataPipeTest, CallingFinishPushesAnEmptyOptional)
{
    for (std::size_t i = 0; i < 4; ++i)
        pipe_.finish(START_SEQ + i);

    for (std::size_t i = 0; i < 4; ++i) {
        auto const data = pipe_.popNext(START_SEQ + i);
        EXPECT_FALSE(data.has_value());
    }
}

TEST_F(ETLExtractionDataPipeTest, CallingCleanupUnblocksOtherThread)
{
    std::atomic_bool unblocked = false;
    auto bgThread = std::thread([this, &unblocked] {
        for (std::size_t i = 0; i < 252; ++i)
            pipe_.push(START_SEQ, 1234);  // 251st element will block this thread here
        unblocked = true;
    });

    // emulate waiting for above thread to push and get blocked
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    EXPECT_FALSE(unblocked);
    pipe_.cleanup();

    bgThread.join();
    EXPECT_TRUE(unblocked);
}
