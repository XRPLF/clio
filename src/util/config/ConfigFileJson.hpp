#pragma once

#include "util/config/ConfigFileInterface.hpp"
#include "util/config/Error.hpp"
#include "util/config/Types.hpp"

#include <boost/json/object.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace util::config {

/** @brief Json representation of config */
class ConfigFileJson final : public ConfigFileInterface {
    boost::json::object jsonObject_;

public:
    /**
     * @brief Construct a new ConfigJson object and stores the values from
     * user's config into a json object.
     *
     * @param jsonObj the Json object to parse; represents user's config
     */
    ConfigFileJson(boost::json::object jsonObj);

    /**
     * @brief Retrieves a configuration value by its key.
     *
     * @param key The key of the configuration value to retrieve.
     * @return A variant containing the same type corresponding to the extracted value.
     */
    [[nodiscard]] Value
    getValue(std::string_view key) const override;

    /**
     * @brief Retrieves an array of configuration values by its key.
     *
     * @param key The key of the configuration array to retrieve.
     * @return A vector of variants holding the config values specified by user.
     */
    [[nodiscard]] std::vector<std::optional<Value>>
    getArray(std::string_view key) const override;

    /**
     * @brief Checks if the configuration contains a specific key.
     *
     * @param key The key to check for.
     * @return True if the key exists, false otherwise.
     */
    [[nodiscard]] bool
    containsKey(std::string_view key) const override;

    /**
     * @brief Retrieves all keys in the configuration file.
     *
     * @return A vector of all keys in the configuration file.
     */
    [[nodiscard]] std::vector<std::string>
    getAllKeys() const override;

    /**
     * @brief Creates a new ConfigFileJson by parsing the provided JSON file and
     * stores the values in the object.
     *
     * @param configFilePath The path to the JSON file to be parsed.
     * @return A ConfigFileJson object if parsing user file is successful. Error otherwise
     */
    [[nodiscard]] static std::expected<ConfigFileJson, Error>
    makeConfigFileJson(std::filesystem::path const& configFilePath);

    /**
     * @brief Get the inner representation of json file.
     * @note This method is mostly used for testing purposes.
     *
     * @return The inner representation of json file.
     */
    [[nodiscard]] boost::json::object const&
    inner() const;

private:
    /**
     * @brief Method to flatten a JSON object into the same structure as the Clio Config.
     *
     * The keys will end up having the same naming conventions in Clio Config.
     * Other than the keys specified in user Config file, no new keys are created.
     *
     * @param obj The JSON object to flatten.
     */
    void
    flattenJson(boost::json::object const& jsonRootObject);
};

}  // namespace util::config
