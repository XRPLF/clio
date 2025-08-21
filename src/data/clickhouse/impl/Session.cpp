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

#include "data/clickhouse/impl/Session.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace data::clickhouse::impl {

Session::Session(const Settings& settings) 
    : settings_(settings), lastError_(""), valid_(true) {
}

std::string Session::buildRequestParams(const std::string& sql, bool isQuery) const {
    std::ostringstream oss;
    
    // Build ClickHouse HTTP request parameters
    oss << "query=" << sql;
    
    // Add database parameter if specified
    if (!settings_.connectionInfo.database.empty()) {
        oss << "&database=" << settings_.connectionInfo.database;
    }
    
    // Add format parameter for query results
    if (isQuery) {
        oss << "&default_format=JSONEachRow";
    }
    
    return oss.str();
}

util::requests::RequestBuilder Session::createRequestBuilder() const {
    return util::requests::RequestBuilder(
        settings_.connectionInfo.host, 
        std::to_string(settings_.connectionInfo.port)
    );
}

Result Session::parseResponse(const std::string& response) const {
    Result result;
    // parse the response into rows and column names
    // (placeholder for now)
    result.columnNames = {"result"};
    result.rows = {{response}};
    return result;
}

Result Session::query(const std::string& sql) const {
    if (!valid_) {
        lastError_ = "Session not valid";
        return Result{};
    }
    
    try {
        auto builder = createRequestBuilder();
        builder.setTarget("/")
               .addData(buildRequestParams(sql, true));
        
        // TODO(NODE-2688): implement actual async HTTP request using boost::asio::spawn
        // auto response = builder.postPlain(yield);
        // if (response) {
        //     return parseResponse(*response);
        // } else {
        //     lastError_ = "HTTP request failed";
        //     return Result{};
        // }
        
        // placeholder...
        Result result;
        result.columnNames = {"query", "status"};
        result.rows = {{sql, "executed"}};
        return result;
        
    } catch (const std::exception& e) {
        lastError_ = "Query failed: " + std::string(e.what());
        return Result{};
    }
}

bool Session::execute(const std::string& sql) const {
    if (!valid_) {
        lastError_ = "Session not valid";
        return false;
    }
    
    try {
        auto builder = createRequestBuilder();
        builder.setTarget("/")
               .addData(buildRequestParams(sql, false));
        
        // TODO(NODE-2688): Implement actual async HTTP request using boost::asio::spawn
        // auto response = builder.postPlain(yield);
        // return response.has_value();
        
        // placeholder...
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = "Execute failed: " + std::string(e.what());
        return false;
    }
}

bool Session::isValid() const {
    return valid_;
}

std::string Session::getLastError() const {
    return lastError_;
}

} // namespace data::clickhouse::impl
