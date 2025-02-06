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
#include "util/TmpFile.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigDescription.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

using namespace app;

struct CliArgsTests : testing::Test {
    testing::StrictMock<testing::MockFunction<int(CliArgs::Action::Run)>> onRunMock;
    testing::StrictMock<testing::MockFunction<int(CliArgs::Action::Exit)>> onExitMock;
    testing::StrictMock<testing::MockFunction<int(CliArgs::Action::Migrate)>> onMigrateMock;
    testing::StrictMock<testing::MockFunction<int(CliArgs::Action::VerifyConfig)>> onVerifyMock;
};

TEST_F(CliArgsTests, Parse_NoArgs)
{
    std::array argv{"clio_server"};
    auto const action = CliArgs::parse(argv.size(), argv.data());

    int const returnCode = 123;
    EXPECT_CALL(onRunMock, Call).WillOnce([](CliArgs::Action::Run const& run) {
        EXPECT_EQ(run.configPath, CliArgs::kDEFAULT_CONFIG_PATH);
        EXPECT_FALSE(run.useNgWebServer);
        return returnCode;
    });
    EXPECT_EQ(
        action.apply(
            onRunMock.AsStdFunction(),
            onExitMock.AsStdFunction(),
            onMigrateMock.AsStdFunction(),
            onVerifyMock.AsStdFunction()
        ),
        returnCode
    );
}

TEST_F(CliArgsTests, Parse_NgWebServer)
{
    for (auto& argv : {std::array{"clio_server", "-w"}, std::array{"clio_server", "--ng-web-server"}}) {
        auto const action = CliArgs::parse(argv.size(), const_cast<char const**>(argv.data()));

        int const returnCode = 123;
        EXPECT_CALL(onRunMock, Call).WillOnce([](CliArgs::Action::Run const& run) {
            EXPECT_EQ(run.configPath, CliArgs::kDEFAULT_CONFIG_PATH);
            EXPECT_TRUE(run.useNgWebServer);
            return returnCode;
        });
        EXPECT_EQ(
            action.apply(
                onRunMock.AsStdFunction(),
                onExitMock.AsStdFunction(),
                onMigrateMock.AsStdFunction(),
                onVerifyMock.AsStdFunction()
            ),
            returnCode
        );
    }
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
        EXPECT_EQ(
            action.apply(
                onRunMock.AsStdFunction(),
                onExitMock.AsStdFunction(),
                onMigrateMock.AsStdFunction(),
                onVerifyMock.AsStdFunction()
            ),
            EXIT_SUCCESS
        );
    }
}

TEST_F(CliArgsTests, Parse_Config)
{
    std::string_view configPath = "some_config_path";
    std::array argv{"clio_server", "--conf", configPath.data()};  // NOLINT(bugprone-suspicious-stringview-data-usage)
    auto const action = CliArgs::parse(argv.size(), argv.data());

    int const returnCode = 123;
    EXPECT_CALL(onRunMock, Call).WillOnce([&configPath](CliArgs::Action::Run const& run) {
        EXPECT_EQ(run.configPath, configPath);
        return returnCode;
    });
    EXPECT_EQ(
        action.apply(
            onRunMock.AsStdFunction(),
            onExitMock.AsStdFunction(),
            onMigrateMock.AsStdFunction(),
            onVerifyMock.AsStdFunction()
        ),
        returnCode
    );
}

TEST_F(CliArgsTests, Parse_VerifyConfig)
{
    std::string_view configPath = "some_config_path";
    std::array argv{"clio_server", configPath.data(), "--verify"};  // NOLINT(bugprone-suspicious-stringview-data-usage)
    auto const action = CliArgs::parse(argv.size(), argv.data());

    int const returnCode = 123;
    EXPECT_CALL(onVerifyMock, Call).WillOnce([&configPath](CliArgs::Action::VerifyConfig const& verify) {
        EXPECT_EQ(verify.configPath, configPath);
        return returnCode;
    });
    EXPECT_EQ(
        action.apply(
            onRunMock.AsStdFunction(),
            onExitMock.AsStdFunction(),
            onMigrateMock.AsStdFunction(),
            onVerifyMock.AsStdFunction()
        ),
        returnCode
    );
}

TEST_F(CliArgsTests, Parse_ConfigDescriptionInvalidPath)
{
    using namespace util::config;
    std::array argv{"clio_server", "--config-description", ""};
    auto const action = CliArgs::parse(argv.size(), argv.data());
    EXPECT_CALL(onExitMock, Call).WillOnce([](CliArgs::Action::Exit const& exit) { return exit.exitCode; });

    EXPECT_EQ(
        action.apply(
            onRunMock.AsStdFunction(),
            onExitMock.AsStdFunction(),
            onMigrateMock.AsStdFunction(),
            onVerifyMock.AsStdFunction()
        ),
        EXIT_FAILURE
    );
}

struct CliArgsTestsWithTmpFile : CliArgsTests {
    TmpFile tmpFile = TmpFile::empty();
};

TEST_F(CliArgsTestsWithTmpFile, Parse_ConfigDescription)
{
    std::array argv{"clio_server", "--config-description", tmpFile.path.c_str()};
    auto const action = CliArgs::parse(argv.size(), argv.data());
    EXPECT_CALL(onExitMock, Call).WillOnce([](CliArgs::Action::Exit const& exit) { return exit.exitCode; });

    // user provide config markdown file name as well
    ASSERT_TRUE(std::filesystem::exists(tmpFile.path));

    EXPECT_EQ(
        action.apply(
            onRunMock.AsStdFunction(),
            onExitMock.AsStdFunction(),
            onMigrateMock.AsStdFunction(),
            onVerifyMock.AsStdFunction()
        ),
        EXIT_SUCCESS
    );
}

TEST_F(CliArgsTestsWithTmpFile, Parse_ConfigDescriptionFileContent)
{
    using namespace util::config;

    std::ofstream file(tmpFile.path);
    ASSERT_TRUE(file.is_open());
    ClioConfigDescription::writeConfigDescriptionToFile(file);
    file.close();

    std::ifstream inFile(tmpFile.path);
    ASSERT_TRUE(inFile.is_open());

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();

    auto const fileContent = buffer.str();
    EXPECT_TRUE(fileContent.find("# Clio Config Description") != std::string::npos);
    EXPECT_TRUE(fileContent.find("This file lists all Clio Configuration definitions in detail.") != std::string::npos);
    EXPECT_TRUE(fileContent.find("## Configuration Details") != std::string::npos);

    // all keys that exist in clio config should be listed in config description file
    for (auto const& key : gClioConfig)
        EXPECT_TRUE(fileContent.find(key.first));
}
