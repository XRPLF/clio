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
#include "data/clickhouse/impl/Result.hpp"
#include "util/requests/RequestBuilder.hpp"
#include <string>
#include <memory>

namespace data::clickhouse::impl {

/**
 * @brief Manages a ClickHouse session (connection).
 */
class Session {
public:
    explicit Session(const Settings& settings);
    ~Session() = default;
    
    // Disable copy
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    
    // Allow move
    Session(Session&& other) noexcept = default;
    Session& operator=(Session&& other) noexcept = default;
    
    // Execute a query and return result
    Result query(const std::string& sql) const;
    
    // Execute a query without returning results (INSERT, CREATE, etc.)
    bool execute(const std::string& sql) const;
    
    // Check if session is valid
    bool isValid() const;
    
    // Get last error message
    std::string getLastError() const;

private:
    Settings settings_;
    mutable std::string lastError_;
    bool valid_;
    
    // Build ClickHouse HTTP request parameters
    std::string buildRequestParams(const std::string& sql, bool isQuery = true) const;
    
    // Parse ClickHouse response
    Result parseResponse(const std::string& response) const;
    
    // Helper to create RequestBuilder
    util::requests::RequestBuilder createRequestBuilder() const;
};

} // namespace data::clickhouse::impl
