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

#include <string>
#include <vector>

namespace data::clickhouse::impl {

/**
 * @brief Represents a ClickHouse prepared statement.
 */
class Statement {
public:
    Statement() = default;
    explicit Statement(std::string query);
    ~Statement() = default;
    
    // Disable copy
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    
    // Allow move
    Statement(Statement&& other) noexcept = default;
    Statement& operator=(Statement&& other) noexcept = default;
    
    // Get the query string
    const std::string& getQuery() const;
    
    // Set the query string
    void setQuery(const std::string& query);

private:
    std::string query_;
};

} // namespace data::clickhouse::impl
