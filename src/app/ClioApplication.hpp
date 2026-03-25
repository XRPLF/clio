#pragma once

#include "app/Stopper.hpp"
#include "util/SignalsHandler.hpp"
#include "util/config/ConfigDefinition.hpp"

namespace app {

/**
 * @brief The main application class
 */
class ClioApplication {
    util::config::ClioConfigDefinition const& config_;
    util::SignalsHandler signalsHandler_;
    Stopper appStopper_;

public:
    /**
     * @brief Construct a new ClioApplication object
     *
     * @param config The configuration of the application
     */
    ClioApplication(util::config::ClioConfigDefinition const& config);

    /**
     * @brief Run the application
     *
     * @param useNgWebServer Whether to use the new web server
     *
     * @return exit code
     */
    int
    run(bool useNgWebServer);
};

}  // namespace app
