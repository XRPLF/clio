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

#include "app/VerifyConfig.hpp"
#include "util/TmpFile.hpp"
#include "util/newconfig/FakeConfigData.hpp"

#include <gtest/gtest.h>

using namespace app;
using namespace util::config;

TEST(VerifyConfigTest, config)
{
    auto const tmpConfigFile = TmpFile(kJSON_DATA);

    // false because json data(kJSON_DATA) is not compatible with current configDefintion
    EXPECT_FALSE(verifyConfig(tmpConfigFile.path));

    // current example config should always be compatible with configDefinition
    EXPECT_TRUE(verifyConfig("../docs/examples/config/example-config.json"));
}
