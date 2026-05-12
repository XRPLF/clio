#pragma once

#include "util/config/Types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace util::config {

/**
 * @brief The interface for configuration files.
 *
 * This class defines the interface for handling configuration files,
 * which can be implemented for different formats such as JSON or YAML.
 */
class ConfigFileInterface {
public:
    virtual ~ConfigFileInterface() = default;

    /**
     * @brief Retrieves the value of configValue.
     *
     * @param key The key of configuration.
     * @return the value associated with key.
     */
    [[nodiscard]] virtual Value
    getValue(std::string_view key) const = 0;

    /**
     * @brief Retrieves an array of configuration values.
     *
     * @param key The key of the configuration array.
     * @return A vector of configuration values some of which could be nullopt
     */
    [[nodiscard]] virtual std::vector<std::optional<Value>>
    getArray(std::string_view key) const = 0;

    /**
     * @brief Checks if key exist in configuration file.
     *
     * @param key The key to search for.
     * @return true if key exists in configuration file, false otherwise.
     */
    [[nodiscard]] virtual bool
    containsKey(std::string_view key) const = 0;

    /**
     * @brief Retrieves all keys in the configuration file.
     *
     * @return A vector of all keys in the configuration file.
     */
    [[nodiscard]] virtual std::vector<std::string>
    getAllKeys() const = 0;
};

}  // namespace util::config
