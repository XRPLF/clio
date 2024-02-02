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

#include "data/BackendInterface.hpp"
#include "data/CassandraBackend.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace data {

/**
 * @brief A factory function that creates the backend based on a config.
 *
 * @param config The clio config to use
 * @return A shared_ptr<BackendInterface> with the selected implementation
 */
inline std::shared_ptr<BackendInterface>
make_Backend(util::Config const& config)
{
    static util::Logger const log{"Backend"};
    LOG(log.info()) << "Constructing BackendInterface";

    auto const readOnly = config.valueOr("read_only", false);

    auto const type = config.value<std::string>("database.type");
    std::shared_ptr<BackendInterface> backend = nullptr;

    // TODO: retire `cassandra-new` by next release after 2.0
    if (boost::iequals(type, "cassandra") or boost::iequals(type, "cassandra-new")) {
        auto cfg = config.section("database." + type);
        backend = std::make_shared<data::cassandra::CassandraBackend>(data::cassandra::SettingsProvider{cfg}, readOnly);
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    auto const rng = backend->hardFetchLedgerRangeNoThrow();
    if (rng)
        backend->setRange(rng->minSequence, rng->maxSequence);

    LOG(log.info()) << "Constructed BackendInterface Successfully";
    return backend;
}
}  // namespace data
