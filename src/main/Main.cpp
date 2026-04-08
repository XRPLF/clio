#include "app/CliArgs.hpp"
#include "app/ClioApplication.hpp"
#include "app/VerifyConfig.hpp"
#include "migration/MigrationApplication.hpp"
#include "rpc/common/impl/HandlerProvider.hpp"
#include "util/ScopeGuard.hpp"
#include "util/TerminationHandler.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

using namespace util::config;

[[nodiscard]]
int
runApp(int argc, char const* argv[])
{
    auto const action = app::CliArgs::parse(argc, argv);
    return action.apply(
        [](app::CliArgs::Action::Exit const& exit) { return exit.exitCode; },
        [](app::CliArgs::Action::VerifyConfig const& verify) {
            if (app::parseConfig(verify.configPath)) {
                std::cout << "Config " << verify.configPath << " is correct" << "\n";
                return EXIT_SUCCESS;
            }
            return EXIT_FAILURE;
        },
        [](app::CliArgs::Action::Run const& run) {
            if (not app::parseConfig(run.configPath))
                return EXIT_FAILURE;

            ClioConfigDefinition const& gClioConfig = getClioConfig();
            PrometheusService::init(gClioConfig);
            if (auto const initSuccess = util::LogService::init(gClioConfig); not initSuccess) {
                std::cerr << initSuccess.error() << std::endl;
                return EXIT_FAILURE;
            }
            app::ClioApplication clio{gClioConfig};
            return clio.run(run.useNgWebServer);
        },
        [](app::CliArgs::Action::Migrate const& migrate) {
            if (not app::parseConfig(migrate.configPath))
                return EXIT_FAILURE;

            if (auto const initSuccess = util::LogService::init(getClioConfig()); not initSuccess) {
                std::cerr << initSuccess.error() << std::endl;
                return EXIT_FAILURE;
            }
            app::MigratorApplication migrator{getClioConfig(), migrate.subCmd};
            return migrator.run();
        }
    );
}

int
main(int argc, char const* argv[])
{
    util::setTerminationHandler();

    util::ScopeGuard const loggerShutdownGuard{[] { util::LogService::shutdown(); }};

    try {
        return runApp(argc, argv);
    } catch (std::exception const& e) {
        LOG(util::LogService::fatal()) << "Exit on exception: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        LOG(util::LogService::fatal()) << "Exit on exception: unknown";
        return EXIT_FAILURE;
    }
}
