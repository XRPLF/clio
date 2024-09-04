//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

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
#include "app/ClioApplication.hpp"
#include "rpc/common/impl/HandlerProvider.hpp"
#include "util/TerminationHandler.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

using namespace util::config;

int
main(int argc, char const* argv[])
try {
    util::setTerminationHandler();

    auto const action = app::CliArgs::parse(argc, argv);
    return action.apply(
        [](app::CliArgs::Action::Exit const& exit) { return exit.exitCode; },
        [](app::CliArgs::Action::Run const& run) {
            auto const errors = ClioConfig.parse(ConfigFileJson{run.configPath});
            if (errors.has_value()) {
                for (auto const& err : errors.value())
                    std::cerr << err.error << std::endl;
                return EXIT_FAILURE;
            }
            util::LogService::init(ClioConfig);
            app::ClioApplication clio{ClioConfig};
            return clio.run();
        }
    );
} catch (std::exception const& e) {
    LOG(util::LogService::fatal()) << "Exit on exception: " << e.what();
    return EXIT_FAILURE;
} catch (...) {
    LOG(util::LogService::fatal()) << "Exit on exception: unknown";
    return EXIT_FAILURE;
}
