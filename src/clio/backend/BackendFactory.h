#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED

#include <boost/algorithm/string.hpp>
#include <clio/backend/BackendInterface.h>
#include <clio/backend/CassandraBackend.h>
#include <clio/backend/PostgresBackend.h>
#include <test/backend/MockBackend.h>
#include <clio/main/Application.h>
#include <clio/main/Config.h>
#include <test/backend/MockBackend.h>

namespace Backend {
static std::unique_ptr<BackendInterface>
make_Backend(Application const& app)
{
    BOOST_LOG_TRIVIAL(info) << __func__ << ": Constructing BackendInterface";

    DatabaseConfig const& dbConfig = app.config().database;

    bool readOnly = app.config().readOnly;

    std::unique_ptr<BackendInterface> backend = nullptr;

    if (dbConfig.type == "cassandra")
    {
        backend = std::make_unique<CassandraBackend>(app);
    }
    else if (dbConfig.type == "postgres")
    {
        backend = std::make_unique<PostgresBackend>(app);
    }
    else if (dbConfig.type == "mock")
    {
        backend = std::make_unique<MockBackend>(app);
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
