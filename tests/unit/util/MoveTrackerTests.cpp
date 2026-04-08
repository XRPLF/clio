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
    EXPECT_TRUE(moveMe.wasMoved());  // NOLINT(bugprone-use-after-move)
    EXPECT_FALSE(other.wasMoved());
}

TEST(MoveTrackerTests, SupportReuse)
{
    auto original = MoveMe();
    auto other = std::move(original);

    original = std::move(other);
    EXPECT_FALSE(original.wasMoved());
    EXPECT_TRUE(other.wasMoved());  // NOLINT(bugprone-use-after-move)
}

TEST(MoveTrackerTests, SelfMove)
{
    auto original = MoveMe();
    [&](MoveMe& from) {
        original = std::move(from);
    }(original);  // avoids the compiler catching self-move

    EXPECT_FALSE(original.wasMoved());
}

TEST(MoveTrackerTests, SelfMoveAfterWasMoved)
{
    auto original = MoveMe();
    [[maybe_unused]] auto fake = std::move(original);

    // NOLINTNEXTLINE(bugprone-use-after-move)
    [&](MoveMe& from) {
        original = std::move(from);
    }(original);  // avoids the compiler catching self-move

    EXPECT_TRUE(original.wasMoved());  // NOLINT(bugprone-use-after-move)
}
