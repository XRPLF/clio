#include "data/Types.hpp"
#include "etl/FakeDiffProvider.hpp"
#include "etl/impl/CursorFromFixDiffNumProvider.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>

using namespace etl;
using namespace util;
using namespace data;
using namespace testing;

namespace {

constexpr auto kSEQ = 30;

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
    auto const provider = etl::impl::CursorFromFixDiffNumProvider{backend_, numDiffs};

    ON_CALL(*backend_, fetchLedgerDiff(_, _)).WillByDefault(Return(diffs));
    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(numDiffs);

    auto const cursors = provider.getCursors(kSEQ);
    ASSERT_EQ(cursors.size(), diffs.size() + 1);

    EXPECT_EQ(cursors.front().start, kFIRST_KEY);
    EXPECT_EQ(cursors.back().end, kLAST_KEY);
}

TEST_F(CursorProviderTest, EmptyCursorIsHandledCorrectly)
{
    auto const diffs = diffProvider.getLatestDiff();
    auto const provider = etl::impl::CursorFromFixDiffNumProvider{backend_, 0};

    ON_CALL(*backend_, fetchLedgerDiff(_, _)).WillByDefault(Return(diffs));
    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(0);

    auto const cursors = provider.getCursors(kSEQ);

    ASSERT_EQ(cursors.size(), 1);
    EXPECT_EQ(cursors.front().start, kFIRST_KEY);
    EXPECT_EQ(cursors.back().end, kLAST_KEY);
}
