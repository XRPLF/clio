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

#include "util/MockAssert.hpp"
#include "util/SourceLocation.hpp"
#include "util/log/PrettyPath.hpp"

#include <gtest/gtest.h>

#include <string_view>

using namespace util;

TEST(PrettyPath, CurrentFile)
{
    auto loc = CURRENT_SRC_LOCATION;
    auto pretty = prettyPath(loc.file_name());
    EXPECT_EQ(pretty, "util/log/PrettyPathTests.cpp");
}

struct PrettyPathDepth : public common::util::WithMockAssert {
    static constexpr std::string_view kTEST_PATH = "my/awesome/path/to/file.cpp";
};

TEST_F(PrettyPathDepth, Zero)
{
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(
        { [[maybe_unused]] auto unused = prettyPath(kTEST_PATH, 0); },
        "maxDepth must be greater than 0"
    );
}

TEST_F(PrettyPathDepth, Small)
{
    auto pretty = prettyPath(kTEST_PATH, 1);
    EXPECT_EQ(pretty, "file.cpp");
}

TEST_F(PrettyPathDepth, Big)
{
    auto pretty = prettyPath(kTEST_PATH, 4);
    EXPECT_EQ(pretty, "awesome/path/to/file.cpp");
}

TEST_F(PrettyPathDepth, MoreThanParts)
{
    auto pretty = prettyPath(kTEST_PATH, 10);
    EXPECT_EQ(pretty, "my/awesome/path/to/file.cpp");
}
