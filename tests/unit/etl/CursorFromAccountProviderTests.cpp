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
#include "etl/impl/CursorFromAccountProvider.hpp"
#include "util/Fixtures.hpp"
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

constexpr auto SEQ = 30;

std::vector<ripple::uint256> const ACCOUNTROOTS = {
    ripple::uint256{"05E1EAC2574BE082B00B16F907CE32E6058DEB8F9E81CF34A00E80A5D71FA4FE"},
    ripple::uint256{"110872C7196EE6EF7032952F1852B11BB461A96FF2D7E06A8003B4BB30FD130B"},
    ripple::uint256{"3B3A84E850C724E914293271785A31D0BFC8B9DD1B6332E527B149AD72E80E18"},
    ripple::uint256{"4EC98C5C3F34C44409BC058998CBD64F6AED3FF6C0CAAEC15F7F42DF14EE9F04"},
    ripple::uint256{"58CEC9F17733EA7BA68C88E6179B8F207D001EE04D4E0366F958CC04FF6AB834"},
    ripple::uint256{"64FB1712146BA604C274CC335C5DE7ADFE52D1F8C3E904A9F9765FE8158A3E01"},
    ripple::uint256{"700BE23B1D9EE3E6BF52543D05843D5345B85D9EDB3D33BBD6B4C3A13C54B38E"},
    ripple::uint256{"82C297FCBCD634C4424F263D17480AA2F13975DF5846A5BB57246022CEEBE441"},
    ripple::uint256{"A2AA4C212DC2CA2C49BF58805F7C63363BC981018A01AC9609A7CBAB2A02CEDF"},
};

struct CursorFromAccountProviderTests : util::prometheus::WithPrometheus, MockBackendTestNaggy {};
}  // namespace

TEST_F(CursorFromAccountProviderTests, EnoughAccountRoots)
{
    auto const numCursors = 9;
    auto const pageSize = 100;
    auto const provider = etl::impl::CursorFromAccountProvider{backend, numCursors, pageSize};

    ON_CALL(*backend, fetchAccountRoots(numCursors, _, SEQ, _)).WillByDefault(Return(ACCOUNTROOTS));
    EXPECT_CALL(*backend, fetchAccountRoots(_, _, _, _)).Times(1);

    auto const cursors = provider.getCursors(SEQ);
    ASSERT_EQ(cursors.size(), numCursors + 1);

    EXPECT_EQ(cursors.front().start, firstKey);
    EXPECT_EQ(cursors.back().end, lastKey);
}
