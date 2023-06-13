//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <backend/BackendInterface.h>
#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <log/Logger.h>

#include <boost/algorithm/string.hpp>

namespace Backend {
std::shared_ptr<BackendInterface>
make_Backend(boost::asio::io_context& ioc, clio::Config const& config)
{
    static clio::Logger log{"Backend"};
    log.info() << "Constructing BackendInterface";

    auto const readOnly = config.valueOr("read_only", false);

    auto const type = config.value<std::string>("database.type");
    std::shared_ptr<BackendInterface> backend = nullptr;

    // TODO: retire `cassandra-new` by next release after 2.0
    if (boost::iequals(type, "cassandra") or boost::iequals(type, "cassandra-new"))
    {
        auto cfg = config.section("database." + type);
        backend =
            std::make_shared<Backend::Cassandra::CassandraBackend>(Backend::Cassandra::SettingsProvider{cfg}, readOnly);
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    auto const rng = backend->hardFetchLedgerRangeNoThrow();
    if (rng)
    {
        backend->updateRange(rng->minSequence);
        backend->updateRange(rng->maxSequence);
    }

    log.info() << "Constructed BackendInterface Successfully";
    return backend;
}
}  // namespace Backend
