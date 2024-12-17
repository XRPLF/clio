
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

#pragma once

#include "util/newconfig/ObjectView.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>
#include <string>

namespace migration::impl {

/**
 * @brief The migrator specification concept
 */
template <typename T, typename Backend>
concept MigratorSpec = requires(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& cfg) {
    // Check that 'name' exists and is a string
    { T::name } -> std::convertible_to<std::string>;

    // Check that 'description' exists and is a string
    { T::description } -> std::convertible_to<std::string>;

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
