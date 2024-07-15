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

#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ValueView.hpp"

#include <../common/newconfig/FakeConfigData.hpp>
#include <gtest/gtest.h>

#include <variant>

using namespace util::config;

struct ValueViewTest : testing::Test {
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(ValueViewTest, ValueView)
{
    ConfigValue cv = ConfigValue{ConfigType::String}.defaultValue("value");
    ValueView vv = ValueView(cv);
    ASSERT_EQ("value", vv.asString());
    ASSERT_EQ(ConfigType::String, cv.type());

    auto v = configData.getValue("header.port");
    ASSERT_EQ(v.type(), ConfigType::Integer);

    ASSERT_EQ("value", configData.getValue("header.text1").asString());
    ASSERT_EQ(123, configData.getValue("header.port").asInt());
    ASSERT_EQ(true, configData.getValue("header.admin").asBool());
    ASSERT_EQ("TSM", configData.getValue("header.sub.sub2Value").asString());
    ASSERT_EQ(444.22, configData.getValue("ip").asDouble());
}

TEST_F(ValueViewTest, wrongTypes)
{
    auto cv = configData.getValue("header.port");
    ValueView vv = ValueView(cv);
    ASSERT_THROW(vv.asBool(), std::bad_variant_access);
    ASSERT_THROW(vv.asString(), std::bad_variant_access);
    ASSERT_THROW(vv.asDouble(), std::bad_variant_access);
}
