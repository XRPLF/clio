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

#include "data/Types.hpp"
#include "etl/FakeDiffProvider.hpp"
#include "etl/impl/CursorFromFixDiffNumProvider.hpp"
#include "util/Fixtures.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>

using namespace etl;
using namespace util;
using namespace data;
using namespace testing;

namespace {

constexpr auto SEQ = 30;

struct CursorProviderTest : util::prometheus::WithPrometheus, MockBackendTestNaggy {
    DiffProvider diffProvider;
};
struct ParametrizedCursorProviderTest : CursorProviderTest, WithParamInterface<std::size_t> {};

INSTANTIATE_TEST_CASE_P(
    CursorProviderTest,
    ParametrizedCursorProviderTest,
    Values(32, 64, 128, 512, 1024, 3, 2, 1),
    [](auto const& info) {
        auto const diffs = info.param;
        return fmt::format("diffs_{}", diffs);
    }
);

};  // namespace

TEST_P(ParametrizedCursorProviderTest, GetCursorsWithDifferentProviderSettings)
{
    auto const numDiffs = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const provider = etl::impl::CursorFromFixDiffNumProvider{backend, numDiffs};

    ON_CALL(*backend, fetchLedgerDiff(_, _)).WillByDefault(Return(diffs));
    EXPECT_CALL(*backend, fetchLedgerDiff(_, _)).Times(numDiffs);

    auto const cursors = provider.getCursors(SEQ);
    ASSERT_EQ(cursors.size(), diffs.size() + 1);

    EXPECT_EQ(cursors.front().start, firstKey);
    EXPECT_EQ(cursors.back().end, lastKey);
}

TEST_F(CursorProviderTest, EmptyCursorIsHandledCorrectly)
{
    auto const diffs = diffProvider.getLatestDiff();
    auto const provider = etl::impl::CursorFromFixDiffNumProvider{backend, 0};

    ON_CALL(*backend, fetchLedgerDiff(_, _)).WillByDefault(Return(diffs));
    EXPECT_CALL(*backend, fetchLedgerDiff(_, _)).Times(0);

    auto const cursors = provider.getCursors(SEQ);

    ASSERT_EQ(cursors.size(), 1);
    EXPECT_EQ(cursors.front().start, firstKey);
    EXPECT_EQ(cursors.back().end, lastKey);
}
