#pragma once

#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <cstdint>
#include <expected>

namespace rpc {

/**
 * @brief Default API version to use if no version is specified by clients
 */
static constexpr uint32_t kAPI_VERSION_DEFAULT = 1u;

/**
 * @brief Minimum API version supported by this build
 */
static constexpr uint32_t kAPI_VERSION_MIN = 1u;

/**
 * @brief Maximum API version supported by this build
 */
static constexpr uint32_t kAPI_VERSION_MAX = 3u;

/**
 * @brief A baseclass for API version helper
 */
class APIVersionParser {
public:
    virtual ~APIVersionParser() = default;

    /**
     * @brief Extracts API version information from a JSON object.
     *
     * @param request A JSON object representing the request
     * @return The specified API version if contained in the JSON object; error string otherwise
     */
    [[nodiscard]] std::expected<uint32_t, std::string> virtual parse(
        boost::json::object const& request
    ) const = 0;
};

}  // namespace rpc
