#pragma once

#include <fmt/format.h>

#include <string>
#include <string_view>
#include <utility>

namespace util::config {

/** @brief Displays the different errors when parsing user config */
struct Error {
    /**
     * @brief Constructs an Error with a custom error message.
     *
     * @param err the error message to display to users.
     */
    Error(std::string err) : error{std::move(err)}
    {
    }

    /**
     * @brief Constructs an Error with a custom error message.
     *
     * @param key the key associated with the error.
     * @param err the error message to display to users.
     */
    Error(std::string_view key, std::string_view err)
        : error{
              fmt::format("The value of {} {}", key, err),
          }
    {
    }

    std::string error;
};

}  // namespace util::config
