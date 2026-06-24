#include "data/Types.hpp"
#include "etl/impl/CursorFromDiffProvider.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <vector>

using namespace etl;
using namespace util;
using namespace data;
using namespace testing;

namespace {

constexpr auto kSeq = 30;

std::vector<data::LedgerObject> const kDiffsForSeq = {
    {.key = xrpl::uint256{"05E1EAC2574BE082B00B16F907CE32E6058DEB8F9E81CF34A00E80A5D71FA4FE"},
     .blob = Blob{}},  // This object is removed in Seq while it exists in Seq-1
    {.key = xrpl::uint256{"110872C7196EE6EF7032952F1852B11BB461A96FF2D7E06A8003B4BB30FD130B"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"3B3A84E850C724E914293271785A31D0BFC8B9DD1B6332E527B149AD72E80E18"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"4EC98C5C3F34C44409BC058998CBD64F6AED3FF6C0CAAEC15F7F42DF14EE9F04"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"58CEC9F17733EA7BA68C88E6179B8F207D001EE04D4E0366F958CC04FF6AB834"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"64FB1712146BA604C274CC335C5DE7ADFE52D1F8C3E904A9F9765FE8158A3E01"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"700BE23B1D9EE3E6BF52543D05843D5345B85D9EDB3D33BBD6B4C3A13C54B38E"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"82C297FCBCD634C4424F263D17480AA2F13975DF5846A5BB57246022CEEBE441"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"A2AA4C212DC2CA2C49BF58805F7C63363BC981018A01AC9609A7CBAB2A02CEDF"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"BC0DAE09C0BFBC4A49AA94B849266588BFD6E1F554B184B5788AC55D6E07EB95"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"DCC8759A35CB946511763AA5553A82AA25F20B901C98C9BB74D423BCFAFF5F9D"},
     .blob = Blob{'s'}},
};

std::vector<data::LedgerObject> const kDiffsForSeqMinuS1 = {
    {.key = xrpl::uint256{"05E1EAC2574BE082B00B16F907CE32E6058DEB8F9E81CF34A00E80A5D71FA4FE"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"110872C7196EE6EF7032952F1852B11BB461A96FF2D7E06A8003B4BB30FD1301"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"3B3A84E850C724E914293271785A31D0BFC8B9DD1B6332E527B149AD72E80E12"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"4EC98C5C3F34C44409BC058998CBD64F6AED3FF6C0CAAEC15F7F42DF14EE9F03"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"58CEC9F17733EA7BA68C88E6179B8F207D001EE04D4E0366F958CC04FF6AB834"},
     .blob = Blob{'s'}},  // This object is changed in both Seq and Seq-1
    {.key = xrpl::uint256{"64FB1712146BA604C274CC335C5DE7ADFE52D1F8C3E904A9F9765FE8158A3E05"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"700BE23B1D9EE3E6BF52543D05843D5345B85D9EDB3D33BBD6B4C3A13C54B386"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"82C297FCBCD634C4424F263D17480AA2F13975DF5846A5BB57246022CEEBE447"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"A2AA4C212DC2CA2C49BF58805F7C63363BC981018A01AC9609A7CBAB2A02CED8"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"BC0DAE09C0BFBC4A49AA94B849266588BFD6E1F554B184B5788AC55D6E07EB99"},
     .blob = Blob{'s'}},
    {.key = xrpl::uint256{"DCC8759A35CB946511763AA5553A82AA25F20B901C98C9BB74D423BCFAFF5F90"},
     .blob = Blob{'s'}},
};

struct CursorFromDiffProviderTests : util::prometheus::WithPrometheus, MockBackendTestNaggy {};
}  // namespace

TEST_F(CursorFromDiffProviderTests, MultipleDiffs)
{
    auto const numCursors = 15;
    auto const provider = etl::impl::CursorFromDiffProvider{backend_, numCursors};

    backend_->setRange(kSeq - 10, kSeq);
    ON_CALL(*backend_, fetchLedgerDiff(kSeq, _)).WillByDefault(Return(kDiffsForSeq));
    ON_CALL(*backend_, fetchLedgerDiff(kSeq - 1, _)).WillByDefault(Return(kDiffsForSeqMinuS1));
    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(2);

    auto const cursors = provider.getCursors(kSeq);
    ASSERT_EQ(cursors.size(), numCursors + 1);

    EXPECT_EQ(cursors.front().start, kFirstKey);
    EXPECT_EQ(cursors.back().end, kLastKey);
}

TEST_F(CursorFromDiffProviderTests, NotEnoughDiffs)
{
    auto const numCursors = 35;
    auto const provider = etl::impl::CursorFromDiffProvider{backend_, numCursors};
    auto const availableDiffs = 10;
    backend_->setRange(kSeq - availableDiffs + 1, kSeq);
    ON_CALL(*backend_, fetchLedgerDiff(_, _))
        .WillByDefault(Return(std::vector<data::LedgerObject>{}));
    ON_CALL(*backend_, fetchLedgerDiff(kSeq, _)).WillByDefault(Return(kDiffsForSeq));
    ON_CALL(*backend_, fetchLedgerDiff(kSeq - 1, _)).WillByDefault(Return(kDiffsForSeqMinuS1));
    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(availableDiffs);

    auto const cursors = provider.getCursors(kSeq);
    auto const removed = 2;   // lost 2 objects because it is removed.
    auto const repeated = 1;  // repeated 1 object
    ASSERT_EQ(
        cursors.size(), kDiffsForSeq.size() + kDiffsForSeqMinuS1.size() - removed - repeated + 1
    );

    EXPECT_EQ(cursors.front().start, kFirstKey);
    EXPECT_EQ(cursors.back().end, kLastKey);
}
