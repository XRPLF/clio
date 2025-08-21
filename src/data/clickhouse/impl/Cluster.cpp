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

#include "data/clickhouse/impl/Cluster.hpp"

namespace data::clickhouse::impl {

Cluster::Cluster(const Settings& settings) 
    : settings_(settings), lastError_(""), valid_(true) {
}

std::unique_ptr<Session> Cluster::createSession() {
    if (!valid_) {
        lastError_ = "Cluster not valid";
        return nullptr;
    }
    
    try {
        return std::make_unique<Session>(settings_);
    } catch (const std::exception& e) {
        lastError_ = "Failed to create session: " + std::string(e.what());
        return nullptr;
    }
}

bool Cluster::isValid() const {
    return valid_;
}

std::string Cluster::getLastError() const {
    return lastError_;
}

} // namespace data::clickhouse::impl
