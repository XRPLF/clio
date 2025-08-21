//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE  IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT,  OR  CONSEQUENTIAL  DAMAGES  OR  ANY
    DAMAGES  WHATSOEVER  RESULTING  FROM  LOSS  OF  USE,  DATA  OR  PROFITS,
    WHETHER  IN  AN  ACTION  OF  CONTRACT,  NEGLIGENCE  OR  OTHER  TORTIOUS
    ACTION,  ARISING  OUT  OF  OR  IN  CONNECTION  WITH  THE  USE  OR
    PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/clickhouse/impl/Settings.hpp"
#include "data/clickhouse/impl/Session.hpp"
#include <memory>

namespace data::clickhouse::impl {

/**
 * @brief Manages a ClickHouse cluster connection.
 */
class Cluster {
public:
    explicit Cluster(const Settings& settings);
    ~Cluster() = default;
    
    // Disable copy
    Cluster(const Cluster&) = delete;
    Cluster& operator=(const Cluster&) = delete;
    
    // Allow move
    Cluster(Cluster&& other) noexcept = default;
    Cluster& operator=(Cluster&& other) noexcept = default;
    
    // Create a new session
    std::unique_ptr<Session> createSession();
    
    // Check if cluster is valid
    bool isValid() const;
    
    // Get last error message
    std::string getLastError() const;

private:
    Settings settings_;
    std::string lastError_;
    bool valid_;
};

} // namespace data::clickhouse::impl
