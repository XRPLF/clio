//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2026, the clio developers.

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

#include "etl/SystemState.hpp"
#include "util/MockPrometheus.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <memory>

using namespace etl;
using namespace util::config;

struct SystemStateTest : util::prometheus::WithPrometheus {};

TEST_F(SystemStateTest, InitialValuesAreCorrect)
{
    auto state = SystemState{};

    EXPECT_FALSE(state.isStrictReadonly);
    EXPECT_FALSE(state.isWriting);
    EXPECT_FALSE(state.etlStarted);
    EXPECT_FALSE(state.isAmendmentBlocked);
    EXPECT_FALSE(state.isCorruptionDetected);
    EXPECT_FALSE(state.isWriterDecidingFallback);
}

struct SystemStateReadOnlyTest : util::prometheus::WithPrometheus,
                                 testing::WithParamInterface<bool> {};

TEST_P(SystemStateReadOnlyTest, MakeSystemStateWithReadOnly)
{
    auto const readOnlyValue = GetParam();
    auto const configJson =
        boost::json::parse(fmt::format(R"JSON({{"read_only": {}}})JSON", readOnlyValue));

    auto config = ClioConfigDefinition{{{"read_only", ConfigValue{ConfigType::Boolean}}}};
    auto const configFile = ConfigFileJson{configJson.as_object()};
    auto const errors = config.parse(configFile);
    ASSERT_FALSE(errors.has_value());

    auto state = SystemState::makeSystemState(config);

    EXPECT_EQ(state->isStrictReadonly, readOnlyValue);
    EXPECT_FALSE(state->isWriting);
    EXPECT_FALSE(state->etlStarted);
    EXPECT_FALSE(state->isAmendmentBlocked);
    EXPECT_FALSE(state->isCorruptionDetected);
    EXPECT_FALSE(state->isWriterDecidingFallback);
}

INSTANTIATE_TEST_SUITE_P(SystemStateTest, SystemStateReadOnlyTest, testing::Values(true, false));
