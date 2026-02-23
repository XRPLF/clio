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

#include "migration/MigrationApplication.hpp"
#include "util/build/Build.hpp"
#include "util/config/ConfigDescription.hpp"

#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace app {

CliArgs::Action
CliArgs::parse(int argc, char const* argv[])
{
    namespace po = boost::program_options;
    // clang-format off
    po::options_description description("Options");
    description.add_options()
        ("help,h", "Print help message and exit")
        ("version,v", "Print version and exit")
        ("conf,c", po::value<std::string>()->default_value(kDEFAULT_CONFIG_PATH), "Configuration file")
        ("ng-web-server,w", "Use ng-web-server")
        ("migrate", po::value<std::string>(), "Start migration helper")
        ("verify", "Checks the validity of config values")
        ("config-description,d", po::value<std::string>(), "Generate config description markdown file")
    ;
    // clang-format on
    po::positional_options_description positional;
    positional.add("conf", 1);

    auto const printHelp = [&description]() {
        std::cout << "Clio server " << util::build::getClioFullVersionString() << "\n\n"
                  << description;
    };

    po::variables_map parsed;
    try {
        po::store(
            po::command_line_parser(argc, argv).options(description).positional(positional).run(),
            parsed
        );
        po::notify(parsed);
    } catch (po::error const& e) {
        std::cerr << "Error: " << e.what() << std::endl << std::endl;
        printHelp();
        return Action{Action::Exit{EXIT_FAILURE}};
    }

    if (parsed.contains("help")) {
        printHelp();
        return Action{Action::Exit{EXIT_SUCCESS}};
    }

    if (parsed.contains("version")) {
        std::cout << util::build::getClioFullVersionString() << '\n'
                  << "Git commit hash: " << util::build::getGitCommitHash() << '\n'
                  << "Git build branch: " << util::build::getGitBuildBranch() << '\n'
                  << "Build date: " << util::build::getBuildDate() << '\n';
        return Action{Action::Exit{EXIT_SUCCESS}};
    }

    if (parsed.contains("config-description")) {
        std::filesystem::path const filePath = parsed["config-description"].as<std::string>();

        auto const res =
            util::config::ClioConfigDescription::generateConfigDescriptionToFile(filePath);
        if (res.has_value())
            return Action{Action::Exit{EXIT_SUCCESS}};

        std::cerr << res.error().error << std::endl;
        return Action{Action::Exit{EXIT_FAILURE}};
    }

    auto configPath = parsed["conf"].as<std::string>();

    if (parsed.contains("migrate")) {
        auto const opt = parsed["migrate"].as<std::string>();
        if (opt == "status") {
            return Action{Action::Migrate{
                .configPath = std::move(configPath), .subCmd = MigrateSubCmd::status()
            }};
        }
        return Action{Action::Migrate{
            .configPath = std::move(configPath), .subCmd = MigrateSubCmd::migration(opt)
        }};
    }

    if (parsed.contains("verify"))
        return Action{Action::VerifyConfig{.configPath = std::move(configPath)}};

    return Action{Action::Run{
        .configPath = std::move(configPath), .useNgWebServer = parsed.contains("ng-web-server")
    }};
}

}  // namespace app
