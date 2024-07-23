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

#include <cstdint>
#include <stdexcept>
#include <variant>

using namespace util::config;

struct ValueViewTest : testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(ValueViewTest, ValueView)
{
    ConfigValue const cv = ConfigValue{ConfigType::String}.defaultValue("value");
    ValueView const vv = ValueView(cv);
    EXPECT_EQ("value", vv.asString());
    EXPECT_EQ(ConfigType::String, vv.type());
    EXPECT_EQ(true, vv.hasValue());
    EXPECT_EQ(false, vv.isOptional());
}

TEST_F(ValueViewTest, differentIntegerTest)
{
    auto const vv = configData.getValue("header.port");
    auto const uint32 = vv.asIntType<uint32_t>();
    auto const uint64 = vv.asIntType<uint64_t>();
    auto const int32 = vv.asIntType<int32_t>();
    auto const int64 = vv.asIntType<int64_t>();
    auto const doubleVal = vv.asIntType<double>();
    auto const floatVal = vv.asIntType<float>();

    EXPECT_EQ(vv.asIntType<int>(), uint32);
    EXPECT_EQ(vv.asIntType<int>(), uint64);
    EXPECT_EQ(vv.asIntType<int>(), int32);
    EXPECT_EQ(vv.asIntType<int>(), int64);
    EXPECT_EQ(vv.asIntType<int>(), doubleVal);
    EXPECT_EQ(vv.asIntType<int>(), floatVal);
}

TEST_F(ValueViewTest, wrongTypes)
{
    auto const vv = configData.getValue("header.port");

    EXPECT_THROW({ [[maybe_unused]] auto a_ = vv.asBool(); }, std::bad_variant_access);
    EXPECT_THROW({ [[maybe_unused]] auto a_ = vv.asString(); }, std::bad_variant_access);
    EXPECT_THROW({ [[maybe_unused]] auto a_ = vv.asDouble(); }, std::bad_variant_access);

    ConfigValue const cv = ConfigValue{ConfigType::Integer}.defaultValue(-5);
    auto const vv2 = ValueView(cv);
    EXPECT_THROW({ [[maybe_unused]] auto a_ = vv2.asIntType<uint32_t>(); }, std::logic_error);
}
