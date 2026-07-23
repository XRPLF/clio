#include "util/MockAssert.hpp"
#include "util/log/PrettyPath.hpp"

#include <gtest/gtest.h>

#include <source_location>
#include <string_view>

using namespace util;

TEST(PrettyPath, CurrentFile)
{
    auto loc = std::source_location::current();
    auto pretty = prettyPath(loc.file_name());
    EXPECT_EQ(pretty, "util/log/PrettyPathTests.cpp");
}

struct PrettyPathDepth : public common::util::WithMockAssert {
    static constexpr std::string_view kTestPath = "my/awesome/path/to/file.cpp";
};

TEST_F(PrettyPathDepth, Zero)
{
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(
        { [[maybe_unused]] auto unused = prettyPath(kTestPath, 0); },
        "maxDepth must be greater than 0"
    );
}

TEST_F(PrettyPathDepth, Small)
{
    auto pretty = prettyPath(kTestPath, 1);
    EXPECT_EQ(pretty, "file.cpp");
}

TEST_F(PrettyPathDepth, Big)
{
    auto pretty = prettyPath(kTestPath, 4);
    EXPECT_EQ(pretty, "awesome/path/to/file.cpp");
}

TEST_F(PrettyPathDepth, MoreThanParts)
{
    auto pretty = prettyPath(kTestPath, 10);
    EXPECT_EQ(pretty, "my/awesome/path/to/file.cpp");
}
