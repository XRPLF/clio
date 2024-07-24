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

#include "app/CliArgs.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <string_view>

using namespace app;

struct CliArgsTests : testing::Test {
    testing::StrictMock<testing::MockFunction<int(CliArgs::Action::Run)>> onRunMock;
    testing::StrictMock<testing::MockFunction<int(CliArgs::Action::Exit)>> onExitMock;
};

TEST_F(CliArgsTests, Parse_NoArgs)
{
    std::array argv{"clio_server"};
    auto const action = CliArgs::parse(argv.size(), argv.data());

    int const returnCode = 123;
    EXPECT_CALL(onRunMock, Call).WillOnce([](CliArgs::Action::Run const& run) {
        EXPECT_EQ(run.configPath, CliArgs::defaultConfigPath);
        return returnCode;
    });
    EXPECT_EQ(action.apply(onRunMock.AsStdFunction(), onExitMock.AsStdFunction()), returnCode);
}

TEST_F(CliArgsTests, Parse_VersionHelp)
{
    for (auto& argv :
         {std::array{"clio_server", "--version"},
          std::array{"clio_server", "-v"},
          std::array{"clio_server", "--help"},
          std::array{"clio_server", "-h"}}) {
        auto const action = CliArgs::parse(argv.size(), const_cast<char const**>(argv.data()));

        EXPECT_CALL(onExitMock, Call).WillOnce([](CliArgs::Action::Exit const& exit) { return exit.exitCode; });
        EXPECT_EQ(action.apply(onRunMock.AsStdFunction(), onExitMock.AsStdFunction()), EXIT_SUCCESS);
    }
}

TEST_F(CliArgsTests, Parse_Config)
{
    std::string_view configPath = "some_config_path";
    std::array argv{"clio_server", "--conf", configPath.data()};

    auto const action = CliArgs::parse(argv.size(), argv.data());

    int const returnCode = 123;
    EXPECT_CALL(onRunMock, Call).WillOnce([&configPath](CliArgs::Action::Run const& run) {
        EXPECT_EQ(run.configPath, configPath);
        return returnCode;
    });
    EXPECT_EQ(action.apply(onRunMock.AsStdFunction(), onExitMock.AsStdFunction()), returnCode);
}
