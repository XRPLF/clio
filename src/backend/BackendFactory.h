#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED

#include <boost/algorithm/string.hpp>
#include <backend/BackendInterface.h>
#include <backend/CassandraBackend.h>
#include <backend/PostgresBackend.h>
#include <main/Application.h>
#include <main/Config.h>

namespace Backend {
static std::unique_ptr<BackendInterface>
make_Backend(Application const& app)
{
    BOOST_LOG_TRIVIAL(info) << __func__ << ": Constructing BackendInterface";

    DatabaseConfig const& dbConfig = app.config().database;

    bool readOnly = app.config().readOnly;

    std::unique_ptr<BackendInterface> backend = nullptr;

    if (std::holds_alternative<CassandraConfig>(dbConfig))
    {
        backend = std::make_unique<CassandraBackend>(app);
    }
    else if (std::holds_alternative<PostgresConfig>(dbConfig))
    {
        backend = std::make_unique<PostgresBackend>(app);
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

    BOOST_LOG_TRIVIAL(info)
        << __func__ << ": Constructed BackendInterface Successfully";

    return backend;
}
}  // namespace Backend

#endif  // RIPPLE_REPORTING_BACKEND_FACTORY
