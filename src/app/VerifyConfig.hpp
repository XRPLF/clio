#pragma once

#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace app {

/**
 * @brief Verifies user's config values are correct
 *
 * @param configPath The path to config
 * @return true if config values are all correct, false otherwise
 */
inline bool
parseConfig(std::string_view configPath)
{
    using namespace util::config;

    auto const json = ConfigFileJson::makeConfigFileJson(configPath);
    if (!json.has_value()) {
        std::cerr << "Error parsing json from config: " << configPath << "\n"
                  << json.error().error << std::endl;
        return false;
    }
    auto const errors = getClioConfig().parse(json.value());
    if (errors.has_value()) {
        for (auto const& err : *errors) {
            std::cerr << "Issues found in provided config '" << configPath << "':\n";
            std::cerr << err.error << std::endl;
        }
        return false;
    }
    return true;
}

}  // namespace app
