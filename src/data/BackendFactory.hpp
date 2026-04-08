#pragma once

#include "data/BackendInterface.hpp"
#include "data/CassandraBackend.hpp"
#include "data/KeyspaceBackend.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "util/config/ConfigDefinition.hpp"
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
 * @param cache The ledger cache to use
 * @return A shared_ptr<BackendInterface> with the selected implementation
 */
inline std::shared_ptr<BackendInterface>
makeBackend(util::config::ClioConfigDefinition const& config, data::LedgerCacheInterface& cache)
{
    using namespace cassandra::impl;
    static util::Logger const log{"Backend"};  // NOLINT(readability-identifier-naming)
    LOG(log.info()) << "Constructing BackendInterface";

    auto const readOnly = config.get<bool>("read_only");

    auto const type = config.get<std::string>("database.type");
    std::shared_ptr<BackendInterface> backend = nullptr;

    if (boost::iequals(type, "cassandra")) {
        auto const cfg = config.getObject("database." + type);
        if (providerFromString(cfg.getValueView("provider").asString()) == Provider::Keyspace) {
            backend = std::make_shared<data::cassandra::KeyspaceBackend>(
                data::cassandra::SettingsProvider{cfg}, cache, readOnly
            );
        } else {
            backend = std::make_shared<data::cassandra::CassandraBackend>(
                data::cassandra::SettingsProvider{cfg}, cache, readOnly
            );
        }
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
