#pragma once

#include "util/config/ObjectView.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>
#include <string>

namespace migration::impl {

/**
 * @brief The migrator specification concept
 */
template <typename T, typename Backend>
concept MigratorSpec =
    requires(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& cfg) {
        // Check that 'kName' exists and is a string
        { T::kName } -> std::convertible_to<std::string>;

        // Check that 'kDescription' exists and is a string
        { T::kDescription } -> std::convertible_to<std::string>;

        // Check that the migrator specifies the backend type it supports
        typename T::Backend;

        // Check that 'runMigration' exists and is callable
        { T::runMigration(backend, cfg) } -> std::same_as<void>;
    };

/**
 * @brief used by variadic template to check all migrators are MigratorSpec
 */
template <typename... Types>
concept AllMigratorSpec = (MigratorSpec<Types, typename Types::Backend> && ...);

}  // namespace migration::impl
