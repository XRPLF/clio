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
 * @brief Represents a ClickHouse query result.
 */
struct Result {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> columnNames;
    
    /**
     * @brief Get the number of rows in the result
     */
    size_t rowCount() const { return rows.size(); }
    
    /**
     * @brief Get the number of columns in the result
     */
    size_t columnCount() const { return columnNames.size(); }
    
    /**
     * @brief Check if the result is empty
     */
    bool empty() const { return rows.empty(); }
};

} // namespace data::clickhouse::impl
