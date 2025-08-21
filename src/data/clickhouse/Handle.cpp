//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/clickhouse/Handle.hpp"

namespace data::clickhouse {

Handle::Handle(Settings const& settings)
{
    initialize(settings);
}

Handle::~Handle() = default;

Handle::Handle(Handle&&) noexcept = default;

Handle&
Handle::operator=(Handle&&) noexcept = default;

void
Handle::initialize(Settings const& settings)
{
    connection_ = std::make_unique<impl::Connection>(settings);
}

MaybeError
Handle::connect()
{
    if (!connection_) {
        return Error{ClickHouseError{"No connection available"}};
    }

    if (connection_->connect()) {
        return {};
    } else {
        return Error{ClickHouseError{"Failed to connect to ClickHouse"}};
    }
}

MaybeError
Handle::execute(std::string const& query)
{
    if (!connection_ || !connection_->isConnected()) {
        return Error{ClickHouseError{"Not connected to ClickHouse"}};
    }

    if (connection_->execute(query)) {
        return {};
    } else {
        return Error{ClickHouseError{"Failed to execute query: " + query}};
    }
}

MaybeError
Handle::executeEach(std::vector<std::string> const& queries)
{
    for (auto const& query : queries) {
        if (auto const result = execute(query); !result) {
            return result;
        }
    }
    return {};
}

ResultOrError
Handle::query(std::string const& /*query*/)
{
    if (!connection_ || !connection_->isConnected()) {
        return Error{ClickHouseError{"Not connected to ClickHouse"}};
    }

    // For now, return a simple result
    Result result;
    result.columnNames = {"status"};
    result.rows = {{"success"}};
    return result;
}

bool
Handle::isConnected() const
{
    return connection_ && connection_->isConnected();
}

}  // namespace data::clickhouse
