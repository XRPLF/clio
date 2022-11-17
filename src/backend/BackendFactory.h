#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED

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

    auto readOnly = config.valueOr("read_only", false);
    auto type = config.value<std::string>("database.type");
    std::shared_ptr<BackendInterface> backend = nullptr;

    if (boost::iequals(type, "cassandra"))
    {
        auto cfg = config.section("database." + type);
        auto ttl = config.valueOr<uint32_t>("online_delete", 0) * 4;
        backend = std::make_shared<CassandraBackend>(ioc, cfg, ttl);
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    backend->open(readOnly);
    auto rng = backend->hardFetchLedgerRangeNoThrow();
    if (rng)
    {
        backend->updateRange(rng->minSequence);
        backend->updateRange(rng->maxSequence);
    }

    log.info() << "Constructed BackendInterface Successfully";

    return backend;
}
}  // namespace Backend

#endif  // RIPPLE_REPORTING_BACKEND_FACTORY
