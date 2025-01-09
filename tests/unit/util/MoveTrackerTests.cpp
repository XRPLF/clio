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

#include "util/MoveTracker.hpp"

#include <gtest/gtest.h>

#include <utility>

namespace {
struct MoveMe : util::MoveTracker {
    using MoveTracker::wasMoved;  // expose as public for tests
};
}  // namespace

TEST(MoveTrackerTests, SimpleChecks)
{
    auto moveMe = MoveMe();
    EXPECT_FALSE(moveMe.wasMoved());

    auto other = std::move(moveMe);
    EXPECT_TRUE(moveMe.wasMoved());
    EXPECT_FALSE(other.wasMoved());
}

TEST(MoveTrackerTests, SupportReuse)
{
    auto original = MoveMe();
    auto other = std::move(original);

    original = std::move(other);
    EXPECT_FALSE(original.wasMoved());
    EXPECT_TRUE(other.wasMoved());
}

TEST(MoveTrackerTests, SelfMove)
{
    auto original = MoveMe();
    [&](MoveMe& from) { original = std::move(from); }(original);  // avoids the compiler catching self-move

    EXPECT_FALSE(original.wasMoved());
}

TEST(MoveTrackerTests, SelfMoveAfterWasMoved)
{
    auto original = MoveMe();
    [[maybe_unused]] auto fake = std::move(original);

    [&](MoveMe& from) { original = std::move(from); }(original);  // avoids the compiler catching self-move

    EXPECT_TRUE(original.wasMoved());
}
